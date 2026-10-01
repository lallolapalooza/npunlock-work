/* SHAVE toolchain driver: moviCompile -> moviAsm -> moviLLD.
 *
 * Runs the same commands the verified corpus runner uses
 * (npunlock-work/corpus/run_corpus.sh), but from inside the process: the
 * caller passes kernel C source and gets a linked SHAVE ELF back. Every stage
 * has a wall-clock timeout (kill the process group) so a misbehaving tool
 * cannot hang the host.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "npurt_internal.h"

#define NPURT_MAX_ARGV 64

static void npurt_path(char *destination, size_t capacity, const char *directory,
                       const char *name) {
  snprintf(destination, capacity, "%s/%s", directory, name);
}

static int npurt_write_file(const char *path, const uint8_t *data, size_t size) {
  FILE *stream = fopen(path, "wb");
  if (stream == NULL) {
    return 0;
  }
  if (fwrite(data, 1, size, stream) != size) {
    fclose(stream);
    return 0;
  }
  return fclose(stream) == 0;
}

static uint8_t *npurt_read_file(const char *path, size_t *size_out) {
  FILE *stream = fopen(path, "rb");
  uint8_t *data;
  long length;
  if (stream == NULL) {
    return NULL;
  }
  if (fseek(stream, 0, SEEK_END) != 0) {
    fclose(stream);
    return NULL;
  }
  length = ftell(stream);
  if (length <= 0 || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return NULL;
  }
  data = (uint8_t *)malloc((size_t)length);
  if (data == NULL) {
    fclose(stream);
    return NULL;
  }
  if (fread(data, 1, (size_t)length, stream) != (size_t)length) {
    free(data);
    fclose(stream);
    return NULL;
  }
  fclose(stream);
  *size_out = (size_t)length;
  return data;
}

static int npurt_append_flag(char **argv, int count, const char *flag, const char *value) {
  if (count + 2 >= NPURT_MAX_ARGV) {
    return count;
  }
  argv[count] = (char *)flag;
  argv[count + 1] = (char *)value;
  return count + 2;
}

/* Runs argv with the given stdio redirections; returns the exit status or -1. */
static int npurt_spawn(const char *const argv[], const char *stdin_path, const char *stdout_path,
                       const char *stderr_path, uint64_t timeout_ns, int *timed_out) {
  pid_t pid;
  uint64_t deadline;
  int status = 0;
  *timed_out = 0;
  pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    int fd;
    setpgid(0, 0);
    if (stdin_path != NULL) {
      fd = open(stdin_path, O_RDONLY);
      if (fd < 0) {
        _exit(126);
      }
      dup2(fd, STDIN_FILENO);
      close(fd);
    }
    if (stdout_path != NULL) {
      fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        _exit(126);
      }
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    if (stderr_path != NULL) {
      fd = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        _exit(126);
      }
      dup2(fd, STDERR_FILENO);
      close(fd);
    }
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }
  deadline = npurt_now_ns() + timeout_ns;
  for (;;) {
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      break;
    }
    if (result < 0 && errno != EINTR) {
      return -1;
    }
    if (npurt_now_ns() >= deadline) {
      *timed_out = 1;
      kill(-pid, SIGKILL);
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      return 124;
    }
    usleep(2000);
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return -1;
}

static void npurt_read_tail(char *destination, size_t capacity, const char *path) {
  char buffer[512];
  FILE *stream = fopen(path, "rb");
  size_t total = 0;
  destination[0] = '\0';
  if (stream == NULL) {
    return;
  }
  while (total + 1 < capacity && fgets(buffer, sizeof(buffer), stream) != NULL) {
    size_t length = strlen(buffer);
    if (total + length + 1 >= capacity) {
      break;
    }
    memcpy(destination + total, buffer, length);
    total += length;
  }
  fclose(stream);
  while (total > 0 && (destination[total - 1] == '\n' || destination[total - 1] == '\r')) {
    destination[--total] = '\0';
  }
  while (total > 0 && destination[0] == '\n') {
    memmove(destination, destination + 1, total);
    destination[--total] = '\0';
  }
}

static void npurt_split_flags(char *storage, const char *flags, int *count, char **argv, int limit) {
  char *cursor = storage;
  size_t length = strlen(flags);
  if (length >= 511) {
    length = 511;
  }
  memcpy(storage, flags, length);
  storage[length] = '\0';
  while (*cursor != '\0' && *count < limit) {
    while (*cursor == ' ' || *cursor == '\t') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    argv[(*count)++] = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      ++cursor;
    }
    if (*cursor != '\0') {
      *cursor++ = '\0';
    }
  }
}

npurt_status npurt_toolchain_compile_locked(const npurt_config *config, const char *source,
                                           size_t source_size, uint8_t **elf, size_t *elf_size,
                                           npurt_diagnostic *diagnostic) {
  char source_path[NPURT_CONFIG_PATH_BYTES + 32];
  char asm_path[NPURT_CONFIG_PATH_BYTES + 32];
  char object_path[NPURT_CONFIG_PATH_BYTES + 32];
  char elf_path[NPURT_CONFIG_PATH_BYTES + 32];
  char error_path[NPURT_CONFIG_PATH_BYTES + 32];
  char log_path[NPURT_CONFIG_PATH_BYTES + 32];
  char compiler[NPURT_CONFIG_PATH_BYTES + 32];
  char assembler[NPURT_CONFIG_PATH_BYTES + 32];
  char linker[NPURT_CONFIG_PATH_BYTES + 32];
  char library_path[NPURT_CONFIG_PATH_BYTES + 32];
  char flags_storage[512];
  char detail[512];

  const char *compile_argv[NPURT_MAX_ARGV];
  const char *assemble_argv[NPURT_MAX_ARGV];
  const char *link_argv[NPURT_MAX_ARGV];
  uint8_t *result = NULL;
  size_t result_size = 0;
  int count;
  int status;
  int timed_out = 0;
  size_t index;

  if (config->toolchain_dir[0] == '\0' || config->work_dir[0] == '\0') {
    NPURT_DIAG(diagnostic, "toolchain_dir and work_dir must be set");
    return NPURT_ERR_ARGUMENT;
  }
  if (mkdir(config->work_dir, 0777) != 0 && errno != EEXIST) {
    NPURT_DIAG(diagnostic, "cannot create work directory %s (%s)", config->work_dir,
               strerror(errno));
    return NPURT_ERR_IO;
  }
  npurt_path(source_path, sizeof(source_path), config->work_dir, "npurt-kernel.c");
  npurt_path(asm_path, sizeof(asm_path), config->work_dir, "npurt-kernel.s");
  npurt_path(object_path, sizeof(object_path), config->work_dir, "npurt-kernel.o");
  npurt_path(elf_path, sizeof(elf_path), config->work_dir, "npurt-kernel.elf");
  npurt_path(log_path, sizeof(log_path), config->work_dir, "npurt-tool.log");
  npurt_path(error_path, sizeof(error_path), config->work_dir, "npurt-tool.err");
  npurt_path(compiler, sizeof(compiler), config->toolchain_dir, "bin/moviCompile");
  npurt_path(assembler, sizeof(assembler), config->toolchain_dir, "bin/moviAsm");
  npurt_path(linker, sizeof(linker), config->toolchain_dir, "bin/moviLLD");

  if (!npurt_write_file(source_path, (const uint8_t *)source, source_size)) {
    NPURT_DIAG(diagnostic, "cannot write %s", source_path);
    return NPURT_ERR_IO;
  }

  /* --- stage 1: C -> assembly ------------------------------------------- */
  count = 0;
  compile_argv[count++] = compiler;
  compile_argv[count++] = "-cc1";
  compile_argv[count++] = "-triple";
  compile_argv[count++] = "shave";
  compile_argv[count++] = "-target-cpu";
  compile_argv[count++] = "3720xx";
  compile_argv[count++] = "-S";
  compile_argv[count++] = "-O2";
  compile_argv[count++] = "-x";
  compile_argv[count++] = "c";
  compile_argv[count++] = source_path;
  compile_argv[count++] = "-ffunction-sections";
  compile_argv[count++] = "-fdata-sections";
  if (config->include_dir[0] != '\0') {
    count = npurt_append_flag((char **)compile_argv, count, "-I", config->include_dir);
  }
  npurt_split_flags(flags_storage, config->extra_cflags, &count, (char **)compile_argv,
                    NPURT_MAX_ARGV - 3);
  count = npurt_append_flag((char **)compile_argv, count, "-o", asm_path);
  compile_argv[count] = NULL;
  if (config->verbosity >= 2) {
    fprintf(stderr, "[npu-runtime] moviCompile %s\n", source_path);
  }
  status = npurt_spawn(compile_argv, NULL, log_path, error_path, config->compile_timeout_ns,
                       &timed_out);
  if (status != 0) {
    npurt_read_tail(detail, sizeof(detail), error_path);
    NPURT_DIAG(diagnostic, "moviCompile failed (exit %d%s): %s", status,
               timed_out ? ", timeout" : "", detail);
    return timed_out ? NPURT_ERR_TIMEOUT : NPURT_ERR_COMPILE;
  }

  /* --- stage 2: assembly -> object -------------------------------------- */
  count = 0;
  assemble_argv[count++] = assembler;
  assemble_argv[count++] = "--cv";
  assemble_argv[count++] = "3720xx";
  assemble_argv[count++] = "--noSPrefixing";
  assemble_argv[count] = NULL;
  status = npurt_spawn(assemble_argv, asm_path, object_path, error_path,
                       config->compile_timeout_ns, &timed_out);
  if (status != 0) {
    npurt_read_tail(detail, sizeof(detail), error_path);
    NPURT_DIAG(diagnostic, "moviAsm failed (exit %d%s): %s", status, timed_out ? ", timeout" : "",
               detail);
    return timed_out ? NPURT_ERR_TIMEOUT : NPURT_ERR_COMPILE;
  }

  /* --- stage 3: link ---------------------------------------------------- */
  count = 0;
  link_argv[count++] = linker;
  link_argv[count++] = "-flavor";
  link_argv[count++] = "gnu";
  link_argv[count++] = "-EL";
  link_argv[count++] = "-e";
  link_argv[count++] = config->entry_symbol;
  link_argv[count++] = "-z";
  link_argv[count++] = "max-page-size=0x10";
  link_argv[count++] = "--gc-sections";
  if (config->link_script[0] != '\0') {
    count = npurt_append_flag((char **)link_argv, count, "-T", config->link_script);
  }
  link_argv[count++] = object_path;
  for (index = 0; index < config->link_library_count && index < NPURT_MAX_LINK_LIBRARIES;
       ++index) {
    if (config->link_libraries[index][0] == '\0') {
      continue;
    }
    snprintf(library_path, sizeof(library_path), "%s/lib/%s.a", config->toolchain_dir,
             config->link_libraries[index]);
    link_argv[count++] = library_path;
  }
  count = npurt_append_flag((char **)link_argv, count, "-o", elf_path);
  link_argv[count] = NULL;
  status = npurt_spawn(link_argv, NULL, log_path, error_path, config->compile_timeout_ns,
                       &timed_out);
  if (status != 0) {
    npurt_read_tail(detail, sizeof(detail), error_path);
    NPURT_DIAG(diagnostic, "moviLLD failed (exit %d%s): %s", status, timed_out ? ", timeout" : "",
               detail);
    return timed_out ? NPURT_ERR_TIMEOUT : NPURT_ERR_COMPILE;
  }

  result = npurt_read_file(elf_path, &result_size);
  if (result == NULL) {
    NPURT_DIAG(diagnostic, "linker produced no readable ELF at %s", elf_path);
    return NPURT_ERR_COMPILE;
  }
  *elf = result;
  *elf_size = result_size;
  return NPURT_OK;
}
