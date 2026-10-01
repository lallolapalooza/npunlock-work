/* ircompile.c - minimal flexible NPU graph executor (multiop measurement harness).
 *
 * Unlike npunlock-linux-probe (fixed 32-byte tensor contract), this harness
 * queries the graph's real argument shapes, allocates exactly those sizes,
 * lets the caller fill inputs, and can time graph create / initialize /
 * execute separately and repeat execution on a live graph.
 *
 * Build: cc -O2 -I<npunlock>/src/workers ircompile.c -o ircompile -ldl
 * usage: ircompile --graph G.blob [--fill ramp|zeros|ones] [--input FILE]
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
    fprintf(stderr, "ircompile: missing symbol %s\n", name);
    exit(3);
  }
  return p;
}

#define LOAD(lib, dest, name) memcpy(&(dest), &(void *){load_sym(lib, name)}, sizeof(void *))

static int fill_input(arg_t *a, const char *mode, const uint8_t *file, size_t file_size) {
  if (file) {
    if (file_size != a->size) {
      fprintf(stderr, "ircompile: --input size %zu != graph arg size %zu\n", file_size, a->size);
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
  const char *xml_path = NULL, *weights_path = NULL, *out_path = NULL, *build_flags = NULL;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--ir") && i + 1 < argc) xml_path = argv[++i];
    else if (!strcmp(argv[i], "--weights") && i + 1 < argc) weights_path = argv[++i];
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) out_path = argv[++i];
    else if (!strcmp(argv[i], "--build-flags") && i + 1 < argc) build_flags = argv[++i];
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
  }
  if (!xml_path || !out_path) {
    fprintf(stderr, "usage: ircompile --ir X.xml [--weights W.bin] --out blob.bin [--build-flags S]\n");
    return 2;
  }

  void *lib = dlopen("libze_loader.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "ircompile: no libze_loader\n"); return 3; }
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
  if (code || count == 0) { fprintf(stderr, "ircompile: no driver (%u)\n", code); return 3; }
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
  if (!device) { fprintf(stderr, "ircompile: no VPU device\n"); return 3; }

  uint32_t ecount = 0;
  ze.driver_get_extensions(driver, &ecount, NULL);
  npunlock_ze_driver_extension_properties *exts = calloc(ecount ? ecount : 1, sizeof(*exts));
  ze.driver_get_extensions(driver, &ecount, exts);
  uint32_t gver = 0;
  for (uint32_t i = 0; i < ecount; i++)
    if (!strcmp(exts[i].name, "ZE_extension_graph")) gver = exts[i].version;
  free(exts);
  if (!gver) { fprintf(stderr, "ircompile: no graph extension\n"); return 3; }

  void *raw = NULL;
  ze.driver_get_extension_address(driver, "ZE_extension_driver_npu", &raw);
  npunlock_ze_npu_get_extension_fn get_ext = NULL;
  memcpy(&get_ext, raw, sizeof(get_ext));
  npunlock_ze_graph_ddi_prefix *ddi = NULL;
  npunlock_ze_driver_extension_npu req = {NPUNLOCK_ZE_STRUCTURE_TYPE_DRIVER_EXTENSION_NPU_EXT, NULL,
                                          "ZE_extension_graph", gver, (void **)&ddi};
  if (get_ext(driver, &req) || !ddi) { fprintf(stderr, "ircompile: no graph ddi\n"); return 3; }
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
  if (code) { fprintf(stderr, "ircompile: context 0x%x\n", code); return 3; }

  /* device graph properties -> graph compiler version */
  npunlock_ze_graph_device_properties2_fn get2 = NULL;
  memcpy(&get2, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_DEVICE_PROPERTIES2], sizeof(get2));
  if (!get2) { fprintf(stderr, "ircompile: no DeviceGetGraphProperties2\n"); return 3; }
  npunlock_ze_device_graph_properties_2 dgp;
  memset(&dgp, 0, sizeof(dgp));
  dgp.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_DEVICE_GRAPH_PROPERTIES_2;
  code = get2(device, &dgp);
  if (code) { fprintf(stderr, "ircompile: DeviceGetGraphProperties2 0x%x\n", code); return 3; }
  printf("graph props: compiler=%u.%u formats=0x%x maxOpset=%u elf=%u.%u.%u runtime=%u.%u.%u\n",
         dgp.compilerVersion.major, dgp.compilerVersion.minor, dgp.graphFormatsSupported,
         dgp.maxOVOpsetVersionSupported, dgp.elfVersion.major, dgp.elfVersion.minor,
         dgp.elfVersion.patch, dgp.runtimeVersion.major, dgp.runtimeVersion.minor,
         dgp.runtimeVersion.patch);
  if (!(dgp.graphFormatsSupported & NPUNLOCK_ZE_GRAPH_FORMAT_NGRAPH_LITE)) {
    fprintf(stderr, "ircompile: NGRAPH_LITE not supported\n");
    return 3;
  }

  size_t xml_size = 0, w_size = 0;
  uint8_t *xml = read_file(xml_path, &xml_size);
  uint8_t *weights = NULL;
  if (weights_path) weights = read_file(weights_path, &w_size);
  if (!xml) { fprintf(stderr, "ircompile: cannot read xml\n"); return 2; }

  size_t total = 24 + xml_size + w_size;
  uint8_t *serialized = malloc(total);
  size_t off = 0;
  serialized[off++] = (uint8_t)dgp.compilerVersion.major;
  serialized[off++] = (uint8_t)(dgp.compilerVersion.major >> 8);
  serialized[off++] = (uint8_t)dgp.compilerVersion.minor;
  serialized[off++] = (uint8_t)(dgp.compilerVersion.minor >> 8);
  uint32_t two = 2;
  memcpy(serialized + off, &two, 4); off += 4;
  memcpy(serialized + off, &(uint64_t){xml_size}, 8); off += 8;
  memcpy(serialized + off, xml, xml_size); off += xml_size;
  memcpy(serialized + off, &(uint64_t){w_size}, 8); off += 8;
  if (w_size) memcpy(serialized + off, weights, w_size);
  fprintf(stderr, "ircompile: xml=%zu weights=%zu serialized=%zu build_flags=%s\n",
          xml_size, w_size, total, build_flags ? build_flags : "(none)");

  double t0 = now_ms();
  npunlock_ze_graph_desc_2 gd = {NPUNLOCK_ZE_STRUCTURE_TYPE_GRAPH_DESC_2, NULL,
                                 NPUNLOCK_ZE_GRAPH_FORMAT_NGRAPH_LITE, total, serialized,
                                 build_flags, 0};
  npunlock_ze_graph_handle gh = NULL;
  code = g.create2(ctx, device, &gd, &gh);
  double t_create = now_ms() - t0;
  if (code || !gh) { fprintf(stderr, "ircompile: create2(NGRAPH_LITE) 0x%x\n", code); return 4; }
  printf("compile create2_ms=%.3f\n", t_create);

  npunlock_ze_graph_get_native2_fn getn2 = NULL;
  memcpy(&getn2, &ddi->slots[NPUNLOCK_ZE_GRAPH_DDI_GET_NATIVE2], sizeof(getn2));
  if (!getn2) { fprintf(stderr, "ircompile: no GetNative2\n"); return 4; }
  const uint8_t *view = NULL;
  size_t blob_size = 0;
  code = getn2(gh, &blob_size, &view);
  if (code || !view || !blob_size) { fprintf(stderr, "ircompile: get_native2 0x%x\n", code); return 4; }
  FILE *o = fopen(out_path, "wb");
  fwrite(view, 1, blob_size, o);
  fclose(o);
  printf("native blob: %zu bytes -> %s (magic %.2x%.2x%.2x%.2x)\n", blob_size, out_path,
         blob_size > 3 ? view[0] : 0, blob_size > 3 ? view[1] : 0, blob_size > 3 ? view[2] : 0,
         blob_size > 3 ? view[3] : 0);
  g.destroy(gh);
  ze.context_destroy(ctx);
  return 0;
}
