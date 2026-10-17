// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "dpcdecl.h"
extern "C" {
ERC DmgrGetLastError();
BOOL DmgrOpen(HIF *, char *);
BOOL DmgrClose(HIF);
BOOL DmgrEnumDevices(int *);
BOOL DmgrGetDvc(int, DVC *);
BOOL DmgrFreeDvcEnum();
BOOL DmgrGetInfo(DVC *, DINFO, void *);
BOOL DmgrSetTransTimeout(HIF, DWORD);
}
