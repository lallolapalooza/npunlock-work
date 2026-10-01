# SUPPORTING — verbatim vendor evidence (instructions, templates, tables)

All line numbers refer to files under `/home/asdf/projects/r2gvp04w-re/`.
Provenance is labelled per block:
* `[STATIC-RE]` — read from Ghidra decompile / string corpus (reproducible now).
* `[VENDOR-PROBE, retired method]` — captured by executing the vendor
  assembler `moviAsm64.dll` offline (Wine, on throwaway copies; method now
  retired per project rule). The recorded bytes are static facts about the
  vendor encoder and are cross-checked against gold/corpus bytes.

---

## 1. Vendor asm-printer template corpus (moviCompile64 = our clang fork)

`re_strings/moviCompile64.ascii.txt`, mnemonic template block **86775–88090**.
These strings are printed by the SHAVE AsmPrinter; the adjacent operand
strings are emitted around them. Verbatim excerpts:

```
86775 (region start; scheduler/diagnostic strings interleaved)
87745  LMULL 
...
87830  .LDI.32 
87831  .LDI.8 
87832  .LDO.16 
87833  .LDO.32 
87834  .LDO.64 
87835  .LDO.64@HIopt 
87836  .LDO.64@LOopt 
87837  .LDO.8 
87838  .LDO.256 
87839  .LDO.128 
...
87861  .LD.32.f16.f32 
87862  .ST.@64_128.f16.u8 
...
87901  .ST.16                     <-- plain 16-bit store template
87902  .ST.16.f32.b16 
87909  .ST.256.f32.b16 
87918  .ST.32 
...
87935  .ST.8 
87936  .STI.16                    <-- STI (auto-increment) 16-bit template
87937  .STI.32 
87938  .STI.16.f32.f16            <-- *** the fused store the gold kernel uses
87939  .STI.8 
87940  .STI.64@LOopt 
87941  .STI.@64_128.f32.f16 
...
87959  .ST.16.f32.f16             <-- *** non-fused fused-convert store (our FS
                                   backend emits this mnemonic)
87960  .ST.16.i16.i8 
```

Verify with:
`sed -n '87830,87865p;87900,87965p' re_strings/moviCompile64.ascii.txt`

### 1a. Register-class template strings `[STATIC-RE]`

`re_strings/moviCompile64.ascii.txt:79259–79291` (class names) and
`78364–78366` (the two generic classes):

```
78364  Generic::Unknown Register Class
78365  Generic::ScalarRC
78366  Generic::VectorRC
...
79259  IRF8_q0
79260  IRF8_q1
...
79263  VRF16_e0
...
79270  VRF16_e7
79271  WVRF16_0
79272  IRF16_h
79273  IRF16_l
79274  VRF32_q0
...
79291  WVRF512
79302  SELECT_WVRF64_0
79309  LSU_CP16_vrf_irf0
```

`IRF*` = **integer register file** views (q0..q3 8-bit, h/l 16-bit
sub-register halves), `VRF*`/`WVRF*` = **vector register file** views,
`TRF_*` = temporary/transfer register file. `LSU_CP16_vrf_irf0` is the
cross-file copy node (`CP16` = 16-bit copy) used to move between VRF and IRF.

### 1b. Explicit-pipe template `[STATIC-RE]`

`re_strings/moviCompile64.ascii.txt:78385–78387` — the compiler's own
`abort` stub, showing the *canonical 3-slot pipe spelling*:

```
78385  BRU.SWIH 1 || LSU0.LDIL i18, 0xFFFF || LSU1.LDIH i18, 0xFFFF  // 'abort'
78386  LSU0.LDIL i18, 0xFFFF
78387  BRU.SWIH 1 || LSU0.LDIH i18, 0xFFFF  // 'abort'
```

Note the **slot order in the text is BRU first, LSU0, LSU1** — pipes are
written in *unit order*, not address order. Operators are literal ` || `.

Corroborating moviAsm diagnostics `[STATIC-RE]`
(`re_strings/moviAsm64.ascii.txt:2557–2641`):

```
2558  BRU unit is used more than once in the same pipe
2559  LSU0 unit is used more than once in the same pipe      (same family)
2562  Invalid NOP in pipe
2563  Double pipe found
2564  Invalid continued pipe after %s
2716  Invalid NOP syntax
```

---

## 2. Vendor asm template strings for the SHAVE ISA (moviAsm64)

`re_strings/moviAsm64.ascii.txt` — the assembler's own mnemonic list.
Exact 16-bit store mnemonics and their line numbers:

```
3513  LSU0.ST.32
3514  LSU0.ST.16
3515  LSU0.ST.8
...
3579  LSU0.STI.32
3580  LSU0.STI.16
3581  LSU0.STI.8
...
4384  LSU0.ST.16.U32.U16
4385  LSU0.ST.16.U32.U16S
4388  LSU0.ST.16.I16.I8
4389  LSU0.ST.16.I16.I8S
4395  LSU0.ST.16.I8.I4
4396  LSU0.ST.16.I8.I4S
4401  LSU0.ST.16.F16.U8
4411  LSU0.STI.32.U32.U8S
4421  LSU0.STI.16.F32.F16       <-- *** vendor narrowed store used by gold
4435  LSU0.STI.16.I8.I4
...
4663  LSU0.ST.16.F32.F16        <-- *** non-narrowing variant
4678  LSU1.ST.16.F32.F16
```

Note **there is no `LSU0.ST.16.F32.F16S`** and no `LSU*.ST.16.I8.I4`
"narrowing-from-f32" mnemonic — the *only* f32→f16 narrowing store is
`.STI.16.F32.F16` / `.ST.16.F32.F16` and its `@64_128` wide siblings.

### 2a. Register-name grammar `[STATIC-RE]`

`re_strings/moviAsm64.ascii.txt:2669`, `6300` (diagnostics), and the class
token block at VA `0x180692ea8` (documented in
`npunlock-work/shave-isa/ENCODING_NOTES.md` §1):

```
2669  Invalid register name: %s(expected format: %s)
        + format templates at 0x180692ec0:
          "<regNo>"            (integer, e.g. i7)
          "<regNo>.<subregNo>" (vector,   e.g. v7.0)
```

Class tokens stored as 4-byte strings in `.rdata` at `0x180692ea8`:
`"VR","VW","IW","IR","I","V"` — i.e. **separate I-class (integer) and
V-class (vector) operand kinds exist in the encoder's record model**.

---

## 3. Vendor compiler's own store-lowering / asm-print switch `[STATIC-RE]`

`re_decompiled/movicomp/decompiled_all.c` — function 2880
`FUN_18015aa60` (SHAVE AsmPrinter, starts line 238695). The store
opcodes are printed in a big `switch` on the MI opcode; the relevant cases:

```
240282  local_38 = ".ST.16 ";            (case 0x91)
240284  local_38 = ".ST.16.f32.b16 ";    (case 0x92)
240343  local_38 = ".ST.8 ";             (case 0x9d)
240345  local_38 = ".STI.16 ";           (case 0x9e)
240347  local_38 = ".STI.32 ";           (case 0x9f)
240351  local_38 = ".STI.16.f32.f16 ";   (case 0xa0)  <-- *** gold's store
240352  local_38 = ".STI.8 ";            (case 0xa1)
240356  local_38 = ".STI.@64_128.f32.f16 ";(case 0xa3)
240993  pcVar82 = ".ST.16.f32.f16 ";     (case 0xd5)  <-- *** our store
241003  pcVar82 = ".ST.16.i16.i8 ";      (case 0xd6)
```

The MI-opcode→text mapping above is the exact inverse of the assembler's
table; `.STI.16.f32.f16` is a *distinct MI opcode* (0xa0) from
`.ST.16.f32.f16` (0xd5). They are **not** the same instruction with a flag.

Operand emission is `FUN_18015f400` (function 2882, line 241466): it indexes
a per-operand record `{char kind; ...; u32 value @+8}` and prints
`0x%.4x` for immediates / delegates register names to `param_1`'s virtual
`printRegName` (`FUN_180c43950`, line 241538). There is **no operand-order
swap** anywhere in this path: the record order in the DAG determines text
order. So vendor text `op0, op1` = DAG operands in order.

---

## 4. Vendor assembler encoder record tables `[STATIC-RE]`

`re_decompiled/moviAsm64/decompiled_all.c`. Instruction definitions are
built at init by `FUN_180433f50(obj, name, span)` from 16-byte records
`{const char* class; u32 a; u32 b}` (class ptr, literal/operand index,
bit width). Extracts relevant to the store question:

```
370934–370982  block ending at:
370979  FUN_180433f50(local_508,"LSU0.STI.16.F32.F16",&local_25d8);
        preceding record list (lines 370964–370978):
          {D,0x40,9} {IR,1,5} {IR,0,5} {D,0x442,13}
```

i.e. **`LSU0.STI.16.F32.F16` = 32 bits = [D=0x442:13][IR,1:5][IR,0:5][D=0x40:9]**;
packed *first record at the MSB* of the width-`b` word (see
`shave-isa/ENCODING_NOTES.md` §3), this is the form whose bytes are
`42741008`. Compare the sibling at

```
392647  FUN_180433f50(local_dc8,"LSU1.ST.16.F32.F16",&local_25d8);
        record list (392632–392646):
          {D,0x449,13} {IR,0,5} {IR,1,5} {D,0,9}
```

Same 32-bit shape, constants `0x449` (unit/opcode) and trailing literal 0.
The **only structural difference** between `ST` and `STI` in the vendor
table is the trailing literal width/value:

```
STI.16.F32.F16 : trailing {D, 0x40, 9}      (bits[8:0] = 0b001000000)
ST .16.F32.F16 : trailing {D, 0x00, 9}
```

and a 1-bit extra constant `D,1,1` in the wide (`@64_128`) variants. Hence
`STI` vs `ST` is literally **bit 3 of the last byte** (0x08), and there is
no separate "increment amount" operand — the increment is *implied by the
mnemonic* (`I` = post-increment by element size).

Cross-check with the 28-bit variants
(`238212 FUN_180433f50(local_920,"LSU0.STI.16.F32.F16",...)`, record list
`{D,1090,11}{D,0,1}{IR,0,5}{IR,1,5}{D,0,5}{D,1,1}`): the trailing
`{D,1,1}` is the same "increment" flag.

---

## 5. LDO opcode-class formation `[STATIC-RE]`

`re_decompiled/movicomp/decompiled_all.c:240030–240056` — the compiler's
own printer cases for LDO:

```
240052  local_38 = ".LDO.32 ";    (case 0x55)
240054  local_38 = ".LDO.64 ";    (case 0x56)
```

`re_strings/moviCompile64.ascii.txt:87833` = ` .LDO.32 `.

`re_strings/moviAsm64.ascii.txt` — `LSU0.LDO.32` appears once as the base
mnemonic; the *class* byte difference (0xc2 vs 0xf9 vs 0x01) is produced by
the `--cv` platform selector:

```
re_strings/moviAsm64.ascii.txt:2438  3600xx
:2439  3600xx-nn
:2463  3720xx-nn
:2465  3720xx
:7419  .?AUMA2x8x@@
:7423  .?AUCV3600xxnn@@
:7535  .?AVCV3600xxPlatform@@
:7539  .?AVMA2x8xPlatform@@
:7542  .?AVMyriad2Platform@@
```
and the selector code at
`re_decompiled/moviAsm64/decompiled_all.c:25625–25643`:

```
25625  memcmp(p,"3720xx-nn",9)
25643  memcmp(p,"3720xx",6)
```

Each platform registers its own builder family
(`shave-isa/ENCODING_NOTES.md` §2 table): `Myriad2Platform` →
`FUN_180434100` (1,495 inline defs). `FUN_180434100` **failed to
decompile** (`decompiled_all.c:170229 /* DECOMPILE_FAILED */`), which is
exactly why our recovered table lacks the Myriad2-specific opcode
constants and why `shave_asm.py` had to calibrate `LSU1.LDIH` from corpus
bytes (`ENCODING_NOTES.md` §7.1).

---

## 6. Gold kernel bytes (ground truth to match)

`npunlock/tests/fixtures/npu3720/add1-fp16.elf`, `.text` @ file off 0xc0,
192 bytes. Row 0x00a0–0x00bf:

```
00a0  ff 41 a0 00 b5 f4 ff 0e eb 07 3c e0 9c 30 10 49
00b0  74 10 08 c6 20 25 81 47 f1 01 50 00 00 00 00 00
```

Decoded (shave_isa disasm + our probes):

```
0xaa  3ce09c30  SAU.ADD.F32 i7, i7, i1      (acc = in + 1.0f)
0xae  10                                       (X/don't-care byte)
0xaf  49741008  LSU1.STI.16.F32.F16 i7, i8  (store, auto-inc)
0xb3  c6202581  IAU.ADD i9, i9, 2           (in-ptr += 2)
0xb7  47f101    BRU.JMP i30                 (return)
```

Loop backedge and data-pointer setup:

```
0x98  42701200  LSU0.LD.32.F16.F32 i7, i9  (load fp16, widen to f32)
0x9d  ...
0x9d  c643fdff  IAU.INCS i10, 0x3fff       (counter decrement)
0xa1  41a000    CMU.CMZ.I32 i5             (flag = (i5 == 0))
0xa4  b5f4ff0e  BRU.BRA i14, 0x98          (loop back, predicated)
```

`dims` is read with the **`i18`-relative LDO** at 0x0005
(`c272642100` = `LSU0.LDO.32 i7, i18, 0x10`) — i.e. the compiler derefs
`params + 0x10` to load the *pointer*, then `load32(dims + d*4)` afterwards
(see the LDO/LD pairs at 0x0018+, and the `SAU.DIV.U32`/`CMU.CMII` guard at
0x005d/0x0062).

Store at 0xaf evidence lines:
* `re_strings/moviCompile64.ascii.txt:87938` (`.STI.16.f32.f16`)
* `re_decompiled/movicomp/decompiled_all.c:240351` (printer case 0xa0)
* `re_decompiled/moviAsm64/decompiled_all.c:370979` (encoder def)
* `re_strings/moviAsm64.ascii.txt:4421` (mnemonic)
