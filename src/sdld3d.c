/*
** Copyright (C) 2006 Exa
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/


#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "sdld3d.h"

typedef struct {
	float x, y, z, r, g, b;
} Light;

#define EPS 0.000001
#define FEQ(a,b) ((a)-EPS < (b) && (b) < (a)+EPS)


// num of interpolants
#define IPLS 12

typedef struct {
	float i[IPLS]; // z, u, v, r, g, b, n[x,y,z], a, x, y
} Vertex;

#define MODULE(x) ((x) >= 0 ? (x) : -(x))
#define ABS(x) ((MODULE(x) > 0.000001) ? MODULE(x) : 1)
#define FLT_EQ(x, y) ((x)-0.000001 < y && (x)+0.000001 > (y))

#define MIN(a,b) (((a) > (b)) ? (b) : (a))
#define MAX(a,b) (((a) < (b)) ? (b) : (a))

// max vertex_buffer available to render between D3D_Begin and D3D_End
#define MAX_VERTICES		100000
#define MAX_FACES			(MAX_VERTICES-2)
#define MAX_MATRICES		100
#define MAX_LIGHTS			1000

#define PI (float)(3.1415926535897932384626)
#define PI2 (float)(2*PI)

// following macros help us to make iterpolant-member
// access more illustrative
#define U(l) ((l) -> i[0])
#define V(l) ((l) -> i[1])
#define R(l) ((l) -> i[2])
#define G(l) ((l) -> i[3])
#define B(l) ((l) -> i[4])
#define A(l) ((l) -> i[5])
#define NX(l) ((l) -> i[6])
#define NY(l) ((l) -> i[7])
#define NZ(l) ((l) -> i[8])
#define Z(l) ((l) -> i[9])
#define X(l) ((l) -> i[IPLS-2])
#define Y(l) ((l) -> i[IPLS-1])

#define UD(l) ((l) -> s[0])
#define VD(l) ((l) -> s[1])
#define RD(l) ((l) -> s[2])
#define GD(l) ((l) -> s[3])
#define BD(l) ((l) -> s[4])
#define AD(l) ((l) -> s[5])
#define NXD(l) ((l) -> s[6])
#define NYD(l) ((l) -> s[7])
#define NZD(l) ((l) -> s[8])
#define ZD(l) ((l) -> s[9])
#define XD(l) ((l) -> s[IPLS-2])
#define YD(l) ((l) -> s[IPLS-1])

// next structure is for linear interpolation of components across triangle
typedef struct lerp {
	float i[IPLS-1];	// interpolants
	float s[IPLS-1];	// steps - one for each interpolant
} lerp;

typedef struct {
	Vertex *a, *b, *c;
	float nx, ny, nz;	// normal
} Face;

static SDL_Surface *screen;		// target frame-buffer
static SDL_Surface *texture;	// current texture
// NOTE: current texture should not change between begin and end

// Auto-normal generation methods
#define D3D_FACET	1
#define D3D_CENTER	2

static float rR, rG, rB, rA;	// current color
static float rNX, rNY, rNZ;		// current normal
static float rU, rV;			// current texture coords

static float *zbuffer;	// x-buffer (aka 1/x-buffer or w-buffer)

static int total_lights; // total lights currently in scene
static Light light_buffer[MAX_LIGHTS];

static int total_vertices;	// total vertices between D3D_Begin and D3D_End

static Vertex vertex_buffer[MAX_VERTICES];
static Face face_buffer[MAX_FACES];

static int current_matrix;	// top matrix in stack
static float matrix_stack[MAX_MATRICES][16];	// matrix stack
static float *tmatrix = matrix_stack[0];		// transformation matrix

// ambient glow
static float ambient_r, ambient_g, ambient_b;

static int ntriangles;	// number of triangles rendered so far
static int draw_type;	// type of drawing - D3D_LINES, D3D_TRIANGLES, etc...
static float near_clip = 100.0f; // aka projection plane aka viewing plane
//static float far_clip = 10000.0f;

static int mapper;		// determines texture mapping method
static int flags;		// flags, that control rendering behaviour

/*static void printm(float *m) {
	int i, j;
	for(i = 0; i < 4; ++i) {
		for(j = 0; j < 4; ++j)
			printf("|%f", m[i*4+j]);
		printf("\n");
	}
}*/

void D3D_Push() {
	if(++current_matrix == MAX_MATRICES) {
		printf("matrix stack overflow\n");
		exit(-1);
	}
	memcpy(matrix_stack[current_matrix], tmatrix, 16*sizeof(float));
	tmatrix = matrix_stack[current_matrix];	
}

void D3D_Pop() {
	if(!current_matrix) {
		printf("matrix stack underflow\n");
		exit(-1);
	}
	tmatrix = matrix_stack[--current_matrix];
}

static void matrixcpy(float *dst, float *src) {
	memcpy(dst, src, 16*sizeof(float));
}

void D3D_MulMM(float *a, float *b, float *r)
{
	int i, j, t, k;

	memset(r, 0, sizeof(float)*16);

	for(i = 0; i < 4; i++)
		for(j = 0; j < 16; j += 4)
			for(t = 0, k = 0; t < 4; t++, k += 4)
				r[i+j] += a[i+k] * b[j+t];
}

void D3D_MulMV(float *m, float *v, float *r)
{
#define M(x,y) m[x]*v[y]
	r[0] = M(0,0) + M(4,1) + M(8 ,2) + M(12,3);
	r[1] = M(1,0) + M(5,1) + M(9 ,2) + M(13,3);
	r[2] = M(2,0) + M(6,1) + M(10,2) + M(14,3);
	r[3] = M(3,0) + M(7,1) + M(11,2) + M(15,3);
#undef M
}

void D3D_LoadIdentityM(float *m) {
	memset(m, 0, 16*sizeof(float));
	m[0] = m[5] = m[10] = m[15] = 1;
}

void D3D_TranslateM(float *m, float x, float y, float z) {
	float a[16], b[16];
	D3D_LoadIdentityM(a);
	a[12] = x;
	a[13] = y;
	a[14] = z;
	D3D_MulMM(m, a, b);
	matrixcpy(m, b);
}

void D3D_ScaleM(float *m, float x, float y, float z)
{
	float a[16], b[16];
	D3D_LoadIdentityM(a);
	a[0] = x;
	a[5] = y;
	a[10] = z;
	D3D_MulMM(m, a, b);
	matrixcpy(m, b);
}


void D3D_RotateM(float *m, float x, float y, float z) {
	float a[16], b[16], s, c;

	x = x/180*PI;
	y = y/180*PI;
	z = z/180*PI;

	D3D_LoadIdentityM(a);
	s = sin(x);
	c = cos(x);
	a[5] = c;
	a[6] = s;
	a[9] = -s;
	a[10] = c;
	D3D_MulMM(m, a, b);
	matrixcpy(m, b);

	D3D_LoadIdentityM(a);
	s = sin(y);
	c = cos(y);
	a[0] = c;
	a[2] = -s;
	a[8] = s;
	a[10] = c;
	D3D_MulMM(m, a, b);
	matrixcpy(m, b);

	D3D_LoadIdentityM(a);
	s = sin(z);
	c = cos(z);
	a[0] = c;
	a[1] = s;
	a[4] = -s;
	a[5] = c;
	D3D_MulMM(m, a, b);
	matrixcpy(m, b);
}

void D3D_LoadIdentity() {
	D3D_LoadIdentityM(tmatrix);
}

void D3D_Scale(float x, float y, float z) {
	D3D_ScaleM(tmatrix, x, y, z);
}

void D3D_Translate(float x, float y, float z) {
	D3D_TranslateM(tmatrix, x, -y, z);
}

void D3D_Rotate(float x, float y, float z) {
	D3D_RotateM(tmatrix, x, y, z);
}


static void lerp_init_y(lerp *l,  Vertex *s, Vertex *e, int first_step, int nsteps) {
	int i;

	for(i = 0; i < IPLS-1; ++i) {
		l->i[i] = s->i[i];
		l->s[i] = (e->i[i] - s->i[i]) / nsteps;
	}

	if(first_step)
		for(i = 0; i < IPLS-1; ++i)
			l->i[i] += first_step*l->s[i];
}

static void lerp_advance_y(lerp *l) {
	int i;
	for(i = 0; i < IPLS-1; ++i) l->i[i] += l->s[i];
}


static void lerp_init_x(lerp *l,  lerp *s, lerp *e,
		int first_step, int nsteps) {
	int i;

	Z(l) = 1/Z(s);

	for(i = 0; i < IPLS-3; ++i) {
		l->i[i] = s->i[i]*Z(l);
		l->s[i] = (e->i[i]*(1/Z(e)) - l->i[i]) / nsteps;
	}

	Z(l) = Z(s);
	l->s[IPLS-3] = (Z(e) - Z(s))/nsteps;

	if(first_step)
		for(i = 0; i < IPLS-2; ++i)
			l->i[i] += first_step*l->s[i];
	//U(l) *= texture->w-1;
	//V(l) *= texture->h-1;
	//UD(l) *= texture->w-1;
	//VD(l) *= texture->h-1;
}

/*static void lerp_advance_x(lerp *l) {
	int i;
	for(i = 0; i < IPLS-2; ++i) l->i[i] += l->s[i];
}*/



static void draw_span(lerp *l, int y, int x, int end_x) {
	typedef unsigned short fixcol;
#define fix(x) ((fixcol)(signed short)((x)*0xffff))
#define unfix(x) ((float)(x)/(0xffff))

	int tt[640];
	memset(tt, 0, 4*640);

	// color layout: AAAA|RRRR|GGGG|BBBB

	Uint16 mm0[4];
	mm0[3] = fix(A(l));
	mm0[2] = fix(R(l));
	mm0[1] = fix(G(l));
	mm0[0] = fix(B(l));

	Uint16 mm1[4];
	mm1[3] = fix(AD(l));
	mm1[2] = fix(RD(l));
	mm1[1] = fix(GD(l));
	mm1[0] = fix(BD(l));

	Uint16 mm2[4];
	mm2[3] = fix(0.0); // ambient color shouldn't affect alpha
	mm2[2] = fix(ambient_r);
	mm2[1] = fix(ambient_g);
	mm2[0] = fix(ambient_b);

	// uv layout: 0000|0000|VVVV|UUUU
	Uint16 mm6[4], uv_delta[4], uv_wh[4], uv_bp[4];

	mm6[0] = fix(U(l));
	mm6[1] = fix(V(l));
	mm6[2] = 0;
	mm6[3] = 0;

	uv_delta[0] = fix(UD(l));
	uv_delta[1] = fix(VD(l));
	uv_delta[2] = 0;
	uv_delta[3] = 0;

	uv_wh[0] = texture->w-1;
	uv_wh[1] = texture->h-1;
	uv_wh[2] = 0;
	uv_wh[3] = 0;

	uv_bp[0] = texture->format->BytesPerPixel;
	uv_bp[1] = texture->pitch;
	uv_bp[2] = 0;
	uv_bp[3] = 0;

	fixcol one[4] = {0xffff, 0xffff, 0xffff, 0xffff};
	float z = Z(l);
	float zd = ZD(l);

	Uint8 *dst = (Uint8*)screen->pixels + y*screen->pitch +
		x*screen->format->BytesPerPixel;

	// now we have following arrangements:
	// EAX is temporary
	// EBX points to zbuffer
	// ECX is used for debugging
	// EDX holds final location in screen buffer
	// ESI points to texture
	// EDI points to screen buffer
	// MM0 holds light color
	// MM1 holds delta for light color
	// MM2 holds ambient
	// MM3 holds resulting color between stages
	// MM4 is temporary
	// MM5 is temporary
	// MM6 holds uv
	// MM7 holds 0
	// XMM6 holds Z
	// XMM7 holds Z Delta
	// others are unsed
	// MMX code assumes that botch: screen and textrue are in BGRA format
	asm (
		"movq %0,%%mm0\n\t"
		"movq %1,%%mm1\n\t"
		"movq %2,%%mm2\n\t"
		"movq %3,%%mm6\n\t"
		"pxor %%mm7,%%mm7\n\t"		// mm7 = 0

		"movss %8,%%xmm6\n\t"		// z
		"movss %9,%%xmm7\n\t"		// zd

		"jmp loop_start\n\t"

		"loop_body:\n\t"

		// Z-BUFFER TEST
		"test $1, %%ecx\n\t"
		"jz skip_ztest\n\t"
		"movss (%%ebx), %%xmm5\n\t"
		"cmpss $2, %%xmm6, %%xmm5\n\t" // xmm5 <= xmm6
		"movd %%xmm5, %%eax\n\t"
		"test %%eax, %%eax\n\t"
		"jz loop_advance\n\t"
		"movss %%xmm6, (%%ebx)\n\t" // save new z-value of this pixel
		"skip_ztest:\n\t"

		// TEXTURE MAPPING (mm3 holds result)
		"movq %%mm6,%%mm3\n\t"			// mm3 = uv
		"pmulhuw %5,%%mm3\n\t"			// mm3 = u*w, v*h
		"pand %5,%%mm3\n\t"				// mm3 = (u*w)%w,(u*h)%h : wrap
		"pmaddwd %7,%%mm3\n\t"			// mm3 = linesz*vh+colorsz*uw
		"movd %%mm3,%%eax\n\t"
		"movd (%%eax,%%esi),%%mm3\n\t"	// mm3 = packed_texture
		"punpcklbw %%mm7,%%mm3\n\t"		// mm3 = texture

		// LIGHTS (mm3 holds result)
		"test $2, %%ecx\n\t"
		//"jz skip_lights\n\t"
		"movq %%mm0,%%mm4\n\t"			// mm4 = light
		"pmulhuw %%mm3,%%mm4\n\t"		// mm4 = light*texture
		"pmulhuw %%mm2,%%mm3\n\t"		// mm3 = ambient*texture
		"paddusb %%mm4,%%mm3\n\t"		// mm3 = (light + ambient)*texture
		"skip_lights:\n\t"

		// BLENDING (mm3 holds result)
		"test $4, %%ecx\n\t"
		"jz skip_blending\n\t"
		"pshufw $0xff,%%mm3,%%mm4\n\t"	// mm4 = src_alpha
		"psllw $8,%%mm4\n\t"			// convert mm4 to fixed point
		"pmulhuw %%mm4,%%mm3\n\t"		// mm3 = src*src_alpha
		"movq %6,%%mm5\n\t"				// mm5 = 1
		"psubw %%mm4,%%mm5\n\t"			// mm5 = 1-src_alpha
		"movd (%%edi),%%mm4\n\t"		// mm4 = dst_packed
		"punpcklbw %%mm7,%%mm4\n\t"		// mm4 = dst
		"pmulhuw %%mm5,%%mm4\n\t"		// mm4 = dst*(1-src_alpha)
		"paddw %%mm4,%%mm3\n\t"			// mm3 = src*src_alpha + dst*(1-src_alpha)
		"skip_blending:\n\t"

		// ENDING
		"packuswb %%mm7,%%mm3\n\t"		// pack pixel in mm3...
		"movd %%mm3,(%%edi)\n\t"		// and puti it to final resting place

		// ADVANCE
		"loop_advance:\n\t"
		"paddw %%mm1,%%mm0\n\t"			// advance light
		"paddw %4,%%mm6\n\t" 			// advance uv
		"addss %%xmm7,%%xmm6\n\t"		// advance z
		"add $4,%%edi\n\t"				// advance x
		"add $4,%%ebx\n\t"

		// LOOP CONTROL
		"loop_start:\n\t"
		"cmp %%edx,%%edi\n\t"
		"jl loop_body\n\t"				// jmp if edi < edx

		"emms\n\t"						// reset FPU after MMX
		:
		:
		"m" (*mm0),			// 0
		"m" (*mm1),			// 1
		"m" (*mm2),			// 2
		"m" (*mm6),			// 3
		"m" (*uv_delta),	// 4
		"m" (*uv_wh),		// 5
		"m" (*one),			// 6
		"m" (*uv_bp),		// 7
		"m" (z),			// 8
		"m" (zd),			// 9
		"c" (flags),		// ecx
		"b" (zbuffer+y*screen->w+x), "d" (dst+(end_x-x)*4),  "D" (dst), "S" (texture->pixels)
	);
}

#if 0
static void draw_span(lerp *l, int y, int x, int end_x) {

	// get pointer to start of scanline
	Uint8 *p = (Uint8*)screen->pixels + y*screen->pitch +
		x*screen->format->BytesPerPixel;

	for(; x < end_x; ++x) {
		if(!(flags & D3D_ZTEST) || zbuffer[y*screen->w + x] <= Z(l)) {
			// save new z-value of this pixel
			zbuffer[y*screen->w + x] = Z(l);

			// restore perspective of u and v
			float z = 1/Z(l); // restore z
			float u = U(l)*z;
			float v = V(l)*z;

			// and now we should map pixel from texture to screen
			float sr, sg, sb, sa;
#define C(x,y,p) (Uint32)(((Uint8*)texture->pixels)[(y)*texture->pitch+(x)*texture->format->BytesPerPixel+p])
			switch(mapper) {
			case D3D_LINEAR: { // they call it bilinear sometimes
				u *= texture->w-2;
				v *= texture->h-2;
				int xl = u;
				int yt = v;


				int xr = xl+1;
				int yb = yt+1;

				float r1 = C(xl,yt,0);
				float g1 = C(xl,yt,1);
				float b1 = C(xl,yt,2);
				float a1 = C(xl,yt,3);

				float r2 = C(xr,yt,0);
				float g2 = C(xr,yt,1);
				float b2 = C(xr,yt,2);
				float a2 = C(xr,yt,3);

				float r3 = C(xl,yb,0);
				float g3 = C(xl,yb,1);
				float b3 = C(xl,yb,2);
				float a3 = C(xl,yb,3);

				float r4 = C(xr,yb,0);
				float g4 = C(xr,yb,1);
				float b4 = C(xr,yb,2);
				float a4 = C(xr,yb,3);
				float t, xd = u-xl, yd = v-yt;

				t = r1+(r2-r1)*xd;
				sr = t + (r3+(r4-r3)*xd - t)*yd;
				t = g1+(g2-g1)*xd;
				sg = t + (g3+(g4-g3)*xd - t)*yd;
				t = b1+(b2-b1)*xd;
				sb = t + (b3+(b4-b3)*xd - t)*yd;
				t = a1+(a2-a1)*xd;
				sa = t + (a3+(a4-a3)*xd - t)*yd;

				break;
			}
			case D3D_NEAREST: {
				int x = u*(texture->w-1);
				int y = v*(texture->h-1);
				sr = C(x,y,0);
				sg = C(x,y,1);
				sb = C(x,y,2);
				sa = C(x,y,3);
				break;
			}
			case D3D_SOLID:
			default:
				sr = 0xff;
				sg = 0xff;
				sb = 0xff;
				sa = 0xff;
				break;
			}
#undef C

			sr = sr*R(l)*z+sr*ambient_r;
			sg = sg*G(l)*z+sg*ambient_g;
			sb = sb*B(l)*z+sb*ambient_b;

			if(flags & D3D_BLENDING) {
				float dr = p[2];
				float dg = p[1];
				float db = p[0];
				sa /= 0xff;
				sa *= A(l)*z;
				sr = sa*sr + dr*(1-sa);
				sg = sa*sg + dg*(1-sa);
				sb = sa*sb + db*(1-sa);
			}

			// next check is vital, and shouldn't be removed
			if(sr>255) sr = 255;
			if(sg>255) sg = 255;
			if(sb>255) sb = 255;

			*(Uint32*)p = ((int)sr <<16) | ((int)sg<<8) | (int)sb;
			//*(Uint32*)p = SDL_MapRGB(screen->format, sr, sg, sb);
		}
		int i;
		// advance
		for(i = 0; i < IPLS-2; ++i) l->i[i] += l->s[i];
		p += screen->format->BytesPerPixel;
	}
}
#endif

static void draw_face3(int y, lerp *a, lerp *b, Face *f) {
	// scanline level
	if(X(a) > X(b)) {
		lerp *t = a;
		a = b;
		b = t;
	}

	int x = (int)X(a);
	int end_x = (int)X(b);

	if(x >= end_x) return;
	lerp l;
	if(x < 0) {
		lerp_init_x(&l, a, b, -x, end_x - x);
		x = 0;
	} else lerp_init_x(&l, a, b, 0, end_x - x);

	if(end_x > screen->w) end_x = screen->w;

	draw_span(&l, y, x, end_x);
	/*#define STEP 64
	while(x < end_x) {
		draw_span(&l, y, x, x+MIN(end_x-x, STEP));
		x += STEP;
	}*/
}



// project vertex onto center of screen surface
void project_vertex(Vertex *p, Vertex *q) {
	// NOTE: we should never modify 'p' here
	int i;
	Z(q) = 1/Z(p);
	X(q) = (screen->w + (screen->w+screen->h)*X(p)*Z(q))*0.5f;
	Y(q) = (screen->h + (screen->w+screen->h)*-Y(p)*Z(q))*0.5f;
	// NOTE: we should write '-y', because in SDL's image-buffer origin is top-left
	// corner, not bottom-right (as in euclidian space).

	for(i = 0; i < IPLS-3; ++i) q->i[i] = p->i[i];

	// dividing by 'z' is nescecary for perspective correction
	for(i = 0; i < IPLS-3; ++i) q->i[i] *= Z(q);
}

static void draw_face2(Vertex *p, Vertex *q, Vertex *r, Face *f) {
	// 2-d triangle level

	// create local projected copies
	Vertex r1, r2, r3, *a = &r1, *b = &r2, *c = &r3;
	project_vertex(p, a);
	project_vertex(q, b);
	project_vertex(r, c);

	// make sure this triangle has on screen parts
	if(X(a) < 0 && X(b) < 0 && X(c) < 0) return;
	if(X(a) >= screen->w && X(b) >= screen->w && X(c) >= screen->w) return;

	// we wont draw, if it's very thin at y axis
	if((int)X(a) == (int)X(b) && (int)X(a) == (int)X(c)) return;

	// sort by 'y' (should be done in float)
	if(Y(a) > Y(b)) {
		Vertex *t = a;
		a = b;
		b = t;
	}
	if(Y(a) > Y(c)) {
		Vertex *t = a;
		a = c;
		c = t;
	}
	if(Y(b) > Y(c)) {
		Vertex *t = b;
		b = c;
		c = t;
	}

	// we should always use integers for lead counters
	int beg_y = Y(a);
	int cen_y = Y(b);
	int end_y = Y(c);
	int y, e;

	if(end_y == beg_y || end_y < 0 || beg_y >= screen->h) return;

	lerp x1, x2;

	if(beg_y < 0) {
		lerp_init_y(&x2, a, c, -beg_y, end_y-beg_y);
		y = 0;
	} else {
		lerp_init_y(&x2, a, c, 0, end_y-beg_y);
		y = beg_y;
	}

	if(y < cen_y) {
		if(beg_y < 0) lerp_init_y(&x1, a, b, -beg_y, cen_y-beg_y);
		else lerp_init_y(&x1, a, b, 0, cen_y-beg_y);

		if(cen_y > screen->h) e = screen->h;
		else e = cen_y;

		for(; y < e; ++y) {
			draw_face3(y, &x1, &x2, f);
			lerp_advance_y(&x1);
			lerp_advance_y(&x2);
		}
	}

	if(cen_y < end_y) {
		lerp_init_y(&x1, b, c, y-cen_y, end_y-cen_y);
		if(end_y > screen->h) end_y = screen->h;

		for(; y < end_y; ++y) {
			draw_face3(y, &x1, &x2, f);
			lerp_advance_y(&x1);
			lerp_advance_y(&x2);
		}
	}

	ntriangles++;
}

// calculate point of intersection between viewing plane and line (a,b)
static void viewplane_clip(Vertex *a, Vertex *b, Vertex *r) {
	float d, t;

	d = Z(b) - Z(a);

	// check, if vertices merge into single one
	// this should not happen, though
	//if(d == 0) d = 0.0000001;

	t = (near_clip - Z(a))/d; // tangent

	int i;
	for(i = 0; i < IPLS; ++i)
		r->i[i] = a->i[i] + (b->i[i]-a->i[i])*t;
}



static void draw_face(Face *f) {
	// 3-d triangle level

	// NOTE: we should never modify user nor provided Face
	//       nor its vertices
	Vertex *a = f->a;
	Vertex *b = f->b;
	Vertex *c = f->c;
	Vertex *t, r1, r2;

	// sort by 'z'
	if(Z(a) > Z(c)) t = a, a = c, c = t;
	if(Z(a) > Z(b)) t = a, a = b, b = t;
	if(Z(b) > Z(c)) t = c, c = b, b = t;

	if(Z(c) < near_clip) return; // fully clipped

	// calculate lights for each vertex
	if(Z(a) < near_clip) {
		viewplane_clip(c, a, &r1);
		if(Z(b) < near_clip) {
			viewplane_clip(c, b, &r2);
			a = &r1;
			b = &r2;
		} else {
			viewplane_clip(b, a, &r2);
			draw_face2(c, b, &r2, f);
			a = &r1;
			b = &r2;
		}
	}

	draw_face2(c, b, a, f);
}


void D3D_Begin(int t) {
	draw_type = t;
	assert(0 < draw_type && draw_type <= D3D_QUAD_STRIP);
	total_vertices = 0;
}

void D3D_End() {
	int i;
	Vertex *v = vertex_buffer;
	Face *f = face_buffer;


	assert(total_vertices != 0);
	if(total_vertices == 1) assert(draw_type == D3D_POINTS);
	else if(total_vertices == 2) assert(draw_type == D3D_LINES);

	switch(draw_type) {
	case D3D_POINTS:
		// not implemented
		break;
	case D3D_LINES:
		// not implemented
		break;
	case D3D_TRIANGLES:
		total_vertices -= total_vertices % 3;
		for(i = 0; i < total_vertices; i += 3, f++) {
			f->a = v+i;
			f->b = v+i+1;
			f->c = v+i+2;
		}
		break;
	case D3D_TRIANGLE_FAN: // every next connected to the first and previous
		for(i = 2; i < total_vertices; i++, f++) {
			f->a = v;
			f->b = v+i-1;
			f->c = v+i;
		}
		break;
	case D3D_TRIANGLE_STRIP: // every one connected to two previous
		for(i = 2; i < total_vertices; i++, f++) {
			f->a = v+i;
			f->b = v+i-1;
			f->c = v+i-2;
		}
		break;
	case D3D_QUADS:
		total_vertices -= total_vertices % 4;
		for(i = 0; i < total_vertices; i+=4) {
			f->a = v+i;
			f->b = v+i+1;
			f->c = v+i+2;
			f++;
			f->a = v+i;
			f->b = v+i+2;
			f->c = v+i+3;
			f++;
		}
		break;
	case D3D_QUAD_STRIP: // every two connected to two previous
		for(i = 2; i < total_vertices; i++, f++) {
			f->a = v+i;
			f->b = v+i-1;
			f->c = v+i-2;
		}
		break;
	}

	int total_faces = f-face_buffer;

	// calculate center of mesh
	float cx = 0, cy = 0, cz = 0;
	for(i = 0; i < total_vertices; i++) {
		cx += X(v+i);
		cy += Y(v+i);
		cz += Z(v+i);
	}
	cx /= total_vertices;
	cy /= total_vertices;
	cz /= total_vertices;

	if(flags & D3D_LIGHTS) { // calculate lights
		for(i = 0; i < total_vertices; i++) {
			float x = X(v+i) - cx;
			float y = Y(v+i) - cy;
			float z = Z(v+i) - cz;
			float m = sqrt(x*x + y*y + z*z);
			NX(v+i) = x/m;
			NY(v+i) = y/m;
			NZ(v+i) = z/m;

			// now we are ready to calculate light value for this vertex
			int j;
			for(j = 0; j < total_lights; ++j) {
				Light *l = light_buffer+j;
				float lx = l->x - X(v+i);
				float ly = l->y - Y(v+i);
				float lz = l->z - Z(v+i);
				float m = sqrt(lx*lx+ly*ly+lz*lz);
				lx /= m;
				ly /= m;
				lz /= m;
				float d = NX(v+i)*lx + NY(v+i)*ly + NZ(v+i)*lz;
				if(d>0) {
					R(v+i) += d*l->r;
					G(v+i) += d*l->g;
					B(v+i) += d*l->b;
				}
			}
		}
	}

	f = face_buffer;

	if(flags & D3D_CULLING) { // back-face culling
		for(i = 0; i < total_faces; i++) {
			// calculate direction vector perpendicular to this face
			// NOTE: we should correct its perspective
			float xa = X(f[i].c)/Z(f[i].c)-X(f[i].a)/Z(f[i].a);
			float ya = Y(f[i].c)/Z(f[i].c)-Y(f[i].a)/Z(f[i].a);
			float za = Z(f[i].c)-Z(f[i].a);
			float xb = X(f[i].c)/Z(f[i].c)-X(f[i].b)/Z(f[i].b);
			float yb = Y(f[i].c)/Z(f[i].c)-Y(f[i].b)/Z(f[i].b);
			float zb = Z(f[i].c)-Z(f[i].b);
			f[i].nx = ya*zb - za*yb;
			f[i].ny = za*xb - xa*zb;
			f[i].nz = xa*yb - ya*xb;
			float m = sqrt(f[i].nx*f[i].nx + f[i].ny*f[i].ny + f[i].nz*f[i].nz);
			f[i].nx /= m;
			f[i].ny /= m;
			f[i].nz /= m;

			float d = (X(f[i].a)/Z(f[i].a)-cx/cz)*f[i].nx + (Y(f[i].a)/Z(f[i].a)-cy/cz)*f[i].ny + (Z(f[i].a)-cz)*f[i].nz;
			if(d < 0) {
				f[i].nx *= -1;
				f[i].ny *= -1;
				f[i].nz *= -1;
			}
			if(f[i].nz < 0) // draw only if triangle faces camera
				draw_face(f+i);
		}
	} else {
		for(i = 0; i < total_faces; i++)
			draw_face(f+i);
	}


	draw_type = D3D_NOTHING;
}

void D3D_Color(float r, float g, float b) {
	rR = r;
	rG = g;
	rB = b;
	rA = 1;
}

void D3D_Color4(float r, float g, float b, float a) {
	rR = r;
	rG = g;
	rB = b;
	rA = a;
}

void D3D_Normal(float x, float y, float z) {
	rNX = x;
	rNY = y;
	rNZ = z;
}

void D3D_TexCoord(float u, float v) {
	rU = u;
	rV = v;
}

void D3D_Vertex(float x, float y, float z) {
	if(total_vertices == MAX_VERTICES) {
		printf("vertex buffer overflow\n");
		exit(-1);
	}

	Vertex *p = &vertex_buffer[total_vertices];

	// translate to screen coordinates
	float *m = tmatrix;
	X(p) = m[0]*x + m[4]*y + m[ 8]*z + m[12];
	Y(p) = m[1]*x + m[5]*y + m[ 9]*z + m[13];
	Z(p) = m[2]*x + m[6]*y + m[10]*z + m[14];

	NX(p) = rNX;
	NY(p) = rNY;
	NZ(p) = rNZ;

	R(p) = rR;
	G(p) = rG;
	B(p) = rB;
	A(p) = rA;

	U(p) = rU;
	V(p) = rV;

	++total_vertices;
}

void D3D_ClearScreen(float r, float g, float b) {
	Uint32 c = SDL_MapRGB(screen->format, (Uint8)(r*0xff), (Uint8)(g*0xff), (Uint8)(b*0xff));

	if(c) SDL_FillRect(screen, 0, c);
	else memset(screen->pixels, 0, screen->w*screen->h*screen->format->BytesPerPixel);
}

void D3D_ClearZBuffer() {
	memset(zbuffer, 0, screen->w*screen->h*sizeof(float));
}

void D3D_SetScreen(SDL_Surface *s) {
	screen = s;
}

void D3D_SetTexture(SDL_Surface *t) {
	texture = t;
}

void D3D_Light(float r, float g, float b) {
	if(total_lights == MAX_LIGHTS) {
		printf("light buffer overflow\n");
		exit(-1);
	}
	Light *l = &light_buffer[total_lights++];
	l->x = tmatrix[12];
	l->y = tmatrix[13];
	l->z = tmatrix[14];
	l->r = r;
	l->g = g;
	l->b = b;
}

void D3D_ClearLights() {
	total_lights = 0;
}

void D3D_SetAmbient(float r, float g, float b) {
	ambient_r = r;
	ambient_g = g;
	ambient_b = b;
}

void D3D_SetNearClip(float z) {
	near_clip = z;
}

void D3D_Enable(int f) {
	flags |= f;
}

void D3D_Disable(int f) {
	flags &= ~f;
}


void D3D_SetMapper(int m) {
	mapper = m;
}

int D3D_Init() {
	// 1600x1200 should be sufficient
	zbuffer = (float*)malloc(1600*1200*sizeof(float));
	D3D_SetMapper(D3D_LINEAR);
	D3D_LoadIdentity();
	D3D_SetAmbient(1, 1, 1);
	//D3D_SetAmbient(0.2, 0.2, 0.2);
	return 0;
}

void D3D_Quit() {
	free(zbuffer);
}
