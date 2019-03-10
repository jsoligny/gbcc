#include "gbcc.h"
#include "bit_utils.h"
#include "debug.h"
#include "input.h"
#include "memory.h"
#include "vram_window.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define VRAM_WINDOW_WIDTH_TILES (VRAM_WINDOW_WIDTH / 8)
#define VRAM_WINDOW_HEIGHT_TILES (VRAM_WINDOW_HEIGHT / 8)

void gbcc_vram_window_initialise(struct gbcc_vram_window *win, struct gbc *gbc)
{
	win->gbc = gbc;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		gbcc_log_error("Failed to initialize SDL: %s\n", SDL_GetError());
	}
	
	win->window = SDL_CreateWindow(
			"GBCC VRAM data",          // window title
			SDL_WINDOWPOS_UNDEFINED,   // initial x position
			SDL_WINDOWPOS_UNDEFINED,   // initial y position
			VRAM_WINDOW_WIDTH,      // width, in pixels
			VRAM_WINDOW_HEIGHT,     // height, in pixels
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE // flags - see below
			);

	if (win->window == NULL) {
		gbcc_log_error("Could not create window: %s\n", SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	win->renderer = SDL_CreateRenderer(
			win->window,
			-1,
			SDL_RENDERER_ACCELERATED
			);

	if (win->renderer == NULL) {
		gbcc_log_error("Could not create renderer: %s\n", SDL_GetError());
		SDL_DestroyWindow(win->window);
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	win->texture = SDL_CreateTexture(
			win->renderer,
			SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STREAMING,
			VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT
			);

	if (win->texture == NULL) {
		gbcc_log_error("Could not create texture: %s\n", SDL_GetError());
		SDL_DestroyRenderer(win->renderer);
		SDL_DestroyWindow(win->window);
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	SDL_RenderSetLogicalSize(
			win->renderer,
			VRAM_WINDOW_WIDTH, VRAM_WINDOW_HEIGHT
			);
	SDL_RenderSetIntegerScale(win->renderer, true);

	for (size_t i = 0; i < VRAM_WINDOW_SIZE; i++) {
		win->buffer[i] = 0;
	}
}

void gbcc_vram_window_destroy(struct gbcc_vram_window *win)
{
	SDL_DestroyTexture(win->texture);
	SDL_DestroyRenderer(win->renderer);
	SDL_DestroyWindow(win->window);
}

void gbcc_vram_window_update(struct gbcc_vram_window *win)
{
	int err;
	/* Do the actual drawing */
	for (int bank = 0; bank < 2; bank++) {
		for (int j = 0; j < VRAM_WINDOW_HEIGHT_TILES / 2; j++) {
			for (int i = 0; i < VRAM_WINDOW_WIDTH_TILES; i++) {
				for (int y = 0; y < 8; y++) {
					uint8_t lo = win->gbc->memory.vram_bank[bank][16 * (j * VRAM_WINDOW_WIDTH_TILES + i) + 2*y];
					uint8_t hi = win->gbc->memory.vram_bank[bank][16 * (j * VRAM_WINDOW_WIDTH_TILES + i) + 2*y + 1];
					for (uint8_t x = 0; x < 8; x++) {
						uint8_t colour = (uint8_t)(check_bit(hi, 7 - x) << 1u) | check_bit(lo, 7 - x);
						uint32_t p = 0;
						switch (colour) {
							case 3:
								p = 0x000000u;
								break;
							case 2:
								p = 0x6b7353u;
								break;
							case 1:
								p = 0x8b956du;
								break;
							case 0:
								p = 0xc4cfa1u;
								break;
						}
						win->buffer[8 * (VRAM_WINDOW_WIDTH * (j + bank * VRAM_WINDOW_HEIGHT_TILES / 2) + i) + VRAM_WINDOW_WIDTH * y + x] = p;
					}
				}
			}
		}
	}
	/* Draw the background */
	err = SDL_UpdateTexture(
			win->texture, 
			NULL, 
			win->buffer, 
			VRAM_WINDOW_WIDTH * sizeof(win->buffer[0])
			);
	if (err < 0) {
		gbcc_log_error("Error updating texture: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	err = SDL_RenderClear(win->renderer);
	if (err < 0) {
		gbcc_log_error("Error clearing renderer: %s\n", SDL_GetError());
		if (win->renderer == NULL) {
			printf("NULL Renderer!\n");
		}
		exit(EXIT_FAILURE);
	}

	err = SDL_RenderCopy(win->renderer, win->texture, NULL, NULL);
	if (err < 0) {
		gbcc_log_error("Error copying texture: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_RenderPresent(win->renderer);
}
