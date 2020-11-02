/* VIPS universal main program.
 *
 * J. Cupitt, 8/4/93.
 * 12/5/06
 * 	- use GOption. g_*_prgname()
 * 16/7/06
 * 	- hmm, was broken for function name as argv1 case
 * 11/7/06
 * 	- add "all" option to -l
 * 14/7/06
 * 	- ignore "--" arguments.
 * 2/9/06
 * 	- do less init ... im_init_world() does more now
 * 18/8/06
 * 	- use IM_EXEEXT
 * 16/10/06
 * 	- add --version
 * 17/10/06
 * 	- add --swig
 * 	- cleanups
 * 	- remove --swig again, sigh
 * 	- add throw() decls to C++ to help SWIG
 * 14/1/07
 * 	- add --list packages
 * 26/2/07
 * 	- add input *VEC arg types to C++ binding
 * 17/8/08
 * 	- add --list formats
 * 29/11/08
 * 	- add --list interpolators
 * 9/2/09
 * 	- and now we just have --list packages/classes/package-name
 * 13/11/09
 * 	- drop _f postfixes, drop many postfixes
 * 24/6/10
 * 	- less chatty error messages
 * 	- oops, don't rename "copy_set" as "copy_"
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG_FATAL
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include <vips/vips.h>

#ifdef OS_WIN32
#define strcasecmp(a,b) _stricmp(a,b)
#endif

static char *main_option_list = NULL;
static char *main_option_usage = NULL;
static char *main_option_plugin = NULL;
static gboolean main_option_links;
static char *main_option_cpph = NULL;
static char *main_option_cppc = NULL;
static gboolean *main_option_version;

static GOptionEntry main_option[] = {
	{ "list", 'l', 0, G_OPTION_ARG_STRING, &main_option_list, 
		N_( "list operations in PACKAGE "
			"(or \"all\", \"packages\", \"classes\")" ),
		N_( "PACKAGE" ) },
	{ "usage", 'u', 0, G_OPTION_ARG_STRING, &main_option_usage, 
		N_( "show usage message for OPERATION" ), 
		N_( "OPERATION" ) },
	{ "plugin", 'p', 0, G_OPTION_ARG_FILENAME, &main_option_plugin, 
		N_( "load PLUGIN" ), 
		N_( "PLUGIN" ) },
	{ "links", 'k', 0, G_OPTION_ARG_NONE, &main_option_links, 
		N_( "print link lines for all operations" ), NULL },
	{ "cpph", 'h', 0, G_OPTION_ARG_STRING, &main_option_cpph, 
		N_( "print C++ decls for PACKAGE (or \"all\")" ), 
		N_( "PACKAGE" ) },
	{ "cppc", 'c', 0, G_OPTION_ARG_STRING, &main_option_cppc, 
		N_( "print C++ binding for PACKAGE (or \"all\")" ), 
		N_( "PACKAGE" ) },
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &main_option_version, 
		N_( "print im_version_string" ), NULL },
	{ NULL }
};

typedef void *(*map_name_fn)( im_function * );

/* Loop over a package.
 */
static void *
map_package( im_package *pack, map_name_fn fn )
{
	int i;
	void *result;

	for( i = 0; i < pack->nfuncs; i++ ) 
		if( (result = fn( pack->table[i] )) )
			return( result );

	return( NULL );
}

/* Apply a function to a vips operation, or map over a package of operations.
 */
static void *
map_name( const char *name, map_name_fn fn )
{
	im_package *pack;
	im_function *func;

	if( strcmp( name, "all" ) == 0 ) 
		/* Do all packages.
		 */
		im_map_packages( (VSListMap2Fn) map_package, fn );
	else if( (pack = im_find_package( name )) )
		/* Do one package.
		 */
		map_package( pack, fn );
	else if( (func = im_find_function( name )) )
		/* Do a single function.
		 */
		fn( func );
	else {
		im_error( "map_name", 
			_( "no package or function \"%s\"" ), name );
		return( fn );
	}

	return( NULL );
}

static void *
list_package( im_package *pack )
{
	printf( "%-20s - %d operations\n", pack->name, pack->nfuncs );
	
	return( NULL );
}

static void *
list_function( im_function *func )
{
	printf( "%-20s - %s\n", func->name, _( func->desc ) );
	
	return( NULL );
}

static void *
list_class( VipsObjectClass *class )
{
	vips_object_print_class( class );

	return( NULL );
}

static void
print_list( const char *name )
{
	if( strcmp( name, "packages" ) == 0 ) 
		im_map_packages( (VSListMap2Fn) list_package, NULL );
	else if( strcmp( name, "classes" ) == 0 ) 
		vips_class_map_concrete_all( g_type_from_name( "VipsObject" ), 
			(VipsClassMap) list_class, NULL );
	else {
		if( map_name( name, list_function ) )
			error_exit( "unknown package \"%s\"", name ); 
	}
}

/* Is s1 a prefix of s2?
 */
static int
isprefix( const char *s1, const char *s2 )
{
	while( *s1 && *s1 == *s2 ) {
		s1++;
		s2++;
	}

	return( *s1 == '\0' );
}

/* Is s1 a postfix of s2?
 */
static int
ispostfix( const char *s1, const char *s2 )
{
	int l1 = strlen( s1 );
	int l2 = strlen( s2 );

	if( l2 < l1 )
		return( 0 );
	
	return( strcasecmp( s1, s2 + l2 - l1 ) == 0 );
}

/* Print "ln -s" lines for this package.
 */
static void *
print_links( im_package *pack )
{
	int i;

	for( i = 0; i < pack->nfuncs; i++ ) 
		printf( "rm -f %s" IM_EXEEXT "; "
			"ln -s vips" IM_EXEEXT " %s" IM_EXEEXT "\n", 
			pack->table[i]->name, pack->table[i]->name );

	return( NULL );
}

/* Does a function have any printing output?
 */
static int
has_print( im_function *fn )
{
	int i;

	for( i = 0; i < fn->argc; i++ )
		if( fn->argv[i].print )
			return( -1 );

	return( 0 );
}

/* Print a usage string from an im_function descriptor.
 */
static void
usage( im_function *fn )
{
	int i;
	im_package *pack = im_package_of_function( fn->name );

	/* Don't print the prgname if we're being run as a symlink.
	 */
	fprintf( stderr, "usage: " );
	if( im_isprefix( "vips", g_get_prgname() ) ) 
		fprintf( stderr, "%s ", g_get_prgname() );
	fprintf( stderr, "%s ", fn->name ); 

	/* Print args requiring command-line input.
	 */
	for( i = 0; i < fn->argc; i++ )
		if( fn->argv[i].desc->flags & IM_TYPE_ARG )
			fprintf( stderr, "%s ", fn->argv[i].name );

	/* Print types of command line args.
	 */
	fprintf( stderr, "\nwhere:\n" );
	for( i = 0; i < fn->argc; i++ )
		if( fn->argv[i].desc->flags & IM_TYPE_ARG )
			fprintf( stderr, "\t%s is of type \"%s\"\n", 
				fn->argv[i].name, fn->argv[i].desc->type );

	/* Print output print args.
	 */
	if( has_print( fn ) ) {
		fprintf( stderr, "prints:\n" );
		for( i = 0; i < fn->argc; i++ )
			if( fn->argv[i].print ) 
				fprintf( stderr, "\t%s of type \"%s\"\n", 
					fn->argv[i].name, 
					fn->argv[i].desc->type );
	}

	/* Print description of this function, and package it comes from.
	 */
	fprintf( stderr, "%s", _( fn->desc ) );
	if( pack )
		fprintf( stderr, ", from package \"%s\"", pack->name );
	fprintf( stderr, "\n" );

	/* Print any flags this function has.
	 */
	fprintf( stderr, "flags: " );
	if( fn->flags & IM_FN_PIO )
		fprintf( stderr, "(PIO function) " );
	else
		fprintf( stderr, "(WIO function) " );
	if( fn->flags & IM_FN_TRANSFORM )
		fprintf( stderr, "(coordinate transformer) " );
	else
		fprintf( stderr, "(no coordinate transformation) " );
	if( fn->flags & IM_FN_PTOP )
		fprintf( stderr, "(point-to-point operation) " );
	else
		fprintf( stderr, "(area operation) " );
	if( fn->flags & IM_FN_NOCACHE )
		fprintf( stderr, "(nocache operation) " );
	else
		fprintf( stderr, "(result can be cached) " );

	fprintf( stderr, "\n" );
}

/* Convert VIPS type name to C++ type name. NULL for type unsupported by C++
 * layer.
 */
static char *
vips2cpp( im_type_desc *ty )
{
	int k;

	/* VIPS types.
	 */
	static char *vtypes[] = {
		IM_TYPE_DOUBLE,
		IM_TYPE_INT,  
		IM_TYPE_COMPLEX,
		IM_TYPE_STRING,
		IM_TYPE_IMAGE,
		IM_TYPE_IMASK,
		IM_TYPE_DMASK,
		IM_TYPE_DISPLAY,
		IM_TYPE_IMAGEVEC,
		IM_TYPE_DOUBLEVEC,
		IM_TYPE_INTVEC
	};

	/* Corresponding C++ types.
	 */
	static char *ctypes[] = {
		"double",
		"int",
		"std::complex<double>",
		"char*",
		"VImage",
		"VIMask",
		"VDMask",
		"VDisplay",
		"std::vector<VImage>",
		"std::vector<double>",
		"std::vector<int>"
	};

	for( k = 0; k < IM_NUMBER( vtypes ); k++ )
		if( strcmp( ty->type, vtypes[k] ) == 0 ) 
			return( ctypes[k] );

	return( NULL );
}

/* Test a function definition for C++ suitability.
 */
static int
is_cppable( im_function *fn )
{
	int j;

	/* Check we know all the types.
	 */
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		if( !vips2cpp( ty ) )
			return( 0 );
	}

	/* We dont wrap output IMAGEVEC/DOUBLEVEC/INTVEC.
	 */
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		if( ty->flags & IM_TYPE_OUTPUT ) 
			if( strcmp( ty->type, IM_TYPE_IMAGEVEC ) == 0 ||
				strcmp( ty->type, IM_TYPE_DOUBLEVEC ) == 0 ||
				strcmp( ty->type, IM_TYPE_INTVEC ) == 0 )
			return( 0 );
	}

	/* Must be at least one image argument (input or output) ... since we 
	 * get inserted in the VImage class. Other funcs get wrapped by hand.
	 */
	for( j = 0; j < fn->argc; j++ ) 
		if( strcmp( fn->argv[j].desc->type, IM_TYPE_IMAGE ) == 0 ) 
			break;
	if( j == fn->argc )
		return( 0 );

	return( -1 );
}

/* Search for the first output arg, and the first IMAGE input arg.
 */
static void
find_ioargs( im_function *fn, int *ia, int *oa )
{
	int j;

	/* Look for first output arg - this will be the result of the
	 * function.
	 */
	*oa = -1;
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		if( ty->flags & IM_TYPE_OUTPUT ) {
			*oa = j;
			break;
		}
	}

	/* Look for first input IMAGE arg. This will become the implicit
	 * "this" arg.
	 */
	*ia = -1;
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		if( !(ty->flags & IM_TYPE_OUTPUT) && 
			strcmp( ty->type, IM_TYPE_IMAGE ) == 0 ) {
				*ia = j;
				break;
			}
	}
}

static gboolean
drop_postfix( char *str, const char *postfix )
{
	if( ispostfix( postfix, str ) ) {
		str[strlen( str ) - strlen( postfix )] = '\0';

		return( TRUE );
	}

	return( FALSE );
}

/* Turn a VIPS name into a C++ name. Eg. im_lintra_vec becomes lin.
 */
static void
c2cpp_name( const char *in, char *out )
{
	static const char *dont_drop[] = {
		"_set",
	};
	static const char *drop[] = {
		"_vec",
		"const",
		"tra",
		"set",
		"_f"
	};

	int i;
	gboolean changed;

	/* Copy, chopping off "im_" prefix.
	 */
	if( isprefix( "im_", in ) )
		strcpy( out, in + 3 );
	else
		strcpy( out, in );

	/* Repeatedly drop postfixes while we can. Stop if we see a dont_drop
	 * postfix.
	 */
	do {
		gboolean found;

		found = FALSE;
		for( i = 0; i < IM_NUMBER( dont_drop ); i++ )
			if( ispostfix( dont_drop[i], out ) ) {
				found = TRUE;
				break;
			}
		if( found )
			break;

		changed = FALSE;
		for( i = 0; i < IM_NUMBER( drop ); i++ )
			changed |= drop_postfix( out, drop[i] );
	} while( changed );
}

/* Print prototype for a function (ie. will be followed by code). 
 *
 * Eg.:
 *	VImage VImage::lin( double a, double b ) throw( VError )
 */
static void *
print_cppproto( im_function *fn )
{
	int j;
	char name[4096];
	int oa, ia;
	int flg;

	/* If it's not cppable, do nothing.
	 */
	if( !is_cppable( fn ) )
		return( NULL );

	/* Make C++ name.
	 */
	c2cpp_name( fn->name, name );

	/* Find input and output args. 
	 */
	find_ioargs( fn, &ia, &oa );

	/* Print output type.
	 */
	if( oa == -1 )
		printf( "void " );
	else 
		printf( "%s ", vips2cpp( fn->argv[oa].desc ) );

	printf( "VImage::%s(", name );

	/* Print arg list.
	 */
	flg = 0;
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		/* Skip ia and oa.
		 */
		if( j == ia || j == oa )
			continue;

		/* Print arg type.
		 */
		if( flg )
			printf( ", %s", vips2cpp( ty ) );
		else {
			printf( " %s", vips2cpp( ty ) );
			flg = 1;
		}

		/* If it's an putput arg, print a "&" to make a reference
		 * argument.
		 */
		if( ty->flags & IM_TYPE_OUTPUT )
			printf( "&" );

		/* Print arg name.
		 */
		printf( " %s", fn->argv[j].name );
	}

	/* End of arg list!
	 */
	if( flg )
		printf( " " );
	printf( ") throw( VError )\n" );

	return( NULL );
}

/* Print cpp decl for a function. 
 *
 * Eg.
 *	VImage lin( double, double ) throw( VError );
 */
static void *
print_cppdecl( im_function *fn )
{
	int j;
	char name[4096];
	int oa, ia;
	int flg;

	/* If it's not cppable, do nothing.
	 */
	if( !is_cppable( fn ) )
		return( NULL );

	/* Make C++ name.
	 */
	c2cpp_name( fn->name, name );

	/* Find input and output args. 
	 */
	find_ioargs( fn, &ia, &oa );
	if( ia == -1 ) 
		/* No input image, so make it a static in the class
		 * declaration.
		 */
		printf( "static " );

	/* Print output type.
	 */
	if( oa == -1 )
		printf( "void " );
	else 
		printf( "%s ", vips2cpp( fn->argv[oa].desc ) );

	/* Print function name and start arg list.
	 */
	printf( "%s(", name );

	/* Print arg list.
	 */
	flg = 0;
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		/* Skip ia and oa.
		 */
		if( j == ia || j == oa )
			continue;

		/* Print arg type.
		 */
		if( flg )
			printf( ", %s", vips2cpp( ty ) );
		else {
			printf( " %s", vips2cpp( ty ) );
			flg = 1;
		}

		/* If it's an putput arg, print a "&" to make a reference
		 * argument.
		 */
		if( ty->flags & IM_TYPE_OUTPUT )
			printf( "&" );
	}

	/* End of arg list!
	 */
	if( flg )
		printf( " " );

	printf( ") throw( VError );\n" );

	return( NULL );
}

static void
print_invec( int j, const char *arg, 
	const char *vips_name, const char *c_name, const char *extract )
{
	printf( "\t((%s*) _vec.data(%d))->n = %s.size();\n",
		vips_name, j, arg );
	printf( "\t((%s*) _vec.data(%d))->vec = new %s[%s.size()];\n",
		vips_name, j, c_name, arg );
	printf( "\tfor( unsigned int i = 0; i < %s.size(); i++ )\n",
		arg );
	printf( "\t\t((%s*) _vec.data(%d))->vec[i] = %s[i]%s;\n",
		vips_name, j, arg, extract );
}

/* Print the definition for a function.
 */
static void *
print_cppdef( im_function *fn )
{
	int j;
	int ia, oa;

	/* If it's not cppable, do nothing.
	 */
	if( !is_cppable( fn ) )
		return( NULL );

	find_ioargs( fn, &ia, &oa );

	printf( "// %s: %s\n", fn->name, _( fn->desc ) );
	print_cppproto( fn );
	printf( "{\n" );

	/* Declare the implicit input image.
	 */
	if( ia != -1 )
		printf( "\tVImage %s = *this;\n", fn->argv[ia].name );

	/* Declare return value, if any.
	 */
	if( oa != -1 )
		printf( "\t%s %s;\n\n", 
			vips2cpp( fn->argv[oa].desc ),
			fn->argv[oa].name );

	/* Declare the arg vector.
	 */
	printf( "\tVargv _vec( \"%s\" );\n\n", fn->name );

	/* Create the input args.
	 */
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		/* Images are special - have to init the vector, even
		 * for output args. Have to translate VImage.
		 */
		if( strcmp( ty->type, IM_TYPE_IMAGE ) == 0 ) {
			printf( "\t_vec.data(%d) = %s.image();\n",
				j, fn->argv[j].name );
			continue;
		}

		/* For output masks, we have to set an input filename. Not
		 * freed, so constant string is OK.
		 */
		if( (ty->flags & IM_TYPE_OUTPUT) && 
			(strcmp( ty->type, IM_TYPE_IMASK ) == 0 ||
			strcmp( ty->type, IM_TYPE_DMASK ) == 0) ) {
			printf( "\t((im_mask_object*) _vec.data(%d))->name = "
				"(char*)\"noname\";\n", j );
			continue;
		}

		/* Skip other output args.
		 */
		if( ty->flags & IM_TYPE_OUTPUT )
			continue;

		if( strcmp( ty->type, IM_TYPE_IMASK ) == 0 )
			/* Mask types are different - have to use
			 * im_mask_object.
			 */
			printf( "\t((im_mask_object*) "
				"_vec.data(%d))->mask = %s.mask().iptr;\n",
				j, fn->argv[j].name );
		else if( strcmp( ty->type, IM_TYPE_DMASK ) == 0 ) 
			printf( "\t((im_mask_object*) "
				"_vec.data(%d))->mask = %s.mask().dptr;\n",
				j, fn->argv[j].name );
		else if( strcmp( ty->type, IM_TYPE_DISPLAY ) == 0 )
			/* Display have to use VDisplay.
			 */
			printf( "\t_vec.data(%d) = %s.disp();\n",
				j, fn->argv[j].name );
		else if( strcmp( ty->type, IM_TYPE_STRING ) == 0 )
			/* Zap input strings directly into _vec.
			 */
			printf( "\t_vec.data(%d) = (im_object) %s;\n",
				j, fn->argv[j].name );
		else if( strcmp( ty->type, IM_TYPE_IMAGEVEC ) == 0 ) 
			print_invec( j, fn->argv[j].name, 
				"im_imagevec_object", "IMAGE *", ".image()" );
		else if( strcmp( ty->type, IM_TYPE_DOUBLEVEC ) == 0 ) 
			print_invec( j, fn->argv[j].name, 
				"im_doublevec_object", "double", "" );
		else if( strcmp( ty->type, IM_TYPE_INTVEC ) == 0 ) 
			print_invec( j, fn->argv[j].name, 
				"im_intvec_object", "int", "" );
		else
			/* Just use vips2cpp().
			 */
			printf( "\t*((%s*) _vec.data(%d)) = %s;\n",
				vips2cpp( ty ), j, fn->argv[j].name );
	}

	/* Call function.
	 */
	printf( "\t_vec.call();\n" );

	/* Extract output args.
	 */
	for( j = 0; j < fn->argc; j++ ) {
		im_type_desc *ty = fn->argv[j].desc;

		/* Skip input args.
		 */
		if( !(ty->flags & IM_TYPE_OUTPUT) )
			continue;

		/* Skip images (done on input side, really).
		 */
		if( strcmp( ty->type, IM_TYPE_IMAGE ) == 0 )
			continue;

		if( strcmp( ty->type, IM_TYPE_IMASK ) == 0 ||
			strcmp( ty->type, IM_TYPE_DMASK ) == 0 ) 
			/* Mask types are different - have to use
			 * im_mask_object.
			 */
			printf( "\t%s.embed( (DOUBLEMASK *)((im_mask_object*)"
				"_vec.data(%d))->mask );\n",
				fn->argv[j].name, j );
		else if( strcmp( ty->type, IM_TYPE_STRING ) == 0 )
			/* Strings are grabbed out of the vec.
			 */
			printf( "\t%s = (char*) _vec.data(%d);\n",
				fn->argv[j].name, j ); 
		else 
			/* Just use vips2cpp().
			 */
			printf( "\t%s = *((%s*)_vec.data(%d));\n",
				fn->argv[j].name, vips2cpp( ty ), j ); 
	}

	/* Note dependancies if out is an image and this function uses
	 * PIO.
	 */
	if( oa != -1 ) {
		im_type_desc *ty = fn->argv[oa].desc;
		
		if( strcmp( ty->type, IM_TYPE_IMAGE ) == 0 &&
			(fn->flags & IM_FN_PIO) ) {
			/* Loop for all input args again ..
			 */
			for( j = 0; j < fn->argc; j++ ) {
				im_type_desc *ty2 = fn->argv[j].desc;

				/* Skip output args.
				 */
				if( ty2->flags & IM_TYPE_OUTPUT )
					continue;

				/* Input image.
				 */
				if( strcmp( ty2->type, IM_TYPE_IMAGE ) == 0 ) 
					printf( "\t%s._ref->addref( "
						"%s._ref );\n",
						fn->argv[oa].name,
						fn->argv[j].name );
				else if( strcmp( ty2->type, IM_TYPE_IMAGEVEC ) 
					== 0 ) {
					/* The out depends on every image in
					 * the input vector.
					 */
					printf( "\tfor( unsigned int i = 0; "
						"i < %s.size(); i++ )\n",
						fn->argv[j].name );
					printf( "\t\t%s._ref->addref( "
						"%s[i]._ref );\n",
						fn->argv[oa].name,
						fn->argv[j].name );
				}
			}
		}
	}

	/* Return result.
	 */
	if( oa != -1 )
		printf( "\n\treturn( %s );\n", fn->argv[oa].name );

	printf( "}\n\n" );

	return( NULL );
}

/* Print C++ decls for function, package or all.
 */
static void
print_cppdecls( char *name )
{
	printf( "// this file automatically generated from\n"
		"// VIPS library %s\n", im_version_string() );

	if( map_name( name, print_cppdecl ) )
		error_exit( "unknown package \"%s\"", name ); 
}

/* Print C++ bindings for function, package or all.
 */
static void
print_cppdefs( char *name )
{
	printf( "// this file automatically generated from\n"
		"// VIPS library %s\n", im_version_string() );

	if( map_name( name, print_cppdef ) )
		error_exit( "unknown package \"%s\"", name ); 
}

/* VIPS universal main program. 
 */
int
main( int argc, char **argv )
{
	GOptionContext *context;
	GError *error = NULL;
	im_function *fn;
	int i, j;

	if( im_init_world( argv[0] ) )
	        error_exit( NULL );
	textdomain( GETTEXT_PACKAGE );
	setlocale( LC_ALL, "" );

#ifdef DEBUG_FATAL
	/* Set masks for debugging ... stop on any problem. 
	 */
	g_log_set_always_fatal(
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING );

	fprintf( stderr, "*** DEBUG_FATAL: will abort() on first warning\n" );
#endif /*!DEBUG_FATAL*/

        context = g_option_context_new( _( "- VIPS driver program" ) );

	g_option_context_add_main_entries( context,
		main_option, GETTEXT_PACKAGE );
	g_option_context_add_group( context, im_get_option_group() );

	if( !g_option_context_parse( context, &argc, &argv, &error ) ) {
		if( error ) {
			fprintf( stderr, "%s\n", error->message );
			g_error_free( error );
		}

		error_exit( "try \"%s --help\"", g_get_prgname() );
	}

	g_option_context_free( context );

	if( main_option_plugin ) {
		if( !im_load_plugin( main_option_plugin ) )
			error_exit( NULL ); 
	}
	if( main_option_cpph ) 
		print_cppdecls( main_option_cpph );
	if( main_option_cppc ) 
		print_cppdefs( main_option_cppc );
	if( main_option_links )
		im_map_packages( (VSListMap2Fn) print_links, NULL );
	if( main_option_list ) 
		print_list( main_option_list );
	if( main_option_usage ) {
		if( !(fn = im_find_function( main_option_usage )) )
			error_exit( NULL );
		usage( fn );
	}
	if( main_option_version ) 
		printf( "vips-%s\n", im_version_string() );

	/* Remove any "--" argument. If one of our arguments is a negative
	 * number, the user will need to have added the "--" flag to stop
	 * GOption parsing. But "--" is still passed down to us and we need to
	 * ignore it.
	 */
	for( i = 1; i < argc - 1; i++ )
		if( strcmp( argv[i], "--" ) == 0 ) {
			for( j = i; j < argc; j++ )
				argv[j] = argv[j + 1];

			argc -= 1;
		}

	/* Should we try to run the thing we are named as?
	 */
	if( !im_isprefix( "vips", g_get_prgname() ) ) {
		char name[256];

		/* Drop any .exe suffix.
		 */
		im_strncpy( name, g_get_prgname(), 256 );
		if( ispostfix( ".exe", name ) )
			name[strlen( name ) - 4] = '\0';

		/* If unknown, try with "im_" prepended.
		 */
		if( !(fn = im_find_function( name )) ) {
			im_snprintf( name, 256, "im_%s", g_get_prgname() );
			if( ispostfix( ".exe", name ) )
				name[strlen( name ) - 4] = '\0';

			if( !(fn = im_find_function( name )) )
				error_exit( NULL );
		}

		/* Execute it!
		 */
		if( im_run_command( name, argc - 1, argv + 1 ) ) {
			/* If there are no arguments and the operation failed,
			 * show usage. There are no-arg operations, so we have
			 * to try running it.
			 */
			if( argc == 1 )
				usage( fn );
			else
				error_exit( NULL );
		}
	}
	else if( argc > 1 ) {
		/* Nope ... run the first arg instead.
		 */
		if( !(fn = im_find_function( argv[1] )) )
			error_exit( NULL );

		if( im_run_command( argv[1], argc - 2, argv + 2 ) ) {
			if( argc == 2 ) 
				usage( fn );
			else
				error_exit( NULL );
		}
	}

	im_close_plugins();

	return( 0 );
}
