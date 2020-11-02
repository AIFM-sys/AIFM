/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

/*
    The original source for this example is
    Copyright (c) 1994-2008 John E. Stone
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote products
       derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

/*****************************************************************************
 * api.h - The declarations and prototypes needed so that 3rd party driver   *
 *         code can run the raytracer.  Third party driver code should       * 
 *         only use the functions in this header file to interface with      *
 *         the rendering engine.                                             *
 *************************************************************************** */


/* 
 *  $Id: api.h,v 1.1.1.1 2012/03/29 17:22:19 uid42307 Exp $
 */


/********************************************/
/* Types defined for use with the API calls */
/********************************************/

#ifdef USESINGLEFLT
typedef float apiflt;   /* generic floating point number */
#else
typedef double apiflt;  /* generic floating point number */
#endif

typedef void * SceneHandle;

typedef struct {
  int texturefunc; /* which texture function to use */
  color col;    /* base object color */
  int shadowcast;  /* does the object cast a shadow */
  apiflt ambient;  /* ambient lighting */
  apiflt diffuse;  /* diffuse reflection */
  apiflt specular; /* specular reflection */
  apiflt opacity;  /* how opaque the object is */ 
  vector ctr;   /* origin of texture */
  vector rot;   /* rotation of texture around origin */
  vector scale; /* scale of texture in x,y,z */ 
  vector uaxs;  /* planar map u axis */
  vector vaxs;  /* planar map v axis */
  char imap[96];   /* name of image map */ 
} apitexture;


/*******************************************************************
 *  NOTE: The value passed in apitexture.texturefunc corresponds to 
 *        the meanings given in this table:
 *
 *   0 - No texture function is applied other than standard lighting.
 *   1 - 3D checkerboard texture.  Red & Blue checkers through 3d space.
 *   2 - Grit texture, roughens up the surface of the object a bit.
 *   3 - 3D marble texture.  Makes a 3D swirl pattern through the object.
 *   4 - 3D wood texture.  Makes a 3D wood pattern through the object.
 *   5 - 3D gradient noise function.
 *   6 - I've forgotten :-)
 *   7 - Cylindrical Image Map  **** IMAGE MAPS REQUIRE the filename 
 *   8 - Spherical Image Map         of the image be put in imap[]
 *   9 - Planar Image Map            part of the texture...
 *        planar requires uaxs, and vaxs..
 *
 *******************************************************************/

/********************************************/
/* Functions implemented to provide the API */
/********************************************/

vector rt_vector(apiflt x, apiflt y, apiflt z); /* helper to make vectors */
color  rt_color(apiflt r, apiflt g, apiflt b);  /* helper to make colors */

void rt_initialize(int *, char ***);/* reset raytracer, memory deallocation */
void rt_finalize(void); /* close down for good.. */

SceneHandle rt_newscene(void);        /* allocate new scene */
void rt_deletescene(SceneHandle); /* delete a scene */
void rt_renderscene(SceneHandle); /* raytrace the current scene */  
void rt_outputfile(SceneHandle, const char * outname); 
void rt_resolution(SceneHandle, int hres, int vres);
void rt_verbose(SceneHandle, int v);
void rt_rawimage(SceneHandle, unsigned char *rawimage);
void rt_background(SceneHandle, color);

/* Parameter values for rt_boundmode() */
#define RT_BOUNDING_DISABLED 0
#define RT_BOUNDING_ENABLED  1

void rt_boundmode(SceneHandle, int);
void rt_boundthresh(SceneHandle, int);

/* Parameter values for rt_displaymode() */
#define RT_DISPLAY_DISABLED  0
#define RT_DISPLAY_ENABLED   1

void rt_displaymode(SceneHandle, int);

void rt_scenesetup(SceneHandle, char *, int, int, int);
  /* scene, output filename, horizontal resolution, vertical resolution,
            verbose mode */


void rt_camerasetup(SceneHandle, apiflt, apiflt, int, int,
	vector, vector,  vector);
  /* camera parms: scene, zoom, aspectratio, antialiasing, raydepth,
		camera center, view direction, up direction */



void * rt_texture(apitexture *);
   /* pointer to the texture struct that would have been passed to each 
      object() call in older revisions.. */




void rt_light(void * , vector, apiflt);     /* add a light */
  /* light parms: texture, center, radius */ 

void rt_sphere(void *, vector, apiflt);    /* add a sphere */
  /* sphere parms: texture, center, radius */

void rt_scalarvol(void *, vector, vector,
		 int, int, int, char *, void *); 

void rt_extvol(void *, vector, vector, int, apiflt (* evaluator)(apiflt, apiflt, apiflt)); 

void rt_box(void *, vector, vector);  
  /* box parms: texture, min, max */

void rt_plane(void *, vector, vector);  
  /* plane parms: texture, center, normal */

void rt_ring(void *, vector, vector, apiflt, apiflt); 
  /* ring parms: texture, center, normal, inner, outer */

void rt_tri(void *, vector, vector, vector);  
  /* tri parms: texture, vertex 0, vertex 1, vertex 2 */

void rt_stri(void *, vector, vector, vector, 
			vector, vector, vector); 
 /* stri parms: texture, vertex 0, vertex 1, vertex 2, norm 0, norm 1, norm 2 */

void rt_heightfield(void *, vector, int, int, apiflt *, apiflt, apiflt);
  /* field parms: texture, center, m, n, field, wx, wy */

void rt_landscape(void *, int, int, vector,  apiflt, apiflt);

void rt_quadsphere(void *, vector, apiflt); /* add quadric sphere */
  /* sphere parms: texture, center, radius */

void rt_cylinder(void *, vector, vector, apiflt);

void rt_fcylinder(void *, vector, vector, apiflt);

void rt_polycylinder(void *, vector *, int, apiflt);


/* new texture handling routines */
void rt_tex_color(void * voidtex, color col); 

#define RT_PHONG_PLASTIC 0
#define RT_PHONG_METAL   1
void rt_tex_phong(void * voidtex, apiflt phong, apiflt phongexp, int type); 
