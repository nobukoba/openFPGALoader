// SPDX-License-Identifier: Apache-2.0
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <dmgr.h>
#include <djtg.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include "mock_adept.hpp"

namespace mock {
std::vector<Device> devices;
std::vector<Clock> clocks;
std::vector<uint32_t> requests;
std::vector<std::string> calls;
std::vector<bool> scanData;
std::vector<uint8_t> lastPairs;
size_t scanPos;
std::string fail, opened;
uint32_t actualSpeed;
int opens, closes, enables, disables, enumFrees;
int failAfter;
void reset()
{
	devices = {{"USB:first", "SERIAL1"}};
	clocks.clear(); requests.clear(); calls.clear(); fail.clear(); opened.clear();
	scanData.clear(); scanPos = 0;
	lastPairs.clear(); failAfter = 0;
	actualSpeed = 0;
	opens = closes = enables = disables = enumFrees = 0;
}
}
namespace {
BOOL ok(const char *name)
{
	mock::calls.emplace_back(name);
	if (mock::fail == name) {
		if (mock::failAfter == 0) return 0;
		--mock::failAfter;
	}
	return 1;
}
bool bit(const BYTE *data, uint32_t index) { return (data[index / 8] >> (index % 8)) & 1; }
void clock(bool tms, bool tdi, BYTE *rx, uint32_t index)
{
	// A deterministic independent TDO stream based on absolute clock number.
	if (rx) {
		const bool value = mock::scanData.empty() ?
			(mock::clocks.size() % 7 == 1 || mock::clocks.size() % 7 == 4) :
			mock::scanData.at(mock::scanPos++);
		if (value) rx[index / 8] |= BYTE(1u << (index % 8));
	}
	mock::clocks.push_back({tms, tdi});
}
void begin(BYTE *rx, DWORD bits, BOOL overlap)
{
	if (overlap)
		throw std::runtime_error("test requires synchronous SDK calls");
	mock::requests.push_back(bits);
	if (rx) std::memset(rx, 0, (bits + 7) / 8);
}
}
extern "C" {
ERC DmgrGetLastError() { return 3087; }
BOOL DmgrEnumDevices(int *count)
{
	*count = static_cast<int>(mock::devices.size());
	return ok("enum");
}
BOOL DmgrGetDvc(int i, DVC *device)
{
	if (!ok("getdevice")) return 0;
	*device = {};
	std::strncpy(device->szConn, mock::devices.at(i).connection.c_str(), sizeof(device->szConn) - 1);
	device->dtp = mock::devices[i].usb ? dtpUSB : 2;
	return 1;
}
BOOL DmgrFreeDvcEnum() { ++mock::enumFrees; return ok("freeenum"); }
BOOL DmgrGetInfo(DVC *device, DINFO info, void *value)
{
	if (!ok("info")) return 0;
	for (const auto &candidate : mock::devices) {
		if (candidate.connection != device->szConn) continue;
		if (info == dinfoSN)
			std::strncpy(static_cast<char *>(value), candidate.serial.c_str(), cchSnMax);
		else if (info == dinfoDCAP)
			*static_cast<DCAP *>(value) = candidate.jtag ? dcapJtg : 0;
		else return 0;
		return 1;
	}
	return 0;
}
BOOL DmgrOpen(HIF *handle, char *selector)
{
	if (!ok("open")) return 0;
	*handle = 1; ++mock::opens; mock::opened = selector; return 1;
}
BOOL DmgrClose(HIF) { ++mock::closes; return ok("close"); }
BOOL DmgrSetTransTimeout(HIF, DWORD timeout)
{
	if (timeout != 5000) throw std::runtime_error("unexpected timeout");
	return ok("timeout");
}
BOOL DjtgEnable(HIF)
{
	if (!ok("enable")) return 0;
	++mock::enables; return 1;
}
BOOL DjtgDisable(HIF) { ++mock::disables; return ok("disable"); }
BOOL DjtgSetSpeed(HIF, DWORD requested, DWORD *actual)
{
	*actual = mock::actualSpeed ? mock::actualSpeed : requested;
	return ok("speed");
}
BOOL DjtgPutTdiBits(HIF, BOOL tms, BYTE *tx, BYTE *rx, DWORD count, BOOL overlap)
{
	if (!ok("tdi")) return 0;
	begin(rx, count, overlap);
	for (DWORD i = 0; i < count; ++i) clock(tms, bit(tx, i), rx, i);
	return 1;
}
BOOL DjtgPutTmsBits(HIF, BOOL tdi, BYTE *tx, BYTE *rx, DWORD count, BOOL overlap)
{
	if (!ok("tms")) return 0;
	begin(rx, count, overlap);
	for (DWORD i = 0; i < count; ++i) clock(bit(tx, i), tdi, rx, i);
	return 1;
}
BOOL DjtgPutTmsTdiBits(HIF, BYTE *tx, BYTE *rx, DWORD count, BOOL overlap)
{
	if (!ok("pairs")) return 0;
	mock::lastPairs.assign(tx, tx + (count + 3) / 4);
	begin(rx, count, overlap);
	for (DWORD i = 0; i < count; ++i) clock(bit(tx, 2 * i + 1), bit(tx, 2 * i), rx, i);
	return 1;
}
BOOL DjtgClockTck(HIF, BOOL tms, BOOL tdi, DWORD count, BOOL overlap)
{
	if (!ok("clock")) return 0;
	begin(nullptr, count, overlap);
	for (DWORD i = 0; i < count; ++i) clock(tms, tdi, nullptr, i);
	return 1;
}
}
