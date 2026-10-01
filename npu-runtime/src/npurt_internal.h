/* Internal declarations shared by the npu-runtime translation units. */
#ifndef NPURT_INTERNAL_H
#define NPURT_INTERNAL_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "npu_runtime.h"

/* Level Zero and npunlock declarations are consumed read-only from the
 * npunlock tree (include/npunlock, src/workers, src/linux_probe). */
#include "level_zero_min.h"
#include "npunlock/patchblob.h"

#define NPURT_CONFIG_PATH_BYTES 512

/* --- utility -------------------------------------------------------------- */

uint64_t npurt_now_ns(void);
void npurt_sha256_hex(const uint8_t *data, size_t size, char hex[NPURT_SHA256_HEX_BYTES]);
void npurt_set_diagnostic(npurt_diagnostic *diagnostic, const char *format, ...);
int npurt_verbosity(void);

/* Snprintf into the diagnostic; keeps the buffer NUL-terminated. */
#define NPURT_DIAG(diag, ...)                                                                      \
  do {                                                                                             \
    if ((diag) != NULL) {                                                                          \
      npurt_set_diagnostic((diag), __VA_ARGS__);                                                   \
    }                                                                                              \
  } while (0)

/* --- configuration -------------------------------------------------------- */

/* --- Level Zero binding --------------------------------------------------- */

typedef struct npurt_ze_functions {
  npunlock_ze_init_fn init;
  npunlock_ze_driver_get_fn driver_get;
  npunlock_ze_init_drivers_fn init_drivers;
  npunlock_ze_driver_get_properties_fn driver_get_properties;
  npunlock_ze_driver_get_extensions_fn driver_get_extensions;
  npunlock_ze_driver_get_extension_address_fn driver_get_extension_address;
  npunlock_ze_device_get_fn device_get;
  npunlock_ze_device_get_properties_fn device_get_properties;
  npunlock_ze_context_create_fn context_create;
  npunlock_ze_context_destroy_fn context_destroy;
  npunlock_ze_device_get_queue_groups_fn device_get_queue_groups;
  npunlock_ze_mem_alloc_host_fn mem_alloc_host;
  npunlock_ze_mem_free_fn mem_free;
  npunlock_ze_command_queue_create_fn command_queue_create;
  npunlock_ze_command_queue_destroy_fn command_queue_destroy;
  npunlock_ze_command_queue_execute_fn command_queue_execute;
  npunlock_ze_command_list_create_fn command_list_create;
  npunlock_ze_command_list_close_fn command_list_close;
  npunlock_ze_command_list_destroy_fn command_list_destroy;
  npunlock_ze_fence_create_fn fence_create;
  npunlock_ze_fence_destroy_fn fence_destroy;
  npunlock_ze_fence_synchronize_fn fence_synchronize;
} npurt_ze_functions;

typedef struct npurt_graph_functions {
  npunlock_ze_graph_create2_fn create2;
  npunlock_ze_graph_destroy_fn destroy;
  npunlock_ze_graph_get_properties3_fn get_properties3;
  npunlock_ze_graph_get_argument_properties3_fn get_argument_properties3;
  npunlock_ze_graph_set_argument_fn set_argument;
  npunlock_ze_graph_initialize_fn initialize;
  npunlock_ze_graph_append_initialize_fn append_initialize;
  npunlock_ze_graph_append_execute_fn append_execute;
} npurt_graph_functions;

/* --- tensors and kernels -------------------------------------------------- */

typedef struct npurt_tensor {
  uint32_t argument_index;
  uint32_t type; /* NPUNLOCK_ZE_GRAPH_ARGUMENT_TYPE_* */
  uint32_t precision;
  uint32_t element_size;
  uint32_t dims_count;
  uint32_t dims[5];
  uint64_t element_count;
  uint64_t byte_size;
  size_t allocation_size;
  void *allocation;
  char name[64];
} npurt_tensor;

struct npurt_kernel {
  char elf_sha256[NPURT_SHA256_HEX_BYTES];
  char blob_sha256[NPURT_SHA256_HEX_BYTES];
  uint8_t *elf;
  size_t elf_size;
  uint8_t *blob;
  size_t blob_size;
  uint32_t patch_target_count;
  uint32_t invocation_count;
  uint64_t invocation_element_count;
  uint64_t invocation_span_bytes;
  npunlock_ze_graph_handle handle;
  /* A closed command list holding exactly one graph execute, kept alive and
   * re-submitted for every invocation (measured: this removes the per-call
   * list/fence churn, and the driver accepts repeated submission of one list). */
  npunlock_ze_command_list_handle list;
  npunlock_ze_fence_handle fence;
  int list_ready;
  npurt_tensor tensors[NPURT_MAX_TENSORS];
  uint32_t tensor_count;
  int input_tensors[NPURT_MAX_TENSORS];
  uint32_t input_count;
  int output_tensor;
  uint64_t patch_ns;
  uint64_t graph_create_ns;
  uint64_t query_ns;
  uint64_t init_ns;
  uint64_t setup_ns;
  uint64_t run_count;
  uint64_t last_used;
  struct npurt_kernel *next;
};

/* --- process-global runtime state ---------------------------------------- */

typedef struct npurt_state {
  pthread_mutex_t lock;
  int configured;
  npurt_config config;
  int warm;
  void *loader;
  npurt_ze_functions ze;
  npurt_graph_functions graph;
  npunlock_ze_driver_handle driver;
  npunlock_ze_device_handle device;
  npunlock_ze_context_handle context;
  uint32_t queue_ordinal;
  npunlock_ze_command_queue_handle queue;
  uint32_t driver_index;
  uint32_t device_index;
  uint32_t driver_version;
  uint32_t vendor_id;
  uint32_t device_id;
  npurt_kernel *cache_head;
  uint32_t cache_count;
  uint64_t cache_use_counter;
  uint64_t cache_hits;
  uint64_t cache_misses;
  uint64_t cache_over_capacity;
} npurt_state;

npurt_state *npurt_state_get(void);

/* --- device operations (all require the global lock) ---------------------- */

npurt_status npurt_device_warm_locked(npurt_state *state, npurt_diagnostic *diagnostic);
void npurt_device_release_locked(npurt_state *state);
void npurt_device_abort_locked(npurt_state *state);
/* One invocation of a loaded kernel: re-submits the kernel's cached command
 * list (created on first use) and waits on its fence. */
npurt_status npurt_device_submit_kernel_locked(npurt_state *state, struct npurt_kernel *kernel,
                                              uint64_t timeout_ns, npurt_timing *timing,
                                              npurt_diagnostic *diagnostic);
npurt_status npurt_device_submit_locked(npurt_state *state, npunlock_ze_graph_handle handle,
                                       int initialize, uint64_t timeout_ns, npurt_timing *timing,
                                       npurt_diagnostic *diagnostic);
int npurt_loader_available(void);

/* --- toolchain ------------------------------------------------------------ */

npurt_status npurt_toolchain_compile_locked(const npurt_config *config, const char *source,
                                           size_t source_size, uint8_t **elf, size_t *elf_size,
                                           npurt_diagnostic *diagnostic);

#endif /* NPURT_INTERNAL_H */
