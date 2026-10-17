// SPDX-License-Identifier: Apache-2.0
#include "digilentAdept.hpp"
#include "mock_adept.hpp"
#include <climits>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition) { if (!condition) throw std::runtime_error("test assertion failed"); }
static void throws(const std::function<void()> &run)
{
	try { run(); } catch (const std::exception &) { return; }
	throw std::runtime_error("expected exception");
}
static bool bit(const std::vector<uint8_t> &v, uint32_t i) { return (v[i / 8] >> (i % 8)) & 1; }
static bool tdo(uint32_t i) { return i % 7 == 1 || i % 7 == 4; }

static void selection()
{
	mock::reset();
	mock::devices = {{"USB:a", "A"}, {"USB:b", "B"}, {"USB:uart", "C", false}, {"NET:d", "D", true, false}};
	throws([] { DigilentAdept d("", "", -1, 6000000); });
	require(mock::opens == 0 && mock::enumFrees == 1);
	{ DigilentAdept d("", "B", -1, 6000000); require(mock::opened == "USB:b"); }
	{ DigilentAdept d("", "", 0, 6000000); require(mock::opened == "USB:a"); }
	throws([] { DigilentAdept d("", "unknown", -1, 6000000); });
	throws([] { DigilentAdept d("", "", 2, 6000000); });
	throws([] { DigilentAdept d("alias", "B", -1, 6000000); });
	throws([] { DigilentAdept d("", "B", 0, 6000000); });
	{ DigilentAdept d("JtagHS3", "", -1, 6000000); require(mock::opened == "JtagHS3"); }
	require(mock::opens == mock::closes && mock::enables == mock::disables);
}

static void shifts()
{
	for (uint32_t speed : {7u, 6000000u}) {
		for (uint32_t len : {0u, 1u, 2u, 7u, 8u, 9u, 31u, 32u, 8191u, 8192u, 8193u, 16391u}) {
			for (bool end : {false, true}) {
				mock::reset();
				DigilentAdept d("", "", -1, speed);
				std::vector<uint8_t> tx((len + 7) / 8, 0xa6), rx(tx.size() + 1, 0xff);
				require(d.writeTDI(tx.data(), rx.data(), len, end) == static_cast<int>(len));
				require(mock::clocks.size() == len);
				for (uint32_t i = 0; i < len; ++i) {
					require(mock::clocks[i].tdi == bit(tx, i));
					require(mock::clocks[i].tms == (end && i == len - 1));
					require(bit(rx, i) == tdo(i));
				}
				require(rx.back() == 0xff); // no overwrite past ceil(bits/8)
				if (len % 8) require((rx[tx.size() - 1] >> (len % 8)) == 0);
				for (auto count : mock::requests) require(count <= speed && count <= 8192);
			}
		}
	}
	mock::reset();
	DigilentAdept d("", "", -1, 1000000);
	uint8_t rx = 0xff;
	require(d.writeTDI(nullptr, &rx, 7, true) == 7);
	for (const auto &c : mock::clocks) require(!c.tdi);
	require(d.writeTDI(nullptr, nullptr, 0, true) == 0);
	mock::clocks.clear();
	std::vector<uint8_t> inPlace = {0xa6, 0x75, 0x01};
	const auto original = inPlace;
	require(d.writeTDI(inPlace.data(), inPlace.data(), 19, true) == 19);
	for (uint32_t i = 0; i < 19; ++i) require(mock::clocks[i].tdi == bit(original, i));
}

static void tmsAndPairs()
{
	// Digilent's DjtgDemo SHIFT-DR setup fixture: packed bytes aa 22 00.
	mock::reset();
	{
		DigilentAdept d("", "", -1, 6000000);
		uint8_t fixtureTms[] = {0x5f, 0}, fixtureTdi[] = {0, 0};
		d.writeTMSTDI(fixtureTms, fixtureTdi, nullptr, 9);
		require(mock::lastPairs == std::vector<uint8_t>({0xaa, 0x22, 0}));
	}
	mock::reset();
	DigilentAdept d("", "", -1, 7); // deliberately non-byte-aligned chunks
	std::vector<uint8_t> tms = {0x1f, 0x93, 0x02}, tx = {0xa6, 0x75, 0x01}, rx(3, 0xff);
	require(d.writeTMS(tms.data(), 19, false, 0) == 19);
	for (uint32_t i = 0; i < 19; ++i) require(mock::clocks[i].tms == bit(tms, i) && !mock::clocks[i].tdi);
	require(d.flush() == 0 && !d.isFull());
	mock::clocks.clear();
	require(d.writeTMSTDI(tms.data(), tx.data(), rx.data(), 19));
	for (uint32_t i = 0; i < 19; ++i) {
		require(mock::clocks[i].tms == bit(tms, i));
		require(mock::clocks[i].tdi == bit(tx, i));
		require(bit(rx, i) == tdo(i));
	}
	require((rx.back() & 0xf8) == 0);
	mock::clocks.clear();
	require(d.toggleClk(1, 1, 23) == 23);
	require(mock::clocks.size() == 23);
	for (const auto &c : mock::clocks) require(c.tms && c.tdi);
	mock::clocks.clear();
	const auto oldTms = tms;
	require(d.writeTMSTDI(tms.data(), tx.data(), tms.data(), 19));
	for (uint32_t i = 0; i < 19; ++i) {
		require(mock::clocks[i].tms == bit(oldTms, i));
		require(bit(tms, i) == tdo(i));
	}
}

static void failures()
{
	for (const char *stage : {"enum", "getdevice", "info", "open", "timeout", "enable", "speed"}) {
		mock::reset(); mock::fail = stage;
		throws([] { DigilentAdept d("", "", -1, 6000000); });
		require(mock::opens == mock::closes && mock::enables == mock::disables);
	}
	for (const char *stage : {"tdi", "tms", "pairs", "clock", "speed"}) {
		mock::reset();
		{
			DigilentAdept d("", "", -1, 6000000);
			mock::fail = stage;
			uint8_t tx = 0xa5, rx = 0;
			throws([&] {
				if (mock::fail == "tdi") d.writeTDI(&tx, &rx, 8, true);
				else if (mock::fail == "tms") d.writeTMS(&tx, 8, true);
				else if (mock::fail == "pairs") d.writeTMSTDI(&tx, &tx, &rx, 8);
				else if (mock::fail == "speed") d.setClkFreq(1000000);
				else d.toggleClk(0, 0, 8);
			});
			const auto calls = mock::calls.size();
			mock::fail.clear();
			throws([&] { d.writeTDI(&tx, &rx, 8, false); });
			require(mock::calls.size() == calls); // no retry after unknown TAP state
		}
		require(mock::opens == mock::closes && mock::enables == mock::disables);
	}
	mock::reset();
	{
		DigilentAdept d("", "", -1, 6000000);
		mock::fail = "tdi"; mock::failAfter = 1;
		std::vector<uint8_t> data(1025, 0xa5);
		throws([&] { d.writeTDI(data.data(), nullptr, 8193, true); });
		require(mock::clocks.size() == 8192); // final exit clock fails after data was shifted
		throws([&] { d.toggleClk(0, 0, 1); });
	}
	require(mock::closes == 1 && mock::disables == 1);
	mock::reset();
	{
		DigilentAdept d("", "", -1, 6000000);
		mock::fail = "disable"; // close is still attempted when disable fails
	}
	require(mock::closes == 1);
}

static void validation()
{
	mock::reset();
	throws([] { DigilentAdept d("", "", -1, 0); });
	require(mock::opens == 0);
	mock::actualSpeed = UINT32_MAX;
	throws([] { DigilentAdept d("", "", -1, 6000000); });
	require(mock::opens == mock::closes);
	mock::actualSpeed = 3000000;
	DigilentAdept d("", "", -1, 6000000);
	require(d.getClkFreq() == 3000000);
	throws([&] { d.setClkFreq(0); });
	throws([&] { d.writeTMS(nullptr, 1, false); });
	throws([&] { d.writeTMSTDI(nullptr, nullptr, nullptr, 1); });
	throws([&] { d.writeTDI(nullptr, nullptr, uint32_t(INT_MAX) + 1, false); });
	require(d.flush() == 0); // argument validation doesn't invalidate the connection
}

int main()
{
	try {
		selection(); shifts(); tmsAndPairs(); failures(); validation();
		std::cout << "Adept mock tests passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
