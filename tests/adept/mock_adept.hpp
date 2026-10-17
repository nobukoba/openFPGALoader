// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mock {
struct Device { std::string connection, serial; bool jtag = true; bool usb = true; };
struct Clock { bool tms, tdi; };
extern std::vector<Device> devices;
extern std::vector<Clock> clocks;
extern std::vector<uint32_t> requests;
extern std::vector<std::string> calls;
extern std::vector<bool> scanData;
extern std::vector<uint8_t> lastPairs;
extern std::string fail;
extern std::string opened;
extern uint32_t actualSpeed;
extern int opens, closes, enables, disables, enumFrees;
extern int failAfter;
void reset();
}
