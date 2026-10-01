/* ===== Function 2301: FUN_1800e8e80 @ 1800e8e80 size=3010 conv=unknown ===== */

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

undefined8 FUN_1800e8e80(longlong param_1,longlong param_2)

{
  longlong lVar1;
  bool bVar2;
  undefined8 *puVar3;
  longlong *plVar4;
  uint uVar5;
  char cVar6;
  byte bVar7;
  int iVar8;
  uint uVar9;
  undefined8 uVar10;
  longlong lVar11;
  longlong lVar12;
  longlong *plVar13;
  undefined8 *puVar14;
  undefined8 *puVar15;
  ushort uVar16;
  undefined4 uVar17;
  byte bVar19;
  longlong lVar20;
  uint uVar21;
  char *pcVar22;
  undefined8 *puVar23;
  longlong *plVar24;
  ulonglong uVar25;
  undefined4 uVar26;
  ulonglong uVar27;
  undefined1 auStack_128 [32];
  ulonglong local_108;
  longlong *local_100;
  longlong *local_f8;
  uint local_f0;
  longlong *local_e8;
  undefined8 *local_e0;
  undefined8 uStack_d8;
  ulonglong local_c8;
  undefined8 local_c0;
  undefined8 local_b8;
  undefined8 *local_a8;
  undefined4 uStack_a0;
  undefined4 uStack_9c;
  undefined1 local_98 [8];
  undefined1 *local_90 [2];
  undefined1 local_80 [56];
  ulonglong local_48;
  longlong *plVar18;
  
  local_48 = DAT_183c4d618 ^ (ulonglong)auStack_128;
  plVar13 = (longlong *)0x0;
  uVar26 = 0;
  for (puVar15 = (undefined8 *)**(undefined8 **)(param_1 + 8); plVar18 = (longlong *)0x0,
      puVar15 != (undefined8 *)(*(undefined8 **)(param_1 + 8))[1]; puVar15 = puVar15 + 2) {
    if ((undefined *)*puVar15 == &DAT_183c816f0) {
      plVar18 = (longlong *)puVar15[1];
      break;
    }
  }
  uVar10 = (**(code **)(*plVar18 + 0x58))(plVar18,&DAT_183c816f0);
  *(undefined8 *)(param_1 + 0x260) = uVar10;
  for (puVar15 = (undefined8 *)**(undefined8 **)(param_1 + 8); plVar18 = plVar13,
      puVar15 != (undefined8 *)(*(undefined8 **)(param_1 + 8))[1]; puVar15 = puVar15 + 2) {
    if ((undefined *)*puVar15 == &DAT_183c816a8) {
      plVar18 = (longlong *)puVar15[1];
      break;
    }
  }
  uVar10 = (**(code **)(*plVar18 + 0x58))(plVar18,&DAT_183c816a8);
  *(undefined8 *)(param_1 + 0x268) = uVar10;
  puVar15 = (undefined8 *)**(undefined8 **)(param_1 + 8);
  do {
    plVar18 = plVar13;
    if (puVar15 == (undefined8 *)(*(undefined8 **)(param_1 + 8))[1]) {
LAB_1800e8f59:
      lVar11 = (**(code **)(*plVar18 + 0x58))(plVar18,&DAT_183c7700b);
      *(undefined8 *)(param_1 + 0x1c0) = *(undefined8 *)(lVar11 + 0x20);
      FUN_1800e3050();
      *(longlong *)(param_1 + 0x1c8) = param_2;
      lVar11 = *(longlong *)(param_2 + 8);
      lVar12 = (**(code **)(*(longlong *)(lVar11 + 0x560) + 0x60))();
      *(longlong *)(param_1 + 0x1b0) = lVar12;
      plVar18 = (longlong *)(lVar12 + 0x58);
      *(longlong **)(param_1 + 0x1b8) = plVar18;
      lVar12 = (**(code **)(*plVar18 + 0x50))(plVar18,local_90,param_2);
      FUN_18009a1e0(param_1 + 0x270,lVar12);
      *(undefined4 *)(param_1 + 0x2b0) = *(undefined4 *)(lVar12 + 0x40);
      if (local_90[0] != local_80) {
        FUN_182e4c5e0();
      }
      *(undefined4 *)(param_1 + 0x158) = 0;
      cVar6 = FUN_180146660(*(undefined8 *)(param_1 + 0x1b0));
      uVar21 = *(uint *)(param_1 + 0x158) & 0xfffffffe;
      if (cVar6 != '\0') {
        uVar21 = *(uint *)(param_1 + 0x158) | 1;
      }
      *(uint *)(param_1 + 0x158) = uVar21;
      cVar6 = FUN_180146680(*(undefined8 *)(param_1 + 0x1b0));
      uVar21 = *(uint *)(param_1 + 0x158) & 0xfffffffd;
      if (cVar6 != '\0') {
        uVar21 = *(uint *)(param_1 + 0x158) | 2;
      }
      *(uint *)(param_1 + 0x158) = uVar21;
      cVar6 = FUN_1801466a0(*(undefined8 *)(param_1 + 0x1b0));
      uVar21 = *(uint *)(param_1 + 0x158) & 0xfffffffb;
      if (cVar6 != '\0') {
        uVar21 = *(uint *)(param_1 + 0x158) | 4;
      }
      *(uint *)(param_1 + 0x158) = uVar21;
      cVar6 = FUN_1801466c0(*(undefined8 *)(param_1 + 0x1b0));
      uVar21 = *(uint *)(param_1 + 0x158) & 0xfffffff7;
      if (cVar6 != '\0') {
        uVar21 = *(uint *)(param_1 + 0x158) | 8;
      }
      *(uint *)(param_1 + 0x158) = uVar21;
      cVar6 = FUN_1801466c0(*(undefined8 *)(param_1 + 0x1b0));
      if ((cVar6 == '\0') || ((uint)DAT_183c6f718 == 0)) {
        bVar2 = false;
      }
      else {
        bVar2 = true;
      }
      uVar21 = *(uint *)(param_1 + 0x158) | 0x10;
      if (!bVar2) {
        uVar21 = *(uint *)(param_1 + 0x158) & 0xffffffef;
      }
      *(uint *)(param_1 + 0x158) = uVar21;
      uVar17 = uVar26;
      plVar18 = plVar13;
      for (lVar12 = *(longlong *)(*(longlong *)(param_1 + 0x1c8) + 0x178);
          lVar12 != *(longlong *)(param_1 + 0x1c8) + 0x170; lVar12 = *(longlong *)(lVar12 + 8)) {
        plVar18 = (longlong *)((longlong)plVar18 + 1);
        uVar17 = SUB84(plVar18,0);
      }
      uVar10 = thunk_FUN_182e45720(uVar17);
      *(undefined8 *)(param_1 + 0x240) = uVar10;
      plVar18 = plVar13;
      for (lVar12 = *(longlong *)(*(longlong *)(param_1 + 0x1c8) + 0x178);
          lVar12 != *(longlong *)(param_1 + 0x1c8) + 0x170; lVar12 = *(longlong *)(lVar12 + 8)) {
        plVar18 = (longlong *)((longlong)plVar18 + 1);
        uVar26 = SUB84(plVar18,0);
      }
      uVar10 = thunk_FUN_182e45720(uVar26);
      *(undefined8 *)(param_1 + 0x248) = uVar10;
      plVar18 = plVar13;
      while( true ) {
        lVar20 = *(longlong *)(param_1 + 0x1c8) + 0x170;
        lVar1 = *(longlong *)(*(longlong *)(param_1 + 0x1c8) + 0x178);
        plVar24 = plVar13;
        for (lVar12 = lVar1; lVar12 != lVar20; lVar12 = *(longlong *)(lVar12 + 8)) {
          plVar24 = (longlong *)((longlong)plVar24 + 1);
        }
        if ((uint)plVar24 <= (uint)plVar18) break;
        *(undefined1 *)((longlong)plVar18 + *(longlong *)(param_1 + 0x240)) = 0;
        *(undefined1 *)((longlong)plVar18 + *(longlong *)(param_1 + 0x248)) = 0;
        plVar18 = (longlong *)(ulonglong)((uint)plVar18 + 1);
      }
      for (; lVar1 != lVar20; lVar1 = *(longlong *)(lVar1 + 8)) {
        for (lVar12 = *(longlong *)(lVar1 + 0x28); lVar12 != lVar1 + 0x20;
            lVar12 = *(longlong *)(lVar12 + 8)) {
          if ((**(short **)(lVar12 + 0x10) == 0x82d) &&
             (*(char *)(*(longlong *)(lVar12 + 0x20) + 0x20) == '\b')) {
            *(undefined1 *)((longlong)*(int *)(lVar1 + 0x38) + *(longlong *)(param_1 + 0x240)) = 1;
          }
          if (((**(short **)(lVar12 + 0x10) - 0x240U & 0xfffd) == 0) &&
             (*(int *)(*(longlong *)(lVar12 + 0x20) + 4) == 0x2e)) {
            *(undefined1 *)((longlong)*(int *)(lVar1 + 0x38) + *(longlong *)(param_1 + 0x248)) = 1;
          }
        }
      }
      FUN_1800ed130(param_1);
      FUN_1800d9fa0(param_1);
      iVar8 = FUN_181a96b70(lVar11);
      if (iVar8 == 0) {
        uVar25 = DAT_183c70448;
        if ((uint)DAT_183c70448 < (uint)DAT_183c6f718) {
          uVar25 = CONCAT44(DAT_183c6f718._4_4_,(uint)DAT_183c6f718);
        }
        local_c8 = uVar25 & 0xffffffff;
        puVar15 = *(undefined8 **)(*(longlong *)(param_1 + 0x1c8) + 0x178);
        uVar21 = *(uint *)(*(longlong *)(param_1 + 0x1b0) + 400);
        local_f0 = (uint)uVar25;
        if ((uint)uVar25 < uVar21) {
          local_f0 = uVar21;
        }
        puVar23 = (undefined8 *)(*(longlong *)(param_1 + 0x1c8) + 0x170);
        local_a8 = puVar23;
        for (; plVar13 = (longlong *)0x0, puVar15 != puVar23; puVar15 = (undefined8 *)puVar15[1]) {
          puVar14 = (undefined8 *)puVar15[5];
          local_f8 = (longlong *)0x0;
          local_e0 = (undefined8 *)0x0;
          uStack_d8 = 0;
          plVar24 = (longlong *)0x0;
          plVar18 = (longlong *)0x0;
          local_e8 = (longlong *)0x0;
          if (puVar14 != puVar15 + 4) {
            do {
              if ((1 < (ushort)(*(short *)puVar14[2] - 0xdU)) &&
                 (3 < (ushort)(*(short *)puVar14[2] - 3U))) {
                if (plVar18 == plVar13) {
                  local_f8 = (longlong *)((longlong)plVar18 - (longlong)plVar24 >> 3);
                  if (local_f8 == (longlong *)0x1fffffffffffffff) {
                    /* WARNING: Subroutine does not return */
                    FUN_180075420();
                  }
                  local_108 = (longlong)local_f8 + 1;
                  uVar25 = (longlong)plVar13 - (longlong)plVar24 >> 3;
                  if (0x1fffffffffffffff - (uVar25 >> 1) < uVar25) {
                    uVar27 = 0x1fffffffffffffff;
                  }
                  else {
                    uVar27 = uVar25 + (uVar25 >> 1);
                    if (uVar27 < local_108) {
                      uVar27 = local_108;
                    }
                  }
                  local_100 = (longlong *)FUN_180095ad0(&local_e0,uVar27);
                  local_100[(longlong)local_f8] = (longlong)puVar14;
                  FUN_182e46bf0(local_100,plVar24,(longlong)plVar18 - (longlong)plVar24);
                  if (plVar24 != (longlong *)0x0) {
                    uVar25 = uVar25 * 8;
                    plVar13 = plVar24;
                    if (0xfff < uVar25) {
                      plVar13 = (longlong *)plVar24[-1];
                      uVar25 = uVar25 + 0x27;
                      if (0x1f < (ulonglong)((longlong)plVar24 + (-8 - (longlong)plVar13)))
                      goto LAB_1800e9a33;
                    }
                    thunk_FUN_182e4c5e0(plVar13,uVar25);
                  }
                  local_e8 = local_100;
                  plVar13 = local_100 + uVar27;
                  plVar18 = local_100 + local_108;
                  plVar24 = local_100;
                  local_f8 = plVar13;
                }
                else {
                  *plVar18 = (longlong)puVar14;
                  plVar18 = plVar18 + 1;
                }
              }
              puVar14 = (undefined8 *)puVar14[1];
            } while (puVar14 != puVar15 + 4);
          }
          uVar25 = local_c8;
          uVar5 = local_f0;
          uVar21 = 0;
          plVar4 = plVar24;
          for (; local_100 = plVar24, plVar24 != plVar18; plVar24 = plVar24 + 1) {
            lVar11 = *plVar24;
            uVar9 = FUN_1801387b0(*(undefined8 *)(param_1 + 0x1b0),lVar11);
            if (uVar9 < uVar21) {
              uVar9 = uVar21;
            }
            if (((ushort)(**(short **)(lVar11 + 0x10) - 1U) < 2) &&
               ((*(byte *)(*(longlong *)(lVar11 + 0x20) + 0x30) & 0x10) != 0)) {
              uVar16 = *(ushort *)(lVar11 + 0x2c);
              bVar19 = (byte)uVar16;
LAB_1800e94ac:
              if (uVar9 < (uint)uVar25) {
                uVar9 = (uint)uVar25;
              }
            }
            else {
              uVar16 = *(ushort *)(lVar11 + 0x2c);
              if (((uVar16 & 0xc) == 0) || ((uVar16 >> 2 & 1) != 0)) {
                bVar7 = (byte)(*(uint *)(*(short **)(lVar11 + 0x10) + 4) >> 0x13) & 1;
              }
              else {
                bVar7 = FUN_18063c670(lVar11,0x80000,1);
                uVar16 = *(ushort *)(lVar11 + 0x2c);
              }
              bVar19 = (byte)uVar16;
              if (bVar7 != 0) goto LAB_1800e94ac;
            }
            uVar21 = uVar9;
            if (((ushort)(**(short **)(lVar11 + 0x10) - 1U) < 2) &&
               ((*(byte *)(*(longlong *)(lVar11 + 0x20) + 0x30) & 8) != 0)) {
LAB_1800e9507:
              if (uVar9 < uVar5) {
                uVar21 = uVar5;
              }
            }
            else {
              if (((uVar16 & 0xc) == 0) || ((uVar16 >> 2 & 1) != 0)) {
                bVar7 = (byte)(*(uint *)(*(short **)(lVar11 + 0x10) + 4) >> 0x12) & 1;
                bVar19 = (byte)uVar16;
              }
              else {
                bVar7 = FUN_18063c670(lVar11,0x40000,1);
                bVar19 = (byte)*(undefined2 *)(lVar11 + 0x2c);
              }
              if (bVar7 != 0) goto LAB_1800e9507;
            }
            if ((bVar19 >> 3 & 1) == 0) {
              while (uVar21 != 0) {
                uVar21 = uVar21 - 1;
                local_108 = *(ulonglong *)(lVar11 + 0x38);
                lVar12 = *(longlong *)(*(longlong *)(param_1 + 0x1b0) + 8);
                local_b8 = *(undefined8 *)(lVar11 + 8);
                local_c0 = puVar15[8];
                if (local_108 != 0) {
                  FUN_1808fc320(&local_108,local_108,2);
                }
                FUN_180630180(local_c0,lVar12 + 0x1ffb0,&local_108,0);
                FUN_180627e50(puVar15,local_98,local_b8);
                plVar24 = local_100;
              }
              uVar21 = 0;
            }
            puVar23 = local_a8;
            plVar13 = local_f8;
            plVar4 = local_e8;
          }
          if (plVar4 != (longlong *)0x0) {
            uVar25 = (longlong)plVar13 - (longlong)plVar4 & 0xfffffffffffffff8;
            plVar13 = plVar4;
            if (0xfff < uVar25) {
              plVar13 = (longlong *)plVar4[-1];
              uVar25 = uVar25 + 0x27;
              if (0x1f < (ulonglong)((longlong)plVar4 + (-8 - (longlong)plVar13))) {
LAB_1800e9a33:
                    /* WARNING: Subroutine does not return */
                FUN_182e4c568();
              }
            }
            thunk_FUN_182e4c5e0(plVar13,uVar25);
          }
        }
      }
      else {
        if ((DAT_183c6e2f8 == '\0') && (DAT_183c6d980 == '\0')) {
          FUN_1800e8800(param_1);
        }
        lVar11 = *(longlong *)(param_1 + 0x1c8);
        for (uVar25 = *(ulonglong *)(lVar11 + 0x178); uVar25 != lVar11 + 0x170U;
            uVar25 = *(ulonglong *)(uVar25 + 8)) {
          pcVar22 = *(char **)(uVar25 + 0x28);
          if (pcVar22 != (char *)(uVar25 + 0x20)) {
LAB_1800e9661:
            if (((*(ushort *)(pcVar22 + 0x2c) & 0xc) == 0) ||
               ((*(ushort *)(pcVar22 + 0x2c) >> 2 & 1) != 0)) {
              bVar7 = (byte)(*(uint *)(*(longlong *)(pcVar22 + 0x10) + 8) >> 9) & 1;
            }
            else {
              bVar7 = FUN_18063c670(pcVar22,0x200,1);
            }
            if (bVar7 == 0) goto code_r0x0001800e969b;
            puVar15 = *(undefined8 **)(param_1 + 0x230);
            uStack_d8 = uStack_d8 & 0xffffffff00000000;
            puVar23 = (undefined8 *)puVar15[1];
            cVar6 = *(char *)((longlong)puVar23 + 0x19);
            puVar14 = puVar15;
            local_e0 = puVar23;
            while (puVar3 = puVar23, cVar6 == '\0') {
              uStack_d8._4_4_ = (uint)(uStack_d8 >> 0x20);
              if ((ulonglong)puVar3[4] < uVar25) {
                puVar23 = (undefined8 *)puVar3[2];
                uStack_d8 = (ulonglong)uStack_d8._4_4_ << 0x20;
              }
              else {
                uStack_d8 = CONCAT44(uStack_d8._4_4_,1);
                puVar23 = (undefined8 *)*puVar3;
                puVar14 = puVar3;
              }
              cVar6 = *(char *)((longlong)puVar23 + 0x19);
              local_e0 = puVar3;
            }
            if ((*(char *)((longlong)puVar14 + 0x19) != '\0') || (uVar25 < (ulonglong)puVar14[4])) {
              if (*(longlong *)(param_1 + 0x238) == 0x555555555555555) {
                    /* WARNING: Subroutine does not return */
                FUN_18007ec50();
              }
              plVar13 = (longlong *)FUN_182e45720(0x30);
              plVar13[4] = uVar25;
              plVar13[5] = 0;
              *plVar13 = (longlong)puVar15;
              plVar13[1] = (longlong)puVar15;
              plVar13[2] = (longlong)puVar15;
              *(undefined2 *)(plVar13 + 3) = 0;
              local_a8 = local_e0;
              uStack_a0 = (undefined4)uStack_d8;
              uStack_9c = uStack_d8._4_4_;
              puVar14 = (undefined8 *)FUN_18007e9f0(param_1 + 0x230,&local_a8,plVar13);
            }
            puVar14[5] = pcVar22;
          }
LAB_1800e9780:
        }
        FUN_1800daaf0(param_1);
        lVar11 = *(longlong *)(param_1 + 0x1d8);
        for (lVar12 = *(longlong *)(param_1 + 0x1d0); lVar12 != lVar11; lVar12 = lVar12 + 0x18) {
          FUN_1800d9ce0(param_1,lVar12);
          FUN_1800e3fa0(param_1);
          puVar15 = *(undefined8 **)(param_1 + 0x128);
          if (puVar15 != *(undefined8 **)(param_1 + 0x130)) {
            do {
              uVar10 = *puVar15;
              FUN_1800ed6d0(param_1,uVar10);
              FUN_1800eb310(param_1,uVar10,
                            (*(longlong *)(param_1 + 0x130) - *(longlong *)(param_1 + 0x128) &
                            0xfffffffffffffff8U) == 8);
              FUN_1800cfe00(param_1);
              lVar1 = *(longlong *)(param_1 + 0x128) + 8;
              FUN_182e46bf0(*(longlong *)(param_1 + 0x128),lVar1,
                            *(longlong *)(param_1 + 0x130) - lVar1);
              *(longlong *)(param_1 + 0x130) = *(longlong *)(param_1 + 0x130) + -8;
              puVar15 = *(undefined8 **)(param_1 + 0x128);
            } while (puVar15 != *(undefined8 **)(param_1 + 0x130));
          }
        }
        FUN_1800d4ee0(param_1);
      }
      FUN_18014d270(param_1 + 0xf8);
      lVar11 = *(longlong *)(param_1 + 0x1e8);
      cVar6 = *(char *)((longlong)*(longlong **)(lVar11 + 8) + 0x19);
      plVar13 = *(longlong **)(lVar11 + 8);
      while (cVar6 == '\0') {
        FUN_1800b4180(param_1 + 0x1e8,param_1 + 0x1e8,plVar13[2]);
        plVar18 = (longlong *)*plVar13;
        thunk_FUN_182e4c5e0(plVar13,0x30);
        plVar13 = plVar18;
        cVar6 = *(char *)((longlong)plVar18 + 0x19);
      }
      *(longlong *)(lVar11 + 8) = lVar11;
      *(longlong *)lVar11 = lVar11;
      *(longlong *)(lVar11 + 0x10) = lVar11;
      *(undefined8 *)(param_1 + 0x1f0) = 0;
      lVar11 = *(longlong *)(param_1 + 0x160);
      FUN_1800cce00(param_1 + 0x160,param_1 + 0x160,*(undefined8 *)(lVar11 + 8));
      *(longlong *)(lVar11 + 8) = lVar11;
      *(longlong *)lVar11 = lVar11;
      *(longlong *)(lVar11 + 0x10) = lVar11;
      *(undefined8 *)(param_1 + 0x168) = 0;
      lVar11 = *(longlong *)(param_1 + 0x230);
      cVar6 = *(char *)((longlong)*(longlong **)(lVar11 + 8) + 0x19);
      plVar13 = *(longlong **)(lVar11 + 8);
      while (cVar6 == '\0') {
        FUN_1800b4180(param_1 + 0x230,param_1 + 0x230,plVar13[2]);
        plVar18 = (longlong *)*plVar13;
        thunk_FUN_182e4c5e0(plVar13,0x30);
        plVar13 = plVar18;
        cVar6 = *(char *)((longlong)plVar18 + 0x19);
      }
      *(longlong *)(lVar11 + 8) = lVar11;
      *(longlong *)lVar11 = lVar11;
      *(longlong *)(lVar11 + 0x10) = lVar11;
      *(undefined8 *)(param_1 + 0x238) = 0;
      lVar11 = *(longlong *)(param_1 + 0x198);
      FUN_1800ccd40(param_1 + 0x198,param_1 + 0x198,*(undefined8 *)(lVar11 + 8));
      *(longlong *)(lVar11 + 8) = lVar11;
      *(longlong *)lVar11 = lVar11;
      *(longlong *)(lVar11 + 0x10) = lVar11;
      *(undefined8 *)(param_1 + 0x1a0) = 0;
      if (*(longlong *)(param_1 + 0x208) != *(longlong *)(param_1 + 0x210)) {
        *(longlong *)(param_1 + 0x210) = *(longlong *)(param_1 + 0x208);
      }
      lVar11 = *(longlong *)(param_1 + 0x1f8);
      FUN_1800ccb40(param_1 + 0x1f8,param_1 + 0x1f8,*(undefined8 *)(lVar11 + 8));
      *(longlong *)(lVar11 + 8) = lVar11;
      *(longlong *)lVar11 = lVar11;
      *(longlong *)(lVar11 + 0x10) = lVar11;
      *(undefined8 *)(param_1 + 0x200) = 0;
      if (*(longlong *)(param_1 + 0x1d0) != *(longlong *)(param_1 + 0x1d8)) {
        FUN_1800cc500(*(longlong *)(param_1 + 0x1d0),*(longlong *)(param_1 + 0x1d8),param_1 + 0x1d0)
        ;
        *(undefined8 *)(param_1 + 0x1d8) = *(undefined8 *)(param_1 + 0x1d0);
      }
      thunk_FUN_182e4c5e0(*(undefined8 *)(param_1 + 0x240));
      thunk_FUN_182e4c5e0(*(undefined8 *)(param_1 + 0x248));
      return 1;
    }
    if ((undefined *)*puVar15 == &DAT_183c7700b) {
      plVar18 = (longlong *)puVar15[1];
      goto LAB_1800e8f59;
    }
    puVar15 = puVar15 + 2;
  } while( true );
code_r0x0001800e969b:
  if ((*pcVar22 >> 2 & 1U) == 0) {
    bVar7 = pcVar22[0x2c];
    while ((bVar7 >> 3 & 1) != 0) {
      pcVar22 = *(char **)(pcVar22 + 8);
      bVar7 = pcVar22[0x2c];
    }
  }
  pcVar22 = *(char **)(pcVar22 + 8);
  if (pcVar22 == (char *)(uVar25 + 0x20)) goto LAB_1800e9780;
  goto LAB_1800e9661;
}



