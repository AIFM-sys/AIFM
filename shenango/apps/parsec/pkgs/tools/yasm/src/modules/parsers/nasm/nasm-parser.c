/*
 * NASM-compatible parser
 *
 *  Copyright (C) 2001-2007  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <util.h>
/*@unused@*/ RCSID("$Id: nasm-parser.c,v 1.1.1.1 2012/03/29 17:20:59 uid42307 Exp $");

#include <libyasm.h>

#include "nasm-parser.h"


static void
nasm_parser_do_parse(yasm_object *object, yasm_preproc *pp,
                     int save_input, yasm_linemap *linemap,
                     yasm_errwarns *errwarns)
{
    yasm_parser_nasm parser_nasm;

    parser_nasm.object = object;
    parser_nasm.linemap = linemap;

    parser_nasm.locallabel_base = (char *)NULL;
    parser_nasm.locallabel_base_len = 0;

    parser_nasm.preproc = pp;
    parser_nasm.errwarns = errwarns;

    parser_nasm.prev_bc = yasm_section_bcs_first(object->cur_section);

    parser_nasm.save_input = save_input;

    parser_nasm.peek_token = NONE;

    parser_nasm.absstart = NULL;
    parser_nasm.abspos = NULL;

    /* initialize scanner structure */
    yasm_scanner_initialize(&parser_nasm.s);

    parser_nasm.state = INITIAL;

    /* yacc debugging, needs YYDEBUG set in bison.y.in to work */
    /* nasm_parser_debug = 1; */

    nasm_parser_parse(&parser_nasm);

    /*yasm_scanner_delete(&parser_nasm.s);*/

    /* Free locallabel base if necessary */
    if (parser_nasm.locallabel_base)
        yasm_xfree(parser_nasm.locallabel_base);

    /* Check for undefined symbols */
    yasm_symtab_parser_finalize(object->symtab, 0, errwarns);
}

/* Define valid preprocessors to use with this parser */
static const char *nasm_parser_preproc_keywords[] = {
    "raw",
    "nasm",
    NULL
};

/* Define parser structure -- see parser.h for details */
yasm_parser_module yasm_nasm_LTX_parser = {
    "NASM-compatible parser",
    "nasm",
    nasm_parser_preproc_keywords,
    "nasm",
    nasm_parser_do_parse
};
