// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "dpcdecl.h"
extern "C" {
BOOL DjtgEnable(HIF);
BOOL DjtgDisable(HIF);
BOOL DjtgSetSpeed(HIF, DWORD, DWORD *);
BOOL DjtgPutTdiBits(HIF, BOOL, BYTE *, BYTE *, DWORD, BOOL);
BOOL DjtgPutTmsBits(HIF, BOOL, BYTE *, BYTE *, DWORD, BOOL);
BOOL DjtgPutTmsTdiBits(HIF, BYTE *, BYTE *, DWORD, BOOL);
BOOL DjtgClockTck(HIF, BOOL, BOOL, DWORD, BOOL);
}
