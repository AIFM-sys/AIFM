/*
 * Copyright (C) 2005 Ben Skeggs.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * Authors:
 *   Ben Skeggs <darktama@iinet.net.au>
 *   Jerome Glisse <j.glisse@gmail.com>
 */
#ifndef __R500_FRAGPROG_H_
#define __R500_FRAGPROG_H_

#include "glheader.h"
#include "macros.h"
#include "enums.h"
#include "shader/prog_parameter.h"
#include "shader/prog_print.h"
#include "shader/program.h"
#include "shader/prog_instruction.h"

#include "r300_context.h"
#include "r300_state.h"
#include "radeon_program.h"

struct r500_fragment_program;

extern void r500TranslateFragmentShader(r300ContextPtr r300,
					struct r500_fragment_program *fp);

struct r500_fragment_program_compiler {
	r300ContextPtr r300;
	struct r500_fragment_program *fp;
	struct r500_fragment_program_code *code;
	struct gl_program *program;
};

extern GLboolean r500FragmentProgramEmit(struct r500_fragment_program_compiler *compiler);

#endif
