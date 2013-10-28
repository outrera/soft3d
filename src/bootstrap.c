/*
** Copyright (C) 2006 Exa
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/



// SDL bootstrap code

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <SDL/SDL.h>

#include "font.h"

extern void draw_scene(SDL_Surface *screen);


BMFont *font;
SDL_Surface *screen;

char keys[(1<<16)-1];


// next one is used for printing our status
void print(char *format, ...) {
	char string[1024];

	va_list ap;
	va_start(ap, format);
	vsprintf(string, format, ap);
	va_end(ap);

	font_draw(screen, font, 5, 5, string);
}

void lock_surface(SDL_Surface *surface) {
	if(SDL_MUSTLOCK(surface))
		assert(SDL_LockSurface(surface) >= 0);
}

void unlock_surface(SDL_Surface *surface) {
	if(SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void resize(int w, int h) {
	assert(screen = SDL_SetVideoMode(w, h, 32,
		SDL_ANYFORMAT|SDL_HWSURFACE/*|SDL_DOUBLEBUF*/|SDL_VIDEORESIZE));
}

int main(int argc, char **argv) {
	int T0 = 0, done = 0, frames = 0;
	float fps = 0;

	assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) >= 0);

	resize(640, 480);

	font = font_create(IMG_Load("pics/font.png"), 16, 16, 0xff, 0xff, 0xff, 0xff);

	while(!done) { // main loop
		SDL_Event event;

		while(SDL_PollEvent(&event)) { // read system events
			switch(event.type) {
				case SDL_VIDEORESIZE:
					resize(event.resize.w, event.resize.h);
				break;
				case SDL_QUIT:
					done = 1;
				break;
				case SDL_KEYDOWN:
					if(event.key.keysym.sym == SDLK_ESCAPE) done = 1;
					keys[event.key.keysym.sym] = 1;
				break;
				case SDL_KEYUP:
					keys[event.key.keysym.sym] = 0;
				break;
			}
		}

		lock_surface(screen);
		draw_scene(screen);
		print("FPS:%.0f\n", fps);
		unlock_surface(screen);
		SDL_Flip(screen);

		frames++;
		int t = SDL_GetTicks();
		if(t - T0 >= 1000) {
		    float seconds = (t - T0) / 1000.0;
		    fps = frames / seconds;;
		    T0 = t;
		    frames = 0;
		}
	}

	SDL_Quit();
	return 0;
}

