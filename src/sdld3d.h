/*
** Copyright (C) 2006 Exa
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/


/* these functions can be used to render some 3-d stuff onto SDL surfaces*/

#ifndef __SDLD3D_H_
#define __SDLD3D_H_

#include <SDL/SDL.h>

// primitive drawing modes
#define D3D_NOTHING				0
#define D3D_LINES				1
#define D3D_POINTS				2
#define D3D_TRIANGLES			3
#define D3D_TRIANGLE_STRIP		4
#define D3D_TRIANGLE_FAN		5
#define D3D_QUADS				6
#define D3D_QUAD_STRIP			7

// texture mapping modes
#define D3D_SOLID				1
#define D3D_NEAREST				2
#define D3D_LINEAR				3

// shading modes
#define D3D_AMBIENT				1
#define D3D_FLAT				2
#define D3D_GORAUD				3

// flags for D3D_Enable/D3D_Disable
#define D3D_ZTEST				0x01 /* do z-buffer test*/
#define D3D_LIGHTS				0x02 /* process scene lights */
#define D3D_BLENDING			0x04 /* blend transarent objects  */
#define D3D_CULLING				0x08 /* perform backspace culling */
#define D3D_AUTO_NORMALS		0x10 /* automatical calculate normal */

// support functions
int D3D_Init();
void D3D_Quit();

void D3D_Enable(int flags); // enables rendering features
void D3D_Disable(int flags); // disables them

void D3D_ClearScreen(float r, float g, float b); // clear screen with specific color
void D3D_ClearZBuffer();
void D3D_ClearLights(); // remove all lights from scene


// scene transformation functions
void D3D_Push();
void D3D_Pop();
void D3D_LoadIdentity();
void D3D_Scale(float x, float y, float z);
void D3D_Translate(float x, float y, float z);
void D3D_Rotate(float x, float y, float z);

// matrix manipulation rutines
void D3D_LoadIdentityM(float *m);
void D3D_ScaleM(float *m, float x, float y, float z);
void D3D_TranslateM(float *m, float x, float y, float z);
void D3D_RotateM(float *m, float x, float y, float z);
void D3D_MulMV(float *m, float *v, float *r);
void D3D_MulMM(float *a, float *b, float *r);

// 3-d drawing related functions
void D3D_Begin(int type);
void D3D_End();
void D3D_SetScreen(SDL_Surface *screen);
void D3D_SetTexture(SDL_Surface *texture);
void D3D_SetMapper(int m);
void D3D_SetShading(int s);
void D3D_SetAmbient(float r, float g, float b); // sets ambient glow


// following functions used to set next vertex parameters
// and should be called before their target D3D_Vertex()
void D3D_Color(float r, float g, float b); // sets color - alpha will be defaulted to one
void D3D_Color4(float r, float g, float b, float a); // same as above but with desired alpha
void D3D_Normal(float x, float y, float z);
void D3D_TexCoord(float u, float v);
void D3D_Light(float r, float g, float b); // add light to scene

void D3D_Vertex(float x, float y, float z);

//void D3D_Vertex(float x, float y, float z,
//	float r, float g, float b, float a, float u, float v);

//void D3D_VertexXYZUV(float x, float y, float z, float u, float v);


#endif
