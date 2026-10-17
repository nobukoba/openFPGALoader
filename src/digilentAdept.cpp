// SPDX-License-Identifier: Apache-2.0
#include "digilentAdept.hpp"

#include <dmgr.h>
#include <djtg.h>
#include <algorithm>
#include <climits>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::string adeptError(const char *operation)
{
	return std::string("Adept: ") + operation + " failed (error " +
		std::to_string(DmgrGetLastError()) +
		"). Check cable connection, target power and whether Vivado/hw_server owns the cable.";
}

bool bit(const uint8_t *data, uint32_t pos)
{
	return data && ((data[pos / 8] >> (pos % 8)) & 1);
}

void putBit(uint8_t *data, uint32_t pos, bool value)
{
	if (value)
		data[pos / 8] |= uint8_t(1u << (pos % 8));
}

// Adept enumeration is process-global. Always release it before opening a cable.
struct Enumeration {
	~Enumeration() { DmgrFreeDvcEnum(); }
};
}

std::string DigilentAdept::selectDevice(const std::string &serial, int index)
{
	int count = 0;
	if (!DmgrEnumDevices(&count))
		throw std::runtime_error(adeptError("enumerate devices"));
	Enumeration enumeration;
	std::vector<std::string> connections;
	for (int i = 0; i < count; ++i) {
		DVC device = {};
		if (!DmgrGetDvc(i, &device))
			throw std::runtime_error(adeptError("get enumerated device"));
		if (device.dtp != dtpUSB)
			continue;
		DCAP capabilities = 0;
		if (!DmgrGetInfo(&device, dinfoDCAP, &capabilities))
			throw std::runtime_error(adeptError("get device capabilities"));
		if (!(capabilities & dcapJtg))
			continue;
		char actualSerial[cchSnMax + 1] = {};
		if (!serial.empty()) {
			if (!DmgrGetInfo(&device, dinfoSN, actualSerial))
				throw std::runtime_error(adeptError("get device serial"));
			actualSerial[cchSnMax] = '\0';
			if (serial != actualSerial)
				continue;
		}
		device.szConn[sizeof(device.szConn) - 1] = '\0';
		connections.emplace_back(device.szConn);
	}
	if (connections.empty())
		throw std::runtime_error("Adept: no matching USB JTAG cable. Check the Adept Runtime and serial number.");
	if (index >= 0) {
		if (static_cast<size_t>(index) >= connections.size())
			throw std::runtime_error("Adept: cable index is out of range.");
		return connections[index];
	}
	if (connections.size() != 1)
		throw std::runtime_error("Adept: multiple USB JTAG cables; select one with --usb-serial-num or --cable-index.");
	return connections.front();
}

DigilentAdept::DigilentAdept(const std::string &device,
	const std::string &serial, int index, uint32_t frequency)
{
	_clkHZ = 0;
	if (!frequency || frequency > INT_MAX)
		throw std::invalid_argument("Adept: invalid JTAG frequency.");
	if (index < -1 || (!device.empty() && (!serial.empty() || index >= 0)) ||
		(!serial.empty() && index >= 0))
		throw std::invalid_argument("Adept: use only one device selector: -d, --usb-serial-num or --cable-index.");
	std::string selector = device.empty() ? selectDevice(serial, index) : device;
	std::vector<char> mutableSelector(selector.begin(), selector.end());
	mutableSelector.push_back('\0');
	try {
		check(DmgrOpen(&_handle, mutableSelector.data()), "open device");
		check(DmgrSetTransTimeout(_handle, 5000), "set transfer timeout");
		check(DjtgEnable(_handle), "enable JTAG");
		_enabled = true;
		setClkFreq(frequency);
	} catch (...) {
		close();
		throw;
	}
}

DigilentAdept::~DigilentAdept() { close(); }

void DigilentAdept::close() noexcept
{
	if (_handle == hifInvalid)
		return;
	if (_enabled && !DjtgDisable(_handle))
		std::cerr << "Adept: failed to disable JTAG during cleanup\n";
	if (!DmgrClose(_handle))
		std::cerr << "Adept: failed to close device during cleanup\n";
	_handle = hifInvalid;
	_enabled = false;
}

void DigilentAdept::ready() const
{
	if (_failed || _handle == hifInvalid)
		throw std::runtime_error("Adept: connection is unusable; reconnect before retrying.");
}

void DigilentAdept::check(BOOL result, const char *operation)
{
	if (!result) {
		_failed = true;
		throw std::runtime_error(adeptError(operation));
	}
}

void DigilentAdept::validateLength(uint32_t len)
{
	if (len > INT_MAX)
		throw std::invalid_argument("Adept: JTAG bit count exceeds interface limit.");
}

uint32_t DigilentAdept::chunkBits() const
{
	// At most one second of clocks per synchronous SDK request (5 s timeout).
	return std::min<uint32_t>(8192, std::max<uint32_t>(1, _clkHZ));
}

int DigilentAdept::setClkFreq(uint32_t frequency)
{
	ready();
	if (!frequency || frequency > INT_MAX)
		throw std::invalid_argument("Adept: invalid JTAG frequency.");
	DWORD actual = 0;
	check(DjtgSetSpeed(_handle, frequency, &actual), "set JTAG speed");
	if (!actual || actual > INT_MAX) {
		_failed = true;
		throw std::runtime_error("Adept: runtime returned an invalid JTAG speed.");
	}
	_clkHZ = actual;
	return static_cast<int>(actual);
}

int DigilentAdept::writeTMS(const uint8_t *tms, uint32_t len,
	bool /* flush_buffer */, const uint8_t tdi)
{
	ready();
	validateLength(len);
	if (len && !tms)
		throw std::invalid_argument("Adept: missing TMS buffer.");
	for (uint32_t pos = 0; pos < len;) {
		const uint32_t count = std::min(chunkBits(), len - pos);
		std::vector<BYTE> data((count + 7) / 8, 0);
		for (uint32_t i = 0; i < count; ++i)
			putBit(data.data(), i, bit(tms, pos + i));
		check(DjtgPutTmsBits(_handle, tdi != 0, data.data(), nullptr,
			count, fFalse), "write TMS");
		pos += count;
	}
	return static_cast<int>(len);
}

int DigilentAdept::writeTDI(const uint8_t *tx, uint8_t *rx,
	uint32_t len, bool end)
{
	ready();
	validateLength(len);
	std::vector<uint8_t> input;
	if (rx && tx == rx && len) {
		input.assign(tx, tx + (static_cast<size_t>(len) + 7) / 8);
		tx = input.data();
	}
	if (rx)
		std::memset(rx, 0, (static_cast<size_t>(len) + 7) / 8);
	// The exit clock must also shift/capture the final data bit.
	const uint32_t ordinary = len - ((end && len) ? 1 : 0);
	for (uint32_t pos = 0; pos < len;) {
		const bool last = pos == ordinary;
		const uint32_t count = last ? 1 : std::min(chunkBits(), ordinary - pos);
		std::vector<BYTE> send((count + 7) / 8, 0), receive(send.size(), 0);
		for (uint32_t i = 0; i < count; ++i)
			putBit(send.data(), i, bit(tx, pos + i));
		check(DjtgPutTdiBits(_handle, last && end, send.data(),
			rx ? receive.data() : nullptr, count, fFalse), "shift TDI/TDO");
		if (rx)
			for (uint32_t i = 0; i < count; ++i)
				putBit(rx, pos + i, bit(receive.data(), i));
		pos += count;
	}
	return static_cast<int>(len);
}

bool DigilentAdept::writeTMSTDI(const uint8_t *tms, const uint8_t *tdi,
	uint8_t *tdo, uint32_t len)
{
	ready();
	validateLength(len);
	if (len && (!tms || !tdi))
		throw std::invalid_argument("Adept: missing TMS/TDI buffer.");
	std::vector<uint8_t> savedTms, savedTdi;
	if (tdo && len) {
		const size_t bytes = (static_cast<size_t>(len) + 7) / 8;
		if (tdo == tms) { savedTms.assign(tms, tms + bytes); tms = savedTms.data(); }
		if (tdo == tdi) { savedTdi.assign(tdi, tdi + bytes); tdi = savedTdi.data(); }
	}
	if (tdo)
		std::memset(tdo, 0, (static_cast<size_t>(len) + 7) / 8);
	for (uint32_t pos = 0; pos < len;) {
		const uint32_t count = std::min(chunkBits(), len - pos);
		std::vector<BYTE> pairs((count + 3) / 4, 0), receive((count + 7) / 8, 0);
		for (uint32_t i = 0; i < count; ++i) {
			// Adept bit pairs are TDI in bit 0, TMS in bit 1, LSB first.
			putBit(pairs.data(), 2 * i, bit(tdi, pos + i));
			putBit(pairs.data(), 2 * i + 1, bit(tms, pos + i));
		}
		check(DjtgPutTmsTdiBits(_handle, pairs.data(),
			tdo ? receive.data() : nullptr, count, fFalse), "shift TMS/TDI/TDO");
		if (tdo)
			for (uint32_t i = 0; i < count; ++i)
				putBit(tdo, pos + i, bit(receive.data(), i));
		pos += count;
	}
	return true;
}

int DigilentAdept::toggleClk(uint8_t tms, uint8_t tdi, uint32_t len)
{
	ready();
	validateLength(len);
	for (uint32_t pos = 0; pos < len;) {
		const uint32_t count = std::min(chunkBits(), len - pos);
		check(DjtgClockTck(_handle, tms != 0, tdi != 0, count, fFalse), "clock TCK");
		pos += count;
	}
	return static_cast<int>(len);
}

int DigilentAdept::flush()
{
	ready();
	return 0; // All SDK transfers are synchronous; nothing remains buffered.
}
