// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "ftdi_d2xx.hpp"

namespace {
// D2XX Classic Interface ABI: FT_STATUS is ULONG, FT_HANDLE is PVOID.
// Load only the installed system DLL, never a DLL from the current directory.
class Library {
public:
    Library() {
        static_assert(sizeof(void *) == 8, "D2XX backend requires Windows x64");
        module = LoadLibraryExW(L"ftd2xx.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module)
            throw std::runtime_error("Cannot load system ftd2xx.dll. Install the official FTDI CDM driver; do not switch to WinUSB.");
    }
    ~Library() { FreeLibrary(module); }
    template<typename... Args> DWORD call(const char *name, Args... args) {
        auto address = GetProcAddress(module, name);
        if (!address)
            throw std::runtime_error(std::string("Missing D2XX function: ") + name);
        using Function = DWORD (WINAPI *)(Args...);
        return reinterpret_cast<Function>(address)(args...);
    }
private:
    HMODULE module;
};
Library &library() { static Library instance; return instance; }
struct Context : ftdi_context {
    Context() : ftdi_context{} { type = TYPE_232H; max_packet_size = 512; }
    void *handle = nullptr;
    std::string error;
    std::string serial;
    std::string description;
};
Context *context(ftdi_context *value) { return static_cast<Context *>(value); }
int result(ftdi_context *value, const char *operation, DWORD status) {
    if (!status) return 0;
    context(value)->error = std::string(operation) + " failed (D2XX status " + std::to_string(status) + ")";
    return -1;
}
template<typename... Args> int command(ftdi_context *value, const char *name, Args... args) {
    return result(value, name, library().call(name, context(value)->handle, args...));
}
struct Device { DWORD location; std::string serial; std::string description; };
}

ftdi_context *d2xx_new() { library(); return new Context(); }
void d2xx_free(ftdi_context *value) {
    if (!value) return;
    d2xx_close(value);
    delete context(value);
}
const char *d2xx_error(ftdi_context *value) { return context(value)->error.c_str(); }
int d2xx_interface(ftdi_context *value, enum ftdi_interface iface) {
    if (iface == INTERFACE_ANY || iface == INTERFACE_A) return 0;
    context(value)->error = "This D2XX backend supports FT232H interface A only.";
    return -1;
}
int d2xx_bus(ftdi_context *value, unsigned int, unsigned int) {
    context(value)->error = "D2XX does not accept libusb bus/address selection; use --ftdi-serial.";
    return -1;
}
int d2xx_open(ftdi_context *value, int vid, int pid, const char *, const char *serial, unsigned int index) {
    auto *ctx = context(value);
    if (vid != 0x0403 || pid != 0x6014 || index != 0) {
        ctx->error = "D2XX currently supports FT232H/HS3 (0403:6014) only; index selection is not supported.";
        return -1;
    }
    DWORD count = 0;
    if (result(value, "FT_CreateDeviceInfoList", library().call("FT_CreateDeviceInfoList", &count)) < 0) return -1;
    std::vector<Device> matches;
    for (DWORD i = 0; i < count; ++i) {
        DWORD flags = 0, type = 0, id = 0, location = 0;
        char found_serial[16] = {}, description[64] = {};
        void *handle = nullptr;
        if (result(value, "FT_GetDeviceInfoDetail", library().call("FT_GetDeviceInfoDetail",
                i, &flags, &type, &id, &location, found_serial, description, &handle)) < 0) return -1;
        found_serial[15] = 0;
        description[63] = 0;
        if (id == 0x04036014 && type == 8 && (!serial || std::string(serial) == found_serial))
            matches.push_back({location, found_serial, description});
    }
    if (matches.size() != 1) {
        ctx->error = "Expected exactly one matching FT232H. Close Vivado/hw_server and select --ftdi-serial. Matching serials:";
        for (const auto &device : matches) ctx->error += " " + device.serial;
        return -1;
    }
    const auto &selected = matches.front();
    if (selected.serial.empty()) {
        ctx->error = "FT232H has no serial number; refusing ambiguous D2XX access.";
        return -1;
    }
    // Use the enumerated location, then verify identity again before configuration.
    if (result(value, "FT_OpenEx", library().call("FT_OpenEx",
            reinterpret_cast<void *>(static_cast<ULONG_PTR>(selected.location)), DWORD(4), &ctx->handle)) < 0) return -1;
    DWORD type = 0, id = 0;
    char actual_serial[16] = {}, description[64] = {};
    DWORD status = library().call("FT_GetDeviceInfo", ctx->handle, &type, &id,
                                 actual_serial, description, static_cast<void *>(nullptr));
    actual_serial[15] = 0;
    description[63] = 0;
    if (status || id != 0x04036014 || type != 8 || selected.serial != actual_serial) {
        ctx->error = "D2XX identity changed during open; no programming attempted.";
        d2xx_close(value);
        return -1;
    }
    ctx->serial = actual_serial;
    ctx->description = description;
    if (command(value, "FT_SetTimeouts", DWORD(3000), DWORD(3000)) < 0) {
        d2xx_close(value);
        return -1;
    }
    return 0;
}
int d2xx_close(ftdi_context *value) {
    auto *ctx = context(value);
    if (!ctx->handle) return 0;
    int status = command(value, "FT_Close");
    ctx->handle = nullptr;
    return status;
}
int d2xx_reset(ftdi_context *value) { return command(value, "FT_ResetDevice"); }
int d2xx_baud(ftdi_context *value, int rate) { return command(value, "FT_SetBaudRate", DWORD(rate)); }
int d2xx_bitmode(ftdi_context *value, unsigned char mask, unsigned char mode) {
    return command(value, "FT_SetBitMode", mask, mode);
}
int d2xx_latency(ftdi_context *value, unsigned char latency) { return command(value, "FT_SetLatencyTimer", latency); }
int d2xx_purge_rx(ftdi_context *value) { return command(value, "FT_Purge", DWORD(1)); }
int d2xx_purge_tx(ftdi_context *value) { return command(value, "FT_Purge", DWORD(2)); }
int d2xx_purge(ftdi_context *value) { return command(value, "FT_Purge", DWORD(3)); }
int d2xx_chunk(ftdi_context *, unsigned int) { return 0; }
int d2xx_character(ftdi_context *value, unsigned char, unsigned char) {
    context(value)->error = "Character-based FTDI transports are not supported by this D2XX backend.";
    return -1;
}
int d2xx_write(ftdi_context *value, const unsigned char *data, int length) {
    DWORD written = 0;
    if (command(value, "FT_Write", const_cast<unsigned char *>(data), DWORD(length), &written) < 0) return -1;
    if (written != DWORD(length)) {
        context(value)->error = "Short D2XX write; stopped without retry.";
        return -1;
    }
    return static_cast<int>(written);
}
int d2xx_read(ftdi_context *value, unsigned char *data, int length) {
    DWORD received = 0;
    if (command(value, "FT_Read", data, DWORD(length), &received) < 0) return -1;
    if (received != DWORD(length)) {
        context(value)->error = "Incomplete D2XX read after 3-second timeout; stopped without retry.";
        return -1;
    }
    return static_cast<int>(received);
}
void d2xx_strings(ftdi_context *value, unsigned char *product, unsigned char *manufacturer, unsigned char *serial) {
    std::strncpy(reinterpret_cast<char *>(product), context(value)->description.c_str(), 199);
    std::strncpy(reinterpret_cast<char *>(serial), context(value)->serial.c_str(), 199);
    std::strcpy(reinterpret_cast<char *>(manufacturer), "FTDI");
}

int d2xx_scan() {
    DWORD count = 0;
    DWORD status = library().call("FT_CreateDeviceInfoList", &count);
    if (status) throw std::runtime_error("D2XX enumeration failed: " + std::to_string(status));
    std::cout << "FTDI D2XX scan (installed CDM driver; FT232H only)" << std::endl;
    unsigned int matches = 0;
    for (DWORD i = 0; i < count; ++i) {
        DWORD flags = 0, type = 0, id = 0, location = 0;
        char serial[16] = {}, description[64] = {};
        void *handle = nullptr;
        status = library().call("FT_GetDeviceInfoDetail", i, &flags, &type, &id,
                                &location, serial, description, &handle);
        if (status) throw std::runtime_error("D2XX device information failed: " + std::to_string(status));
        serial[15] = 0;
        description[63] = 0;
        if (id == 0x04036014 && type == 8) {
            std::cout << "0403:6014 serial=" << serial << " description=" << description
                      << ((flags & 1) ? " [busy]" : "") << std::endl;
            ++matches;
        }
    }
    std::cout << "Matching FT232H devices: " << matches << std::endl;
    return 0;
}

