/*
 * npu_runtime — reusable NPU execution layer for compiled SHAVE kernels.
 *
 * A ggml/llama.cpp backend (or any host program) links this library to run a
 * SHAVE kernel on the Meteor Lake NPU with one call per invocation, without
 * respawning the compile/patch/bundle/probe CLI stages.
 *
 * Contract (all measured on the MTL NPU, 2026-09-28):
 *   - warm() brings up Level Zero once: loader, NPU device, context, graph
 *     extension entry points, compute queue group.
 *   - npurt_kernel_create() is the per-KERNEL stage: it patches a carrier blob
 *     with the kernel ELF, compiles the graph once, initializes it and keeps
 *     the resulting device graph handle alive. Kernels are cached by the
 *     SHA-256 of the patched graph blob.
 *   - npurt_run() is the per-INVOCATION stage: copy input to the graph's input
 *     allocation, reset the output allocation, submit one graph execute on a
 *     fresh command list and wait on a fence.
 *
 * Threading: every entry point takes one process-global lock, so calls are
 * safe from any thread but serialize (single-threaded queue semantics). One
 * NPU device, one graph execution at a time. Measured: 4 threads x 50 calls
 * complete with zero failures at ~1.14 ms/call (against ~1.00 ms single
 * threaded).
 *
 * Shapes are static: the tensor sizes come from the carrier graph baked into
 * the blob and cannot change at run time; mismatched buffers are rejected.
 *
 * This header is C89/C99-clean C and is included by C++ callers unchanged.
 */
#ifndef NPU_RUNTIME_H
#define NPU_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NPURT_DIAGNOSTIC_BYTES 512
#define NPURT_SHA256_HEX_BYTES 65
#define NPURT_MAX_TENSORS 8
#define NPURT_PATH_BYTES 512

typedef enum npurt_status {
  NPURT_OK = 0,
  NPURT_ERR_ARGUMENT = 1,
  NPURT_ERR_STATE = 2,
  NPURT_ERR_IO = 3,
  NPURT_ERR_COMPILE = 4,
  NPURT_ERR_PATCH = 5,
  NPURT_ERR_DEVICE = 6,
  NPURT_ERR_NO_NPU = 7,
  NPURT_ERR_TIMEOUT = 8,
  NPURT_ERR_SHAPE = 9,
  NPURT_ERR_NOT_FOUND = 10,
  NPURT_ERR_UNSUPPORTED = 11,
  NPURT_ERR_OUT_OF_MEMORY = 12
} npurt_status;

/* Stable short name ("ok", "device", "timeout", ...); never NULL. */
const char *npurt_status_name(npurt_status status);

typedef struct npurt_diagnostic {
  char message[NPURT_DIAGNOSTIC_BYTES];
} npurt_diagnostic;

/* Zeroes the diagnostic; safe on NULL. */
void npurt_diagnostic_init(npurt_diagnostic *diagnostic);

/* ------------------------------------------------------------------------- */
/* Configuration                                                             */
/* ------------------------------------------------------------------------- */

typedef enum npurt_precision {
  NPURT_PRECISION_UNKNOWN = 0,
  NPURT_PRECISION_FP32 = 1,
  NPURT_PRECISION_FP16 = 2
} npurt_precision;

/*
 * Runtime configuration. Zero-initialize and then call npurt_config_defaults()
 * so that environment overrides and the build-time defaults are applied; the
 * fields with sentinel values are resolved there.
 */
#define NPURT_MAX_LINK_LIBRARIES 4
#define NPURT_LINK_LIBRARY_BYTES 64

typedef struct npurt_config {
  /* SHAVE toolchain root containing bin/{moviCompile,moviAsm,moviLLD}. Only
   * needed by npurt_compile_source(). */
  char toolchain_dir[NPURT_PATH_BYTES];
  /* Carrier graph blob that the kernel is patched into. */
  char carrier_blob[NPURT_PATH_BYTES];
  /* Linker script used by the kernel link stage. */
  char link_script[NPURT_PATH_BYTES];
  /* Include directory providing <npunlock/npu3720_kernel.h>. */
  char include_dir[NPURT_PATH_BYTES];
  /* Scratch directory for compile artifacts (created on demand). */
  char work_dir[NPURT_PATH_BYTES];
  /* Entry symbol of the compiled kernel. */
  char entry_symbol[128];
  /* Extra flags appended to the moviCompile command line (single string). */
  char extra_cflags[512];
  /* Optional link libraries without path or extension, e.g. {"mlibm"}. */
  char link_libraries[NPURT_MAX_LINK_LIBRARIES][NPURT_LINK_LIBRARY_BYTES];
  size_t link_library_count;
  /* UINT32_MAX selects the first Level Zero VPU device. */
  uint32_t device_index;
  /*
   * Soft cache limit for created kernels; 0 selects the default (8). The
   * runtime never destroys a handle the caller still holds, so exceeding the
   * limit only logs a warning (and bumps npurt_cache_stats::over_capacity):
   * call npurt_kernel_free() for kernels you no longer need.
   */
  uint32_t max_cached_kernels;
  /* Fence timeout used when npurt_run_request::timeout_ns is 0; 0 selects 5 s. */
  uint64_t default_timeout_ns;
  /* Per-stage timeout for the toolchain stages; 0 selects 25 s. */
  uint64_t compile_timeout_ns;
  /* 0 = silent, 1 = warnings on stderr, 2 = stage trace on stderr. */
  int verbosity;
  /* 1 = call zeGraphSetArgument before every execution (default, verified). */
  int rebind_arguments_each_run;
} npurt_config;

/* Fills *config with build-time defaults overridden by the environment:
 *   NPURT_NPUNLOCK_ROOT, NPURT_TOOLCHAIN_DIR, NPURT_CARRIER_BLOB,
 *   NPURT_INCLUDE_DIR, NPURT_WORK_DIR, NPURT_ENTRY_SYMBOL, NPURT_EXTRA_CFLAGS.
 */
void npurt_config_defaults(npurt_config *config);

/* Applies *config to the process-global runtime. Must be called before
 * npurt_warm()/npurt_kernel_create() to take effect; passing NULL restores
 * the defaults. Copies every string. */
npurt_status npurt_configure(const npurt_config *config, npurt_diagnostic *diagnostic);

/* ------------------------------------------------------------------------- */
/* Device lifecycle                                                          */
/* ------------------------------------------------------------------------- */

/* Explicit bring-up: Level Zero loader, NPU device, context, graph extension
 * and queue discovery. Idempotent. Also performed implicitly by the first
 * npurt_kernel_create(). */
npurt_status npurt_warm(npurt_diagnostic *diagnostic);

int npurt_is_warm(void);

/*
 * Ends the session: releases every cached kernel (graph handles and their host
 * allocations) and refuses further npurt_run() calls until npurt_warm() is
 * called again. Idempotent, safe at process exit.
 *
 * The Level Zero loader/driver/device/context themselves live for the whole
 * process: measured with tools/session_probe.c, destroying the context and
 * re-initialising the driver inside one process leaves the loader such that
 * the next submit faults inside libze_loader.so.1 (unloading the loader first
 * does not help). npurt_warm() therefore re-arms the existing session, which
 * is cheap and crash-free (verified 5/5).
 */
void npurt_shutdown(void);

/* ------------------------------------------------------------------------- */
/* Toolchain (per kernel, optional)                                          */
/* ------------------------------------------------------------------------- */

/* Compiles a C kernel source to a SHAVE ELF through the configured toolchain
 * (moviCompile -> moviAsm -> moviLLD). Returns a malloc'd buffer that the
 * caller releases with npurt_free(). */
npurt_status npurt_compile_source(const char *source, size_t source_size, uint8_t **elf,
                                  size_t *elf_size, npurt_diagnostic *diagnostic);

/* npurt_malloc/npurt_free exist so callers never mix allocators across a DLL
 * boundary; both are plain malloc/free on this platform. */
void *npurt_malloc(size_t size);
void npurt_free(void *pointer);

/* ------------------------------------------------------------------------- */
/* Kernels (per kernel, cached)                                              */
/* ------------------------------------------------------------------------- */

typedef struct npurt_kernel npurt_kernel;

typedef struct npurt_tensor_info {
  uint32_t argument_index;
  uint32_t precision; /* npurt_precision */
  uint32_t element_size;
  uint32_t dims_count;
  uint32_t dims[5];
  uint64_t element_count;
  uint64_t byte_size;
  int is_input;
  char name[64];
} npurt_tensor_info;

typedef struct npurt_kernel_info {
  char elf_sha256[NPURT_SHA256_HEX_BYTES];
  char blob_sha256[NPURT_SHA256_HEX_BYTES];
  uint64_t elf_bytes;
  uint64_t blob_bytes;
  uint32_t patch_target_count;
  uint32_t invocation_count;
  uint64_t invocation_element_count;
  uint64_t invocation_span_bytes;
  uint64_t patch_ns;       /* patchblob: carrier + ELF -> graph blob */
  uint64_t graph_create_ns;/* pfnCreate2: driver graph compile/bind */
  uint64_t query_ns;       /* argument discovery + host allocations */
  uint64_t init_ns;        /* graph initialize (skipped when not required) */
  uint64_t setup_ns;       /* total npurt_kernel_create, cold */
  uint32_t input_count;
  uint32_t output_count;
  uint32_t tensor_count;
  npurt_tensor_info tensors[NPURT_MAX_TENSORS];
  uint64_t run_count;
} npurt_kernel_info;

/* Patches, loads, initializes and caches one kernel. The cache is keyed by the
 * SHA-256 of the generated graph blob, so a repeated call with the same ELF
 * returns the same handle without touching the device. */
npurt_status npurt_kernel_create(const uint8_t *elf, size_t elf_size, npurt_kernel **kernel,
                                 npurt_diagnostic *diagnostic);

/* Cache probe: looks up an ELF already prepared by npurt_kernel_create().
 * Returns NPURT_ERR_NOT_FOUND when the kernel is not cached. */
npurt_status npurt_kernel_lookup(const uint8_t *elf, size_t elf_size, npurt_kernel **kernel);

/* Creates a kernel from an already patched graph blob (the path a build-time
 * pipeline such as ggml-shave's embedded kernels uses). Cached by blob hash. */
npurt_status npurt_kernel_create_from_blob(const uint8_t *blob, size_t blob_size,
                                           npurt_kernel **kernel, npurt_diagnostic *diagnostic);

/* Patches several ACT groups of one carrier with different kernel ELFs, in
 * order, and loads the result. `group_index` selects the discovered ACT group
 * (UINT32_MAX patches every discovered target, like the patchblob CLI). */
typedef struct npurt_patch_entry {
  const uint8_t *elf;
  size_t elf_size;
  uint32_t group_index;
} npurt_patch_entry;

npurt_status npurt_kernel_create_layered(const npurt_patch_entry *entries, size_t entry_count,
                                         npurt_kernel **kernel, npurt_diagnostic *diagnostic);

void npurt_kernel_info_get(const npurt_kernel *kernel, npurt_kernel_info *info);

/* Drops one kernel from the cache and releases its device resources. */
void npurt_kernel_free(npurt_kernel *kernel);

typedef struct npurt_cache_stats {
  uint32_t entries;
  uint32_t capacity;
  uint64_t hits;
  uint64_t misses;
  uint64_t over_capacity; /* inserts that exceeded `capacity` */
} npurt_cache_stats;

void npurt_cache_stats_get(npurt_cache_stats *stats);

/* Creates an independent session for the same kernel image: same graph blob,
 * its own graph handle and host allocations. Batches of clones can be submitted
 * in one command list (see npurt_run_batch), which is how several invocations
 * overlap; a clone is not part of the kernel cache and must be released with
 * npurt_kernel_free(). */
npurt_status npurt_kernel_clone(const npurt_kernel *kernel, npurt_kernel **clone,
                                npurt_diagnostic *diagnostic);

/* ------------------------------------------------------------------------- */
/* Execution (per invocation)                                                */
/* ------------------------------------------------------------------------- */

typedef struct npurt_run_request {
  const void *input; /* graph input tensor bytes */
  size_t input_size;
  void *output; /* graph output tensor bytes, overwritten */
  size_t output_size;
  /* Optional shape assertion against the graph's declared input dims. */
  const uint32_t *dims;
  uint32_t dims_count;
  /* 0 selects npurt_config::default_timeout_ns. */
  uint64_t timeout_ns;
} npurt_run_request;

typedef struct npurt_timing {
  uint64_t prepare_ns;  /* copy in, reset out, optional rebind */
  uint64_t session_ns;  /* command list + fence create, graph append, close */
  uint64_t submit_ns;   /* zeCommandQueueExecuteCommandLists (host call) */
  uint64_t wait_ns;     /* zeFenceHostSynchronize until the device is done */
  uint64_t teardown_ns; /* list/fence destroy */
  uint64_t total_ns;    /* whole npurt_run, including locking and validation */
  uint64_t run_index;   /* 0 for the first execution on this handle */
} npurt_timing;

/*
 * Multi-input form for graphs whose tensors are exposed as separate arguments
 * (for example an IR-built two-operand carrier: `Maximum(1,N)` with two host
 * inputs). `npurt_run` is the single-input convenience wrapper.
 */
typedef struct npurt_run_inputs_request {
  const void *const *inputs;   /* input_count buffers, in graph argument order */
  const size_t *input_sizes;
  size_t input_count;
  void *output;
  size_t output_size;
  const uint32_t *dims;        /* optional assertion against input 0 */
  uint32_t dims_count;
  uint64_t timeout_ns;
} npurt_run_inputs_request;

npurt_status npurt_run_inputs(npurt_kernel *kernel, const npurt_run_inputs_request *request,
                              npurt_timing *timing, npurt_diagnostic *diagnostic);


/*
 * One graph execution: copy the input tile into the graph's host allocation,
 * reset the output, submit one graph execute and wait on a fence.
 *
 * `output` is reset to quiet NaN (0x7e00 for FP16, 0x7fc00000 for FP32)
 * before submission, so unwritten elements are visible instead of showing
 * stale data from the previous invocation.
 *
 * Shapes are static: `input_size`, `output_size` and the optional `dims` are
 * asserted against the driver-reported graph tensors and any mismatch is
 * rejected with NPURT_ERR_SHAPE before the device is touched.
 *
 * NPURT_ERR_TIMEOUT means the fence did not signal within `timeout_ns`.
 * Measured (tools/run_checks.sh step 5): a 100 us timeout on a 1 ms
 * invocation returns NPURT_ERR_TIMEOUT and the following invocations on the
 * same handle still succeed, so the caller may retry.
 */
npurt_status npurt_run(npurt_kernel *kernel, const npurt_run_request *request, npurt_timing *timing,
                       npurt_diagnostic *diagnostic);

/*
 * Submits `count` invocations (one per kernel handle, in order) as a single
 * queue submission: one command list, one fence, one wait. Each handle keeps
 * its own input/output allocation, so every invocation produces its own
 * result. Measured on the 1x16 carrier: a single submission of N executions
 * costs ~0.55 ms + N x 0.40 ms against N x 0.96 ms when each call waits for
 * its own completion.
 *
 * All handles must be usable kernel sessions (clones of one another or of the
 * same image works); `requests[i]` describes the buffer sizes and the timeout
 * for handle `kernels[i]`. `timing` reports the whole batch.
 */
npurt_status npurt_run_batch(npurt_kernel *const *kernels, const npurt_run_request *requests,
                             size_t count, npurt_timing *timing, npurt_diagnostic *diagnostic);

/* The same batch, with the full multi-input request per entry (one entry per
 * kernel handle, all of them submitted together). */
npurt_status npurt_run_batch_inputs(npurt_kernel *const *kernels,
                                    const npurt_run_inputs_request *requests, size_t count,
                                    npurt_timing *timing, npurt_diagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* NPU_RUNTIME_H */
