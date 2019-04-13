#include "gbcc.h"
#include "apu.h"
#include "bit_utils.h"
#include "cpu.h"
#include "debug.h"
#include "hdma.h"
#include "memory.h"
#include "ops.h"
#include "ppu.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

static void clock_div(struct gbc *gbc);
static void check_interrupts(struct gbc *gbc);
static void cpu_clock(struct gbc *gbc);

/* TODO: Check order of all of these */
void gbcc_emulate_cycle(struct gbc *gbc)
{
	check_interrupts(gbc);
	gbcc_apu_clock(gbc);
	gbcc_ppu_clock(gbc);
	cpu_clock(gbc);
	clock_div(gbc);
	if (gbc->double_speed) {
		cpu_clock(gbc);
		clock_div(gbc);
	}
}

void cpu_clock(struct gbc *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	/* CPU clocks every 4 cycles */
	cpu->clock++;
	cpu->clock &= 3u;
	if (cpu->clock != 0) {
		return;
	}
	if (cpu->ime_timer.timer > 0) {
		cpu->ime_timer.timer--;
		if (cpu->ime_timer.timer == 1) {
			cpu->ime = cpu->ime_timer.target_state;
		}
	}
	if (!cpu->halt.set && !gbc->stop) {
		if (cpu->dma.timer > 0) {
			cpu->dma.running = true;
			gbc->memory.oam[low_byte(cpu->dma.source)] = gbcc_memory_read(gbc, cpu->dma.source, false);
			cpu->dma.timer--;
			cpu->dma.source++;
		} else {
			cpu->dma.running = false;
		}
		if (cpu->dma.requested) {
			cpu->dma.requested = false;
			cpu->dma.timer = DMA_TIMER;
			cpu->dma.source = cpu->dma.new_source;
		}
	}
	if (!cpu->instruction.running) {
		if (cpu->ime && cpu->interrupt.request) {
			INTERRUPT(gbc);
			return;
		}
		if (cpu->halt.set || gbc->stop) {
			return;
		}
		//printf("%04X\n", cpu->reg.pc);
		cpu->opcode = gbcc_fetch_instruction(gbc);
		//gbcc_print_op(gbc);
		cpu->instruction.running = true;
	}
	if (cpu->instruction.prefix_cb) {
		gbcc_ops[0xCB](gbc);
	} else {
		gbcc_ops[cpu->opcode](gbc);
	}
}

void clock_div(struct gbc *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	//printf("Div clock = %04X\n", cpu->div_timer);
	cpu->div_timer++;
	uint8_t tac = gbcc_memory_read(gbc, TAC, false);
	uint16_t mask;
	switch (tac & 0x03u) {
		/* 
		 * TIMA register detects the falling edge of a bit in
		 * the internal DIV timer, which is selected by TAC.
		 */
		case 0:
			mask = bit16(9);
			break;
		case 1:
			mask = bit16(3);
			break;
		case 2:
			mask = bit16(5);
			break;
		case 3:
			mask = bit16(7);
			break;
	}
	/* If TAC is disabled, this will always see 0 */
	mask *= check_bit(tac, 2);
	if (!(cpu->div_timer & mask) && cpu->tac_bit) {
		/* 
		 * The selected bit was previously high, and is now low, so
		 * the tima increment logic triggers.
		 */
		uint8_t tima = gbcc_memory_read(gbc, TIMA, false);
		tima++;
		if (tima == 0) {
			/* 
			 * TIMA overflow
			 * Rather than being reloaded immediately, TIMA takes
			 * 4 cycles to be reloaded, and another 4 to write, 
			 * so we just queue it here. This also affects the
			 * interrupt.
			 */
			cpu->tima_reload = 8;
		}
		gbcc_memory_write(gbc, TIMA, tima, false);
	}
	cpu->tac_bit = cpu->div_timer & mask;
	if (cpu->tima_reload > 0) {
		cpu->tima_reload--;
		if (cpu->tima_reload == 4) {
			/*
			 * Some more weird behaviour here: if TIMA has been
			 * written to while this copy & interrupt are waiting,
			 * they get cancelled, and everything proceeds as
			 * normal.
			 */
			if (gbcc_memory_read(gbc, TIMA, false) != 0) {
				cpu->tima_reload = 0;
			} else {
				gbcc_memory_copy(gbc, TMA, TIMA, false);
				gbcc_memory_set_bit(gbc, IF, 2, false);
			}
		}
		if (cpu->tima_reload == 0) {
			gbcc_memory_copy(gbc, TMA, TIMA, false);
		}
	}
	if (!gbc->apu.disabled) {
		/* APU also updates based on falling edge of DIV timer bit */
		if (gbc->double_speed) {
			mask = bit16(13);
		} else {
			mask = bit16(12);
		}
		if (!(cpu->div_timer & mask) && gbc->apu.div_bit) {
			gbcc_apu_sequencer_clock(gbc);
		}
		gbc->apu.div_bit = cpu->div_timer & mask;
	}
}

void check_interrupts(struct gbc *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t iereg = gbcc_memory_read(gbc, IE, false);
	uint8_t ifreg = gbcc_memory_read(gbc, IF, false);
	uint8_t interrupt = (uint8_t)(iereg & ifreg) & 0x1Fu;
	if (interrupt) {
		cpu->halt.set = false;
		gbc->stop = false;
		if (cpu->ime) {
			cpu->interrupt.request = true;
		}
	}
}

uint8_t gbcc_fetch_instruction(struct gbc *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->halt.skip) {
		/* HALT bug; CPU fails to increment pc */
		cpu->halt.skip = false;
		return gbcc_memory_read(gbc, cpu->reg.pc, false);
	} 
	return gbcc_memory_read(gbc, cpu->reg.pc++, false);
}