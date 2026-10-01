/* Public API: process-global state, kernel cache, graph session, execution. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "npurt_internal.h"

#define NPURT_PAGE_SIZE 4096u
#define NPURT_DEFAULT_TIMEOUT_NS 5000000000ull

static npurt_state g_state = {.lock = PTHREAD_MUTEX_INITIALIZER};

__attribute__((constructor)) static void npurt_global_constructor(void) {
  npurt_config_defaults(&g_state.config);
  if (g_state.config.max_cached_kernels == 0) {
    g_state.config.max_cached_kernels = 8;
  }
  g_state.configured = 1;
}

npurt_state *npurt_state_get(void) { return &g_state; }

static void npurt_lock(void) { pthread_mutex_lock(&g_state.lock); }
static void npurt_unlock(void) { pthread_mutex_unlock(&g_state.lock); }

/* --- configuration -------------------------------------------------------- */

npurt_status npurt_configure(const npurt_config *config, npurt_diagnostic *diagnostic) {
  npurt_status status = NPURT_OK;
  npurt_lock();
  if (config == NULL) {
    npurt_config_defaults(&g_state.config);
  } else if (g_state.warm || g_state.cache_head != NULL) {
    NPURT_DIAG(diagnostic, "npurt_configure must be called before warm()/kernel_create()");
    status = NPURT_ERR_STATE;
  } else {
    g_state.config = *config;
    if (g_state.config.max_cached_kernels == 0) {
      g_state.config.max_cached_kernels = 8;
    }
    if (g_state.config.default_timeout_ns == 0) {
      g_state.config.default_timeout_ns = NPURT_DEFAULT_TIMEOUT_NS;
    }
    if (g_state.config.compile_timeout_ns == 0) {
      g_state.config.compile_timeout_ns = 25000000000ull;
    }
  }
  npurt_unlock();
  return status;
}

npurt_status npurt_warm(npurt_diagnostic *diagnostic) {
  npurt_status status;
  npurt_lock();
  status = npurt_device_warm_locked(&g_state, diagnostic);
  npurt_unlock();
  return status;
}

int npurt_is_warm(void) { return g_state.warm; }

/* --- cache ---------------------------------------------------------------- */

static void npurt_kernel_destroy_locked(npurt_kernel *kernel) {
  size_t index;
  if (kernel == NULL) {
    return;
  }
  if (g_state.context != NULL && g_state.ze.mem_free != NULL) {
    for (index = 0; index < kernel->tensor_count; ++index) {
      if (kernel->tensors[index].allocation != NULL) {
        g_state.ze.mem_free(g_state.context, kernel->tensors[index].allocation);
      }
    }
  }
  if (kernel->fence != NULL && g_state.ze.fence_destroy != NULL) {
    g_state.ze.fence_destroy(kernel->fence);
  }
  if (kernel->list != NULL && g_state.ze.command_list_destroy != NULL) {
    g_state.ze.command_list_destroy(kernel->list);
  }
  if (kernel->handle != NULL && g_state.graph.destroy != NULL) {
    g_state.graph.destroy(kernel->handle);
  }
  free(kernel->blob);
  free(kernel->elf);
  free(kernel);
}

static void npurt_cache_remove_locked(npurt_kernel *kernel) {
  npurt_kernel **link = &g_state.cache_head;
  while (*link != NULL) {
    if (*link == kernel) {
      *link = kernel->next;
      if (g_state.cache_count > 0) {
        --g_state.cache_count;
      }
      npurt_kernel_destroy_locked(kernel);
      return;
    }
    link = &(*link)->next;
  }
}

/*
 * The cache never destroys a handle the caller can still hold: it deduplicates
 * kernels and grows past `max_cached_kernels` (with a warning) instead. Only
 * npurt_kernel_free() and npurt_shutdown() release device resources.
 */
static void npurt_cache_check_capacity_locked(void) {
  if (g_state.cache_count > g_state.config.max_cached_kernels) {
    ++g_state.cache_over_capacity;
    if (g_state.config.verbosity >= 1) {
      fprintf(stderr,
              "[npu-runtime] %u kernels cached (soft limit %u); call npurt_kernel_free() for "
              "kernels you no longer need\n",
              g_state.cache_count, g_state.config.max_cached_kernels);
    }
  }
}

/* Two lookup keys: the SHA-256 of the patched graph blob (the authoritative
 * identity of the loaded kernel image) and the SHA-256 of the kernel ELF, which
 * lets npurt_kernel_create() answer from the cache without re-patching. */
static npurt_kernel *npurt_cache_find_blob_locked(const char *key) {
  npurt_kernel *cursor;
  for (cursor = g_state.cache_head; cursor != NULL; cursor = cursor->next) {
    if (cursor->blob_sha256[0] != '\0' && strcmp(cursor->blob_sha256, key) == 0) {
      cursor->last_used = ++g_state.cache_use_counter;
      return cursor;
    }
  }
  return NULL;
}

static npurt_kernel *npurt_cache_find_elf_locked(const char *key) {
  npurt_kernel *cursor;
  for (cursor = g_state.cache_head; cursor != NULL; cursor = cursor->next) {
    if (cursor->elf_sha256[0] != '\0' && strcmp(cursor->elf_sha256, key) == 0) {
      cursor->last_used = ++g_state.cache_use_counter;
      return cursor;
    }
  }
  return NULL;
}

void npurt_cache_stats_get(npurt_cache_stats *stats) {
  if (stats == NULL) {
    return;
  }
  npurt_lock();
  stats->entries = g_state.cache_count;
  stats->capacity = g_state.config.max_cached_kernels;
  stats->hits = g_state.cache_hits;
  stats->misses = g_state.cache_misses;
  stats->over_capacity = g_state.cache_over_capacity;
  npurt_unlock();
}

/* --- patching ------------------------------------------------------------- */

static npurt_status npurt_patch_locked(const npurt_patch_entry *entries, size_t entry_count,
                                      uint8_t **blob_out, size_t *blob_size_out,
                                      uint32_t *target_count_out, npurt_diagnostic *diagnostic) {
  uint8_t *carrier = NULL;
  size_t carrier_size = 0;
  uint8_t *current = NULL;
  size_t current_size = 0;
  uint32_t total_targets = 0;
  size_t entry_index;

  *blob_out = NULL;
  *blob_size_out = 0;
  *target_count_out = 0;

  if (g_state.config.carrier_blob[0] == '\0') {
    NPURT_DIAG(diagnostic, "no carrier blob is configured");
    return NPURT_ERR_ARGUMENT;
  }
  {
    FILE *stream = fopen(g_state.config.carrier_blob, "rb");
    long length;
    if (stream == NULL) {
      NPURT_DIAG(diagnostic, "cannot open carrier blob %s (%s)", g_state.config.carrier_blob,
                 strerror(errno));
      return NPURT_ERR_IO;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) <= 0 ||
        fseek(stream, 0, SEEK_SET) != 0) {
      fclose(stream);
      NPURT_DIAG(diagnostic, "carrier blob %s is empty or unreadable",
                 g_state.config.carrier_blob);
      return NPURT_ERR_IO;
    }
    carrier = (uint8_t *)malloc((size_t)length);
    if (carrier == NULL || fread(carrier, 1, (size_t)length, stream) != (size_t)length) {
      free(carrier);
      fclose(stream);
      return NPURT_ERR_IO;
    }
    carrier_size = (size_t)length;
    fclose(stream);
  }
  current = carrier;
  current_size = carrier_size;
  carrier = NULL;

  for (entry_index = 0; entry_index < entry_count; ++entry_index) {
    patchblob_discovery_result discovery;
    patchblob_options options;
    patchblob_result result;
    patchblob_target *targets = NULL;
    size_t target_count = 0;
    size_t index;
    npunlock_status status;
    memset(&discovery, 0, sizeof(discovery));
    memset(&result, 0, sizeof(result));
    status = patchblob_discover_targets((npunlock_view){current, current_size}, &discovery);
    if (status != NPUNLOCK_STATUS_OK) {
      NPURT_DIAG(diagnostic, "patchblob_discover_targets failed (%s)",
                 npunlock_status_name(status));
      patchblob_discovery_result_release(&discovery);
      free(current);
      return NPURT_ERR_PATCH;
    }
    if (discovery.target_count == 0) {
      NPURT_DIAG(diagnostic, "carrier blob exposes no patchable ACT target");
      patchblob_discovery_result_release(&discovery);
      free(current);
      return NPURT_ERR_PATCH;
    }
    targets = (patchblob_target *)calloc(discovery.target_count, sizeof(*targets));
    if (targets == NULL) {
      patchblob_discovery_result_release(&discovery);
      free(current);
      return NPURT_ERR_OUT_OF_MEMORY;
    }
    for (index = 0; index < discovery.target_count; ++index) {
      if (entries[entry_index].group_index != UINT32_MAX &&
          discovery.targets[index].group_index != entries[entry_index].group_index) {
        continue;
      }
      targets[target_count++] = discovery.targets[index].target;
    }
    if (target_count == 0) {
      NPURT_DIAG(diagnostic, "carrier blob has no ACT group %u",
                 entries[entry_index].group_index);
      free(targets);
      patchblob_discovery_result_release(&discovery);
      free(current);
      return NPURT_ERR_PATCH;
    }
    memset(&options, 0, sizeof(options));
    options.struct_size = sizeof(options);
    options.image_alignment = 0x400u;
    options.tail_padding = 0x80u;
    status = patchblob_patch(&options, (npunlock_view){current, current_size},
                             (npunlock_view){entries[entry_index].elf, entries[entry_index].elf_size},
                             targets, target_count, &result);
    free(targets);
    patchblob_discovery_result_release(&discovery);
    if (status != NPUNLOCK_STATUS_OK) {
      if (result.diagnostic.json.data != NULL) {
        NPURT_DIAG(diagnostic, "patchblob_patch failed (%s): %.*s", npunlock_status_name(status),
                   (int)(result.diagnostic.json.size > 300 ? 300 : result.diagnostic.json.size),
                   (const char *)result.diagnostic.json.data);
      } else {
        NPURT_DIAG(diagnostic, "patchblob_patch failed (%s)", npunlock_status_name(status));
      }
      patchblob_result_release(&result);
      free(current);
      return NPURT_ERR_PATCH;
    }
    if (result.graph_blob.data == NULL || result.graph_blob.size == 0) {
      NPURT_DIAG(diagnostic, "patchblob_patch produced an empty blob");
      patchblob_result_release(&result);
      free(current);
      return NPURT_ERR_PATCH;
    }
    {
      uint8_t *next = (uint8_t *)malloc(result.graph_blob.size);
      if (next == NULL) {
        patchblob_result_release(&result);
        free(current);
        return NPURT_ERR_OUT_OF_MEMORY;
      }
      memcpy(next, result.graph_blob.data, result.graph_blob.size);
      free(current);
      current = next;
      current_size = result.graph_blob.size;
    }
    total_targets += (uint32_t)target_count;
    patchblob_result_release(&result);
  }

  *blob_out = current;
  *blob_size_out = current_size;
  *target_count_out = total_targets;
  return NPURT_OK;
}

/* --- graph session -------------------------------------------------------- */

static int npurt_argument_data_size(const npunlock_ze_graph_argument_properties_3 *properties,
                                   size_t *data_size) {
  size_t elements = 1;
  size_t element_size;
  uint32_t index;
  if (properties->dims_count == 0 || properties->dims_count > 5) {
    return 0;
  }
  if (properties->devicePrecision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16) {
    element_size = sizeof(uint16_t);
  } else if (properties->devicePrecision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP32) {
    element_size = sizeof(uint32_t);
  } else {
    return 0;
  }
  for (index = 0; index < properties->dims_count; ++index) {
    if (properties->dims[index] == 0 || elements > (SIZE_MAX / properties->dims[index])) {
      return 0;
    }
    elements *= properties->dims[index];
  }
  if (elements == 0 || elements > SIZE_MAX / element_size) {
    return 0;
  }
  *data_size = elements * element_size;
  return *data_size != 0;
}

static void npurt_reset_tensor(const npurt_tensor *tensor) {
  if (tensor->allocation == NULL) {
    return;
  }
  if (tensor->precision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16) {
    uint16_t *words = (uint16_t *)tensor->allocation;
    size_t count = (size_t)tensor->byte_size / sizeof(uint16_t);
    size_t index;
    for (index = 0; index < count; ++index) {
      words[index] = 0x7e00u; /* quiet NaN in binary16 */
    }
  } else {
    uint32_t *words = (uint32_t *)tensor->allocation;
    size_t count = (size_t)tensor->byte_size / sizeof(uint32_t);
    size_t index;
    for (index = 0; index < count; ++index) {
      words[index] = 0x7fc00000u;
    }
  }
}

static npurt_status npurt_bind_arguments_locked(npurt_kernel *kernel, npurt_diagnostic *diagnostic) {
  uint32_t index;
  for (index = 0; index < kernel->tensor_count; ++index) {
    uint32_t code = g_state.graph.set_argument(kernel->handle, kernel->tensors[index].argument_index,
                                              kernel->tensors[index].allocation);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "pfnSetArgumentValue(arg %u) failed (0x%08x)",
                 kernel->tensors[index].argument_index, code);
      return NPURT_ERR_DEVICE;
    }
  }
  return NPURT_OK;
}

static npurt_status npurt_load_blob_locked(const uint8_t *blob, size_t blob_size, npurt_kernel *kernel,
                                          npurt_diagnostic *diagnostic) {
  npunlock_ze_graph_desc_2 descriptor;
  npunlock_ze_graph_properties_3 properties;
  uint64_t start;
  uint32_t code;
  uint32_t index;
  size_t total_output_bytes = 0;
  int input_count = 0;
  int output_count = 0;

  memset(&descriptor, 0, sizeof(descriptor));
  memset(&properties, 0, sizeof(properties));
  kernel->handle = NULL;
  kernel->tensor_count = 0;
  kernel->input_count = 0;
  kernel->output_tensor = -1;

  start = npurt_now_ns();
  descriptor.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_DESC_2;
  descriptor.format = NPUNLOCK_ZE_GRAPH_FORMAT_NATIVE;
  descriptor.inputSize = blob_size;
  descriptor.pInput = blob;
  code = g_state.graph.create2(g_state.context, g_state.device, &descriptor, &kernel->handle);
  kernel->graph_create_ns = npurt_now_ns() - start;
  if (code != NPUNLOCK_ZE_SUCCESS || kernel->handle == NULL) {
    NPURT_DIAG(diagnostic, "pfnCreate2(native graph, %zu bytes) failed (0x%08x)", blob_size, code);
    return NPURT_ERR_DEVICE;
  }

  start = npurt_now_ns();
  properties.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_PROPERTIES_3;
  code = g_state.graph.get_properties3(kernel->handle, &properties);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "pfnGetProperties3 failed (0x%08x)", code);
    return NPURT_ERR_DEVICE;
  }
  if (properties.numGraphArgs == 0 || properties.numGraphArgs > NPURT_MAX_TENSORS) {
    NPURT_DIAG(diagnostic, "graph exposes %u arguments (supported: 1..%u)",
               properties.numGraphArgs, (unsigned)NPURT_MAX_TENSORS);
    return NPURT_ERR_UNSUPPORTED;
  }
  for (index = 0; index < properties.numGraphArgs; ++index) {
    npunlock_ze_graph_argument_properties_3 argument;
    npunlock_ze_host_mem_alloc_desc allocation_desc;
    npurt_tensor *tensor = &kernel->tensors[kernel->tensor_count];
    size_t data_size = 0;
    const char *name_end;
    memset(&argument, 0, sizeof(argument));
    argument.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_ARGUMENT_PROPERTIES_3;
    code = g_state.graph.get_argument_properties3(kernel->handle, index, &argument);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "pfnGetArgumentProperties3(%u) failed (0x%08x)", index, code);
      return NPURT_ERR_DEVICE;
    }
    if (!npurt_argument_data_size(&argument, &data_size)) {
      NPURT_DIAG(diagnostic, "graph argument %u has an unsupported dtype or shape", index);
      return NPURT_ERR_UNSUPPORTED;
    }
    memset(tensor, 0, sizeof(*tensor));
    tensor->argument_index = index;
    tensor->type = argument.type;
    tensor->precision = argument.devicePrecision;
    tensor->element_size = (argument.devicePrecision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16)
                               ? 2u
                               : 4u;
    tensor->dims_count = argument.dims_count;
    memcpy(tensor->dims, argument.dims, sizeof(tensor->dims));
    tensor->byte_size = data_size;
    tensor->element_count = data_size / tensor->element_size;
    name_end = memchr(argument.name, 0, sizeof(argument.name));
    if (name_end != NULL) {
      size_t length = (size_t)(name_end - argument.name);
      if (length >= sizeof(tensor->name)) {
        length = sizeof(tensor->name) - 1;
      }
      memcpy(tensor->name, argument.name, length);
      tensor->name[length] = '\0';
    }
    tensor->allocation_size = (data_size + NPURT_PAGE_SIZE - 1) & ~(size_t)(NPURT_PAGE_SIZE - 1);
    memset(&allocation_desc, 0, sizeof(allocation_desc));
    allocation_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    if (argument.type == NPUNLOCK_ZE_GRAPH_ARGUMENT_TYPE_INPUT) {
      allocation_desc.flags = NPUNLOCK_ZE_HOST_MEM_ALLOC_WRITE_COMBINED;
    }
    code = g_state.ze.mem_alloc_host(g_state.context, &allocation_desc, tensor->allocation_size,
                                     NPURT_PAGE_SIZE, &tensor->allocation);
    if (code != NPUNLOCK_ZE_SUCCESS || tensor->allocation == NULL) {
      NPURT_DIAG(diagnostic, "zeMemAllocHost(arg %u, %zu bytes) failed (0x%08x)", index,
                 tensor->allocation_size, code);
      return NPURT_ERR_DEVICE;
    }
    memset(tensor->allocation, 0, tensor->allocation_size);
    if (argument.type == NPUNLOCK_ZE_GRAPH_ARGUMENT_TYPE_INPUT) {
      kernel->input_tensors[kernel->input_count] = (int)kernel->tensor_count;
      ++kernel->input_count;
      ++input_count;
    } else {
      npurt_reset_tensor(tensor);
      kernel->output_tensor = (int)kernel->tensor_count;
      ++output_count;
      total_output_bytes += data_size;
    }
    ++kernel->tensor_count;
  }
  kernel->query_ns = npurt_now_ns() - start;
  if (input_count < 1 || output_count != 1) {
    NPURT_DIAG(diagnostic, "graph has %d input(s) and %d output(s); the runtime needs at least one "
                           "input and exactly one output tensor",
               input_count, output_count);
    return NPURT_ERR_UNSUPPORTED;
  }
  {
    npurt_status status = npurt_bind_arguments_locked(kernel, diagnostic);
    if (status != NPURT_OK) {
      return status;
    }
  }
  start = npurt_now_ns();
  if ((properties.initStageRequired & NPUNLOCK_ZE_GRAPH_STAGE_INITIALIZE) != 0) {
    if (g_state.graph.initialize == NULL) {
      NPURT_DIAG(diagnostic, "graph requires direct initialization but exposes no entry point");
      return NPURT_ERR_UNSUPPORTED;
    }
    code = g_state.graph.initialize(kernel->handle);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "pfnGraphInitialize failed (0x%08x)", code);
      return NPURT_ERR_DEVICE;
    }
  }
  if ((properties.initStageRequired & NPUNLOCK_ZE_GRAPH_STAGE_COMMAND_LIST_INITIALIZE) != 0) {
    npurt_timing timing;
    npurt_status status;
    if (g_state.graph.append_initialize == NULL) {
      NPURT_DIAG(diagnostic, "graph requires command-list initialization but exposes no entry point");
      return NPURT_ERR_UNSUPPORTED;
    }
    memset(&timing, 0, sizeof(timing));
    status = npurt_device_submit_locked(&g_state, kernel->handle, 1,
                                        g_state.config.default_timeout_ns, &timing, diagnostic);
    if (status != NPURT_OK) {
      return status;
    }
    status = npurt_bind_arguments_locked(kernel, diagnostic);
    if (status != NPURT_OK) {
      return status;
    }
  }
  kernel->init_ns = npurt_now_ns() - start;
  (void)total_output_bytes;
  return NPURT_OK;
}

static npurt_status npurt_kernel_finish_locked(npurt_kernel *kernel, npurt_diagnostic *diagnostic) {
  if (kernel->handle == NULL) {
    return NPURT_ERR_DEVICE;
  }
  kernel->last_used = ++g_state.cache_use_counter;
  kernel->next = g_state.cache_head;
  g_state.cache_head = kernel;
  ++g_state.cache_count;
  npurt_cache_check_capacity_locked();
  (void)diagnostic;
  return NPURT_OK;
}

/* --- public kernel creation ---------------------------------------------- */

npurt_status npurt_kernel_create_layered(const npurt_patch_entry *entries, size_t entry_count,
                                         npurt_kernel **kernel_out, npurt_diagnostic *diagnostic) {
  npurt_kernel *kernel;
  uint8_t *blob = NULL;
  size_t blob_size = 0;
  uint32_t target_count = 0;
  npurt_status status;
  uint64_t start;

  if (kernel_out == NULL || entries == NULL || entry_count == 0) {
    return NPURT_ERR_ARGUMENT;
  }
  *kernel_out = NULL;
  npurt_lock();
  start = npurt_now_ns();
  status = npurt_device_warm_locked(&g_state, diagnostic);
  if (status != NPURT_OK) {
    npurt_unlock();
    return status;
  }
  status = npurt_patch_locked(entries, entry_count, &blob, &blob_size, &target_count, diagnostic);
  if (status != NPURT_OK) {
    npurt_unlock();
    return status;
  }
  {
    char key[NPURT_SHA256_HEX_BYTES];
    npurt_kernel *cached;
    npurt_sha256_hex(blob, blob_size, key);
    cached = npurt_cache_find_blob_locked(key);
    if (cached != NULL) {
      ++g_state.cache_hits;
      free(blob);
      *kernel_out = cached;
      if (g_state.config.verbosity >= 2) {
        fprintf(stderr, "[npu-runtime] kernel cache hit %s\n", key);
      }
      npurt_unlock();
      return NPURT_OK;
    }
    ++g_state.cache_misses;
    kernel = (npurt_kernel *)calloc(1, sizeof(*kernel));
    if (kernel == NULL) {
      free(blob);
      npurt_unlock();
      return NPURT_ERR_OUT_OF_MEMORY;
    }
    kernel->blob = blob;
    kernel->blob_size = blob_size;
    memcpy(kernel->blob_sha256, key, sizeof(key));
    kernel->patch_target_count = target_count;
    kernel->patch_ns = npurt_now_ns() - start;
    status = npurt_load_blob_locked(blob, blob_size, kernel, diagnostic);
    if (status != NPURT_OK) {
      npurt_kernel_destroy_locked(kernel);
      npurt_unlock();
      return status;
    }
    status = npurt_kernel_finish_locked(kernel, diagnostic);
    kernel->setup_ns = npurt_now_ns() - start;
    if (status == NPURT_OK) {
      *kernel_out = kernel;
    }
    npurt_unlock();
    return status;
  }
}

npurt_status npurt_kernel_create(const uint8_t *elf, size_t elf_size, npurt_kernel **kernel,
                                 npurt_diagnostic *diagnostic) {
  npurt_patch_entry entry;
  npurt_status status;
  char elf_hash[NPURT_SHA256_HEX_BYTES];
  if (elf == NULL || elf_size == 0 || kernel == NULL) {
    return NPURT_ERR_ARGUMENT;
  }
  *kernel = NULL;
  npurt_sha256_hex(elf, elf_size, elf_hash);
  /* Fast path: this exact ELF was prepared already, so no patch pass and no
   * device work is needed. */
  npurt_lock();
  {
    npurt_kernel *cached = npurt_cache_find_elf_locked(elf_hash);
    if (cached != NULL) {
      ++g_state.cache_hits;
      *kernel = cached;
      npurt_unlock();
      return NPURT_OK;
    }
  }
  npurt_unlock();
  memset(&entry, 0, sizeof(entry));
  entry.elf = elf;
  entry.elf_size = elf_size;
  entry.group_index = UINT32_MAX;
  status = npurt_kernel_create_layered(&entry, 1, kernel, diagnostic);
  if (status == NPURT_OK) {
    npurt_lock();
    if ((*kernel)->elf == NULL) {
      (*kernel)->elf = (uint8_t *)malloc(elf_size);
      if ((*kernel)->elf != NULL) {
        memcpy((*kernel)->elf, elf, elf_size);
        (*kernel)->elf_size = elf_size;
        memcpy((*kernel)->elf_sha256, elf_hash, sizeof(elf_hash));
      }
    }
    npurt_unlock();
  }
  return status;
}

npurt_status npurt_kernel_create_from_blob(const uint8_t *blob, size_t blob_size,
                                           npurt_kernel **kernel_out, npurt_diagnostic *diagnostic) {
  npurt_kernel *kernel;
  npurt_status status;
  char key[NPURT_SHA256_HEX_BYTES];
  uint64_t start;
  if (blob == NULL || blob_size == 0 || kernel_out == NULL) {
    return NPURT_ERR_ARGUMENT;
  }
  *kernel_out = NULL;
  npurt_lock();
  start = npurt_now_ns();
  status = npurt_device_warm_locked(&g_state, diagnostic);
  if (status != NPURT_OK) {
    npurt_unlock();
    return status;
  }
  npurt_sha256_hex(blob, blob_size, key);
  {
    npurt_kernel *cached = npurt_cache_find_blob_locked(key);
    if (cached != NULL) {
      ++g_state.cache_hits;
      *kernel_out = cached;
      npurt_unlock();
      return NPURT_OK;
    }
  }
  ++g_state.cache_misses;
  kernel = (npurt_kernel *)calloc(1, sizeof(*kernel));
  if (kernel == NULL) {
    npurt_unlock();
    return NPURT_ERR_OUT_OF_MEMORY;
  }
  kernel->blob = (uint8_t *)malloc(blob_size);
  if (kernel->blob == NULL) {
    free(kernel);
    npurt_unlock();
    return NPURT_ERR_OUT_OF_MEMORY;
  }
  memcpy(kernel->blob, blob, blob_size);
  kernel->blob_size = blob_size;
  memcpy(kernel->blob_sha256, key, sizeof(key));
  kernel->patch_ns = npurt_now_ns() - start;
  status = npurt_load_blob_locked(kernel->blob, kernel->blob_size, kernel, diagnostic);
  if (status != NPURT_OK) {
    npurt_kernel_destroy_locked(kernel);
    npurt_unlock();
    return status;
  }
  status = npurt_kernel_finish_locked(kernel, diagnostic);
  kernel->setup_ns = npurt_now_ns() - start;
  if (status == NPURT_OK) {
    *kernel_out = kernel;
  }
  npurt_unlock();
  return status;
}

npurt_status npurt_kernel_lookup(const uint8_t *elf, size_t elf_size, npurt_kernel **kernel) {
  npurt_status status;
  if (kernel == NULL || elf == NULL || elf_size == 0) {
    return NPURT_ERR_ARGUMENT;
  }
  *kernel = NULL;
  npurt_lock();
  {
    char elf_hash[NPURT_SHA256_HEX_BYTES];
    npurt_kernel *cached;
    npurt_sha256_hex(elf, elf_size, elf_hash);
    cached = npurt_cache_find_elf_locked(elf_hash);
    if (cached != NULL) {
      ++g_state.cache_hits;
      *kernel = cached;
      npurt_unlock();
      return NPURT_OK;
    }
  }
  npurt_unlock();
  status = NPURT_ERR_NOT_FOUND;
  return status;
}

void npurt_kernel_info_get(const npurt_kernel *kernel, npurt_kernel_info *info) {
  uint32_t index;
  if (info == NULL) {
    return;
  }
  memset(info, 0, sizeof(*info));
  if (kernel == NULL) {
    return;
  }
  memcpy(info->elf_sha256, kernel->elf_sha256, sizeof(info->elf_sha256));
  memcpy(info->blob_sha256, kernel->blob_sha256, strlen(kernel->blob_sha256));
  info->elf_bytes = kernel->elf_size;
  info->blob_bytes = kernel->blob_size;
  info->patch_target_count = kernel->patch_target_count;
  info->invocation_count = kernel->invocation_count;
  info->invocation_element_count = kernel->invocation_element_count;
  info->invocation_span_bytes = kernel->invocation_span_bytes;
  info->patch_ns = kernel->patch_ns;
  info->graph_create_ns = kernel->graph_create_ns;
  info->query_ns = kernel->query_ns;
  info->init_ns = kernel->init_ns;
  info->setup_ns = kernel->setup_ns;
  info->run_count = kernel->run_count;
  info->input_count = kernel->input_count;
  info->output_count = kernel->output_tensor >= 0 ? 1u : 0u;
  info->tensor_count = kernel->tensor_count;
  for (index = 0; index < kernel->tensor_count && index < NPURT_MAX_TENSORS; ++index) {
    const npurt_tensor *tensor = &kernel->tensors[index];
    npurt_tensor_info *out = &info->tensors[index];
    out->argument_index = tensor->argument_index;
    out->precision = tensor->precision;
    out->element_size = tensor->element_size;
    out->dims_count = tensor->dims_count;
    memcpy(out->dims, tensor->dims, sizeof(out->dims));
    out->element_count = tensor->element_count;
    out->byte_size = tensor->byte_size;
    out->is_input = tensor->type == NPUNLOCK_ZE_GRAPH_ARGUMENT_TYPE_INPUT;
    memcpy(out->name, tensor->name, sizeof(out->name));
  }
}

void npurt_kernel_free(npurt_kernel *kernel) {
  if (kernel == NULL) {
    return;
  }
  npurt_lock();
  npurt_cache_remove_locked(kernel);
  npurt_unlock();
}

/*
 * Independent session for the same blob: needed when several invocations must
 * be in flight at once (one command list, one wait). Clones are not cached.
 */
npurt_status npurt_kernel_clone(const npurt_kernel *kernel, npurt_kernel **clone,
                                npurt_diagnostic *diagnostic) {
  npurt_kernel *copy;
  npurt_status status;
  if (kernel == NULL || clone == NULL) {
    return NPURT_ERR_ARGUMENT;
  }
  *clone = NULL;
  npurt_lock();
  if (!g_state.warm) {
    npurt_unlock();
    NPURT_DIAG(diagnostic, "the runtime is shut down");
    return NPURT_ERR_STATE;
  }
  copy = (npurt_kernel *)calloc(1, sizeof(*copy));
  if (copy == NULL) {
    npurt_unlock();
    return NPURT_ERR_OUT_OF_MEMORY;
  }
  memcpy(copy->elf_sha256, kernel->elf_sha256, sizeof(copy->elf_sha256));
  memcpy(copy->blob_sha256, kernel->blob_sha256, sizeof(copy->blob_sha256));
  copy->blob = (uint8_t *)malloc(kernel->blob_size);
  if (copy->blob == NULL) {
    free(copy);
    npurt_unlock();
    return NPURT_ERR_OUT_OF_MEMORY;
  }
  memcpy(copy->blob, kernel->blob, kernel->blob_size);
  copy->blob_size = kernel->blob_size;
  copy->patch_target_count = kernel->patch_target_count;
  status = npurt_load_blob_locked(copy->blob, copy->blob_size, copy, diagnostic);
  if (status != NPURT_OK) {
    npurt_kernel_destroy_locked(copy);
    npurt_unlock();
    return status;
  }
  *clone = copy;
  npurt_unlock();
  return NPURT_OK;
}

static npurt_status npurt_run_batch_common(npurt_kernel *const *kernels,
                                           const npurt_run_inputs_request *requests, size_t count,
                                           npurt_timing *timing, npurt_diagnostic *diagnostic) {
  npunlock_ze_command_list_desc list_desc;
  npunlock_ze_fence_desc fence_desc;
  npunlock_ze_command_list_handle list = NULL;
  npunlock_ze_fence_handle fence = NULL;
  npurt_status status = NPURT_OK;
  uint64_t start;
  uint64_t mark;
  uint64_t wait_start = 0;
  uint64_t end = 0;
  uint64_t timeout = 0;
  npurt_timing local;
  uint32_t code;
  size_t index;

  if (timing != NULL) {
    memset(timing, 0, sizeof(*timing));
  }
  if (kernels == NULL || requests == NULL || count == 0) {
    return NPURT_ERR_ARGUMENT;
  }
  memset(&local, 0, sizeof(local));
  memset(&list_desc, 0, sizeof(list_desc));
  memset(&fence_desc, 0, sizeof(fence_desc));
  for (index = 0; index < count; ++index) {
    const npurt_tensor *output;
    uint32_t input_slot;
    if (kernels[index] == NULL || kernels[index]->input_count == 0 ||
        kernels[index]->output_tensor < 0 || requests[index].inputs == NULL ||
        requests[index].input_sizes == NULL) {
      NPURT_DIAG(diagnostic, "batch entry %zu is not a usable kernel session", index);
      return NPURT_ERR_ARGUMENT;
    }
    if (requests[index].input_count != kernels[index]->input_count) {
      NPURT_DIAG(diagnostic, "batch entry %zu supplies %zu inputs but the graph exposes %u", index,
                 requests[index].input_count, kernels[index]->input_count);
      return NPURT_ERR_SHAPE;
    }
    for (input_slot = 0; input_slot < kernels[index]->input_count; ++input_slot) {
      const npurt_tensor *tensor =
          &kernels[index]->tensors[kernels[index]->input_tensors[input_slot]];
      if (requests[index].inputs[input_slot] == NULL ||
          requests[index].input_sizes[input_slot] != tensor->byte_size) {
        NPURT_DIAG(diagnostic, "batch entry %zu input %u is %zu bytes, graph tensor is %llu", index,
                   input_slot, requests[index].input_sizes[input_slot],
                   (unsigned long long)tensor->byte_size);
        return NPURT_ERR_SHAPE;
      }
    }
    output = &kernels[index]->tensors[kernels[index]->output_tensor];
    if (requests[index].output == NULL || requests[index].output_size != output->byte_size) {
      NPURT_DIAG(diagnostic, "batch entry %zu output is %zu bytes, graph tensor is %llu", index,
                 requests[index].output_size, (unsigned long long)output->byte_size);
      return NPURT_ERR_SHAPE;
    }
    if (requests[index].timeout_ns > timeout) {
      timeout = requests[index].timeout_ns;
    }
  }
  if (timeout == 0) {
    timeout = g_state.config.default_timeout_ns;
  }

  npurt_lock();
  if (!g_state.warm) {
    npurt_unlock();
    NPURT_DIAG(diagnostic, "the runtime is shut down");
    return NPURT_ERR_STATE;
  }
  if (g_state.queue == NULL) {
    npunlock_ze_command_queue_desc queue_desc;
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queue_desc.ordinal = g_state.queue_ordinal;
    code = g_state.ze.command_queue_create(g_state.context, g_state.device, &queue_desc,
                                           &g_state.queue);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      npurt_unlock();
      NPURT_DIAG(diagnostic, "zeCommandQueueCreate failed (0x%08x)", code);
      return NPURT_ERR_DEVICE;
    }
  }
  start = npurt_now_ns();
  for (index = 0; index < count; ++index) {
    const npurt_tensor *output = &kernels[index]->tensors[kernels[index]->output_tensor];
    uint32_t input_slot;
    for (input_slot = 0; input_slot < kernels[index]->input_count; ++input_slot) {
      const npurt_tensor *tensor =
          &kernels[index]->tensors[kernels[index]->input_tensors[input_slot]];
      memcpy(tensor->allocation, requests[index].inputs[input_slot],
             requests[index].input_sizes[input_slot]);
    }
    npurt_reset_tensor(output);
    if (g_state.config.rebind_arguments_each_run) {
      status = npurt_bind_arguments_locked(kernels[index], diagnostic);
      if (status != NPURT_OK) {
        npurt_unlock();
        return status;
      }
    }
  }
  list_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
  list_desc.commandQueueGroupOrdinal = g_state.queue_ordinal;
  fence_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_FENCE_DESC;
  code = g_state.ze.command_list_create(g_state.context, g_state.device, &list_desc, &list);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    npurt_unlock();
    NPURT_DIAG(diagnostic, "zeCommandListCreate failed (0x%08x)", code);
    return NPURT_ERR_DEVICE;
  }
  for (index = 0; index < count; ++index) {
    code = g_state.graph.append_execute(list, kernels[index]->handle, NULL, NULL, 0, NULL);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      g_state.ze.command_list_destroy(list);
      npurt_unlock();
      NPURT_DIAG(diagnostic, "pfnAppendGraphExecute(%zu) failed (0x%08x)", index, code);
      return NPURT_ERR_DEVICE;
    }
  }
  code = g_state.ze.command_list_close(list);
  if (code == NPUNLOCK_ZE_SUCCESS) {
    code = g_state.ze.fence_create(g_state.queue, &fence_desc, &fence);
  }
  mark = npurt_now_ns();
  local.prepare_ns = mark - start;
  if (code == NPUNLOCK_ZE_SUCCESS) {
    code = g_state.ze.command_queue_execute(g_state.queue, 1, &list, fence);
  }
  wait_start = npurt_now_ns();
  local.submit_ns = wait_start - mark;
  if (code == NPUNLOCK_ZE_SUCCESS) {
    code = g_state.ze.fence_synchronize(fence, timeout);
  }
  end = npurt_now_ns();
  local.wait_ns = end - wait_start;
  if (code != NPUNLOCK_ZE_SUCCESS) {
    status = code == 0x00000001u ? NPURT_ERR_TIMEOUT : NPURT_ERR_DEVICE;
    NPURT_DIAG(diagnostic, "batch submission failed (0x%08x)%s", code,
               status == NPURT_ERR_TIMEOUT ? ": fence timeout" : "");
  } else {
    for (index = 0; index < count; ++index) {
      const npurt_tensor *output = &kernels[index]->tensors[kernels[index]->output_tensor];
      memcpy(requests[index].output, output->allocation, requests[index].output_size);
      ++((npurt_kernel *)kernels[index])->run_count;
    }
    local.run_index = ((npurt_kernel *)kernels[0])->run_count - 1;
  }
  if (fence != NULL) {
    g_state.ze.fence_destroy(fence);
  }
  g_state.ze.command_list_destroy(list);
  local.teardown_ns = npurt_now_ns() - end;
  local.total_ns = npurt_now_ns() - start;
  npurt_unlock();
  if (timing != NULL) {
    *timing = local;
  }
  return status;
}

npurt_status npurt_run_batch_inputs(npurt_kernel *const *kernels,
                                       const npurt_run_inputs_request *requests, size_t count,
                                       npurt_timing *timing, npurt_diagnostic *diagnostic) {
  return npurt_run_batch_common(kernels, requests, count, timing, diagnostic);
}

npurt_status npurt_run_batch(npurt_kernel *const *kernels, const npurt_run_request *requests,
                             size_t count, npurt_timing *timing, npurt_diagnostic *diagnostic) {
  npurt_run_inputs_request *converted;
  const void **input_pointers;
  size_t *input_sizes;
  npurt_status status;
  size_t index;

  if (kernels == NULL || requests == NULL || count == 0) {
    return NPURT_ERR_ARGUMENT;
  }
  converted = (npurt_run_inputs_request *)calloc(count, sizeof(*converted));
  input_pointers = (const void **)calloc(count, sizeof(*input_pointers));
  input_sizes = (size_t *)calloc(count, sizeof(*input_sizes));
  if (converted == NULL || input_pointers == NULL || input_sizes == NULL) {
    free(converted);
    free(input_pointers);
    free(input_sizes);
    return NPURT_ERR_OUT_OF_MEMORY;
  }
  for (index = 0; index < count; ++index) {
    input_pointers[index] = requests[index].input;
    input_sizes[index] = requests[index].input_size;
    converted[index].inputs = &input_pointers[index];
    converted[index].input_sizes = &input_sizes[index];
    converted[index].input_count = 1;
    converted[index].output = requests[index].output;
    converted[index].output_size = requests[index].output_size;
    converted[index].dims = requests[index].dims;
    converted[index].dims_count = requests[index].dims_count;
    converted[index].timeout_ns = requests[index].timeout_ns;
  }
  status = npurt_run_batch_common(kernels, converted, count, timing, diagnostic);
  free(converted);
  free(input_pointers);
  free(input_sizes);
  return status;
}

/* --- execution ------------------------------------------------------------ */

npurt_status npurt_run_inputs(npurt_kernel *kernel, const npurt_run_inputs_request *request,
                              npurt_timing *timing, npurt_diagnostic *diagnostic) {
  npurt_status status = NPURT_OK;
  uint64_t start;
  uint64_t timeout;
  const npurt_tensor *output;
  npurt_timing local_timing;
  uint32_t index;

  if (timing != NULL) {
    memset(timing, 0, sizeof(*timing));
  }
  if (kernel == NULL || request == NULL || request->inputs == NULL ||
      request->input_sizes == NULL) {
    NPURT_DIAG(diagnostic, "npurt_run_inputs requires a kernel and its input buffers");
    return NPURT_ERR_ARGUMENT;
  }
  if (kernel->input_count == 0 || kernel->output_tensor < 0 || kernel->handle == NULL) {
    NPURT_DIAG(diagnostic, "kernel handle is not usable (no graph session)");
    return NPURT_ERR_ARGUMENT;
  }
  if (request->input_count != kernel->input_count) {
    NPURT_DIAG(diagnostic, "request has %zu inputs but the graph exposes %u (static arity)",
               request->input_count, kernel->input_count);
    return NPURT_ERR_SHAPE;
  }
  output = &kernel->tensors[kernel->output_tensor];
  if (request->output == NULL || request->output_size != output->byte_size) {
    NPURT_DIAG(diagnostic,
               "output buffer is %zu bytes but the graph output tensor is %llu bytes (static shape)",
               request->output_size, (unsigned long long)output->byte_size);
    return NPURT_ERR_SHAPE;
  }
  for (index = 0; index < kernel->input_count; ++index) {
    const npurt_tensor *input = &kernel->tensors[kernel->input_tensors[index]];
    if (request->inputs[index] == NULL || request->input_sizes[index] != input->byte_size) {
      NPURT_DIAG(diagnostic,
                 "input %u is %zu bytes but the graph tensor is %llu bytes (static shape)", index,
                 request->input_sizes[index], (unsigned long long)input->byte_size);
      return NPURT_ERR_SHAPE;
    }
  }
  if (request->dims != NULL) {
    const npurt_tensor *input = &kernel->tensors[kernel->input_tensors[0]];
    if (request->dims_count != input->dims_count) {
      NPURT_DIAG(diagnostic, "declared rank %u does not match the graph rank %u",
                 request->dims_count, input->dims_count);
      return NPURT_ERR_SHAPE;
    }
    for (index = 0; index < input->dims_count; ++index) {
      if (request->dims[index] != input->dims[index]) {
        NPURT_DIAG(diagnostic, "declared dim[%u]=%u does not match the graph dim %u", index,
                   request->dims[index], input->dims[index]);
        return NPURT_ERR_SHAPE;
      }
    }
  }
  timeout = request->timeout_ns != 0 ? request->timeout_ns : g_state.config.default_timeout_ns;
  memset(&local_timing, 0, sizeof(local_timing));
  npurt_lock();
  if (!g_state.warm || kernel->handle == NULL) {
    npurt_unlock();
    NPURT_DIAG(diagnostic, "the runtime is shut down");
    return NPURT_ERR_STATE;
  }
  start = npurt_now_ns();
  local_timing.run_index = kernel->run_count;
  {
    uint64_t prepare_start = npurt_now_ns();
    for (index = 0; index < kernel->input_count; ++index) {
      const npurt_tensor *input = &kernel->tensors[kernel->input_tensors[index]];
      memcpy(input->allocation, request->inputs[index], request->input_sizes[index]);
    }
    npurt_reset_tensor(output);
    if (g_state.config.rebind_arguments_each_run) {
      status = npurt_bind_arguments_locked(kernel, diagnostic);
      if (status != NPURT_OK) {
        npurt_unlock();
        return status;
      }
    }
    local_timing.prepare_ns = npurt_now_ns() - prepare_start;
  }
  {
    npurt_timing submit_timing;
    memset(&submit_timing, 0, sizeof(submit_timing));
    status = npurt_device_submit_kernel_locked(&g_state, kernel, timeout, &submit_timing,
                                               diagnostic);
    local_timing.session_ns = submit_timing.session_ns;
    local_timing.submit_ns = submit_timing.submit_ns;
    local_timing.wait_ns = submit_timing.wait_ns;
    local_timing.teardown_ns = submit_timing.teardown_ns;
    if (status != NPURT_OK) {
      npurt_unlock();
      if (timing != NULL) {
        *timing = local_timing;
      }
      return status;
    }
  }
  memcpy(request->output, output->allocation, request->output_size);
  local_timing.total_ns = npurt_now_ns() - start;
  ++kernel->run_count;
  npurt_unlock();
  if (timing != NULL) {
    *timing = local_timing;
  }
  return NPURT_OK;
}

npurt_status npurt_run(npurt_kernel *kernel, const npurt_run_request *request, npurt_timing *timing,
                       npurt_diagnostic *diagnostic) {
  npurt_run_inputs_request multi;
  const void *inputs[1];
  size_t sizes[1];
  if (request == NULL) {
    NPURT_DIAG(diagnostic, "npurt_run requires a request");
    return NPURT_ERR_ARGUMENT;
  }
  inputs[0] = request->input;
  sizes[0] = request->input_size;
  memset(&multi, 0, sizeof(multi));
  multi.inputs = inputs;
  multi.input_sizes = sizes;
  multi.input_count = 1;
  multi.output = request->output;
  multi.output_size = request->output_size;
  multi.dims = request->dims;
  multi.dims_count = request->dims_count;
  multi.timeout_ns = request->timeout_ns;
  return npurt_run_inputs(kernel, &multi, timing, diagnostic);
}

/* --- lifecycle ------------------------------------------------------------ */

npurt_status npurt_compile_source(const char *source, size_t source_size, uint8_t **elf,
                                  size_t *elf_size, npurt_diagnostic *diagnostic) {
  npurt_status status;
  if (source == NULL || source_size == 0 || elf == NULL || elf_size == NULL) {
    return NPURT_ERR_ARGUMENT;
  }
  *elf = NULL;
  *elf_size = 0;
  npurt_lock();
  status = npurt_toolchain_compile_locked(&g_state.config, source, source_size, elf, elf_size,
                                          diagnostic);
  npurt_unlock();
  return status;
}

void npurt_shutdown(void) {
  npurt_lock();
  while (g_state.cache_head != NULL) {
    npurt_kernel *kernel = g_state.cache_head;
    g_state.cache_head = kernel->next;
    npurt_kernel_destroy_locked(kernel);
  }
  g_state.cache_count = 0;
  npurt_device_release_locked(&g_state);
  npurt_unlock();
}
