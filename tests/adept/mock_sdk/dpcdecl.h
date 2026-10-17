// SPDX-License-Identifier: Apache-2.0
// Minimal independently declared API surface for tests, not a vendor SDK.
#pragma once
#include <cstdint>
using BYTE = unsigned char;
#ifndef _WIN32
using BOOL = int;
using DWORD = uint32_t;
#endif
using HIF = DWORD;
using DTP = DWORD;
using DCAP = DWORD;
using DINFO = DWORD;
using ERC = int;
constexpr HIF hifInvalid = 0;
constexpr BOOL fFalse = 0;
constexpr DTP dtpUSB = 1;
constexpr DCAP dcapJtg = 1;
constexpr DINFO dinfoSN = 5, dinfoDCAP = 9;
constexpr DWORD cchSnMax = 15;
struct DVC {
	char szName[64];
	char szConn[261];
	DTP dtp;
};
