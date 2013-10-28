/*
** Copyright (C) 2006 Exa
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/


// this code is modified from NeHe opengl lesson

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <SDL_image.h>

#include "sdld3d.h"

/* Number of stars */
#define NUM 20

// Define the star structure
typedef struct {
    float r, g, b;		// Color
    float dist;			// Distance From Center
    float start;		// Starting Distance from Center
    float angle;		// Current Angle
} star;


void draw_scene(SDL_Surface *surface) {
	int loop;
	SDL_Surface *screen = surface;
    static float spin = 0;
	static int frame;

	static int twinkle = 0; // Twinkling stars


	static star stars[NUM];			// Make an array of size 'NUM' of stars

	static float zoom = -15.0f;		// Viewing Distance Away From Stars
	static float tilt = 25.0f;		// Tilt The View

	static SDL_Surface *texture;	// Storage For One Texture
	static int first_time = 1;

	if(first_time) {
		first_time = 0;
		srand(time(0));
		D3D_Init();
		atexit(D3D_Quit);
		texture = IMG_Load("pics/star.png");

		// Create A Loop That Goes Through All The Stars
		for(loop = 0; loop < NUM; loop++) {
			// Start All The Stars At Angle Zero
			stars[loop].angle = 0.0f;

			// Calculate Distance From The Center
			stars[loop].dist = ((float)loop / NUM) * 5.0f;
			stars[loop].start = stars[loop].dist;

			// Give Star A Random Color
			stars[loop].r = (float)(rand() % 256)/255;
			stars[loop].g = (float)(rand() % 256)/255;
			stars[loop].b = (float)(rand() % 256)/255;
		}
	}

	D3D_SetScreen(screen);
	D3D_SetMapper(D3D_NEAREST);
	D3D_SetTexture(texture);

	D3D_Enable(D3D_BLENDING);
	D3D_Disable(D3D_LIGHTS|D3D_CULLING|D3D_ZTEST);


    /* Clear The Screen And The Depth Buffer */
	D3D_ClearScreen(0,0,0);
	D3D_ClearZBuffer();

	D3D_LoadIdentity();

	D3D_Translate(0, 0, 700*2);
	D3D_SetAmbient(0.0,0.0,0.0);
	D3D_Scale(50, 50, 50); // scale scene by screen size

	/* Loop Through All The Stars */
	for(loop = 0; loop < NUM; loop++) {
		D3D_Push(); // Save our position, so we can restore it After Drawing This Star

		D3D_Translate(0, 0, zoom); // Zoom Into The Screen (Using The Value In 'zoom')

		// rotate entire starfield
		D3D_Rotate(0, 0, (float)(frame)/4);
		D3D_Rotate(0, frame, 0);
		D3D_Rotate((float)frame/2, 0,0);

		// Tilt The View And Rotate To The Current Stars Angle
		D3D_Rotate(tilt, 0, 0);
		D3D_Rotate(0, stars[loop].angle, 0);

		// Move Forward On The X Plane
		D3D_Translate(stars[loop].dist, 0.0f, 0.0f);

		// Cancel All Star Rotations (it is sprite after all)
		D3D_Rotate(0, -stars[loop].angle, 0);
		D3D_Rotate(-tilt, 0, 0);

		D3D_Rotate(-(float)frame/2, 0, 0);
		D3D_Rotate(0, -frame, 0);
		D3D_Rotate(0, 0, -(float)(frame)/4);

		if(twinkle) { // Twinkling Stars Enabled
			// Assign A Color
			D3D_Color(stars[NUM-loop-1].r, stars[NUM-loop-1].g, stars[NUM-loop-1].b);

			// Begin Drawing The Textured Quad
			D3D_Begin(D3D_QUADS);
				D3D_TexCoord(0.0f, 0.0f); D3D_Vertex(-1.0f, -1.0f, 0.0f );
				D3D_TexCoord(1.0f, 0.0f); D3D_Vertex( 1.0f, -1.0f, 0.0f );
				D3D_TexCoord(1.0f, 1.0f); D3D_Vertex( 1.0f,  1.0f, 0.0f );
				D3D_TexCoord(0.0f, 1.0f); D3D_Vertex(-1.0f,  1.0f, 0.0f );
			D3D_End();
		}

		D3D_Rotate(0, 0, spin); // Rotate The Star On The Z Axis

		// this will make star's spawn/death less sudden
		float brightness = (stars[loop].start-stars[loop].dist)*stars[loop].dist;
		if(brightness>1) brightness = 1;


		// Draw Star Using Its Color
		D3D_Color4(stars[loop].r, stars[loop].g, stars[loop].b, brightness);
		D3D_Begin(D3D_QUADS);
		D3D_TexCoord(0, 0); D3D_Vertex(-1, -1, 0);
		D3D_TexCoord(1, 0); D3D_Vertex( 1, -1, 0);
		D3D_TexCoord(1, 1); D3D_Vertex( 1,  1, 0);
		D3D_TexCoord(0, 1); D3D_Vertex(-1,  1, 0);
		D3D_End();

		spin += 0.01f; // Used To Spin The Stars
		stars[loop].angle += (float)loop / NUM; // Changes The Angle Of A Star
		stars[loop].dist -= 0.01f; // Changes The Distance Of A Star

		if(stars[loop].dist < 0.0f) { // Is The Star In The Middle Yet
			stars[loop].dist += 5.0f; // Move The Star 5 Units From The Center
			stars[loop].start = stars[loop].dist;

			// Give It A New Color
			stars[loop].r = (float)(rand() % 256)/255;
			stars[loop].g = (float)(rand() % 256)/255;
			stars[loop].b = (float)(rand() % 256)/255;
		}
		D3D_Pop();
	}
	++frame;
}
