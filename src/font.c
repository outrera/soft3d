#include <SDL.h>
#include <SDL_image.h>

#include "font.h"

BMFont *font_create(SDL_Surface *s, int cell_w, int cell_h, int r, int g, int b, int a) {
	int i, j;

	BMFont *f = (BMFont *)malloc(sizeof(BMFont));

	f->cell_w = cell_w;
	f->cell_h = cell_h;
	Uint32 rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif

	SDL_Surface *t = SDL_CreateRGBSurface(SDL_SRCALPHA|SDL_HWSURFACE,
		s->w, s->h, 32, rmask, gmask, bmask, amask);

	for(i = 0; i < s->h; ++i) {
		Uint8  *ps = s->pixels + s->pitch*i;
		Uint32 *pt = t->pixels + t->pitch*i;
		for(j = 0; j < s->w; ++j) {
			pt[j] = SDL_MapRGBA(t->format, r, g, b, ps[j]);
		}
	}

	for(i = 0; i < 256; ++i) f->widths[i] = f->cell_w;

	f->s = SDL_DisplayFormatAlpha(t);
	SDL_FreeSurface(t);

	return f;
}

void font_draw(SDL_Surface *s, BMFont *f, int x, int y, char *text) {
	int c;
	int cur_x = x;

	while((c = (Uint8)*text++)) {
		if(c == '\n') {
			y += f->cell_h;
			cur_x = x;
			continue;
		}

		SDL_Rect dst;
		dst.x = x;
 		dst.y = y;
		SDL_Rect src;
		src.x = f->cell_w*(c%16);
		src.y = f->cell_h*(c/16);
		src.w = f->widths[c];
		src.h = f->cell_h;

		SDL_BlitSurface(f->s, &src, s, &dst);

		x += f->widths[c];
	}
}


void font_free(BMFont *f) {
	SDL_FreeSurface(f->s);
	free(f);
}
