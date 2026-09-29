/*
 * Copyright 2018 Paul Stoffregen
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <Arduino.h>
#include "MiniW5500.h"


// UNO Mini fixed-chip specialization. The SPI framing/transfer loops are
// retained from the project-verified Mode 3 driver; other chip probes are gone.
uint8_t W5100Class::chip = 0;
W5100Class W5100;

uint8_t W5100Class::init(void)
{
	static bool initialized = false;

	if (initialized) return 1;

	// Keep the proven power-on reset allowance for this physical module.
	delay(560);

	SPI.begin();
	initSS();
	resetSS();
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

	// Project hardware is W5500 only. Keep the original bounded reset and
	// register-identity checks; do not probe incompatible SPI protocols.
	if (!isW5500()) {
		chip = 0;
		SPI.endTransaction();
		return 0;
	}
	SPI.endTransaction();
	initialized = true;
	return 1; // successful init
}

// Soft reset the WIZnet chip, by writing to its MR register reset bit
uint8_t W5100Class::softReset(void)
{
	uint8_t count=0;

	// write to reset bit
	writeMR(0x80);
	// then wait for soft reset to complete
	do {
		uint8_t mr = readMR();
		if (mr == 0) return 1;
		delay(1);
	} while (++count < 20);
	return 0;
}

uint8_t W5100Class::isW5500(void)
{
	chip = 55;
	if (!softReset()) return 0;
	writeMR(0x08);
	if (readMR() != 0x08) return 0;
	writeMR(0x10);
	if (readMR() != 0x10) return 0;
	writeMR(0x00);
	if (readMR() != 0x00) return 0;
	uint8_t ver = readVERSIONR_W5500();
	if (ver != 4) return 0;
	return 1;
}

uint16_t W5100Class::write(uint16_t addr, const uint8_t *buf, uint16_t len)
{
	uint8_t cmd[8];

	{ // W5500 only
		setSS();
		if (addr < 0x100) {
			// common registers 00nn
			cmd[0] = 0;
			cmd[1] = addr & 0xFF;
			cmd[2] = 0x04;
		} else if (addr < 0x8000) {
			// socket registers  10nn, 11nn, 12nn, 13nn, etc
			cmd[0] = 0;
			cmd[1] = addr & 0xFF;
			cmd[2] = ((addr >> 3) & 0xE0) | 0x0C;
		} else if (addr < 0xC000) {
			// transmit buffers  8000-87FF, 8800-8FFF, 9000-97FF, etc
			//  10## #nnn nnnn nnnn
			cmd[0] = addr >> 8;
			cmd[1] = addr & 0xFF;
			cmd[2] = ((addr >> 6) & 0xE0) | 0x14; // 2K buffers
		} else {
			// receive buffers
			cmd[0] = addr >> 8;
			cmd[1] = addr & 0xFF;
			cmd[2] = ((addr >> 6) & 0xE0) | 0x1C; // 2K buffers
		}
		if (len <= 5) {
			for (uint8_t i=0; i < len; i++) {
				cmd[i + 3] = buf[i];
			}
			SPI.transfer(cmd, len + 3);
		} else {
			SPI.transfer(cmd, 3);
			for (uint16_t i=0; i < len; i++) {
				SPI.transfer(buf[i]);
			}
		}
		resetSS();
	}
	return len;
}

uint16_t W5100Class::read(uint16_t addr, uint8_t *buf, uint16_t len)
{
	uint8_t cmd[4];

	{ // W5500 only
		setSS();
		if (addr < 0x100) {
			// common registers 00nn
			cmd[0] = 0;
			cmd[1] = addr & 0xFF;
			cmd[2] = 0x00;
		} else if (addr < 0x8000) {
			// socket registers  10nn, 11nn, 12nn, 13nn, etc
			cmd[0] = 0;
			cmd[1] = addr & 0xFF;
			cmd[2] = ((addr >> 3) & 0xE0) | 0x08;
		} else if (addr < 0xC000) {
			// transmit buffers  8000-87FF, 8800-8FFF, 9000-97FF, etc
			//  10## #nnn nnnn nnnn
			cmd[0] = addr >> 8;
			cmd[1] = addr & 0xFF;
			cmd[2] = ((addr >> 6) & 0xE0) | 0x10; // 2K buffers
		} else {
			// receive buffers
			cmd[0] = addr >> 8;
			cmd[1] = addr & 0xFF;
			cmd[2] = ((addr >> 6) & 0xE0) | 0x18; // 2K buffers
		}
		SPI.transfer(cmd, 3);
		memset(buf, 0, len);
		SPI.transfer(buf, len);
		resetSS();
	}
	return len;
}
