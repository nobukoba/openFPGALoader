// SPDX-License-Identifier: Apache-2.0
// Compile the real transport against a synthetic D2XX API, without hardware.
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>
#include "../../src/ftdi_d2xx.cpp"
static DWORD devices = 1, opens = 0, closes = 0;
static bool wrong_identity = false, short_write = false;
static DWORD create_list(DWORD *count) { *count = devices; return 0; }
static DWORD detail(DWORD i, DWORD *flags, DWORD *type, DWORD *id, DWORD *loc,
                    char *serial, char *desc, void **handle) {
    *flags = 0; *type = 8; *id = 0x04036014; *loc = i + 1; *handle = nullptr;
    std::strcpy(serial, i ? "OTHER" : "HS3TEST");
    std::strcpy(desc, "Synthetic FT232H"); return 0;
}
static DWORD open_device(void *, DWORD mode, void **handle) {
    assert(mode == 4); ++opens; *handle = reinterpret_cast<void *>(1); return 0;
}
static DWORD info(void *, DWORD *type, DWORD *id, char *serial, char *desc, void *) {
    *type = 8; *id = 0x04036014;
    std::strcpy(serial, wrong_identity ? "CHANGED" : "HS3TEST");
    std::strcpy(desc, "Synthetic FT232H"); return 0;
}
static DWORD close_device(void *) { ++closes; return 0; }
static DWORD timeouts(void *, DWORD read, DWORD write) { assert(read == 3000 && write == 3000); return 0; }
static DWORD write_data(void *, unsigned char *, DWORD n, DWORD *done) { *done = short_write ? n - 1 : n; return 0; }
static DWORD read_data(void *, unsigned char *, DWORD, DWORD *done) { *done = 0; return 0; }
HMODULE LoadLibraryExW(const wchar_t *, void *, DWORD flags) { assert(flags == LOAD_LIBRARY_SEARCH_SYSTEM32); return reinterpret_cast<void *>(1); }
int FreeLibrary(HMODULE) { return 1; }
FARPROC GetProcAddress(HMODULE, const char *name) {
#define EXPORT(n, f) if (!std::strcmp(name, n)) return reinterpret_cast<FARPROC>(f)
    EXPORT("FT_CreateDeviceInfoList", create_list);
    EXPORT("FT_GetDeviceInfoDetail", detail);
    EXPORT("FT_OpenEx", open_device);
    EXPORT("FT_GetDeviceInfo", info);
    EXPORT("FT_Close", close_device);
    EXPORT("FT_SetTimeouts", timeouts);
    EXPORT("FT_Write", write_data);
    EXPORT("FT_Read", read_data);
    throw std::runtime_error(std::string("Unexpected API call: ") + name);
}
int main() {
    auto *ctx = d2xx_new();
    devices = 2;
    assert(d2xx_open(ctx, 0x0403, 0x6014, nullptr, nullptr, 0) < 0);
    assert(opens == 0);
    assert(std::strstr(d2xx_error(ctx), "HS3TEST OTHER"));
    assert(d2xx_open(ctx, 0x0403, 0x6014, nullptr, "HS3TEST", 0) == 0);
    unsigned char data[4] = {};
    short_write = true;
    assert(d2xx_write(ctx, data, 4) < 0);
    assert(opens == 1);
    assert(d2xx_read(ctx, data, 4) < 0);
    d2xx_free(ctx);
    assert(closes == 1);
    ctx = d2xx_new();
    wrong_identity = true;
    assert(d2xx_open(ctx, 0x0403, 0x6014, nullptr, "HS3TEST", 0) < 0);
    assert(closes == 2);
    d2xx_free(ctx);
    assert(closes == 2);
    ctx = d2xx_new();
    assert(d2xx_open(ctx, 0x03fd, 0x0008, nullptr, nullptr, 0) < 0);
    assert(d2xx_bus(ctx, 1, 2) < 0);
    assert(d2xx_interface(ctx, INTERFACE_B) < 0);
    d2xx_free(ctx);
}

