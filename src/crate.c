/*
** Copyright (C) 2006 Exa
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <SDL_image.h>

#include "sdld3d.h"

void draw_quad() {
	D3D_Color4(0, 0, 0, 1); 
	D3D_Push();
	D3D_Begin(D3D_TRIANGLE_STRIP);
	D3D_TexCoord(0, 1), D3D_Vertex(-1,  1, 0);
	D3D_TexCoord(0, 0), D3D_Vertex(-1, -1, 0);
	D3D_TexCoord(1, 1), D3D_Vertex( 1,  1, 0);
	D3D_TexCoord(1, 0), D3D_Vertex( 1, -1, 0);
	D3D_End();
	D3D_Pop();
}

void draw_cube() {
	D3D_Push();
	D3D_Color4(0, 0, 0, 1); 
	D3D_Begin(D3D_QUAD_STRIP);
		D3D_TexCoord(0, 0); D3D_Vertex(-1, -1, -1);
		D3D_TexCoord(0, 1); D3D_Vertex(-1,  1, -1);
		D3D_TexCoord(1, 0); D3D_Vertex( 1, -1, -1);
		D3D_TexCoord(1, 1); D3D_Vertex( 1,  1, -1);
		D3D_TexCoord(0, 0); D3D_Vertex( 1, -1,  1);
		D3D_TexCoord(0, 1); D3D_Vertex( 1,  1,  1);
		D3D_TexCoord(1, 0); D3D_Vertex(-1, -1,  1);
		D3D_TexCoord(1, 1); D3D_Vertex(-1,  1,  1);
		D3D_TexCoord(0, 0); D3D_Vertex(-1, -1, -1);
		D3D_TexCoord(0, 1); D3D_Vertex(-1,  1, -1);
	D3D_End();
	D3D_Begin(D3D_QUADS);
		D3D_TexCoord(1, 0); D3D_Vertex( 1,  1, -1);
		D3D_TexCoord(0, 0); D3D_Vertex(-1,  1, -1);
		D3D_TexCoord(0, 1); D3D_Vertex(-1,  1,  1);
		D3D_TexCoord(1, 1); D3D_Vertex( 1,  1,  1);

		D3D_TexCoord(1, 0); D3D_Vertex( 1, -1, -1);
		D3D_TexCoord(0, 0); D3D_Vertex(-1, -1, -1);
		D3D_TexCoord(0, 1); D3D_Vertex(-1, -1,  1);
		D3D_TexCoord(1, 1); D3D_Vertex( 1, -1,  1);
	D3D_End();
	D3D_Pop();
}

float frand() {
	return ((float)rand()/RAND_MAX);
}


#define NLIGTHS 1

float lights[NLIGTHS][3];

SDL_Surface *screen;


#define FPS 60 // number of frames per second to render

extern char keys[];
int milsec = 1000 / FPS; // number of milisecond in one frame
SDL_Surface *tex_crate,  *tex_light;

void add_light(float x, float y, float z) {
	D3D_Push();
		D3D_Translate(x, y, z);

		D3D_Light(1, 1, 1);

		D3D_Scale(0.5, 0.5, 0.5);
		D3D_Disable(D3D_CULLING|D3D_LIGHTS);
		D3D_Enable(D3D_BLENDING);
		D3D_SetTexture(tex_light);
		draw_quad();
	D3D_Pop();
}


void draw_scene(SDL_Surface *surface) {
	int i;
	static int frame;
	static int start_ticks;
	static int first_time = 1;

	screen = surface;



	if(first_time) {
		D3D_Init();
		srand(time(0));
		tex_crate = IMG_Load("pics/crate.jpg");
		tex_crate = SDL_DisplayFormatAlpha(tex_crate);
		tex_light = IMG_Load("pics/star.png");

		for(i = 0; i < NLIGTHS; ++i) {
			lights[i][0] = 10*(1+0.5-frand());
			lights[i][1] = 0;
			lights[i][2] = 0;
		}
		start_ticks = SDL_GetTicks();
		first_time = 0;
	}

	D3D_SetScreen(screen);
	D3D_ClearScreen(0,0,0);
	D3D_ClearZBuffer();
	D3D_ClearLights();

	float ticks = (float)(SDL_GetTicks() - start_ticks)/((float)1000/16);

	D3D_LoadIdentity();

	// move scene root to center of screen

	// scale scene
	D3D_Scale(50, 50, 50);
	D3D_Translate(0, 0, 8);

	D3D_Push();
	D3D_Rotate(0, ticks, 0);
	for(i = 0; i < NLIGTHS; ++i) {
		D3D_Push();
		D3D_Translate(lights[i][0], lights[i][1], lights[i][2]);
		D3D_Light(1,1,1);
		D3D_Pop();
	}
	D3D_Pop();

	D3D_Push();
	//D3D_Translate(tx, ty, tz);
	//D3D_Scale(1, 1, 1);
	D3D_Rotate(-30, ticks, 0);


	D3D_Disable(D3D_BLENDING);
	D3D_Enable(D3D_LIGHTS|D3D_CULLING|D3D_ZTEST);
	D3D_SetAmbient(0.1, 0.1, 0.1); // make ambient half-dark
	D3D_SetTexture(tex_crate);
	draw_cube();
	D3D_Pop();

	D3D_Push();
	D3D_Rotate(0, ticks, 0);
	for(i = 0; i < NLIGTHS; ++i) {
		D3D_Push();
		D3D_Translate(lights[i][0], lights[i][1], lights[i][2]);
		D3D_Rotate(0, -ticks, 0);
		D3D_Scale(0.5, 0.5, 0.5);
		D3D_Disable(D3D_CULLING|D3D_LIGHTS);
		D3D_Enable(D3D_BLENDING|D3D_ZTEST);
		D3D_SetAmbient(1.0, 1.0, 1.0);
		D3D_SetTexture(tex_light);
		draw_quad();
		D3D_Pop();
	}
	D3D_Pop();

	frame++;
}
