/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 annoyatron255, No0ne (https://github.com/No0ne)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"

atomic_uint_fast8_t scan_matrix[13] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0x20,
	0x41
};

uint8_t kb_addr = 0;
uint8_t kb_inst = 0;
uint8_t kb_leds = 0;

uint8_t prev_rpt[] = {0, 0, 0, 0, 0, 0, 0, 0};

void kb_reset() {
	// TODO
}

void kb_send_key(uint8_t key, bool state, uint8_t modifiers) {
	// TODO
}

void tuh_kb_set_leds(uint8_t leds) {
	if (kb_addr) {
		kb_leds = leds;
		printf("HID device address = %d, instance = %d, LEDs = %d\n", kb_addr, kb_inst, kb_leds);
		tuh_hid_set_report(kb_addr, kb_inst, 0, HID_REPORT_TYPE_OUTPUT, &kb_leds, sizeof(kb_leds));
	}
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
	printf("HID device address = %d, instance = %d is mounted", dev_addr, instance);

	if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
		printf(" - keyboard");

		if (!kb_addr) {
			printf(", primary");
			kb_addr = dev_addr;
			kb_inst = instance;
			kb_reset();
		}

		tuh_hid_receive_report(dev_addr, instance);
	}

	printf("\n");
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
	printf("HID device address = %d, instance = %d is unmounted", dev_addr, instance);

	if (dev_addr == kb_addr && instance == kb_inst) {
		printf(" - keyboard, primary");
		kb_addr = 0;
		kb_inst = 0;
	}

	printf("\n");
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
	if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD && report[1] == 0) {
		if (report[0] != prev_rpt[0]) {
			uint8_t rbits = report[0];
			uint8_t pbits = prev_rpt[0];

			for (uint8_t j = 0; j < 8; j++) {
				if ((rbits & 1) != (pbits & 1)) {
					kb_send_key(HID_KEY_CONTROL_LEFT + j, rbits & 1, report[0]);
				}

				rbits = rbits >> 1;
				pbits = pbits >> 1;
			}
		}

		for (uint8_t i = 2; i < 8; i++) {
			if (prev_rpt[i]) {
				bool brk = true;

				for (uint8_t j = 2; j < 8; j++) {
					if(prev_rpt[i] == report[j]) {
						brk = false;
						break;
					}
				}

				if (brk) {
					kb_send_key(prev_rpt[i], false, report[0]);
				}
			}

			if (report[i]) {
				bool make = true;

				for (uint8_t j = 2; j < 8; j++) {
					if(report[i] == prev_rpt[j]) {
						make = false;
						break;
					}
				}

				if (make) {
					kb_send_key(report[i], true, report[0]);
				}
			}
		}

		memcpy(prev_rpt, report, sizeof(prev_rpt));

	}

	tuh_hid_receive_report(dev_addr, instance);
}

void bitbang_wy60() {
	// Data out pin
	gpio_init(GPIO_DATA_OUT);
	gpio_set_dir(GPIO_DATA_OUT, GPIO_OUT);
	gpio_put(GPIO_DATA_OUT, 1);

	// Clock pin
	gpio_init(GPIO_CLOCK_IN);
	gpio_set_dir(GPIO_CLOCK_IN, GPIO_IN);

	bool clk_prev = gpio_get(GPIO_CLOCK_IN);
	absolute_time_t prev_high_time = 0;
	int bit_index = 0;
	while (1) {
		// Sample
		bool clk_now = gpio_get(GPIO_CLOCK_IN);
		absolute_time_t now = get_absolute_time();

		// Reset sequence if clock is low for 36 us or more
		if (!clk_now && absolute_time_diff_us(prev_high_time, now) > 36) {
			bit_index = 0;
		}
		if (clk_now) {
			prev_high_time = now;
		}
		// Falling edge
		if (clk_prev && !clk_now) {
			// Set data
			if (bit_index == 85) {
				gpio_put(GPIO_DATA_OUT, 0);
			} else {
				gpio_put(GPIO_DATA_OUT, 1);
			}

			// Increment index
			bit_index++;
			if (bit_index > 8 * sizeof(scan_matrix) / sizeof(scan_matrix[0])) {
				bit_index = 0;
			}
		}

		clk_prev = clk_now;
	}
}

int main() {
	board_init();
	tuh_init(BOARD_TUH_RHPORT);

	// Setup LED
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	gpio_put(PICO_DEFAULT_LED_PIN, 0);

	printf("%s-%s\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);

	/// \tag::setup_multicore[]
	multicore_launch_core1(bitbang_wy60);

	while (1) {
		tuh_task();
	}
}
