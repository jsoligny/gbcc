#ifndef GBCC_H
#define GBCC_H

#include "apu.h"
#include "constants.h"
#include "cpu.h"
#include "ppu.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

struct gbc {
	/* CPU */
	struct cpu cpu;
	
	/* APU */
	struct apu apu;

	/* PPU */
	struct ppu ppu;

	/* Non-Register state data */
	enum CART_MODE mode;
	struct {
		uint16_t source;
		uint16_t dest;
		uint16_t length;
		bool hblank;
	} hdma;
	bool stop;
	struct {
		struct timespec current;
		struct timespec old;
	} real_time;
	bool quit;
	bool pause;
	bool interlace;
	int8_t save_state;
	int8_t load_state;
	bool double_speed;
	float turbo_speed;

	/* Memory map */
	struct {
		/* GBC areas */
		uint8_t *rom0;	/* Non-switchable ROM */
		uint8_t *romx;	/* Switchable ROM */
		uint8_t *vram;	/* VRAM (switchable in GBC mode) */
		uint8_t *sram;	/* Cartridge RAM */
		uint8_t *wram0;	/* Non-switchable Work RAM */
		uint8_t *wramx;	/* Work RAM (switchable in GBC mode) */
		uint8_t *echo;	/* Mirror of WRAM */
		uint8_t oam[OAM_SIZE];	/* Object Attribute Table */
		uint8_t unused[UNUSED_SIZE];	/* Complicated unused memory */
		uint8_t ioreg[IOREG_SIZE];	/* I/O Registers */
		uint8_t hram[HRAM_SIZE];	/* Internal CPU RAM */
		uint8_t iereg;	/* Interrupt enable flags */
		/* Emulator areas */
		uint8_t wram_bank[8][WRAM0_SIZE];	/* Actual location of WRAM */
		uint8_t vram_bank[2][VRAM_SIZE]; 	/* Actual location of VRAM */
	} memory;

	/* Current key states */
	struct {
		bool a;
		bool b;
		bool start;
		bool select;
		struct {
			bool up;
			bool down;
			bool left;
			bool right;
		} dpad;
		bool turbo;
	} keys;

	/* Cartridge data & flags */
	struct {
		const char *filename;
		uint8_t *rom;
		size_t rom_size;
		size_t rom_banks;
		uint8_t *ram;
		size_t ram_size;
		size_t ram_banks;
		bool battery;
		bool timer;
		bool rumble;
		struct gbcc_mbc {
			enum MBC type;
			bool sram_enable;
			uint8_t rom0_bank;
			uint16_t romx_bank;
			uint8_t sram_bank;
			uint8_t ramg;
			uint8_t romb0;
			uint8_t romb1;
			uint8_t ramb;
			bool mode;
			struct gbcc_rtc {
				uint8_t seconds;
				uint8_t minutes;
				uint8_t hours;
				uint8_t day_low;
				uint8_t day_high;
				uint8_t latch;
				uint8_t cur_reg;
				struct timespec base_time;
				bool mapped;
			} rtc;
		} mbc;
		char title[CART_TITLE_SIZE + 1];
	} cart;
};

void gbcc_initialise(struct gbc *gbc, const char *filename);
void gbcc_free(struct gbc *gbc);

#endif /* GBCC_H */
