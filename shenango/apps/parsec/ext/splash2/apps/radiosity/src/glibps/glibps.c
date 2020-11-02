/* -*-mode:c-*- */
/***************************************************************
*
*       Radiosity
*
*	Graphic driver for PostScript
*
*
***************************************************************/

#include <stdio.h>
#include <math.h>



#define SCREEN_WIDTH   (6.0*72)
#define SCREEN_HEIGHT  (4.8*72)
#define SCREEN_DEPTH   (65536)
#define ASPECT_RATIO  ((float)SCREEN_WIDTH/(float)SCREEN_HEIGHT)

#define PRE_CAT  (1)
#define POST_CAT (0)

#define DEFAULT_WINDOW_HEIGHT (2000.0)
#define DEFAULT_WINDOW_WIDTH  (DEFAULT_WINDOW_HEIGHT*ASPECT_RATIO)
#define DEFAULT_FRONT_PLANE_Z (2000.0)
#define DEFAULT_BACK_PLANE_Z  (-4000.0)
#define DEFAULT_PRP_Z         (10000.0)    /* Projection point Z coord. */


/*** Data structure used in radiosity algorithm ***/

typedef struct {
      float x, y, z ;
} Point ;

typedef struct {
      float r, g, b ;
} Rgb ;


typedef struct
{
      float v[4] ;                   /* x, y, z, and w */
} Vertex ;

typedef struct
{
      float m[4][4] ;                /* m[row][column], row vector assumed */
} Matrix ;


/**************************************************
*
*    Globals
*
***************************************************/

static Matrix  trans_mtx ;		/* WC -> DC */
static Vertex  prp ;			/* Projection point */
static Vertex  active_prp ;		/* Projection point in effect (WC) */
static float   view_rotx, view_roty ;	/* Viewing */
static float   view_zoom ;

static float   clip_right, clip_left ;	/* View volume (X) */
static float   clip_top, clip_bottom ;	/*             (Y) */
static float   clip_front, clip_back ;	/*             (Z) */


static FILE *ps_fd ;



void gset_unit_matrix(), gconcatenate_matrix(), gscale_matrix() ;
void gtranslate_matrix(), grotate_x_matrix(), grotate_y_matrix() ;
void grotate_z_matrix(), gtransform(), ginverse_matrix() ;


/************************************************************************
*
*    ps_open()
*    ps_close()
*
*************************************************************************/



int ps_open( file )
    char *file ;
{
      void init_transformation() ;
      void setup_transformation() ;

      if( (ps_fd = fopen( file, "w" )) == 0 )
      {
	    perror( file ) ;
	    return( 0 ) ;
      }

      /* Print out preamble */
      fprintf( ps_fd, "%%!PS-Adobe-1.0\n" ) ;
      fprintf( ps_fd, "%%%%EndComments\n" ) ;
      fprintf( ps_fd, "%%%%Pages: 1\n" ) ;
      fprintf( ps_fd, "%%%%EndProlog\n" ) ;
      fprintf( ps_fd, "%%%%Page: 1 1\n" ) ;
      fprintf( ps_fd, "\n" ) ;

      /* Default line cap/join */
      fprintf( ps_fd, "1 setlinecap 1 setlinejoin\n" ) ;

      /* Initialize transformation */
      init_transformation() ;
      setup_transformation() ;
}



void ps_close()
{
      if( ps_fd == 0 )
	    return ;


      fprintf( ps_fd, "showpage\n" ) ;
      fprintf( ps_fd, "%%%%Trailer\n" ) ;
      fclose( ps_fd ) ;

      ps_fd = 0 ;
}



/**************************************************
*
*    ps_linewidth()
*
***************************************************/

void ps_linewidth( w )
   
    float w ;
{
      if( ps_fd == 0 )
	    return ;

      fprintf( ps_fd, "%f setlinewidth\n", w ) ;
}



/**************************************************
*
*    ps_line()
*
***************************************************/

void ps_line( p1, p2 )

    Point *p1, *p2 ;
{
      Vertex  v1, v2 ;
      float x1, y1, x2, y2 ;

      if( ps_fd == 0 )
	    return ;

      v1.v[0] = p1->x ;  v1.v[1] = p1->y ;  v1.v[2] = p1->z ;  v1.v[3] = 1.0 ;
      v2.v[0] = p2->x ;  v2.v[1] = p2->y ;  v2.v[2] = p2->z ;  v2.v[3] = 1.0 ;
      gtransform( &v1, &v1, &trans_mtx ) ;
      gtransform( &v2, &v2, &trans_mtx ) ;
      x1 = v1.v[0] / v1.v[3] ;
      y1 = v1.v[1] / v1.v[3] ;
      x2 = v2.v[0] / v2.v[3] ;
      y2 = v2.v[1] / v2.v[3] ;


      fprintf( ps_fd, "newpath\n%f %f moveto\n", x1, y1 ) ;
      fprintf( ps_fd, "%f %f lineto\nstroke\n", x2, y2 ) ;
}


/**************************************************
*
*    ps_polygonedge()
*
***************************************************/

void ps_polygonedge( n, p_list )

    int n ;
    Point *p_list ;
{
      float dcx, dcy ;
      Vertex v ;
      int i ;

      if( ps_fd == 0 )
	    return ;

      /* Transform */
      v.v[0] = p_list[0].x ;
      v.v[1] = p_list[0].y ;
      v.v[2] = p_list[0].z ;
      v.v[3] = 1.0 ;
      gtransform( &v, &v, &trans_mtx ) ;
      dcx = v.v[0] / v.v[3] ; 
      dcy = v.v[1] / v.v[3] ; 
      fprintf( ps_fd, "newpath\n%f %f moveto\n", dcx, dcy ) ;

      for( i = 1 ; i < n ; i++ )
      {
	    /* Transform */
	    v.v[0] = p_list[i].x ;
	    v.v[1] = p_list[i].y ;
	    v.v[2] = p_list[i].z ;
	    v.v[3] = 1.0 ;
	    gtransform( &v, &v, &trans_mtx ) ;
	    dcx = v.v[0] / v.v[3] ; 
	    dcy = v.v[1] / v.v[3] ; 

	    fprintf( ps_fd, "%f %f lineto\n", dcx, dcy ) ;
      }
      
      fprintf( ps_fd, "closepath stroke\n" ) ;
}


/**************************************************
*
*    ps_polygon()
*
***************************************************/

void ps_polygon( n, p_list )

    int n ;
    Point *p_list ;
{
      float dcx, dcy ;
      Vertex v ;
      int i ;

      if( ps_fd == 0 )
	    return ;

      /* Transform */
      v.v[0] = p_list[0].x ;
      v.v[1] = p_list[0].y ;
      v.v[2] = p_list[0].z ;
      v.v[3] = 1.0 ;
      gtransform( &v, &v, &trans_mtx ) ;
      dcx = v.v[0] / v.v[3] ; 
      dcy = v.v[1] / v.v[3] ; 
      fprintf( ps_fd, "newpath\n%f %f moveto\n", dcx, dcy ) ;

      for( i = 1 ; i < n ; i++ )
      {
	    /* Transform */
	    v.v[0] = p_list[i].x ;
	    v.v[1] = p_list[i].y ;
	    v.v[2] = p_list[i].z ;
	    v.v[3] = 1.0 ;
	    gtransform( &v, &v, &trans_mtx ) ;
	    dcx = v.v[0] / v.v[3] ; 
	    dcy = v.v[1] / v.v[3] ; 

	    fprintf( ps_fd, "%f %f lineto\n", dcx, dcy ) ;
      }
      
      fprintf( ps_fd, "closepath fill\n" ) ;
}


/**************************************************
*
*    ps_spolygon()
*
***************************************************/

void ps_spolygon( n, p_list, c_list )

    int n ;
    Point *p_list ;
    Rgb   *c_list ;
{
      float dcx, dcy ;
      Vertex v ;
      int i ;
      float gray_scale ;

      if( ps_fd == 0 )
	    return ;

      /* Transform */
      v.v[0] = p_list[0].x ;
      v.v[1] = p_list[0].y ;
      v.v[2] = p_list[0].z ;
      v.v[3] = 1.0 ;
      gtransform( &v, &v, &trans_mtx ) ;
      dcx = v.v[0] / v.v[3] ; 
      dcy = v.v[1] / v.v[3] ; 
      fprintf( ps_fd, "newpath\n%f %f moveto\n", dcx, dcy ) ;

      for( i = 1 ; i < n ; i++ )
      {
	    /* Transform */
	    v.v[0] = p_list[i].x ;
	    v.v[1] = p_list[i].y ;
	    v.v[2] = p_list[i].z ;
	    v.v[3] = 1.0 ;
	    gtransform( &v, &v, &trans_mtx ) ;
	    dcx = v.v[0] / v.v[3] ; 
	    dcy = v.v[1] / v.v[3] ; 

	    fprintf( ps_fd, "%f %f lineto\n", dcx, dcy ) ;
      }
      
      gray_scale = c_list[0].g ;
      if( gray_scale > 1.0 )
	    gray_scale = 1.0 ;
      else if( gray_scale < 0.0 )
	    gray_scale = 0.0 ;
		  
      fprintf( ps_fd, "closepath %f setgray fill\n", gray_scale ) ;
}


/**************************************************
*
*    ps_clear()
*
***************************************************/

void ps_clear()
{
}



/**************************************************
*
*    ps_setup_view()
*
***************************************************/

void ps_setup_view( rot_x, rot_y, dist, zoom )

    float rot_x, rot_y, dist, zoom ;
{
      void setup_transformation() ;

      prp.v[0] = 0.0 ;
      prp.v[1] = 0.0 ;
      prp.v[2] = (float)dist ;
      prp.v[3] = 0.0 ;
      view_rotx = rot_x ;
      view_roty = rot_y ;
      view_zoom = zoom  ;

      setup_transformation() ;
}



/**************************************************
*
*    setup_transformation()
*
***************************************************/

void setup_transformation()
{
      float cf_z, cb_z ;
      int light ;
      Matrix pmat ;
      
      /* Set to unit matrix */
      gset_unit_matrix( &trans_mtx ) ;

      /* View orientation matrix */
      grotate_x_matrix( POST_CAT, &trans_mtx, view_rotx ) ;
      grotate_y_matrix( POST_CAT, &trans_mtx, view_roty ) ;

      /* Compute active (currently effective) projection point */
      ginverse_matrix( &pmat, &trans_mtx ) ;
      gtransform( &active_prp, &prp, &pmat ) ;

      /* Perspective projection */
      gset_unit_matrix( &pmat ) ;
      pmat.m[2][3] = - 1 / prp.v[2] ;
      gconcatenate_matrix( POST_CAT, &trans_mtx, &pmat ) ;

      cf_z = prp.v[2] * clip_front / ( prp.v[2] - clip_front ) ;
      cb_z = prp.v[2] * clip_back  / ( prp.v[2] - clip_back  ) ;

      /* Window-Viewport */
      gscale_matrix( POST_CAT, &trans_mtx,
		   (float)SCREEN_WIDTH  / (clip_right - clip_left),
		   (float)SCREEN_HEIGHT / (clip_top - clip_bottom),
		   (float)SCREEN_DEPTH  / (cf_z - cb_z) ) ;

      gtranslate_matrix( POST_CAT, &trans_mtx,
	      -(float)SCREEN_WIDTH * clip_left / (clip_right - clip_left),
	      -(float)SCREEN_HEIGHT* clip_top  / (clip_bottom - clip_top),
	      -(float)SCREEN_DEPTH * cb_z / (cf_z - cb_z) ) ;

      gtranslate_matrix( POST_CAT, &trans_mtx,
			(float)(1.0*72), (float)(0.5*72), 0 ) ;
}




/**************************************************
*
*    init_transformation()
*
***************************************************/

void init_transformation()
{
      /* Initialize matrix, just in case */
      gset_unit_matrix( &trans_mtx ) ;

      /* Initialize Projection point */
      prp.v[0] = 0.0 ;
      prp.v[1] = 0.0 ;
      prp.v[2] = DEFAULT_PRP_Z ;
      prp.v[3] = 0.0 ;
 
      /* Viewing */
      view_rotx = view_roty = 0.0 ;
      view_zoom = 1.0 ;
  
      /* Initialize view volume boundary */
      clip_right =  DEFAULT_WINDOW_WIDTH / 2.0 ;
      clip_left  = -DEFAULT_WINDOW_WIDTH / 2.0 ;
      clip_top   =  DEFAULT_WINDOW_HEIGHT / 2.0 ;
      clip_bottom= -DEFAULT_WINDOW_HEIGHT / 2.0 ; 
      clip_front =  DEFAULT_FRONT_PLANE_Z ;
      clip_back  =  DEFAULT_BACK_PLANE_Z ;
}


/********************************************
*
*    set_matrix()
*
*********************************************/

void  gset_matrix( mtx, m11, m12, m13, m14,  m21, m22, m23, m24,
		       m31, m32, m33, m34,  m41, m42, m43, m44 )

    Matrix *mtx ;
    float m11, m12, m13, m14 ;
    float m21, m22, m23, m24 ;
    float m31, m32, m33, m34 ;
    float m41, m42, m43, m44 ;
{
      mtx->m[0][0] = m11 ; mtx->m[0][1] = m12 ;
      mtx->m[0][2] = m13 ; mtx->m[0][3] = m14 ;

      mtx->m[1][0] = m21 ; mtx->m[1][1] = m22 ;
      mtx->m[1][2] = m23 ; mtx->m[1][3] = m24 ;

      mtx->m[2][0] = m31 ; mtx->m[2][1] = m32 ;
      mtx->m[2][2] = m33 ; mtx->m[2][3] = m34 ;

      mtx->m[3][0] = m41 ; mtx->m[3][1] = m42 ;
      mtx->m[3][2] = m43 ; mtx->m[3][3] = m44 ;
}    



/********************************************
*
*    set_unit_matrix()
*
*********************************************/

void  gset_unit_matrix( mtx )

    Matrix *mtx ;
{
      int  row, col ;
      
      /* Clear the matrix */
      for( row = 0 ; row < 4 ; row++ )
	    for( col = 0 ; col < 4 ; col++ )
		  mtx->m[row][col] = 0.0 ;

      /* Set 1.0s along diagonal line */
      for( row = 0 ; row < 4 ; row++ )
	    mtx->m[row][row] = 1.0 ;
}    




/********************************************
*
*    concatenate_matrix()
*
*          m1 <- m1 * m2 (precat = 1)
*          m1 <- m2 * m1 (precat = 0)
*
*********************************************/

void  gconcatenate_matrix( precat, m1, m2 )

    int precat ;
    Matrix *m1, *m2 ;
{
      int  row, col, scan ;
      Matrix *dest ;
      Matrix temp ;
      

      /* Swap pointer according to the concatenation mode */
      dest = m1 ;
      if( precat == 1 )
      {
	    m1 = m2 ;
	    m2 = dest ;
      }
      
      /* concatenate it */
      for( row = 0 ; row < 4 ; row++ )
	    for( col = 0 ; col < 4 ; col++ ) 
	    {
		  temp.m[row][col] = 0.0 ;
		  for( scan = 0 ; scan < 4 ; scan++ )
			temp.m[row][col] +=
			      m1->m[row][scan] * m2->m[scan][col];
	    }

      *dest = temp ;
}


/********************************************
*
*    scale_matrix()
*
*          m1 <- SCALE * m1 (precat = 1)
*          m1 <- m1 * SCALE (precat = 0)
*
*********************************************/

void  gscale_matrix( precat, m1, sx, sy, sz )

    int precat ;
    Matrix *m1 ;
    float sx, sy, sz ;
{
      Matrix smat ;
      
      /* Initialize to unit matrix */
      gset_unit_matrix( &smat ) ;

      /* Set scale values */
      smat.m[0][0] = sx ;
      smat.m[1][1] = sy ;
      smat.m[2][2] = sz ;

      /* concatenate */
      gconcatenate_matrix( precat, m1, &smat ) ;
}



/********************************************
*
*    translate_matrix()
*
*          m1 <- T * m1 (precat = 1)
*          m1 <- m1 * T (precat = 0)
*
*********************************************/

void  gtranslate_matrix( precat, m1, tx, ty, tz )

    int precat ;
    Matrix *m1 ;
    float tx, ty, tz ;
{
      Matrix tmat ;
      
      /* Initialize to unit matrix */
      gset_unit_matrix( &tmat ) ;

      /* Set scale values */
      tmat.m[3][0] = tx ;
      tmat.m[3][1] = ty ;
      tmat.m[3][2] = tz ;

      /* concatenate */
      gconcatenate_matrix( precat, m1, &tmat ) ;
}



/********************************************
*
*    rotate_x_matrix()
*    rotate_y_matrix()
*    rotate_z_matrix()
*
*          m1 <- ROT * m1 (precat = 1)
*          m1 <- m1 * ROT (precat = 0)
*
*********************************************/

void  grotate_x_matrix( precat, m1, rot )

    int precat ;
    Matrix *m1 ;
    float rot ;
{
      Matrix rmat ;
      float s_val, c_val ;
      
      /* Initialize to unit matrix */
      gset_unit_matrix( &rmat ) ;

      /* Set scale values */
      s_val = sin( rot * M_PI / 180.0 ) ;
      c_val = cos( rot * M_PI / 180.0 ) ;
      rmat.m[1][1] = c_val ;
      rmat.m[1][2] = s_val ;
      rmat.m[2][1] = -s_val ;
      rmat.m[2][2] = c_val ;

      /* concatenate */
      gconcatenate_matrix( precat, m1, &rmat ) ;
}




void  grotate_y_matrix( precat, m1, rot )

    int precat ;
    Matrix *m1 ;
    float rot ;
{
      Matrix rmat ;
      float s_val, c_val ;
      
      /* Initialize to unit matrix */
      gset_unit_matrix( &rmat ) ;

      /* Set scale values */
      s_val = sin( rot * M_PI / 180.0 ) ;
      c_val = cos( rot * M_PI / 180.0 ) ;
      rmat.m[0][0] = c_val ;
      rmat.m[0][2] = -s_val ;
      rmat.m[2][0] = s_val ;
      rmat.m[2][2] = c_val ;

      /* concatenate */
      gconcatenate_matrix( precat, m1, &rmat ) ;
}




void  grotate_z_matrix( precat, m1, rot )

    int precat ;
    Matrix *m1 ;
    float rot ;
{
      Matrix rmat ;
      float s_val, c_val ;
      
      /* Initialize to unit matrix */
      gset_unit_matrix( &rmat ) ;

      /* Set scale values */
      s_val = sin( rot * M_PI / 180.0 ) ;
      c_val = cos( rot * M_PI / 180.0 ) ;
      rmat.m[0][0] = c_val ;
      rmat.m[0][1] = s_val ;
      rmat.m[1][0] = -s_val ;
      rmat.m[1][1] = c_val ;

      /* concatenate */
      gconcatenate_matrix( precat, m1, &rmat ) ;
}


/********************************************
*
*    transform()
*
*          v1 <- v2 * mtx
*
*********************************************/

void gtransform( v1, v2, mtx )

    Vertex *v1, *v2 ;
    Matrix *mtx ;
{
      float x, y, z, w ;

      x  = v2->v[0] * mtx->m[0][0] ;
       y  = v2->v[0] * mtx->m[0][1] ;
        z  = v2->v[0] * mtx->m[0][2] ;
         w  = v2->v[0] * mtx->m[0][3] ;

      x += v2->v[1] * mtx->m[1][0] ;
       y += v2->v[1] * mtx->m[1][1] ;
        z += v2->v[1] * mtx->m[1][2] ;
         w += v2->v[1] * mtx->m[1][3] ;

      x += v2->v[2] * mtx->m[2][0] ;
       y += v2->v[2] * mtx->m[2][1] ;
        z += v2->v[2] * mtx->m[2][2] ;
         w += v2->v[2] * mtx->m[2][3] ;

      x += v2->v[3] * mtx->m[3][0] ;
       y += v2->v[3] * mtx->m[3][1] ;
        z += v2->v[3] * mtx->m[3][2] ;
         w += v2->v[3] * mtx->m[3][3] ;

      v1->v[0] = x ;
       v1->v[1] = y ;
        v1->v[2] = z ;
         v1->v[3] = w ;
}


/********************************************
*
*    inverse_matrix()
*
*          m1 <- inv(m2)
*
*********************************************/


void ginverse_matrix( m1, m2 )

    Matrix *m1, *m2 ;
{
      double detval ;
      double det(), cdet() ;
      
      /* det(m2) */
      detval = det( m2 ) ;

      /* Clamel's solution */
      m1->m[0][0] =  cdet( m2, 1,2,3, 1,2,3 ) / detval ;
      m1->m[0][1] = -cdet( m2, 0,2,3, 1,2,3 ) / detval ;
      m1->m[0][2] =  cdet( m2, 0,1,3, 1,2,3 ) / detval ;
      m1->m[0][3] = -cdet( m2, 0,1,2, 1,2,3 ) / detval ;

      m1->m[1][0] = -cdet( m2, 1,2,3, 0,2,3 ) / detval ;
      m1->m[1][1] =  cdet( m2, 0,2,3, 0,2,3 ) / detval ;
      m1->m[1][2] = -cdet( m2, 0,1,3, 0,2,3 ) / detval ;
      m1->m[1][3] =  cdet( m2, 0,1,2, 0,2,3 ) / detval ;

      m1->m[2][0] =  cdet( m2, 1,2,3, 0,1,3 ) / detval ;
      m1->m[2][1] = -cdet( m2, 0,2,3, 0,1,3 ) / detval ;
      m1->m[2][2] =  cdet( m2, 0,1,3, 0,1,3 ) / detval ;
      m1->m[2][3] = -cdet( m2, 0,1,2, 0,1,3 ) / detval ;

      m1->m[3][0] = -cdet( m2, 1,2,3, 0,1,2 ) / detval ;
      m1->m[3][1] =  cdet( m2, 0,2,3, 0,1,2 ) / detval ;
      m1->m[3][2] = -cdet( m2, 0,1,3, 0,1,2 ) / detval ;
      m1->m[3][3] =  cdet( m2, 0,1,2, 0,1,2 ) / detval ;
}



double det( m )

    Matrix *m ;
{
      double det_sum ;
      double cdet() ;
      
      /* Expand with respect to column 4 */
      det_sum = 0.0 ;
      if( m->m[0][3] != 0.0 )
	    det_sum -= m->m[0][3] * cdet( m, 1, 2, 3,  0, 1, 2 ) ;
      if( m->m[1][3] != 0.0 )
	    det_sum += m->m[1][3] * cdet( m, 0, 2, 3,  0, 1, 2 ) ;
      if( m->m[2][3] != 0.0 )
	    det_sum -= m->m[2][3] * cdet( m, 0, 1, 3,  0, 1, 2 ) ;
      if( m->m[3][3] != 0.0 )
	    det_sum += m->m[3][3] * cdet( m, 0, 1, 2,  0, 1, 2 ) ;

      return( det_sum ) ;
}


double cdet( m, r0, r1, r2, c0, c1, c2 )

    Matrix *m ;
    int r0, r1, r2, c0, c1, c2 ;
{
        double temp ;

        temp  = m->m[r0][c0] * m->m[r1][c1] * m->m[r2][c2] ;
        temp += m->m[r1][c0] * m->m[r2][c1] * m->m[r0][c2] ;
        temp += m->m[r2][c0] * m->m[r0][c1] * m->m[r1][c2] ;

        temp -= m->m[r2][c0] * m->m[r1][c1] * m->m[r0][c2] ;
        temp -= m->m[r1][c0] * m->m[r0][c1] * m->m[r2][c2] ;
        temp -= m->m[r0][c0] * m->m[r2][c1] * m->m[r1][c2] ;

        return( temp ) ;
}



/********************************************
*
*    normalize_vector()
*
*          v1 <- normalized( v2 )
*
*          W component is ignored.
*
*********************************************/


void gnormalize_vector( v1, v2 )

    Vertex *v1, *v2 ;
{
      float t0, t1, t2 ;

      t0 = v2->v[0] * v2->v[0] ;
       t1 = v2->v[1] * v2->v[1] ;
        t2 = v2->v[2] * v2->v[2] ;

      t0 = 1.0 / sqrt( t0 + t1 + t2 ) ;

      v1->v[0] = v2->v[0] * t0 ;
       v1->v[1] = v2->v[1] * t0 ;
        v1->v[2] = v2->v[2] * t0 ;
}


/********************************************
*
*    inner_product()
*
*          (v1.v2) <- inner_product( v1, v2 )
*
*          W component is ignored.
*
*********************************************/

float ginner_product( v1, v2 )

    Vertex *v1, *v2 ;
{
      float ip ;
      
      ip  = v1->v[0] * v2->v[0] ;
      ip += v1->v[1] * v2->v[1] ;
      ip += v1->v[2] * v2->v[2] ;

      return( ip ) ;
}


/********************************************
*
*    print_vector()
*
*
*********************************************/

void gprint_vector( v )

    Vertex *v ;
{
      printf( "(%g,%g,%g,%g)\n", v->v[0], v->v[1], v->v[2], v->v[3] ) ;
}
