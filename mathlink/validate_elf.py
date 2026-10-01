#!/usr/bin/env python3
"""Validate a SHAVE ELF with npunlock's own npunlock_parse_shave_elf."""
import ctypes, sys, pathlib

class View(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t)]

class Buffer(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t),
                ("release", ctypes.c_void_p), ("context", ctypes.c_void_p)]

class Diagnostic(ctypes.Structure):
    _fields_ = [("struct_size", ctypes.c_uint32), ("json", Buffer)]

class Image(ctypes.Structure):
    _fields_ = [("file_offset", ctypes.c_size_t), ("size", ctypes.c_size_t),
                ("address", ctypes.c_uint32), ("entry", ctypes.c_uint32)]

lib = ctypes.CDLL("/home/asdf/projects/npunlock-work/npunlock/build/fresh/libnpunlock.so")
lib.npunlock_parse_shave_elf.argtypes = [View, ctypes.POINTER(Image), ctypes.POINTER(Diagnostic)]
lib.npunlock_diagnostic_release.argtypes = [ctypes.POINTER(Diagnostic)]
lib.npunlock_parse_shave_elf.restype = ctypes.c_int
lib.npunlock_status_name.argtypes = [ctypes.c_int]
lib.npunlock_status_name.restype = ctypes.c_char_p

for path in sys.argv[1:]:
    data = pathlib.Path(path).read_bytes()
    buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
    img = Image()
    diag = Diagnostic(ctypes.sizeof(Diagnostic))
    status = lib.npunlock_parse_shave_elf(View(buf, len(data)), ctypes.byref(img), ctypes.byref(diag))
    name = lib.npunlock_status_name(status).decode()
    detail = ctypes.string_at(diag.json.data, diag.json.size).decode("utf-8","replace") if diag.json.size else ""
    if status == 0:
        print(f"VALID   {path}: status=0 ({name}) file_offset=0x{img.file_offset:x} "
              f"size={img.size} (0x{img.size:x}) address=0x{img.address:08x} entry=0x{img.entry:08x}")
    else:
        print(f"INVALID {path}: status={status} ({name}): {detail}")
    lib.npunlock_diagnostic_release(ctypes.byref(diag))
