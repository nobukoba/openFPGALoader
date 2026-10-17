// SPDX-License-Identifier: Apache-2.0
#ifndef SRC_DIGILENTADEPT_HPP_
#define SRC_DIGILENTADEPT_HPP_

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <dpcdecl.h>
#include <string>
#include "jtagInterface.hpp"

// Optional Adept SDK backend. No libusb/libftdi handle is used here.
class DigilentAdept final : public JtagInterface {
 public:
	DigilentAdept(const std::string &device, const std::string &serial,
		int index, uint32_t frequency);
	~DigilentAdept() override;
	DigilentAdept(const DigilentAdept &) = delete;
	DigilentAdept &operator=(const DigilentAdept &) = delete;
	int setClkFreq(uint32_t frequency) override;
	int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		const uint8_t tdi = 1) override;
	int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	bool writeTMSTDI(const uint8_t *tms, const uint8_t *tdi,
		uint8_t *tdo, uint32_t len) override;
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t len) override;
	int get_buffer_size() override { return 1024; }
	bool isFull() override { return false; }
	int flush() override;

 private:
	HIF _handle = hifInvalid;
	bool _enabled = false;
	bool _failed = false;
	void check(BOOL result, const char *operation);
	void ready() const;
	void close() noexcept;
	uint32_t chunkBits() const;
	static void validateLength(uint32_t len);
	static std::string selectDevice(const std::string &serial, int index);
};
#endif
