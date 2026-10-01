/* mprobe.c - minimal flexible NPU graph executor (multiop measurement harness).
 *
 * Unlike npunlock-linux-probe (fixed 32-byte tensor contract), this harness
 * queries the graph's real argument shapes, allocates exactly those sizes,
 * lets the caller fill inputs, and can time graph create / initialize /
 * execute separately and repeat execution on a live graph.
 *
 * Build: cc -O2 -I<npunlock>/src/workers mprobe.c -o mprobe -ldl
 * usage: mprobe --graph G.blob [--fill ramp|zeros|ones] [--input FILE]
 *               [--repeat N] [--dump PREFIX] [--json]
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "level_zero_min.h"

#define AUTO_INDEX UINT32_MAX

typedef struct arg {
  uint32_t index;
  uint32_t type;   /* 0 input, 1 output */
  uint32_t precision;
  uint32_t dims[5];
  uint32_t dims_count;
  char name[256];
  size_t size;
  void *allocation;
} arg_t;

typedef struct fns {
  npunlock_ze_init_drivers_fn init_drivers;
  npunlock_ze_init_fn init;
  npunlock_ze_driver_get_fn driver_get;
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
} fns_t;

typedef struct gfns {
  npunlock_ze_graph_create2_fn create2;
  npunlock_ze_graph_destroy_fn destroy;
  npunlock_ze_graph_get_properties3_fn get_properties3;
  npunlock_ze_graph_get_argument_properties3_fn get_argument_properties3;
  npunlock_ze_graph_set_argument_fn set_argument;
  npunlock_ze_graph_initialize_fn initialize;
  npunlock_ze_graph_append_initialize_fn append_initialize;
  npunlock_ze_graph_append_execute_fn append_execute;
} gfns_t;

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static void *load_sym(void *lib, const char *name) {
  void *p = dlsym(lib, name);
  if (!p) {
    fprintf(stderr, "mprobe: missing symbol %s\n", name);
    exit(3);
  }
  return p;
}

#define LOAD(lib, dest, name) memcpy(&(dest), &(void *){load_sym(lib, name)}, sizeof(void *))

static int fill_input(arg_t *a, const char *mode, const uint8_t *file, size_t file_size) {
  if (file) {
    if (file_size != a->size) {
      fprintf(stderr, "mprobe: --input size %zu != graph arg size %zu\n", file_size, a->size);
      return 0;
    }
    memcpy(a->allocation, file, file_size);
    return 1;
  }
  if (a->precision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16) {
    uint16_t *p = a->allocation;
    size_t n = a->size / 2;
    for (size_t i = 0; i < n; i++) {
      if (!strcmp(mode, "zeros")) p[i] = 0x0000;
      else if (!strcmp(mode, "ones")) p[i] = 0x3C00;
      else p[i] = (uint16_t)(0x3C00 + i); /* ramp */
    }
  } else {
    uint32_t *p = a->allocation;
    size_t n = a->size / 4;
    for (size_t i = 0; i < n; i++) p[i] = !strcmp(mode, "ones") ? 0x3f800000u : (uint32_t)i;
  }
  return 1;
}

static uint8_t *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *d = malloc((size_t)n);
  if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
  fclose(f);
  *size = (size_t)n;
  return d;
}

int main(int argc, char **argv) {
  const char *graph_path = NULL, *fill = "ramp", *input_path = NULL, *dump_prefix = NULL;
  int repeat = 1, json = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--graph") && i + 1 < argc) graph_path = argv[++i];
    else if (!strcmp(argv[i], "--fill") && i + 1 < argc) fill = argv[++i];
    else if (!strcmp(argv[i], "--input") && i + 1 < argc) input_path = argv[++i];
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_prefix = argv[++i];
    else if (!strcmp(argv[i], "--repeat") && i + 1 < argc) repeat = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--json")) json = 1;
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
  }
  if (!graph_path) { fprintf(stderr, "usage: mprobe --graph G.blob [options]\n"); return 2; }
  size_t graph_size = 0;
  uint8_t *graph = read_file(graph_path, &graph_size);
  if (!graph) { fprintf(stderr, "mprobe: cannot read %s\n", graph_path); return 2; }

  void *lib = dlopen("libze_loader.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "mprobe: no libze_loader\n"); return 3; }
  fns_t ze = {0};
  LOAD(lib, ze.driver_get_extensions, "zeDriverGetExtensionProperties");
  LOAD(lib, ze.driver_get_extension_address, "zeDriverGetExtensionFunctionAddress");
  LOAD(lib, ze.device_get, "zeDeviceGet");
  LOAD(lib, ze.device_get_properties, "zeDeviceGetProperties");
  LOAD(lib, ze.context_create, "zeContextCreate");
  LOAD(lib, ze.context_destroy, "zeContextDestroy");
  LOAD(lib, ze.device_get_queue_groups, "zeDeviceGetCommandQueueGroupProperties");
  LOAD(lib, ze.mem_alloc_host, "zeMemAllocHost");
  LOAD(lib, ze.mem_free, "zeMemFree");
  LOAD(lib, ze.command_queue_create, "zeCommandQueueCreate");
  LOAD(lib, ze.command_queue_destroy, "zeCommandQueueDestroy");
  LOAD(lib, ze.command_queue_execute, "zeCommandQueueExecuteCommandLists");
  LOAD(lib, ze.command_list_create, "zeCommandListCreate");
  LOAD(lib, ze.command_list_close, "zeCommandListClose");
  LOAD(lib, ze.command_list_destroy, "zeCommandListDestroy");
  LOAD(lib, ze.fence_create, "zeFenceCreate");
  LOAD(lib, ze.fence_destroy, "zeFenceDestroy");
  LOAD(lib, ze.fence_synchronize, "zeFenceHostSynchronize");
  void *dummy = dlsym(lib, "zeInitDrivers");
  if (dummy) memcpy(&ze.init_drivers, &dummy, sizeof(void *));
  else { LOAD(lib, ze.init, "zeInit"); LOAD(lib, ze.driver_get, "zeDriverGet"); }

  npunlock_ze_init_driver_type_desc itd = {NPUNLOCK_ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC, NULL,
                                           NPUNLOCK_ZE_INIT_DRIVER_TYPE_FLAG_NPU};
  uint32_t count = 0;
  uint32_t code;
  npunlock_ze_driver_handle driver = NULL;
  npunlock_ze_device_handle device = NULL;
  if (ze.init_drivers) code = ze.init_drivers(&count, NULL, &itd);
  else { code = ze.init(0); if (!code) code = ze.driver_get(&count, NULL); }
  if (code || count == 0) { fprintf(stderr, "mprobe: no driver (%u)\n", code); return 3; }
  npunlock_ze_driver_handle *drivers = calloc(count, sizeof(*drivers));
  if (ze.init_drivers) ze.init_drivers(&count, drivers, &itd);
  else ze.driver_get(&count, drivers);
  for (uint32_t d = 0; d < count && !device; d++) {
    uint32_t dn = 0;
    if (ze.device_get(drivers[d], &dn, NULL) || dn == 0) continue;
    npunlock_ze_device_handle *devs = calloc(dn, sizeof(*devs));
    ze.device_get(drivers[d], &dn, devs);
    for (uint32_t i = 0; i < dn; i++) {
      npunlock_ze_device_properties p = {0};
      p.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      if (!ze.device_get_properties(devs[i], &p) && p.type == NPUNLOCK_ZE_DEVICE_TYPE_VPU) {
        driver = drivers[d];
        device = devs[i];
        break;
      }
    }
    free(devs);
  }
  if (!device) { fprintf(stderr, "mprobe: no VPU device\n"); return 3; }

  uint32_t ecount = 0;
  ze.driver_get_extensions(driver, &ecount, NULL);
  npunlock_ze_driver_extension_properties *exts = calloc(ecount ? ecount : 1, sizeof(*exts));
  ze.driver_get_extensions(driver, &ecount, exts);
  uint32_t gver = 0;
  for (uint32_t i = 0; i < ecount; i++)
    if (!strcmp(exts[i].name, "ZE_extension_graph")) gver = exts[i].version;
  free(exts);
  if (!gver) { fprintf(stderr, "mprobe: no graph extension\n"); return 3; }

  void *raw = NULL;
  ze.driver_get_extension_address(driver, "ZE_extension_driver_npu", &raw);
  npunlock_ze_npu_get_extension_fn get_ext = NULL;
  memcpy(&get_ext, raw, sizeof(get_ext));
  npunlock_ze_graph_ddi_prefix *ddi = NULL;
  npunlock_ze_driver_extension_npu req = {NPUNLOCK_ZE_STRUCTURE_TYPE_DRIVER_EXTENSION_NPU_EXT, NULL,
                                          "ZE_extension_graph", gver, (void **)&ddi};
  if (get_ext(driver, &req) || !ddi) { fprintf(stderr, "mprobe: no graph ddi\n"); return 3; }
  gfns_t g = {0};
  memcpy(&g.create2, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_CREATE2], sizeof(void *));
  memcpy(&g.destroy, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_DESTROY], sizeof(void *));
  memcpy(&g.get_properties3, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_GET_PROPERTIES3], sizeof(void *));
  memcpy(&g.get_argument_properties3, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_GET_ARGUMENT_PROPERTIES3],
         sizeof(void *));
  memcpy(&g.set_argument, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_SET_ARGUMENT], sizeof(void *));
  memcpy(&g.initialize, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_INITIALIZE], sizeof(void *));
  memcpy(&g.append_initialize, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_APPEND_INITIALIZE],
         sizeof(void *));
  memcpy(&g.append_execute, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_APPEND_EXECUTE], sizeof(void *));

  npunlock_ze_context_desc cd = {NPUNLOCK_ZE_STRUCTURE_TYPE_CONTEXT_DESC, NULL, 0};
  npunlock_ze_context_handle ctx = NULL;
  code = ze.context_create(driver, &cd, &ctx);
  if (code) { fprintf(stderr, "mprobe: context 0x%x\n", code); return 3; }

  double t0 = now_ms();
  npunlock_ze_graph_desc_2 gd = {NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_DESC_2, NULL,
                                 NPUNLOCK_ZE_GRAPH_FORMAT_NATIVE, graph_size, graph, NULL, 0};
  npunlock_ze_graph_handle gh = NULL;
  code = g.create2(ctx, device, &gd, &gh);
  double t_create = now_ms() - t0;
  if (code || !gh) { fprintf(stderr, "mprobe: create2 0x%x\n", code); return 4; }

  npunlock_ze_graph_properties_3 gp = {0};
  gp.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_PROPERTIES_3;
  g.get_properties3(gh, &gp);
  arg_t *args = calloc(gp.numGraphArgs ? gp.numGraphArgs : 1, sizeof(*args));
  for (uint32_t i = 0; i < gp.numGraphArgs; i++) {
    npunlock_ze_graph_argument_properties_3 ap = {0};
    ap.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_ARGUMENT_PROPERTIES_3;
    g.get_argument_properties3(gh, i, &ap);
    args[i].index = i;
    args[i].type = ap.type;
    args[i].precision = ap.devicePrecision;
    args[i].dims_count = ap.dims_count;
    memcpy(args[i].dims, ap.dims, sizeof(ap.dims));
    memcpy(args[i].name, ap.name, sizeof(ap.name));
    size_t elems = 1;
    for (uint32_t k = 0; k < ap.dims_count; k++) elems *= ap.dims[k];
    args[i].size = elems * (ap.devicePrecision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16 ? 2 : 4);
    printf("arg[%u] %-8s %s dims=%u", i, ap.type ? "output" : "input",
           ap.devicePrecision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16 ? "f16" : "f32",
           ap.dims_count);
    for (uint32_t k = 0; k < ap.dims_count; k++) printf("x%u", ap.dims[k]);
    printf(" bytes=%zu name=%s\n", args[i].size, args[i].name);
  }
  printf("graph: numGraphArgs=%u initStageRequired=0x%x create_ms=%.3f\n", gp.numGraphArgs,
         gp.initStageRequired, t_create);

  uint8_t *input_file = NULL;
  size_t input_size = 0;
  if (input_path) {
    input_file = read_file(input_path, &input_size);
    if (!input_file) { fprintf(stderr, "mprobe: cannot read input\n"); return 2; }
  }
  for (uint32_t i = 0; i < gp.numGraphArgs; i++) {
    npunlock_ze_host_mem_alloc_desc hd = {NPUNLOCK_ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, NULL,
                                          args[i].type ? 0 : NPUNLOCK_ZE_HOST_MEM_ALLOC_WRITE_COMBINED};
    size_t alloc = (args[i].size + 4095) & ~(size_t)4095;
    code = ze.mem_alloc_host(ctx, &hd, alloc, 4096, &args[i].allocation);
    if (code) { fprintf(stderr, "mprobe: alloc 0x%x\n", code); return 4; }
    memset(args[i].allocation, 0, alloc);
    if (!args[i].type) {
      if (!fill_input(&args[i], fill, input_file, input_size)) return 2;
    } else if (args[i].precision == NPUNLOCK_ZE_GRAPH_ARGUMENT_PRECISION_FP16) {
      uint16_t *p = args[i].allocation;
      for (size_t k = 0; k < args[i].size / 2; k++) p[k] = 0x7e00; /* fp16 qNaN */
    }
  }
  if (!g.set_argument || !g.append_execute) { fprintf(stderr, "mprobe: ddi incomplete\n"); return 4; }
  for (uint32_t i = 0; i < gp.numGraphArgs; i++)
    if ((code = g.set_argument(gh, i, args[i].allocation))) {
      fprintf(stderr, "mprobe: set_argument 0x%x\n", code);
      return 4;
    }

  uint32_t qcount = 0, ordinal = 0;
  ze.device_get_queue_groups(device, &qcount, NULL);
  npunlock_ze_command_queue_group_properties *qg = calloc(qcount ? qcount : 1, sizeof(*qg));
  for (uint32_t i = 0; i < qcount; i++) qg[i].stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
  ze.device_get_queue_groups(device, &qcount, qg);
  for (uint32_t i = 0; i < qcount; i++)
    if (qg[i].numQueues && (qg[i].flags & NPUNLOCK_ZE_COMMAND_QUEUE_GROUP_COMPUTE)) ordinal = i;
  free(qg);

  double t_init = 0, t_exec_total = 0, t_exec_first = 0;
  npunlock_ze_command_queue_desc qd = {NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, NULL, ordinal, 0, 0, 0, 0};
  npunlock_ze_command_list_desc ld = {NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, NULL, ordinal, 0};
  npunlock_ze_fence_desc fd = {NPUNLOCK_ZE_STRUCTURE_TYPE_FENCE_DESC, NULL, 0};
  npunlock_ze_command_queue_handle q = NULL;
  npunlock_ze_command_list_handle cl = NULL;
  npunlock_ze_fence_handle f = NULL;
  if ((code = ze.command_queue_create(ctx, device, &qd, &q))) { fprintf(stderr, "mprobe: q 0x%x\n", code); return 4; }
  if ((code = ze.command_list_create(ctx, device, &ld, &cl))) { fprintf(stderr, "mprobe: cl 0x%x\n", code); return 4; }
  if ((code = ze.fence_create(q, &fd, &f))) { fprintf(stderr, "mprobe: fence 0x%x\n", code); return 4; }

  t0 = now_ms();
  if ((gp.initStageRequired & NPUNLOCK_ZE_GRAPH_STAGE_INITIALIZE) && g.initialize) {
    code = g.initialize(gh);
    if (code) { fprintf(stderr, "mprobe: initialize 0x%x\n", code); return 5; }
  }
  if (gp.initStageRequired & NPUNLOCK_ZE_GRAPH_STAGE_COMMAND_LIST_INITIALIZE) {
    code = g.append_initialize(cl, gh, NULL, 0, NULL);
    if (code) { fprintf(stderr, "mprobe: append_initialize 0x%x\n", code); return 5; }
    ze.command_list_close(cl);
    ze.command_queue_execute(q, 1, &cl, f);
    ze.fence_synchronize(f, 5000000000ull);
    ze.command_list_destroy(cl);
    ze.command_list_create(ctx, device, &ld, &cl);
  }
  t_init = now_ms() - t0;

  for (int r = 0; r < repeat; r++) {
    if (r > 0) {
      for (uint32_t i = 0; i < gp.numGraphArgs; i++) g.set_argument(gh, i, args[i].allocation);
    }
    t0 = now_ms();
    code = g.append_execute(cl, gh, NULL, NULL, 0, NULL);
    if (!code) code = ze.command_list_close(cl);
    if (!code) code = ze.command_queue_execute(q, 1, &cl, f);
    if (!code) code = ze.fence_synchronize(f, 5000000000ull);
    double dt = now_ms() - t0;
    if (r == 0) t_exec_first = dt;
    t_exec_total += dt;
    if (code) { fprintf(stderr, "mprobe: exec[%d] 0x%x\n", r, code); return 6; }
    if (r + 1 < repeat) {
      ze.command_list_destroy(cl);
      ze.command_list_create(ctx, device, &ld, &cl);
    }
  }
  printf("timing create_ms=%.3f init_ms=%.3f exec_first_ms=%.3f exec_mean_ms=%.3f "
         "exec_total_ms=%.3f repeat=%d\n",
         t_create, t_init, t_exec_first, t_exec_total / repeat, t_exec_total, repeat);

  for (uint32_t i = 0; i < gp.numGraphArgs; i++) {
    if (!args[i].type) continue;
    printf("OUTPUT arg[%u] (%zu bytes):", i, args[i].size);
    uint8_t *p = args[i].allocation;
    for (size_t k = 0; k < args[i].size; k++) printf(" %02x", p[k]);
    printf("\n");
    if (dump_prefix) {
      char path[512];
      snprintf(path, sizeof(path), "%s-%u.bin", dump_prefix, i);
      FILE *o = fopen(path, "wb");
      fwrite(args[i].allocation, 1, args[i].size, o);
      fclose(o);
      fprintf(stderr, "mprobe: wrote %s\n", path);
    }
  }
  if (json) printf("{\"create_ms\":%.3f,\"init_ms\":%.3f,\"exec_first_ms\":%.3f,\"exec_mean_ms\":%.3f}\n",
                   t_create, t_init, t_exec_first, t_exec_total / repeat);
  g.destroy(gh);
  ze.context_destroy(ctx);
  return 0;
}
