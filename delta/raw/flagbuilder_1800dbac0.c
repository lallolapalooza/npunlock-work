/* ===== Function 2260: FUN_1800dbac0 @ 1800dbac0 size=3891 conv=unknown ===== */

ulonglong * FUN_1800dbac0(ulonglong *param_1,ulonglong param_2)

{
  byte bVar1;
  char cVar2;
  ulonglong *puVar3;
  ulonglong uVar4;
  code *UNRECOVERED_JUMPTABLE;
  ulonglong uVar5;
  ulonglong uVar6;
  int iVar7;
  uint uVar8;
  longlong lVar9;
  bool bVar10;
  
  param_1[1] = 0;
  uVar6 = param_2 & 0xffffffff;
  bVar1 = FUN_1800d0f00(param_2 & 0xffffffff);
  uVar5 = (ulonglong)bVar1;
  lVar9 = 0x180000000;
  uVar8 = (uint)uVar6;
  *param_1 = uVar5;
  switch(uVar8) {
  case 0x171:
  case 0x172:
  case 0x173:
  case 0x174:
  case 0x175:
  case 0x176:
  case 0x177:
  case 0x178:
  case 0x1de:
  case 0x1df:
  case 0x1e0:
  case 0x1e1:
  case 0x1e2:
  case 0x1e3:
  case 0x1e4:
  case 0x1e5:
  case 0x1e6:
  case 0x1e7:
  case 0x1e8:
  case 0x1e9:
  case 0x1ea:
  case 0x1eb:
  case 0x1ec:
  case 0x1ed:
    bVar10 = true;
    break;
  default:
    bVar10 = false;
  }
  uVar4 = uVar5 | 2;
  if (!bVar10) {
    uVar4 = uVar5 & 0xfffffffffffffffd;
  }
  *param_1 = uVar4;
  if (uVar8 < 0xda7) {
    if (uVar8 != 0xda6) {
      switch(uVar8) {
      case 0xae7:
      case 0xae9:
      case 0xaeb:
      case 0xaed:
      case 0xaef:
      case 0xaf1:
      case 0xaf3:
      case 0xaf5:
      case 0xaf7:
      case 0xaf9:
      case 0xafb:
      case 0xafd:
      case 0xaff:
      case 0xb01:
      case 0xb41:
      case 0xb43:
      case 0xb45:
      case 0xb47:
      case 0xb49:
      case 0xb4b:
      case 0xb4d:
      case 0xb4f:
      case 0xb51:
      case 0xb53:
      case 0xb55:
      case 0xb57:
      case 0xb59:
      case 0xb5b:
        break;
      default:
        goto switchD_1800dbb51_caseD_ae8;
      }
    }
switchD_1800dbb51_caseD_ae7:
    bVar10 = true;
  }
  else {
    switch(uVar8) {
    case 0xda8:
    case 0xdaa:
    case 0xdac:
    case 0xdae:
    case 0xdb0:
    case 0xdb2:
    case 0xdb4:
    case 0xdb6:
    case 0xdb8:
    case 0xdba:
    case 0xdbc:
    case 0xdbe:
    case 0xdbf:
    case 0xdc3:
    case 0xdc4:
    case 0xdc8:
    case 0xdca:
    case 0xdcc:
    case 0xdce:
    case 0xdd0:
    case 0xdd2:
    case 0xe30:
    case 0xe32:
    case 0xe34:
    case 0xe36:
    case 0xe38:
    case 0xe3a:
    case 0xe3c:
    case 0xe3e:
    case 0xe40:
    case 0xe42:
    case 0xe44:
    case 0xe46:
    case 0xe48:
    case 0xe49:
    case 0xe4d:
    case 0xe4e:
    case 0xe52:
    case 0xe54:
    case 0xe56:
    case 0xe58:
    case 0xe5a:
    case 0xe5c:
      goto switchD_1800dbb51_caseD_ae7;
    default:
switchD_1800dbb51_caseD_ae8:
      bVar10 = false;
    }
  }
  uVar5 = *param_1 | 4;
  if (!bVar10) {
    uVar5 = *param_1 & 0xfffffffffffffffb;
  }
  *param_1 = uVar5;
  if (uVar8 < 0xda8) {
    if (uVar8 != 0xda7) {
      switch(uVar8) {
      case 0xae5:
      case 0xae6:
      case 0xae8:
      case 0xaea:
      case 0xaec:
      case 0xaee:
      case 0xaf0:
      case 0xaf2:
      case 0xaf4:
      case 0xaf6:
      case 0xaf8:
      case 0xafa:
      case 0xafc:
      case 0xafe:
      case 0xb00:
      case 0xb02:
      case 0xb3f:
      case 0xb40:
      case 0xb42:
      case 0xb44:
      case 0xb46:
      case 0xb48:
      case 0xb4a:
      case 0xb4c:
      case 0xb4e:
      case 0xb50:
      case 0xb52:
      case 0xb54:
      case 0xb56:
      case 0xb58:
      case 0xb5a:
      case 0xb5c:
        break;
      default:
        goto switchD_1800dbbbf_caseD_ae7;
      }
    }
switchD_1800dbbbf_caseD_ae5:
    bVar10 = true;
  }
  else {
    switch(uVar8) {
    case 0xda9:
    case 0xdab:
    case 0xdad:
    case 0xdaf:
    case 0xdb1:
    case 0xdb3:
    case 0xdb5:
    case 0xdb7:
    case 0xdb9:
    case 0xdbb:
    case 0xdbd:
    case 0xdc0:
    case 0xdc1:
    case 0xdc2:
    case 0xdc5:
    case 0xdc6:
    case 0xdc7:
    case 0xdc9:
    case 0xdcb:
    case 0xdcd:
    case 0xdcf:
    case 0xdd1:
    case 0xdd3:
    case 0xe31:
    case 0xe33:
    case 0xe35:
    case 0xe37:
    case 0xe39:
    case 0xe3b:
    case 0xe3d:
    case 0xe3f:
    case 0xe41:
    case 0xe43:
    case 0xe45:
    case 0xe47:
    case 0xe4a:
    case 0xe4b:
    case 0xe4c:
    case 0xe4f:
    case 0xe50:
    case 0xe51:
    case 0xe53:
    case 0xe55:
    case 0xe57:
    case 0xe59:
    case 0xe5b:
    case 0xe5d:
      goto switchD_1800dbbbf_caseD_ae5;
    default:
switchD_1800dbbbf_caseD_ae7:
      bVar10 = false;
    }
  }
  uVar5 = *param_1 | 8;
  if (!bVar10) {
    uVar5 = *param_1 & 0xfffffffffffffff7;
  }
  cVar2 = FUN_1800d1310(uVar6 & 0xffffffff);
  uVar4 = uVar5 | 0x10;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xffffffffffffffef;
  }
  cVar2 = FUN_1800d1750(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x20;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xffffffffffffffdf;
  }
  cVar2 = FUN_1800d1d80(uVar6 & 0xffffffff);
  iVar7 = (int)uVar6;
  uVar4 = uVar5 | 0x40;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xffffffffffffffbf;
  }
  if (((iVar7 == 0x250) || (iVar7 == 0x251)) || (iVar7 == 0x252)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar5 = uVar4 | 0x80;
  if (!bVar10) {
    uVar5 = uVar4 & 0xffffffffffffff7f;
  }
  *param_1 = uVar5;
  if (iVar7 - 0x338U < 0x2af) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xdcd28 +
                           (ulonglong)*(byte *)(lVar9 + 0xdcd30 + (ulonglong)(iVar7 - 0x338U)) * 4)
                 + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbc9c. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  *param_1 = uVar5 & 0xfffffffffffffeff;
  if (iVar7 - 0x3afU < 0x68) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xdcfe0 +
                           (ulonglong)*(byte *)(lVar9 + 0xdcfe8 + (ulonglong)(iVar7 - 0x3afU)) * 4)
                 + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbcdd. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  *param_1 = *param_1 & 0xfffffffffffffdff;
  if (iVar7 - 0x2e7U < 0x51) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xdd050 +
                           (ulonglong)*(byte *)(lVar9 + 0xdd058 + (ulonglong)(iVar7 - 0x2e7U)) * 4)
                 + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbd1e. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  *param_1 = *param_1 & 0xfffffffffffffbff;
  if (iVar7 - 0x5e7U < 0x44) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xdd0ac +
                           (ulonglong)*(byte *)(lVar9 + 0xdd0b0 + (ulonglong)(iVar7 - 0x5e7U)) * 4)
                 + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbd5f. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  uVar4 = *param_1 & 0xfffffffffffff7ff;
  cVar2 = FUN_1800d21f0(uVar6 & 0xffffffff);
  uVar8 = (uint)uVar6;
  uVar5 = uVar4 | 0x1000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xffffffffffffefff;
  }
  *param_1 = uVar5;
  if (uVar8 - 0x62b < 0xbc) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xdd0f4 +
                           (ulonglong)*(byte *)(lVar9 + 0xdd0f8 + (ulonglong)(uVar8 - 0x62b)) * 4) +
                 lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbdbd. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  if ((uVar8 == 0x728) || (uVar8 == 0x72a)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar4 = uVar5 & 0xffffffffffffdfff | 0x4000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xffffffffffff9fff;
  }
  *param_1 = uVar4;
  if (uVar8 < 0xf31) {
    if (uVar8 != 0xf30) {
      if (uVar8 - 0xbd5 < 0x10) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd1b4 +
                               (ulonglong)*(byte *)(lVar9 + 0xdd1b8 + (ulonglong)(uVar8 - 0xbd5)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbe31. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
      goto LAB_1800dbe59;
    }
    bVar10 = true;
  }
  else {
    if (uVar8 - 0xf31 < 0x1f) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd1c8 +
                             (ulonglong)*(byte *)(lVar9 + 0xdd1cc + (ulonglong)(uVar8 - 0xf31)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dbe53. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dbe59:
    bVar10 = false;
  }
  uVar5 = *param_1 | 0x8000;
  if (!bVar10) {
    uVar5 = *param_1 & 0xffffffffffff7fff;
  }
  cVar2 = FUN_1800d2270(uVar6 & 0xffffffff);
  iVar7 = (int)uVar6;
  uVar4 = uVar5 | 0x10000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xfffffffffffeffff;
  }
  if (((iVar7 == 0x7d9) || (iVar7 == 0x7ef)) || ((iVar7 == 0x803 || (iVar7 == 0x807)))) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar5 = uVar4 | 0x20000;
  if (!bVar10) {
    uVar5 = uVar4 & 0xfffffffffffdffff;
  }
  if ((iVar7 == 0x861) || (iVar7 == 0x8bc)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar4 = uVar5 | 0x100000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xffffffffffefffff;
  }
  if ((iVar7 == 0x862) || (iVar7 == 0x8bd)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar5 = uVar4 | 0x200000;
  if (!bVar10) {
    uVar5 = uVar4 & 0xffffffffffdfffff;
  }
  if (((iVar7 == 0x863) || (iVar7 == 0x8c2)) || (iVar7 == 0x8fa)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar4 = uVar5 | 0x400000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xffffffffffbfffff;
  }
  if (((iVar7 == 0x864) || (iVar7 == 0x8c7)) || (iVar7 == 0x8fb)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar5 = uVar4 | 0x800000;
  if (!bVar10) {
    uVar5 = uVar4 & 0xffffffffff7fffff;
  }
  if (((iVar7 == 0x8e0) || (iVar7 == 0x8f9)) ||
     ((iVar7 == 0xa4e || ((iVar7 == 0xa63 || (iVar7 == 0xa64)))))) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar4 = uVar5 | 0x1000000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xfffffffffeffffff;
  }
  cVar2 = FUN_1800d22d0(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x2000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xfffffffffdffffff;
  }
  cVar2 = FUN_1800b4dc0(uVar6 & 0xffffffff);
  uVar4 = uVar5 | 0x4000000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xfffffffffbffffff;
  }
  cVar2 = FUN_1800d24b0(uVar6 & 0xffffffff);
  iVar7 = (int)uVar6;
  uVar5 = uVar4 | 0x8000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xfffffffff7ffffff;
  }
  if (((iVar7 == 0x829) || (iVar7 == 0x82a)) || (iVar7 == 0x82b)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar4 = uVar5 | 0x10000000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xffffffffefffffff;
  }
  if (((iVar7 == 0x82c) || (iVar7 == 0x82d)) || (iVar7 == 0x82e)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar5 = uVar4 | 0x20000000;
  if (!bVar10) {
    uVar5 = uVar4 & 0xffffffffdfffffff;
  }
  cVar2 = FUN_1800d2520(uVar6 & 0xffffffff);
  uVar8 = (uint)uVar6;
  uVar4 = uVar5 | 0x40000000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xffffffffbfffffff;
  }
  *param_1 = uVar4;
  if (uVar8 - 0x181 < 0x8d) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xdd1ec +
                           (ulonglong)*(byte *)(lVar9 + 0xdd1f4 + (ulonglong)(uVar8 - 0x181)) * 4) +
                 lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc08c. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  *param_1 = uVar4 & 0xffffffff7fffffff;
  if (uVar8 < 0xfc5) {
    if (uVar8 != 0xfc4) {
      if (uVar8 - 0xc14 < 0x75) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd284 +
                               (ulonglong)*(byte *)(lVar9 + 0xdd28c + (ulonglong)(uVar8 - 0xc14)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc0e4. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
      goto LAB_1800dc10e;
    }
    bVar10 = true;
  }
  else {
    if (uVar8 - 0xfc6 < 0xb5) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd304 +
                             (ulonglong)*(byte *)(lVar9 + 0xdd30c + (ulonglong)(uVar8 - 0xfc6)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc108. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dc10e:
    bVar10 = false;
  }
  uVar5 = *param_1 | 0x100000000;
  if (!bVar10) {
    uVar5 = *param_1 & 0xfffffffeffffffff;
  }
  *param_1 = uVar5;
  if (uVar8 < 0xfc6) {
    if (uVar8 != 0xfc5) {
      if (uVar8 - 0xc12 < 0x78) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd3c4 +
                               (ulonglong)*(byte *)(lVar9 + 0xdd3cc + (ulonglong)(uVar8 - 0xc12)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc164. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
      goto LAB_1800dc18e;
    }
    bVar10 = true;
  }
  else {
    if (uVar8 - 0xfc7 < 0xb5) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd444 +
                             (ulonglong)*(byte *)(lVar9 + 0xdd44c + (ulonglong)(uVar8 - 0xfc7)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc188. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dc18e:
    bVar10 = false;
  }
  uVar5 = *param_1 | 0x200000000;
  if (!bVar10) {
    uVar5 = *param_1 & 0xfffffffdffffffff;
  }
  cVar2 = FUN_1800d2a20(uVar6 & 0xffffffff);
  uVar4 = uVar5 | 0x800000000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xfffffff7ffffffff;
  }
  cVar2 = FUN_1800d2d80(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x1000000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xffffffefffffffff;
  }
  uVar8 = (uint)uVar6;
  if (uVar8 == 0x7ae) {
    *param_1 = uVar5 | 0x8000000000;
  }
  else {
    *param_1 = uVar5 & 0xffffff7fffffffff;
    if (uVar8 - 0xf3 < 0x140) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd504 +
                             (ulonglong)*(byte *)(lVar9 + 0xdd508 + (ulonglong)(uVar8 - 0xf3)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc2d7. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
  }
  if (uVar8 == 0xbcd) {
    uVar5 = *param_1 & 0xfffffeffffffffff | 0x20000000000;
LAB_1800dc26b:
    uVar5 = uVar5 & 0xfffffbffffffffff;
    if (uVar8 == 0xcc7) {
      *param_1 = uVar5 | 0x80000000000;
      goto LAB_1800dc330;
    }
  }
  else {
    uVar5 = *param_1 & 0xfffffcffffffffff;
    if (uVar8 != 0xbd0) goto LAB_1800dc26b;
    uVar5 = uVar5 | 0x40000000000;
  }
  *param_1 = uVar5 & 0xfffff7ffffffffff;
  if (uVar8 < 0x10d9) {
    if (uVar8 != 0x10d8) {
      if (uVar8 - 0xbfc < 0xe2) {
LAB_1800dc330:
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd648 +
                               (ulonglong)*(byte *)(lVar9 + 0xdd650 + (ulonglong)(uVar8 - 0xbfc)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc344. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
      goto LAB_1800dc36c;
    }
    bVar10 = true;
  }
  else {
    if (uVar8 - 0x10d9 < 0x41) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd734 +
                             (ulonglong)*(byte *)(lVar9 + 0xdd738 + (ulonglong)(uVar8 - 0x10d9)) * 4
                             ) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc366. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dc36c:
    bVar10 = false;
  }
  uVar5 = *param_1 | 0x100000000000;
  if (!bVar10) {
    uVar5 = *param_1 & 0xffffefffffffffff;
  }
  *param_1 = uVar5;
  if (uVar8 < 0xd3c) {
    if (uVar8 != 0xd3b) {
      if (uVar8 - 0xb99 < 0x75) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd77c +
                               (ulonglong)*(byte *)(lVar9 + 0xdd784 + (ulonglong)(uVar8 - 0xb99)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc3ca. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
      goto LAB_1800dc483;
    }
LAB_1800dc47f:
    bVar10 = true;
  }
  else {
    if (uVar8 < 0xebb) {
      if (uVar8 == 0xeba) goto LAB_1800dc47f;
      if (uVar8 - 0xd3c < 0x12) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd7fc +
                               (ulonglong)*(byte *)(lVar9 + 0xdd800 + (ulonglong)(uVar8 - 0xd3c)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc3ff. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
    }
    else if (uVar8 < 0xfab) {
      if (uVar8 == 0xfaa) goto LAB_1800dc47f;
      if (uVar8 - 0xebb < 0xb3) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd814 +
                               (ulonglong)*(byte *)(lVar9 + 0xdd81c + (ulonglong)(uVar8 - 0xebb)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc42e. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
    }
    else if (uVar8 < 0x121d) {
      if (uVar8 == 0x121c) goto LAB_1800dc47f;
      if (uVar8 - 0xfab < 0x17) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdd8d0 +
                               (ulonglong)*(byte *)(lVar9 + 0xdd8d4 + (ulonglong)(uVar8 - 0xfab)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc45b. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
    }
    else if (uVar8 - 0x121d < 0x5b) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd8ec +
                             (ulonglong)*(byte *)(lVar9 + 0xdd8f4 + (ulonglong)(uVar8 - 0x121d)) * 4
                             ) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc47d. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dc483:
    bVar10 = false;
  }
  uVar5 = *param_1 | 0x200000000000;
  if (!bVar10) {
    uVar5 = *param_1 & 0xffffdfffffffffff;
  }
  if (uVar8 == 0x9ff) {
    uVar5 = uVar5 | 0x400000000000;
    bVar10 = false;
  }
  else {
    uVar5 = uVar5 & 0xffffbfffffffffff;
    if ((uVar8 == 0xa00) || (uVar8 == 0xa25)) {
      bVar10 = true;
    }
    else {
      bVar10 = false;
    }
  }
  uVar4 = uVar5 | 0x800000000000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xffff7fffffffffff;
  }
  if (((uVar8 == 0xa01) || (uVar8 == 0xa38)) || (uVar8 == 0xa4f)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar5 = uVar4 | 0x1000000000000;
  if (!bVar10) {
    uVar5 = uVar4 & 0xfffeffffffffffff;
  }
  if (((uVar8 == 0xa02) || (uVar8 == 0xa39)) || (uVar8 == 0xa50)) {
    bVar10 = true;
  }
  else {
    bVar10 = false;
  }
  uVar4 = uVar5 | 0x2000000000000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xfffdffffffffffff;
  }
  cVar2 = FUN_1800d33b0(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x4000000000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xfffbffffffffffff;
  }
  *param_1 = uVar5;
  uVar8 = (uint)uVar6;
  if (uVar8 < 0x9d7) {
    if (uVar8 != 0x9d6) {
      if (uVar8 < 0x1c3) {
        if ((uVar8 != 0x1c2) && (uVar8 != 0x167)) {
          bVar10 = uVar8 == 0x168;
LAB_1800dc5e9:
          if (!bVar10) goto LAB_1800dc5ef;
        }
      }
      else if (uVar8 < 0x73f) {
        if (uVar8 != 0x73e) {
          if (uVar8 - 0x1c3 < 6) {
            UNRECOVERED_JUMPTABLE =
                 (code *)((ulonglong)*(uint *)(lVar9 + 0xdda24 + (ulonglong)(uVar8 - 0x1c3) * 4) +
                         lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc7c6. Too many branches */
                    /* WARNING: Treating indirect jump as call */
            puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
            return puVar3;
          }
          goto LAB_1800dc5ef;
        }
      }
      else if (uVar8 != 0x73f) {
        bVar10 = uVar8 == 0x7ae;
        goto LAB_1800dc5e9;
      }
    }
LAB_1800dc833:
    bVar10 = true;
  }
  else {
    if (uVar8 < 0xd7d) {
      if (uVar8 == 0xd7c) goto LAB_1800dc833;
      if (uVar8 - 0x9d7 < 0xc3) {
        UNRECOVERED_JUMPTABLE =
             (code *)((ulonglong)
                      *(uint *)(lVar9 + 0xdda3c +
                               (ulonglong)*(byte *)(lVar9 + 0xdda40 + (ulonglong)(uVar8 - 0x9d7)) *
                               4) + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc80b. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
        return puVar3;
      }
    }
    else if (uVar8 - 0xd7d < 0x13) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xddb04 +
                             (ulonglong)*(byte *)(lVar9 + 0xddb08 + (ulonglong)(uVar8 - 0xd7d)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc831. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dc5ef:
    bVar10 = false;
  }
  uVar4 = uVar5 | 0x8000000000000;
  if (!bVar10) {
    uVar4 = uVar5 & 0xfff7ffffffffffff;
  }
  if (uVar8 == 0xf2d) {
    *param_1 = uVar4 | 0x10000000000000;
  }
  else {
    *param_1 = uVar4 & 0xffefffffffffffff;
    if (uVar8 - 0x23f < 0x16) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xddb1c +
                             (ulonglong)*(byte *)(lVar9 + 0xddb20 + (ulonglong)(uVar8 - 0x23f)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc86e. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
  }
  uVar4 = *param_1 & 0xffbfffffffffffff;
  cVar2 = FUN_1800ac770(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x80000000000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xff7fffffffffffff;
  }
  cVar2 = FUN_1800d34f0(uVar6 & 0xffffffff);
  uVar4 = uVar5 | 0x100000000000000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xfeffffffffffffff;
  }
  cVar2 = FUN_1800d3590(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x200000000000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xfdffffffffffffff;
  }
  cVar2 = FUN_1800ad490(uVar6 & 0xffffffff);
  uVar4 = uVar5 | 0x400000000000000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xfbffffffffffffff;
  }
  cVar2 = FUN_1800d3650(uVar6 & 0xffffffff);
  uVar5 = uVar4 | 0x800000000000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0xf7ffffffffffffff;
  }
  cVar2 = FUN_1800b5670(uVar6 & 0xffffffff);
  uVar4 = uVar5 | 0x1000000000000000;
  if (cVar2 == '\0') {
    uVar4 = uVar5 & 0xefffffffffffffff;
  }
  *param_1 = uVar4;
  uVar8 = (uint)uVar6;
  if (uVar8 < 0xd3d) {
    if (uVar8 == 0xd3c) goto LAB_1800dc899;
    if (uVar8 - 0xae5 < 0xc9) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdd950 +
                             (ulonglong)*(byte *)(lVar9 + 0xdd958 + (ulonglong)(uVar8 - 0xae5)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc79a. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
LAB_1800dc895:
    bVar10 = false;
  }
  else {
    if ((((uVar8 != 0xd3d) && (uVar8 != 0xd3e)) && (uVar8 != 0xd3f)) &&
       ((uVar8 != 0xd46 && (uVar8 != 0xd47)))) goto LAB_1800dc895;
LAB_1800dc899:
    bVar10 = true;
  }
  uVar5 = uVar4 | 0x2000000000000000;
  if (!bVar10) {
    uVar5 = uVar4 & 0xdfffffffffffffff;
  }
  *param_1 = uVar5;
  if (uVar8 - 0xae7 < 0x267) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xddb38 +
                           (ulonglong)*(byte *)(lVar9 + 0xddb40 + (ulonglong)(uVar8 - 0xae7)) * 4) +
                 lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc8e6. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  uVar4 = *param_1 & 0xbfffffffffffffff;
  cVar2 = FUN_1800ae100(uVar6 & 0xffffffff);
  uVar8 = (uint)uVar6;
  uVar5 = uVar4 | 0x8000000000000000;
  if (cVar2 == '\0') {
    uVar5 = uVar4 & 0x7fffffffffffffff;
  }
  *param_1 = uVar5;
  uVar5 = param_1[1];
  cVar2 = FUN_1800d3720(uVar6 & 0xffffffff);
  uVar6 = uVar5 | 1;
  if (cVar2 == '\0') {
    uVar6 = uVar5 & 0xfffffffffffffffe;
  }
  param_1[1] = uVar6;
  if (uVar8 < 0xea5) {
    if (uVar8 == 0xea4) {
LAB_1800dc9e1:
      bVar10 = true;
      goto LAB_1800dc9e7;
    }
    if (uVar8 - 0xdbe < 0xc2) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xddda8 +
                             (ulonglong)*(byte *)(lVar9 + 0xdddb0 + (ulonglong)(uVar8 - 0xdbe)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc990. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
  }
  else if (uVar8 < 0x1220) {
    if (uVar8 == 0x121f) goto LAB_1800dc9e1;
    if (uVar8 - 0xea5 < 0x5f) {
      UNRECOVERED_JUMPTABLE =
           (code *)((ulonglong)
                    *(uint *)(lVar9 + 0xdde74 +
                             (ulonglong)*(byte *)(lVar9 + 0xdde7c + (ulonglong)(uVar8 - 0xea5)) * 4)
                   + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc9bd. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
      return puVar3;
    }
  }
  else if (uVar8 - 0x1221 < 0x3d) {
    UNRECOVERED_JUMPTABLE =
         (code *)((ulonglong)
                  *(uint *)(lVar9 + 0xddedc +
                           (ulonglong)*(byte *)(lVar9 + 0xddee4 + (ulonglong)(uVar8 - 0x1221)) * 4)
                 + lVar9);
                    /* WARNING: Could not recover jumptable at 0x0001800dc9df. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    puVar3 = (ulonglong *)(*UNRECOVERED_JUMPTABLE)(UNRECOVERED_JUMPTABLE);
    return puVar3;
  }
  bVar10 = false;
LAB_1800dc9e7:
  uVar5 = uVar6 | 2;
  if (!bVar10) {
    uVar5 = uVar6 & 0xfffffffffffffffd;
  }
  cVar2 = FUN_1800d3d80(uVar8);
  uVar6 = uVar5 | 4;
  if (cVar2 == '\0') {
    uVar6 = uVar5 & 0xfffffffffffffffb;
  }
  param_1[1] = uVar6;
  return param_1;
}



