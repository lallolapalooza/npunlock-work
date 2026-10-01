/* Level Zero bring-up and one-shot graph submission.
 *
 * Mirrors the bring-up sequence the npunlock linux probe validates
 * (npunlock/src/linux_probe/infer_engine.c): loader -> NPU device -> graph
 * extension -> context -> queue group -> graph handle. The difference is that
 * this layer keeps the loader, device, context and command queue alive across
 * invocations so that only the command list / fence work is per call.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npurt_internal.h"

#define NPURT_PAGE_SIZE 4096u

/* level-3 trace: every driver call on the execution path */
#define NPURT_TRACE(...)                                                                           \
  do {                                                                                             \
    if (npurt_state_get()->config.verbosity >= 3) {                                                \
      fprintf(stderr, "[npu-runtime:submit] " __VA_ARGS__);                                        \
      fputc('\n', stderr);                                                                         \
      fflush(stderr);                                                                              \
    }                                                                                              \
  } while (0)

int npurt_loader_available(void) {
  void *loader = dlopen("libze_loader.so.1", RTLD_NOW | RTLD_LOCAL);
  if (loader == NULL) {
    return 0;
  }
  dlclose(loader);
  return 1;
}

static int npurt_load_export(void *library, const char *name, void *destination, size_t size) {
  void *symbol = dlsym(library, name);
  if (symbol == NULL || size != sizeof(symbol)) {
    return 0;
  }
  memcpy(destination, &symbol, size);
  return 1;
}

static int npurt_load_ze_functions(void *library, npurt_ze_functions *ze) {
  int required;
  memset(ze, 0, sizeof(*ze));
#define NPURT_LOAD(name, field) npurt_load_export(library, name, &ze->field, sizeof(ze->field))
  required = NPURT_LOAD("zeDriverGetProperties", driver_get_properties) &&
             NPURT_LOAD("zeDriverGetExtensionProperties", driver_get_extensions) &&
             NPURT_LOAD("zeDriverGetExtensionFunctionAddress", driver_get_extension_address) &&
             NPURT_LOAD("zeDeviceGet", device_get) &&
             NPURT_LOAD("zeDeviceGetProperties", device_get_properties) &&
             NPURT_LOAD("zeContextCreate", context_create) &&
             NPURT_LOAD("zeContextDestroy", context_destroy) &&
             NPURT_LOAD("zeDeviceGetCommandQueueGroupProperties", device_get_queue_groups) &&
             NPURT_LOAD("zeMemAllocHost", mem_alloc_host) && NPURT_LOAD("zeMemFree", mem_free) &&
             NPURT_LOAD("zeCommandQueueCreate", command_queue_create) &&
             NPURT_LOAD("zeCommandQueueDestroy", command_queue_destroy) &&
             NPURT_LOAD("zeCommandQueueExecuteCommandLists", command_queue_execute) &&
             NPURT_LOAD("zeCommandListCreate", command_list_create) &&
             NPURT_LOAD("zeCommandListClose", command_list_close) &&
             NPURT_LOAD("zeCommandListDestroy", command_list_destroy) &&
             NPURT_LOAD("zeFenceCreate", fence_create) && NPURT_LOAD("zeFenceDestroy", fence_destroy) &&
             NPURT_LOAD("zeFenceHostSynchronize", fence_synchronize);
  if (!required) {
    return 0;
  }
  if (NPURT_LOAD("zeInitDrivers", init_drivers)) {
    return 1;
  }
  return NPURT_LOAD("zeInit", init) && NPURT_LOAD("zeDriverGet", driver_get);
#undef NPURT_LOAD
}

static npurt_status npurt_select_npu(const npurt_config *config, const npurt_ze_functions *ze,
                                     npurt_state *state, npurt_diagnostic *diagnostic) {
  npunlock_ze_init_driver_type_desc init = {NPUNLOCK_ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC, NULL,
                                            NPUNLOCK_ZE_INIT_DRIVER_TYPE_FLAG_NPU};
  npunlock_ze_driver_handle *drivers = NULL;
  uint32_t driver_count = 0;
  uint32_t driver_index;
  uint32_t code;
  if (ze->init_drivers != NULL) {
    code = ze->init_drivers(&driver_count, NULL, &init);
  } else {
    code = ze->init(0);
    if (code == NPUNLOCK_ZE_SUCCESS) {
      code = ze->driver_get(&driver_count, NULL);
    }
  }
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "Level Zero driver enumeration failed (0x%08x)", code);
    return NPURT_ERR_DEVICE;
  }
  if (driver_count == 0) {
    NPURT_DIAG(diagnostic, "no Level Zero driver is installed");
    return NPURT_ERR_NO_NPU;
  }
  drivers = (npunlock_ze_driver_handle *)calloc(driver_count, sizeof(*drivers));
  if (drivers == NULL) {
    return NPURT_ERR_OUT_OF_MEMORY;
  }
  code = ze->init_drivers != NULL ? ze->init_drivers(&driver_count, drivers, &init)
                                  : ze->driver_get(&driver_count, drivers);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    free(drivers);
    NPURT_DIAG(diagnostic, "Level Zero zeInitDrivers/zeDriverGet failed (0x%08x)", code);
    return NPURT_ERR_DEVICE;
  }
  for (driver_index = 0; driver_index < driver_count; ++driver_index) {
    npunlock_ze_device_handle *devices = NULL;
    uint32_t device_count = 0;
    uint32_t device_index;
    if (config->device_index != UINT32_MAX && config->device_index != driver_index) {
      continue;
    }
    if (ze->device_get(drivers[driver_index], &device_count, NULL) != NPUNLOCK_ZE_SUCCESS ||
        device_count == 0) {
      continue;
    }
    devices = (npunlock_ze_device_handle *)calloc(device_count, sizeof(*devices));
    if (devices == NULL) {
      free(drivers);
      return NPURT_ERR_OUT_OF_MEMORY;
    }
    if (ze->device_get(drivers[driver_index], &device_count, devices) != NPUNLOCK_ZE_SUCCESS) {
      free(devices);
      continue;
    }
    for (device_index = 0; device_index < device_count; ++device_index) {
      npunlock_ze_device_properties properties = {0};
      properties.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      if (config->device_index != UINT32_MAX && config->device_index != device_index) {
        continue;
      }
      if (ze->device_get_properties(devices[device_index], &properties) == NPUNLOCK_ZE_SUCCESS &&
          properties.type == NPUNLOCK_ZE_DEVICE_TYPE_VPU) {
        state->driver = drivers[driver_index];
        state->device = devices[device_index];
        state->driver_index = driver_index;
        state->device_index = device_index;
        state->vendor_id = properties.vendorId;
        state->device_id = properties.deviceId;
        free(devices);
        free(drivers);
        return NPURT_OK;
      }
    }
    free(devices);
  }
  free(drivers);
  NPURT_DIAG(diagnostic, "no Level Zero VPU device (NPU) was found");
  return NPURT_ERR_NO_NPU;
}

static uint32_t npurt_find_graph_version(const npurt_ze_functions *ze,
                                         npunlock_ze_driver_handle driver,
                                         npurt_diagnostic *diagnostic) {
  npunlock_ze_driver_extension_properties *extensions = NULL;
  uint32_t count = 0;
  uint32_t index;
  uint32_t version = 0;
  uint32_t code = ze->driver_get_extensions(driver, &count, NULL);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeDriverGetExtensionProperties(count) failed (0x%08x)", code);
    return 0;
  }
  extensions = (npunlock_ze_driver_extension_properties *)calloc(count, sizeof(*extensions));
  if (extensions == NULL && count != 0) {
    return 0;
  }
  code = ze->driver_get_extensions(driver, &count, extensions);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    free(extensions);
    NPURT_DIAG(diagnostic, "zeDriverGetExtensionProperties(list) failed (0x%08x)", code);
    return 0;
  }
  for (index = 0; index < count; ++index) {
    if (memchr(extensions[index].name, 0, sizeof(extensions[index].name)) != NULL &&
        strcmp(extensions[index].name, "ZE_extension_graph") == 0) {
      version = extensions[index].version;
      break;
    }
  }
  free(extensions);
  if (version == 0) {
    NPURT_DIAG(diagnostic, "driver does not advertise ZE_extension_graph");
  }
  return version;
}

static int npurt_get_graph_functions(const npurt_ze_functions *ze, npunlock_ze_driver_handle driver,
                                     uint32_t version, npurt_graph_functions *graph,
                                     npurt_diagnostic *diagnostic) {
  void *raw_driver_ddi = NULL;
  npunlock_ze_npu_get_extension_fn get_extension = NULL;
  npunlock_ze_graph_ddi_prefix *ddi = NULL;
  npunlock_ze_driver_extension_npu request;
  uint32_t code = ze->driver_get_extension_address(driver, "ZE_extension_driver_npu", &raw_driver_ddi);
  memset(&request, 0, sizeof(request));
  if (code != NPUNLOCK_ZE_SUCCESS || raw_driver_ddi == NULL) {
    NPURT_DIAG(diagnostic, "zeDriverGetExtensionFunctionAddress failed (0x%08x)", code);
    return 0;
  }
  memcpy(&get_extension, raw_driver_ddi, sizeof(get_extension));
  if (get_extension == NULL) {
    NPURT_DIAG(diagnostic, "NPU driver extension table is invalid");
    return 0;
  }
  request.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_DRIVER_EXTENSION_NPU_EXT;
  request.name = "ZE_extension_graph";
  request.version = version;
  request.ppFunctionAddress = (void **)&ddi;
  code = get_extension(driver, &request);
  if (code != NPUNLOCK_ZE_SUCCESS || ddi == NULL) {
    NPURT_DIAG(diagnostic, "ZE_extension_graph v%u lookup failed (0x%08x)", version, code);
    return 0;
  }
  memset(graph, 0, sizeof(*graph));
#define NPURT_GET(slot, field) memcpy(&graph->field, &ddi->slots[slot], sizeof(graph->field))
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_CREATE2, create2);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_DESTROY, destroy);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_GET_PROPERTIES3, get_properties3);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_GET_ARGUMENT_PROPERTIES3, get_argument_properties3);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_SET_ARGUMENT, set_argument);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_INITIALIZE, initialize);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_APPEND_INITIALIZE, append_initialize);
  NPURT_GET(NPUNLOCK_ZE_GRAPH_DDI_APPEND_EXECUTE, append_execute);
#undef NPURT_GET
  if (graph->create2 == NULL || graph->destroy == NULL || graph->get_properties3 == NULL ||
      graph->get_argument_properties3 == NULL || graph->set_argument == NULL ||
      graph->append_execute == NULL) {
    NPURT_DIAG(diagnostic, "required native graph entry points are unavailable");
    return 0;
  }
  return 1;
}

static int npurt_find_compute_queue(const npurt_ze_functions *ze, npunlock_ze_device_handle device,
                                    uint32_t *ordinal, npurt_diagnostic *diagnostic) {
  npunlock_ze_command_queue_group_properties *groups;
  uint32_t count = 0;
  uint32_t index;
  uint32_t code = ze->device_get_queue_groups(device, &count, NULL);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeDeviceGetCommandQueueGroupProperties(count) failed (0x%08x)", code);
    return 0;
  }
  groups = (npunlock_ze_command_queue_group_properties *)calloc(count, sizeof(*groups));
  if (groups == NULL && count != 0) {
    return 0;
  }
  for (index = 0; index < count; ++index) {
    groups[index].stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
  }
  code = ze->device_get_queue_groups(device, &count, groups);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    free(groups);
    NPURT_DIAG(diagnostic, "zeDeviceGetCommandQueueGroupProperties(list) failed (0x%08x)", code);
    return 0;
  }
  for (index = 0; index < count; ++index) {
    if (groups[index].numQueues != 0 &&
        (groups[index].flags & NPUNLOCK_ZE_COMMAND_QUEUE_GROUP_COMPUTE) != 0) {
      *ordinal = index;
      free(groups);
      return 1;
    }
  }
  free(groups);
  NPURT_DIAG(diagnostic, "the NPU exposes no compute command queue group");
  return 0;
}

npurt_status npurt_device_warm_locked(npurt_state *state, npurt_diagnostic *diagnostic) {
  npunlock_ze_driver_properties driver_properties;
  npunlock_ze_context_desc context_desc;
  uint32_t graph_version;
  uint32_t code;

  if (state->warm) {
    return NPURT_OK;
  }
  if (state->context != NULL) {
    /* session established earlier in this process: re-arm without touching the
     * driver (see npurt_device_release_locked) */
    state->warm = 1;
    if (state->config.verbosity >= 2) {
      fprintf(stderr, "[npu-runtime] warm: re-armed existing Level Zero session\n");
    }
    return NPURT_OK;
  }
  memset(&driver_properties, 0, sizeof(driver_properties));
  memset(&context_desc, 0, sizeof(context_desc));

  if (state->loader == NULL) {
    state->loader = dlopen("libze_loader.so.1", RTLD_NOW | RTLD_LOCAL);
    if (state->loader == NULL) {
      NPURT_DIAG(diagnostic, "Level Zero loader libze_loader.so.1 was not found");
      return NPURT_ERR_NO_NPU;
    }
  }
  if (!npurt_load_ze_functions(state->loader, &state->ze)) {
    NPURT_DIAG(diagnostic, "the Level Zero loader is missing a required export");
    npurt_device_release_locked(state);
    return NPURT_ERR_DEVICE;
  }
  {
    npurt_status status = npurt_select_npu(&state->config, &state->ze, state, diagnostic);
    if (status != NPURT_OK) {
      npurt_device_abort_locked(state);
      return status;
    }
  }
  driver_properties.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
  code = state->ze.driver_get_properties(state->driver, &driver_properties);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeDriverGetProperties failed (0x%08x)", code);
    npurt_device_abort_locked(state);
    return NPURT_ERR_DEVICE;
  }
  state->driver_version = driver_properties.driverVersion;

  graph_version = npurt_find_graph_version(&state->ze, state->driver, diagnostic);
  if (graph_version == 0 ||
      !npurt_get_graph_functions(&state->ze, state->driver, graph_version, &state->graph,
                                 diagnostic)) {
    npurt_device_abort_locked(state);
    return NPURT_ERR_UNSUPPORTED;
  }
  context_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_CONTEXT_DESC;
  code = state->ze.context_create(state->driver, &context_desc, &state->context);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeContextCreate failed (0x%08x)", code);
    npurt_device_abort_locked(state);
    return NPURT_ERR_DEVICE;
  }
  if (!npurt_find_compute_queue(&state->ze, state->device, &state->queue_ordinal, diagnostic)) {
    npurt_device_abort_locked(state);
    return NPURT_ERR_UNSUPPORTED;
  }
  state->warm = 1;
  if (state->config.verbosity >= 2) {
    fprintf(stderr, "[npu-runtime] warm: driver %u device %u (vendor 0x%04x device 0x%04x) queue %u\n",
            state->driver_index, state->device_index, state->vendor_id, state->device_id,
            state->queue_ordinal);
  }
  return NPURT_OK;
}

/*
 * Soft release: drops the command queue and marks the session closed, but
 * deliberately keeps the Level Zero loader, driver, device and context alive.
 *
 * Measured with tools/session_probe.c: destroying the context and re-running
 * zeInitDrivers/zeContextCreate in the same process leaves the loader in a
 * state where the next zeCommandQueueExecuteCommandLists faults with SIGSEGV
 * inside libze_loader.so.1 (twice reproducible). Unloading the loader
 * (dlclose) before re-initialising does not help. The Level Zero session is
 * therefore process-lifetime state, which is also how the driver SDK intends
 * to be used: npurt_warm() after npurt_shutdown() re-arms the same session
 * instead of rebuilding it.
 */
void npurt_device_release_locked(npurt_state *state) {
  if (state->queue != NULL && state->ze.command_queue_destroy != NULL) {
    state->ze.command_queue_destroy(state->queue);
  }
  state->queue = NULL;
  state->warm = 0;
}

/*
 * Hard cleanup used only when bring-up failed before a context existed; safe
 * because no driver session was established.
 */
void npurt_device_abort_locked(npurt_state *state) {
  if (state->queue != NULL && state->ze.command_queue_destroy != NULL) {
    state->ze.command_queue_destroy(state->queue);
  }
  state->queue = NULL;
  if (state->context != NULL && state->ze.context_destroy != NULL) {
    state->ze.context_destroy(state->context);
  }
  state->context = NULL;
  state->driver = NULL;
  state->device = NULL;
  state->queue_ordinal = 0;
  memset(&state->graph, 0, sizeof(state->graph));
  memset(&state->ze, 0, sizeof(state->ze));
  if (state->loader != NULL) {
    dlclose(state->loader);
  }
  state->loader = NULL;
  state->warm = 0;
}

npurt_status npurt_device_submit_locked(npurt_state *state, npunlock_ze_graph_handle handle,
                                       int initialize, uint64_t timeout_ns, npurt_timing *timing,
                                       npurt_diagnostic *diagnostic) {
  npunlock_ze_command_queue_desc queue_desc;
  npunlock_ze_command_list_desc list_desc;
  npunlock_ze_fence_desc fence_desc;
  npunlock_ze_command_queue_handle queue = NULL;
  npunlock_ze_command_list_handle list = NULL;
  npunlock_ze_fence_handle fence = NULL;
  uint64_t start = npurt_now_ns();
  uint64_t mark;
  uint64_t submit_start;
  uint64_t wait_start = 0;
  uint64_t teardown_start = 0;
  uint32_t code;
  npurt_status status = NPURT_ERR_DEVICE;

  memset(&queue_desc, 0, sizeof(queue_desc));
  memset(&list_desc, 0, sizeof(list_desc));
  memset(&fence_desc, 0, sizeof(fence_desc));
  queue_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
  queue_desc.ordinal = state->queue_ordinal;
  list_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
  list_desc.commandQueueGroupOrdinal = state->queue_ordinal;
  fence_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_FENCE_DESC;

  if (state->queue == NULL) {
    code = state->ze.command_queue_create(state->context, state->device, &queue_desc, &state->queue);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "zeCommandQueueCreate failed (0x%08x)", code);
      goto done;
    }
  }
  queue = state->queue;
  code = state->ze.command_list_create(state->context, state->device, &list_desc, &list);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeCommandListCreate failed (0x%08x)", code);
    goto done;
  }
  NPURT_TRACE("list=%p created", (void *)list);
  code = initialize ? state->graph.append_initialize(list, handle, NULL, 0, NULL)
                    : state->graph.append_execute(list, handle, NULL, NULL, 0, NULL);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, initialize ? "pfnAppendGraphInitialize failed (0x%08x)"
                                      : "pfnAppendGraphExecute failed (0x%08x)", code);
    goto done;
  }
  code = state->ze.command_list_close(list);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeCommandListClose failed (0x%08x)", code);
    goto done;
  }
  code = state->ze.fence_create(queue, &fence_desc, &fence);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeFenceCreate failed (0x%08x)", code);
    goto done;
  }
  NPURT_TRACE("queue=%p handle=%p initialize=%d", (void *)state->queue, (void *)handle, initialize);
  mark = npurt_now_ns();
  if (timing != NULL) {
    timing->session_ns = mark - start;
  }
  submit_start = npurt_now_ns();
  NPURT_TRACE("executing queue=%p list=%p fence=%p", (void *)queue, (void *)list, (void *)fence);
  code = state->ze.command_queue_execute(queue, 1, &list, fence);
  wait_start = npurt_now_ns();
  NPURT_TRACE("execute returned 0x%08x", code);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeCommandQueueExecuteCommandLists failed (0x%08x)", code);
    goto done;
  }
  code = state->ze.fence_synchronize(fence, timeout_ns);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    if (timing != NULL) {
      timing->wait_ns = npurt_now_ns() - wait_start;
    }
    if (code == 0x70000005u /* ZE_RESULT_NOT_READY */ || code == 0x00000001u) {
      NPURT_DIAG(diagnostic, "graph execution did not finish within %llu ns (0x%08x)",
                 (unsigned long long)timeout_ns, code);
      status = NPURT_ERR_TIMEOUT;
    } else {
      NPURT_DIAG(diagnostic, "zeFenceHostSynchronize failed (0x%08x)", code);
    }
    goto done;
  }
  NPURT_TRACE("sync returned 0x%08x", code);
  teardown_start = npurt_now_ns();
  if (timing != NULL) {
    timing->submit_ns = wait_start - submit_start;
    timing->wait_ns = teardown_start - wait_start;
  }
  status = NPURT_OK;

done:
  if (fence != NULL) {
    state->ze.fence_destroy(fence);
  }
  if (list != NULL) {
    state->ze.command_list_destroy(list);
  }
  if (timing != NULL) {
    uint64_t end = npurt_now_ns();
    timing->teardown_ns = teardown_start != 0 ? end - teardown_start : 0;
    timing->total_ns = end - start;
  }

  (void)mark;
  return status;
}

/*
 * Per-invocation fast path.
 *
 * The command list that carries the graph execute is built once and re-submitted
 * for every call; Level Zero permits submitting a closed command list again
 * after it has completed, and the fence is re-armed by the submission. That
 * removes the list/fence create+destroy churn from the steady-state path
 * (measured at ~0.12 ms of the ~1.0 ms invocation on the 1x16 carrier).
 */
npurt_status npurt_device_submit_kernel_locked(npurt_state *state, struct npurt_kernel *kernel,
                                              uint64_t timeout_ns, npurt_timing *timing,
                                              npurt_diagnostic *diagnostic) {
  npunlock_ze_command_list_desc list_desc;
  npunlock_ze_fence_desc fence_desc;
  uint64_t start = npurt_now_ns();
  uint64_t mark;
  uint64_t submit_start;
  uint64_t wait_start = 0;
  uint64_t end = 0;
  uint32_t code;
  npurt_status status = NPURT_ERR_DEVICE;

  memset(&list_desc, 0, sizeof(list_desc));
  memset(&fence_desc, 0, sizeof(fence_desc));
  list_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
  list_desc.commandQueueGroupOrdinal = state->queue_ordinal;
  fence_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_FENCE_DESC;

  if (state->queue == NULL) {
    npunlock_ze_command_queue_desc queue_desc;
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queue_desc.ordinal = state->queue_ordinal;
    code = state->ze.command_queue_create(state->context, state->device, &queue_desc, &state->queue);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "zeCommandQueueCreate failed (0x%08x)", code);
      return NPURT_ERR_DEVICE;
    }
  }
  if (!kernel->list_ready) {
    code = state->ze.command_list_create(state->context, state->device, &list_desc, &kernel->list);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "zeCommandListCreate failed (0x%08x)", code);
      return NPURT_ERR_DEVICE;
    }
    code = state->graph.append_execute(kernel->list, kernel->handle, NULL, NULL, 0, NULL);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "pfnAppendGraphExecute failed (0x%08x)", code);
      return NPURT_ERR_DEVICE;
    }
    code = state->ze.command_list_close(kernel->list);
    if (code != NPUNLOCK_ZE_SUCCESS) {
      NPURT_DIAG(diagnostic, "zeCommandListClose failed (0x%08x)", code);
      return NPURT_ERR_DEVICE;
    }
    kernel->list_ready = 1;
    NPURT_TRACE("kernel list=%p created", (void *)kernel->list);
  }
  /* A fence must be fresh for every submission: re-using one leaves it in the
   * signaled state, zeFenceHostSynchronize then returns before the device has
   * produced this invocation's output and the caller reads reset (NaN) or stale
   * bytes. Measured: with a shared fence every second invocation returned the
   * untouched output window. */
  code = state->ze.fence_create(state->queue, &fence_desc, &kernel->fence);
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeFenceCreate failed (0x%08x)", code);
    return NPURT_ERR_DEVICE;
  }
  mark = npurt_now_ns();
  if (timing != NULL) {
    timing->session_ns = mark - start;
  }
  submit_start = npurt_now_ns();
  code = state->ze.command_queue_execute(state->queue, 1, &kernel->list, kernel->fence);
  wait_start = npurt_now_ns();
  if (code != NPUNLOCK_ZE_SUCCESS) {
    NPURT_DIAG(diagnostic, "zeCommandQueueExecuteCommandLists failed (0x%08x)", code);
    goto done;
  }
  code = state->ze.fence_synchronize(kernel->fence, timeout_ns);
  end = npurt_now_ns();
  if (timing != NULL) {
    timing->submit_ns = wait_start - submit_start;
    timing->wait_ns = end - wait_start;
  }
  if (code != NPUNLOCK_ZE_SUCCESS) {
    if (code == 0x00000001u) {
      NPURT_DIAG(diagnostic, "graph execution did not finish within %llu ns (0x%08x)",
                 (unsigned long long)timeout_ns, code);
      status = NPURT_ERR_TIMEOUT;
    } else {
      NPURT_DIAG(diagnostic, "zeFenceHostSynchronize failed (0x%08x)", code);
    }
    goto done;
  }
  status = NPURT_OK;

done:
  if (kernel->fence != NULL) {
    state->ze.fence_destroy(kernel->fence);
    kernel->fence = NULL;
  }
  if (timing != NULL) {
    uint64_t finish = npurt_now_ns();
    timing->teardown_ns = finish - end;
    timing->total_ns = finish - start;
  }
  return status;
}
