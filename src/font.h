#ifndef FONT_H
#define FONT_H

#include <SDL.h>
#include <SDL_image.h>

typedef struct {
	SDL_Surface *s;
	int cell_w, cell_h;
	Uint8 widths[256];
} BMFont;

BMFont *font_create(SDL_Surface *s, int cell_w, int cell_h, int r, int g, int b, int a);
void font_draw(SDL_Surface *s, BMFont *f, int x, int y, char *text);
int font_draw_width(SDL_Surface *s, BMFont *f, int x, int y, char *text);
void font_free(BMFont *f);

#endif
