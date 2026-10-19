// SPDX-License-Identifier: Apache-2.0
// Optional Windows FT232H transport. The OS driver is never changed.
#pragma once
#include <ftdi.h>

int d2xx_scan();
struct ftdi_context *d2xx_new();
void d2xx_free(struct ftdi_context *);
const char *d2xx_error(struct ftdi_context *);
int d2xx_interface(struct ftdi_context *, enum ftdi_interface);
int d2xx_open(struct ftdi_context *, int, int, const char *, const char *, unsigned int);
int d2xx_bus(struct ftdi_context *, unsigned int, unsigned int);
int d2xx_close(struct ftdi_context *);
int d2xx_reset(struct ftdi_context *);
int d2xx_baud(struct ftdi_context *, int);
int d2xx_bitmode(struct ftdi_context *, unsigned char, unsigned char);
int d2xx_latency(struct ftdi_context *, unsigned char);
int d2xx_read(struct ftdi_context *, unsigned char *, int);
int d2xx_write(struct ftdi_context *, const unsigned char *, int);
int d2xx_purge_rx(struct ftdi_context *);
int d2xx_purge_tx(struct ftdi_context *);
int d2xx_purge(struct ftdi_context *);
int d2xx_chunk(struct ftdi_context *, unsigned int);
int d2xx_character(struct ftdi_context *, unsigned char, unsigned char);
void d2xx_strings(struct ftdi_context *, unsigned char *, unsigned char *, unsigned char *);

// Limit the adaptation to translation units using FTDIpp_MPSSE.
#define ftdi_new d2xx_new
#define ftdi_free d2xx_free
#define ftdi_get_error_string d2xx_error
#define ftdi_set_interface d2xx_interface
#define ftdi_usb_open_desc_index d2xx_open
#define ftdi_usb_open_bus_addr d2xx_bus
#define ftdi_usb_close d2xx_close
#define ftdi_usb_reset d2xx_reset
#define ftdi_set_baudrate d2xx_baud
#define ftdi_set_bitmode d2xx_bitmode
#define ftdi_set_latency_timer d2xx_latency
#define ftdi_read_data d2xx_read
#define ftdi_write_data d2xx_write
#define ftdi_tciflush d2xx_purge_rx
#define ftdi_tcoflush d2xx_purge_tx
#define ftdi_tcioflush d2xx_purge
#define ftdi_read_data_set_chunksize d2xx_chunk
#define ftdi_write_data_set_chunksize d2xx_chunk
#define ftdi_set_event_char d2xx_character
#define ftdi_set_error_char d2xx_character

