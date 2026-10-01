# Corpus diagnostics (verbatim toolchain messages)

## kernels

### a01_add1_fp16 — compile=OK asm=OK link=OK validate=OK (text=384)

### a02_add1_fp16_header — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### a03_gelu_fp16 — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### a04_gelu_fp32 — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### a05_scale_f32 — compile=OK asm=OK link=OK validate=OK (text=400)

### a06_mix_f16 — compile=OK asm=OK link=OK validate=OK (text=432)

### a07_weighted_mix_f16 — compile=OK asm=OK link=OK validate=OK (text=432)

### b01_int_arith — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t39: i32 = mulhu t24, Constant:i32<-1431655765>`

### b02_div_mod — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t87: i32,i32 = sdivrem t57, t58`

### b03_casts_fp — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t79: f32 = uint_to_fp t78`

### b04_mem_widths — compile=OK asm=OK link=OK validate=OK (text=352)

### b05_loops — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### b06_cond — compile=OK asm=OK link=OK validate=OK (text=496)

### b07_switch — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t4: ch = br_jt t2:1, JumpTable:i32<0>, t2`

### b08_calls_internal — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee GlobalAddress); see TARGET_NOTES.md`

### b09_libm — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### b10_structs — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t59: i32 = fp_to_uint t58`

### b11_int64 — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### b12_bitfields — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t83: i32 = fp_to_uint t89`

### b13_varargs — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee GlobalAddress); see TARGET_NOTES.md`

### b14_volatile — compile=OK asm=OK link=OK validate=OK (text=256)

### b15_recursion — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee GlobalAddress); see TARGET_NOTES.md`

### b16_fcmp — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `error: SHAVE: unsupported fcmp in wave 1: t24: i32 = <<Unknown Node #370>> t8, t27, setule:ch`
- `fatal error: error in backend: SHAVE wave 1 does not support floating-point compares; see TARGET_NOTES.md`

### b17_alloca — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `error: SHAVE: unsupported IR node in wave 1: t51: i32 = FrameIndex<0>`
- `fatal error: error in backend: SHAVE backend wave 1: unsupported IR (see message above and TARGET_NOTES.md)`

### b18_globals — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t39: i32 = GlobalAddress<i32* @counter> 0`

### b19_fp_arith — compile=OK asm=OK link=OK validate=OK (text=80)

### b20_funcptr — compile=OK asm=OK link=OK validate=OK (text=176)

### b21_memset_like — compile=OK asm=OK link=OK validate=OK (text=688)

### b22_double — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### b23_i64_float — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### b24_signed_index — compile=OK asm=OK link=OK validate=OK (text=240)

### b25_large_const — compile=OK asm=OK link=OK validate=OK (text=144)

### b26_goto — compile=OK asm=OK link=OK validate=OK (text=416)

## repros

### r01_udiv_by_const — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t56: i32 = mulhu t49, Constant:i32<-1431655765>`

### r02_udiv_by_value — compile=OK asm=OK link=OK validate=OK (text=80)

### r03_umod_by_const — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t57: i32 = mulhu t49, Constant:i32<-858993459>`

### r04_sdiv_i32 — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t68: i32,i32 = sdivrem t49, t53`

### r05_srem_i32 — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t68: i32,i32 = sdivrem t49, t53`

### r06_uint_to_float — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: f32 = uint_to_fp t49`

### r07_float_to_uint — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: i32 = fp_to_uint t49`

### r08_float_to_int — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: i32 = fp_to_sint t49`

### r09_int_to_float — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: f32 = sint_to_fp t49`

### r10_jumptable_switch — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t1: i32 = GlobalAddress<[7 x i32]* @switch.table.controlled_act> 0`

### r11_switch_3cases — compile=OK asm=OK link=OK validate=OK (text=144)

### r12_global_var — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: i32 = GlobalAddress<i32* @g_counter> 0`

### r13_rodata_table — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t52: i32 = GlobalAddress<[8 x i32]* @table> 0`

### r14_alloca_array — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `error: SHAVE: unsupported IR node in wave 1: t2: i32 = FrameIndex<0>`
- `fatal error: error in backend: SHAVE backend wave 1: unsupported IR (see message above and TARGET_NOTES.md)`

### r15_fcmp — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `error: SHAVE: unsupported fcmp in wave 1: t83: i32 = setcc t49, t88, setogt:ch`
- `fatal error: error in backend: SHAVE wave 1 does not support floating-point compares; see TARGET_NOTES.md`

### r16_call_internal — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee GlobalAddress); see TARGET_NOTES.md`

### r17_call_libm_tanhf — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### r18_i64_div — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r19_i64_mul — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t116: i32 = mulhu t66, t106`

### r20_double_add — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r21_f64_compare — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r22_variable_shift — compile=OK asm=OK link=OK validate=OK (text=96)

### r23_signed_minmax — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 18 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### r24_bitfield — compile=OK asm=OK link=OK validate=OK (text=80)

### r25_f32_div — compile=OK asm=OK link=OK validate=OK (text=80)

### r26_f32_minmax — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: only i32 value-selects are supported in wave 1; see TARGET_NOTES.md`

### r27_volatile_mmio — compile=OK asm=OK link=OK validate=OK (text=48)

### r28_struct_return — compile=OK asm=OK link=OK validate=OK (text=64)

### r29_half_arith — compile=OK asm=OK link=OK validate=OK (text=80)

### r30_struct_copy — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `error: SHAVE: unsupported IR node in wave 1: t39: i32 = FrameIndex<0>`
- `fatal error: error in backend: SHAVE backend wave 1: unsupported IR (see message above and TARGET_NOTES.md)`

### r33_i64_sdiv — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r34_i64_to_float — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r35_funcptr_call — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee select); see TARGET_NOTES.md`

### r36_global_init_data — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t54: i32 = GlobalAddress<[4 x i32]* @g_table> 0`

### r37_regpressure — compile=OK asm=OK link=OK validate=OK (text=336)

### r38_half_roundtrip — compile=OK asm=OK link=OK validate=OK (text=112)

### r39_i32_i64_ext — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### r40_memcpy_builtin — compile=OK asm=OK link=OK validate=OK (text=144)

### r41_f64_param — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r42_undefined_entry — compile=OK asm=OK link=FAIL validate=SKIP (text=-)
- `legalize: 6 delay-slot NOP(s)`
- `ld.lld: error: output file too large: 18446744073692774968 bytes`

### r43_empty_tu — compile=OK asm=OK link=FAIL validate=SKIP (text=-)
- `ld.lld: error: output file too large: 18446744073692774968 bytes`

### r44_builtins — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t83: i32 = ctlz t50`

### r45_f32_ops — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t100: f32 = fabs t99`

### r46_half_ops — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t69: f32 = sint_to_fp t68`

### r47_volatile_loop — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `error: SHAVE: unsupported IR node in wave 1: t39: i32 = FrameIndex<0>`
- `fatal error: error in backend: SHAVE backend wave 1: unsupported IR (see message above and TARGET_NOTES.md)`

### r48_memset_builtin — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r49_ctlz — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t65: i32 = ctlz t50`

### r50_cttz — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t65: i32 = cttz t50`

### r51_popcount — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: i32 = ctpop t49`

### r52_bswap — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: i32 = bswap t49`

### r53_fabs — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t50: f32 = fabs t49`

### r54_string_literal — compile=OK asm=OK link=OK validate=OK (text=32)

### r55_float_arg_call — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t58: i32 = fp_to_uint t57`

### r56_bool_enum — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: Cannot select: t77: i32 = mulhu t49, Constant:i32<-1431655765>`

### r57_long_double — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee ExternalSymbol); see TARGET_NOTES.md`

### r58_i64_shift_var — compile=OK asm=OK link=OK validate=OK (text=160)

### r59_deep_chain — compile=OK asm=OK link=OK validate=OK (text=464)

### r60_big_function — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `UNREACHABLE executed at /home/asdf/projects/llvm-14/llvm/lib/Target/SHAVE/SHAVEInstrInfo.cpp:41!`

### r61_regspill_crash — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `UNREACHABLE executed at /home/asdf/projects/llvm-14/llvm/lib/Target/SHAVE/SHAVEInstrInfo.cpp:41!`

### r62_signed_relational — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: unsupported condition code 21 in wave 1 (floating-point and signed compares are unsupported); see TARGET_NOTES.md`

### r63_f32_constpool — compile=OK asm=OK link=OK validate=OK (text=80)

### r64_fcmp_only — compile=FAIL asm=- link=- validate=SKIP (text=-)
- `fatal error: error in backend: SHAVE: only i32 value-selects are supported in wave 1; see TARGET_NOTES.md`

