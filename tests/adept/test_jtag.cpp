// SPDX-License-Identifier: Apache-2.0
#include "jtag.hpp"
#include "mock_adept.hpp"
#include <iostream>
#include <stdexcept>

static void require(bool condition) { if (!condition) throw std::runtime_error("JTAG test assertion failed"); }
int main()
{
	try {
		// Detection after a successful cable open must not leak the SDK handle.
		mock::reset(); mock::fail = "tdi";
		bool failed = false;
		try {
			Jtag j(cable_list.at("digilent_adept"), nullptr, "", "", 6000000,
				0, "", 0, false, "", {});
		} catch (const std::exception &) { failed = true; }
		require(failed && mock::opens == 1 && mock::closes == 1 && mock::disables == 1);

		// Known Kintex-7 IDCODE followed by end-of-chain, through the real Jtag class.
		mock::reset();
		for (uint32_t word : {0x0364c093u, 0xffffffffu})
			for (unsigned bit = 0; bit < 32; ++bit) mock::scanData.push_back((word >> bit) & 1);
		{
			Jtag j(cable_list.at("digilent_adept"), nullptr, "", "", 6000000,
				0, "", 0, false, "", {});
			const auto devices = j.get_devices_list();
			require(devices.size() == 1 && devices.front() == 0x0364c093u);
		}
		require(mock::opens == 1 && mock::closes == 1 && mock::disables == 1);
		std::cout << "Adept Jtag integration tests passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n'; return 1;
	}
}
