/*
 * Runs one kernel through the runtime and prints the resulting output tensor as
 * hex, so the result can be diffed against npunlock's independent ground truth
 * (llvm-14/tmpdump/dump_probe --bundle <bundle> prints ACTUAL).
 *
 *   npu-runtime-dump --elf KERNEL.elf [--carrier BLOB] [--runs N] [--input-hex HEX]
 *
 * The default input is the 16 fp16 values of npunlock's porting probe bundle
 * (tools/porting_probe.py INPUT_VALUES), so the bytes are directly comparable
 * with `dump_probe`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npu_runtime.h"

static const uint16_t default_input[16] = {
    0xc400, 0xc000, 0xbc00, 0xb800, 0x0000, 0x3400, 0x3800, 0x3c00,
    0x4000, 0x4200, 0x4400, 0x4800, 0x4c00, 0x5000, 0x5400, 0x5800};

static uint8_t *read_file(const char *path, size_t *size) {
  FILE *stream = fopen(path, "rb");
  long length;
  uint8_t *data;
  if (stream == NULL) {
    return NULL;
  }
  fseek(stream, 0, SEEK_END);
  length = ftell(stream);
  fseek(stream, 0, SEEK_SET);
  if (length <= 0) {
    fclose(stream);
    return NULL;
  }
  data = (uint8_t *)malloc((size_t)length);
  if (fread(data, 1, (size_t)length, stream) != (size_t)length) {
    free(data);
    fclose(stream);
    return NULL;
  }
  fclose(stream);
  *size = (size_t)length;
  return data;
}

static int parse_hex(const char *text, uint8_t *destination, size_t capacity, size_t *size_out) {
  size_t length = 0;
  while (text[0] != '\0' && length < capacity) {
    char pair[3];
    while (text[0] == ' ' || text[0] == ',') {
      ++text;
    }
    if (text[0] == '\0') {
      break;
    }
    if (text[1] == '\0' ||
        !((text[0] >= '0' && text[0] <= '9') || (text[0] >= 'a' && text[0] <= 'f') ||
          (text[0] >= 'A' && text[0] <= 'F')) ||
        !((text[1] >= '0' && text[1] <= '9') || (text[1] >= 'a' && text[1] <= 'f') ||
          (text[1] >= 'A' && text[1] <= 'F'))) {
      return 0;
    }
    pair[0] = text[0];
    pair[1] = text[1];
    pair[2] = '\0';
    destination[length++] = (uint8_t)strtoul(pair, NULL, 16);
    text += 2;
  }
  *size_out = length;
  return length != 0;
}

int main(int argc, char **argv) {
  const char *elf_path = NULL;
  const char *blob_path = NULL;
  const char *carrier = NULL;
  const char *source_path = NULL;
  const char *input_hex[8];
  size_t input_hex_count = 0;
  int runs = 1;
  int index;
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *kernel = NULL;
  npurt_kernel_info info;
  uint8_t *elf = NULL;
  size_t elf_size = 0;
  uint8_t *input[8];
  size_t input_bytes[8];
  size_t input_count = 0;
  uint8_t *output;
  size_t output_bytes = 0;
  int run;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--elf") == 0 && index + 1 < argc) {
      elf_path = argv[++index];
    } else if (strcmp(argv[index], "--blob") == 0 && index + 1 < argc) {
      blob_path = argv[++index];
    } else if (strcmp(argv[index], "--carrier") == 0 && index + 1 < argc) {
      carrier = argv[++index];
    } else if (strcmp(argv[index], "--from-source") == 0 && index + 1 < argc) {
      source_path = argv[++index];
    } else if (strcmp(argv[index], "--input-hex") == 0 && index + 1 < argc) {
      if (input_hex_count >= 8) {
        fprintf(stderr, "too many --input-hex options\n");
        return 2;
      }
      input_hex[input_hex_count++] = argv[++index];
    } else if (strcmp(argv[index], "--runs") == 0 && index + 1 < argc) {
      runs = atoi(argv[++index]);
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[index]);
      return 2;
    }
  }
  if (elf_path == NULL && source_path == NULL && blob_path == NULL) {
    fprintf(stderr,
            "usage: npu-runtime-dump (--elf KERNEL.elf | --from-source KERNEL.c | --blob GRAPH.blob)"
            " [--carrier BLOB] [--runs N] [--input-hex HEX]\n");
    return 2;
  }

  npurt_config_defaults(&config);
  if (carrier != NULL) {
    snprintf(config.carrier_blob, sizeof(config.carrier_blob), "%s", carrier);
  }
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/dump");
  config.verbosity = 1;
  npurt_diagnostic_init(&diagnostic);
  if (npurt_configure(&config, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "configure: %s\n", diagnostic.message);
    return 1;
  }
  if (source_path != NULL) {
    size_t source_size = 0;
    char *source = (char *)read_file(source_path, &source_size);
    if (source == NULL) {
      fprintf(stderr, "cannot read %s\n", source_path);
      return 1;
    }
    if (npurt_compile_source(source, source_size, &elf, &elf_size, &diagnostic) != NPURT_OK) {
      fprintf(stderr, "compile: %s\n", diagnostic.message);
      return 1;
    }
    free(source);
    printf("compiled %s -> %zu byte ELF\n", source_path, elf_size);
  } else {
    const char *path = blob_path != NULL ? blob_path : elf_path;
    elf = read_file(path, &elf_size);
    if (elf == NULL) {
      fprintf(stderr, "cannot read %s\n", path);
      return 1;
    }
  }
  if (blob_path != NULL
          ? npurt_kernel_create_from_blob(elf, elf_size, &kernel, &diagnostic) != NPURT_OK
          : npurt_kernel_create(elf, elf_size, &kernel, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "kernel create: %s\n", diagnostic.message);
    return 1;
  }
  npurt_kernel_info_get(kernel, &info);
  if (info.output_count != 1 || info.input_count == 0) {
    fprintf(stderr, "expected one output and at least one input tensor, got %u/%u\n",
            info.input_count, info.output_count);
    return 1;
  }
  for (index = 0; index < (int)info.tensor_count; ++index) {
    if (info.tensors[index].is_input) {
      if (input_count < 8) {
        input_bytes[input_count++] = (size_t)info.tensors[index].byte_size;
      }
    } else {
      output_bytes = (size_t)info.tensors[index].byte_size;
    }
  }
  for (index = 0; index < (int)input_count; ++index) {
    size_t element;
    input[index] = (uint8_t *)calloc(1, input_bytes[index]);
    if (index < (int)input_hex_count) {
      size_t parsed = 0;
      if (!parse_hex(input_hex[index], input[index], input_bytes[index], &parsed) ||
          parsed != input_bytes[index]) {
        fprintf(stderr, "--input-hex #%d is malformed or has the wrong size (graph input is %zu)\n",
                index, input_bytes[index]);
        return 2;
      }
    } else if (input_bytes[index] == sizeof(default_input) && input_hex_count == 0) {
      memcpy(input[index], default_input, sizeof(default_input));
    } else {
      for (element = 0; element < input_bytes[index] / 2; ++element) {
        ((uint16_t *)input[index])[element] = (uint16_t)(0x3c00u + index * 0x40u + element);
      }
    }
    printf("input%-2d :", index);
    for (element = 0; element < (input_bytes[index] < 64 ? input_bytes[index] : 64); ++element) {
      printf(" %02x", input[index][element]);
    }
    if (input_bytes[index] > 64) {
      printf(" ... (%zu bytes)", input_bytes[index]);
    }
    printf("\n");
  }
  output = (uint8_t *)calloc(1, output_bytes);
  for (run = 0; run < runs; ++run) {
    npurt_run_inputs_request request;
    npurt_timing timing;
    npurt_status status;
    memset(&request, 0, sizeof(request));
    memset(&timing, 0, sizeof(timing));
    memset(output, 0, output_bytes);
    request.inputs = (const void *const *)input;
    request.input_sizes = input_bytes;
    request.input_count = input_count;
    request.output = output;
    request.output_size = output_bytes;
    status = npurt_run_inputs(kernel, &request, &timing, &diagnostic);
    if (status != NPURT_OK) {
      fprintf(stderr, "run %d: %s %s\n", run, npurt_status_name(status), diagnostic.message);
      return 1;
    }
    printf("output  :");
    for (index = 0; index < (int)output_bytes; ++index) {
      printf(" %02x", output[index]);
    }
    printf("   (%.3f ms)\n", (double)timing.total_ns / 1e6);
  }
  npurt_shutdown();
  free(elf);
  for (index = 0; index < (int)input_count; ++index) {
    free(input[index]);
  }
  free(output);
  return 0;
}
