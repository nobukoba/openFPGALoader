// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
using DWORD = std::uint32_t;
using ULONG_PTR = std::uintptr_t;
using HMODULE = void *;
using FARPROC = void (*)();
#define WINAPI
constexpr DWORD LOAD_LIBRARY_SEARCH_SYSTEM32 = 0x800;
HMODULE LoadLibraryExW(const wchar_t *, void *, DWORD);
FARPROC GetProcAddress(HMODULE, const char *);
int FreeLibrary(HMODULE);

