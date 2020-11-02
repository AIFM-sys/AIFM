/*
 * x86 architecture description
 *
 *  Copyright (C) 2002-2007  Peter Johnson
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
/*@unused@*/ RCSID("$Id: x86arch.c,v 1.1.1.1 2012/03/29 17:21:00 uid42307 Exp $");

#include <libyasm.h>

#include "x86arch.h"


yasm_arch_module yasm_x86_LTX_arch;


static /*@only@*/ yasm_arch *
x86_create(const char *machine, const char *parser,
           /*@out@*/ yasm_arch_create_error *error)
{
    yasm_arch_x86 *arch_x86;
    unsigned int amd64_machine;

    *error = YASM_ARCH_CREATE_OK;

    if (yasm__strcasecmp(machine, "x86") == 0)
        amd64_machine = 0;
    else if (yasm__strcasecmp(machine, "amd64") == 0)
        amd64_machine = 1;
    else {
        *error = YASM_ARCH_CREATE_BAD_MACHINE;
        return NULL;
    }

    arch_x86 = yasm_xmalloc(sizeof(yasm_arch_x86));

    arch_x86->arch.module = &yasm_x86_LTX_arch;

    /* default to all instructions/features enabled */
    arch_x86->active_cpu = 0;
    arch_x86->cpu_enables_size = 1;
    arch_x86->cpu_enables = yasm_xmalloc(sizeof(wordptr));
    arch_x86->cpu_enables[0] = BitVector_Create(64, FALSE);
    BitVector_Fill(arch_x86->cpu_enables[0]);

    arch_x86->amd64_machine = amd64_machine;
    arch_x86->mode_bits = 0;
    arch_x86->force_strict = 0;
    arch_x86->default_rel = 0;

    if (yasm__strcasecmp(parser, "nasm") == 0)
        arch_x86->parser = X86_PARSER_NASM;
    else if (yasm__strcasecmp(parser, "gas") == 0
             || yasm__strcasecmp(parser, "gnu") == 0)
        arch_x86->parser = X86_PARSER_GAS;
    else {
        yasm_xfree(arch_x86);
        *error = YASM_ARCH_CREATE_BAD_PARSER;
        return NULL;
    }

    return (yasm_arch *)arch_x86;
}

static void
x86_destroy(/*@only@*/ yasm_arch *arch)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    unsigned int i;
    for (i=0; i<arch_x86->cpu_enables_size; i++)
        BitVector_Destroy(arch_x86->cpu_enables[i]);
    yasm_xfree(arch_x86->cpu_enables);
    yasm_xfree(arch);
}

static const char *
x86_get_machine(const yasm_arch *arch)
{
    const yasm_arch_x86 *arch_x86 = (const yasm_arch_x86 *)arch;
    if (arch_x86->amd64_machine)
        return "amd64";
    else
        return "x86";
}

static unsigned int
x86_get_address_size(const yasm_arch *arch)
{
    const yasm_arch_x86 *arch_x86 = (const yasm_arch_x86 *)arch;
    if (arch_x86->mode_bits != 0)
        return arch_x86->mode_bits;
    if (arch_x86->amd64_machine)
        return 64;
    else
        return 32;
}

static int
x86_set_var(yasm_arch *arch, const char *var, unsigned long val)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    if (yasm__strcasecmp(var, "mode_bits") == 0)
        arch_x86->mode_bits = (unsigned int)val;
    else if (yasm__strcasecmp(var, "force_strict") == 0)
        arch_x86->force_strict = (unsigned int)val;
    else if (yasm__strcasecmp(var, "default_rel") == 0) {
        if (arch_x86->mode_bits != 64)
            yasm_warn_set(YASM_WARN_GENERAL,
                          N_("ignoring default rel in non-64-bit mode"));
        else
            arch_x86->default_rel = (unsigned int)val;
    } else
        return 1;
    return 0;
}

static void
x86_dir_cpu(yasm_object *object, yasm_valparamhead *valparams,
            yasm_valparamhead *objext_valparams, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)object->arch;

    yasm_valparam *vp;
    yasm_vps_foreach(vp, valparams) {
        /*@null@*/ /*@dependent@*/ const char *s = yasm_vp_string(vp);
        if (s)
            yasm_x86__parse_cpu(arch_x86, s, strlen(s));
        else if (vp->type == YASM_PARAM_EXPR) {
            const yasm_intnum *intcpu;
            intcpu = yasm_expr_get_intnum(&vp->param.e, 0);
            if (!intcpu)
                yasm_error_set(YASM_ERROR_SYNTAX,
                               N_("invalid argument to [%s]"), "CPU");
            else {
                char strcpu[16];
                sprintf(strcpu, "%lu", yasm_intnum_get_uint(intcpu));
                yasm_x86__parse_cpu(arch_x86, strcpu, strlen(strcpu));
            }
        } else
            yasm_error_set(YASM_ERROR_SYNTAX, N_("invalid argument to [%s]"),
                           "CPU");
    }
}

static void
x86_dir_bits(yasm_object *object, yasm_valparamhead *valparams,
             yasm_valparamhead *objext_valparams, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)object->arch;
    yasm_valparam *vp;
    /*@only@*/ /*@null@*/ yasm_expr *e = NULL;
    const yasm_intnum *intn;
    long lval;

    if ((vp = yasm_vps_first(valparams)) && !vp->val &&
        (e = yasm_vp_expr(vp, object->symtab, line)) != NULL &&
        (intn = yasm_expr_get_intnum(&e, 0)) != NULL &&
        (lval = yasm_intnum_get_int(intn)) &&
        (lval == 16 || lval == 32 || lval == 64))
        arch_x86->mode_bits = (unsigned char)lval;
    else
        yasm_error_set(YASM_ERROR_VALUE, N_("invalid argument to [%s]"),
                       "BITS");
    if (e)
        yasm_expr_destroy(e);
}

static void
x86_dir_code16(yasm_object *object, yasm_valparamhead *valparams,
               yasm_valparamhead *objext_valparams, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)object->arch;
    arch_x86->mode_bits = 16;
}

static void
x86_dir_code32(yasm_object *object, yasm_valparamhead *valparams,
               yasm_valparamhead *objext_valparams, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)object->arch;
    arch_x86->mode_bits = 32;
}

static void
x86_dir_code64(yasm_object *object, yasm_valparamhead *valparams,
               yasm_valparamhead *objext_valparams, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)object->arch;
    arch_x86->mode_bits = 64;
}

static const unsigned char **
x86_get_fill(const yasm_arch *arch)
{
    const yasm_arch_x86 *arch_x86 = (const yasm_arch_x86 *)arch;

    /* Fill patterns that GAS uses. */
    static const unsigned char *fill16[16] = {
        NULL,                           /* unused                       */
        (const unsigned char *)
        "\x90",                         /* 1 - nop                      */
        (const unsigned char *)
        "\x89\xf6",                     /* 2 - mov si, si               */
        (const unsigned char *)
        "\x8d\x74\x00",                 /* 3 - lea si, [si+byte 0]      */
        (const unsigned char *)
        "\x8d\xb4\x00\x00",             /* 4 - lea si, [si+word 0]      */
        (const unsigned char *)
        "\x90"                          /* 5 - nop                      */
        "\x8d\xb4\x00\x00",             /*     lea si, [si+word 0]      */
        (const unsigned char *)
        "\x89\xf6"                      /* 6 - mov si, si               */
        "\x8d\xbd\x00\x00",             /*     lea di, [di+word 0]      */
        (const unsigned char *)
        "\x8d\x74\x00"                  /* 7 - lea si, [si+byte 0]      */
        "\x8d\xbd\x00\x00",             /*     lea di, [di+word 0]      */
        (const unsigned char *)
        "\x8d\xb4\x00\x00"              /* 8 - lea si, [si+word 0]      */
        "\x8d\xbd\x00\x00",             /*     lea di, [di+word 0]      */
        (const unsigned char *)
        "\xeb\x07\x90\x90\x90\x90\x90"  /* 9 - jmp $+9; nop fill        */
        "\x90\x90",
        (const unsigned char *)
        "\xeb\x08\x90\x90\x90\x90\x90"  /* 10 - jmp $+10; nop fill      */
        "\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x09\x90\x90\x90\x90\x90"  /* 11 - jmp $+11; nop fill      */
        "\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0a\x90\x90\x90\x90\x90"  /* 12 - jmp $+12; nop fill      */
        "\x90\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0b\x90\x90\x90\x90\x90"  /* 13 - jmp $+13; nop fill      */
        "\x90\x90\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0c\x90\x90\x90\x90\x90"  /* 14 - jmp $+14; nop fill      */
        "\x90\x90\x90\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0d\x90\x90\x90\x90\x90"  /* 15 - jmp $+15; nop fill      */
        "\x90\x90\x90\x90\x90\x90\x90\x90"
    };
    static const unsigned char *fill32[16] = {
        NULL,                           /* unused                       */
        (const unsigned char *)
        "\x90",                         /* 1 - nop                      */
        (const unsigned char *)
        "\x89\xf6",                     /* 2 - mov esi, esi             */
        (const unsigned char *)
        "\x8d\x76\x00",                 /* 3 - lea esi, [esi+byte 0]    */
        (const unsigned char *)
        "\x8d\x74\x26\x00",             /* 4 - lea esi, [esi*1+byte 0]  */
        (const unsigned char *)
        "\x90"                          /* 5 - nop                      */
        "\x8d\x74\x26\x00",             /*     lea esi, [esi*1+byte 0]  */
        (const unsigned char *)
        "\x8d\xb6\x00\x00\x00\x00",     /* 6 - lea esi, [esi+dword 0]   */
        (const unsigned char *)
        "\x8d\xb4\x26\x00\x00\x00\x00", /* 7 - lea esi, [esi*1+dword 0] */
        (const unsigned char *)
        "\x90"                          /* 8 - nop                      */
        "\x8d\xb4\x26\x00\x00\x00\x00", /*     lea esi, [esi*1+dword 0] */
#if 0
        /* GAS uses these */
        (const unsigned char *)
        "\x89\xf6"                      /* 9 - mov esi, esi             */
        "\x8d\xbc\x27\x00\x00\x00\x00", /*     lea edi, [edi*1+dword 0] */
        (const unsigned char *)
        "\x8d\x76\x00"                  /* 10 - lea esi, [esi+byte 0]   */
        "\x8d\xbc\x27\x00\x00\x00\x00", /*      lea edi, [edi+dword 0]  */
        (const unsigned char *)
        "\x8d\x74\x26\x00"              /* 11 - lea esi, [esi*1+byte 0] */
        "\x8d\xbc\x27\x00\x00\x00\x00", /*      lea edi, [edi*1+dword 0]*/
        (const unsigned char *)
        "\x8d\xb6\x00\x00\x00\x00"      /* 12 - lea esi, [esi+dword 0]  */
        "\x8d\xbf\x00\x00\x00\x00",     /*      lea edi, [edi+dword 0]  */
        (const unsigned char *)
        "\x8d\xb6\x00\x00\x00\x00"      /* 13 - lea esi, [esi+dword 0]  */
        "\x8d\xbc\x27\x00\x00\x00\x00", /*      lea edi, [edi*1+dword 0]*/
        (const unsigned char *)
        "\x8d\xb4\x26\x00\x00\x00\x00"  /* 14 - lea esi, [esi*1+dword 0]*/
        "\x8d\xbc\x27\x00\x00\x00\x00", /*      lea edi, [edi*1+dword 0]*/
#else
        /* But on newer processors, these are recommended */
        (const unsigned char *)
        "\xeb\x07\x90\x90\x90\x90\x90"  /* 9 - jmp $+9; nop fill        */
        "\x90\x90",
        (const unsigned char *)
        "\xeb\x08\x90\x90\x90\x90\x90"  /* 10 - jmp $+10; nop fill      */
        "\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x09\x90\x90\x90\x90\x90"  /* 11 - jmp $+11; nop fill      */
        "\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0a\x90\x90\x90\x90\x90"  /* 12 - jmp $+12; nop fill      */
        "\x90\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0b\x90\x90\x90\x90\x90"  /* 13 - jmp $+13; nop fill      */
        "\x90\x90\x90\x90\x90\x90",
        (const unsigned char *)
        "\xeb\x0c\x90\x90\x90\x90\x90"  /* 14 - jmp $+14; nop fill      */
        "\x90\x90\x90\x90\x90\x90\x90",
#endif
        (const unsigned char *)
        "\xeb\x0d\x90\x90\x90\x90\x90"  /* 15 - jmp $+15; nop fill      */
        "\x90\x90\x90\x90\x90\x90\x90\x90"
    };
    static const unsigned char *fill64[16] = {
        NULL,                           /* unused                       */
        (const unsigned char *)
        "\x90",                         /* 1 - nop                      */
        (const unsigned char *)
        "\x66\x90",                     /* 2 - o16; nop                 */
        (const unsigned char *)
        "\x66\x66\x90",                 /* 3 - o16; o16; nop            */
        (const unsigned char *)
        "\x66\x66\x66\x90",             /* 4 - o16; o16; o16; nop       */
        (const unsigned char *)
        "\x66\x66\x90\x66\x90",         /* 5 */
        (const unsigned char *)
        "\x66\x66\x90\x66\x66\x90",     /* 6 */
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x90", /* 7 */
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x66"  /* 8 */
        "\x90",
        (const unsigned char *)
        "\x66\x66\x90\x66\x66\x90\x66"  /* 9 */
        "\x66\x90",
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x90"  /* 10 */
        "\x66\x66\x90",
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x66"  /* 11 */
        "\x90\x66\x66\x90",
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x66"  /* 12 */
        "\x90\x66\x66\x66\x90",
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x90"  /* 13 */
        "\x66\x66\x90\x66\x66\x90",
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x66"  /* 14 */
        "\x90\x66\x66\x90\x66\x66\x90",
        (const unsigned char *)
        "\x66\x66\x66\x90\x66\x66\x66"  /* 15 */
        "\x90\x66\x66\x66\x90\x66\x66\x90"
    };
    switch (arch_x86->mode_bits) {
        case 16:
            return fill16;
        case 32:
            return fill32;
        case 64:
            return fill64;
        default:
            yasm_error_set(YASM_ERROR_VALUE,
                           N_("Invalid mode_bits in x86_get_fill"));
            return NULL;
    }
}

unsigned int
yasm_x86__get_reg_size(uintptr_t reg)
{
    switch ((x86_expritem_reg_size)(reg & ~0xFUL)) {
        case X86_REG8:
        case X86_REG8X:
            return 8;
        case X86_REG16:
            return 16;
        case X86_REG32:
        case X86_CRREG:
        case X86_DRREG:
        case X86_TRREG:
            return 32;
        case X86_REG64:
        case X86_MMXREG:
            return 64;
        case X86_XMMREG:
            return 128;
        case X86_YMMREG:
            return 256;
        case X86_FPUREG:
            return 80;
        default:
            yasm_error_set(YASM_ERROR_VALUE, N_("unknown register size"));
    }
    return 0;
}

static unsigned int
x86_get_reg_size(yasm_arch *arch, uintptr_t reg)
{
    return yasm_x86__get_reg_size(reg);
}

static uintptr_t
x86_reggroup_get_reg(yasm_arch *arch, uintptr_t reggroup,
                     unsigned long regindex)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    switch ((x86_expritem_reg_size)(reggroup & ~0xFUL)) {
        case X86_XMMREG:
        case X86_YMMREG:
            if (arch_x86->mode_bits == 64) {
                if (regindex > 15)
                    return 0;
                return reggroup | (regindex & 15);
            }
            /*@fallthrough@*/
        case X86_MMXREG:
        case X86_FPUREG:
            if (regindex > 7)
                return 0;
            return reggroup | (regindex & 7);
        default:
            yasm_error_set(YASM_ERROR_VALUE, N_("bad register group"));
    }
    return 0;
}

static void
x86_reg_print(yasm_arch *arch, uintptr_t reg, FILE *f)
{
    static const char *name8[] = {"al","cl","dl","bl","ah","ch","dh","bh"};
    static const char *name8x[] = {
        "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
        "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
    };
    static const char *name16[] = {
        "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
        "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
    };
    static const char *name32[] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };
    static const char *name64[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };

    switch ((x86_expritem_reg_size)(reg & ~0xFUL)) {
        case X86_REG8:
            fprintf(f, "%s", name8[reg&0xF]);
            break;
        case X86_REG8X:
            fprintf(f, "%s", name8x[reg&0xF]);
            break;
        case X86_REG16:
            fprintf(f, "%s", name16[reg&0xF]);
            break;
        case X86_REG32:
            fprintf(f, "%s", name32[reg&0xF]);
            break;
        case X86_REG64:
            fprintf(f, "%s", name64[reg&0xF]);
            break;
        case X86_MMXREG:
            fprintf(f, "mm%d", (int)(reg&0xF));
            break;
        case X86_XMMREG:
            fprintf(f, "xmm%d", (int)(reg&0xF));
            break;
        case X86_YMMREG:
            fprintf(f, "ymm%d", (int)(reg&0xF));
            break;
        case X86_CRREG:
            fprintf(f, "cr%d", (int)(reg&0xF));
            break;
        case X86_DRREG:
            fprintf(f, "dr%d", (int)(reg&0xF));
            break;
        case X86_TRREG:
            fprintf(f, "tr%d", (int)(reg&0xF));
            break;
        case X86_FPUREG:
            fprintf(f, "st%d", (int)(reg&0xF));
            break;
        default:
            yasm_error_set(YASM_ERROR_VALUE, N_("unknown register size"));
    }
}

static void
x86_segreg_print(yasm_arch *arch, uintptr_t segreg, FILE *f)
{
    static const char *name[] = {"es","cs","ss","ds","fs","gs"};
    fprintf(f, "%s", name[segreg&7]);
}

/* Define x86 machines -- see arch.h for details */
static const yasm_arch_machine x86_machines[] = {
    { "IA-32 and derivatives", "x86" },
    { "AMD64", "amd64" },
    { NULL, NULL }
};

static const yasm_directive x86_directives[] = {
    { "cpu",            "nasm", x86_dir_cpu,    YASM_DIR_ARG_REQUIRED },
    { "bits",           "nasm", x86_dir_bits,   YASM_DIR_ARG_REQUIRED },
    { ".code16",        "gas",  x86_dir_code16, YASM_DIR_ANY },
    { ".code32",        "gas",  x86_dir_code32, YASM_DIR_ANY },
    { ".code64",        "gas",  x86_dir_code64, YASM_DIR_ANY },
    { NULL, NULL, NULL, 0 }
};

/* Define arch structure -- see arch.h for details */
yasm_arch_module yasm_x86_LTX_arch = {
    "x86 (IA-32 and derivatives), AMD64",
    "x86",
    x86_directives,
    x86_create,
    x86_destroy,
    x86_get_machine,
    x86_get_address_size,
    x86_set_var,
    yasm_x86__parse_check_insnprefix,
    yasm_x86__parse_check_regtmod,
    x86_get_fill,
    yasm_x86__floatnum_tobytes,
    yasm_x86__intnum_tobytes,
    x86_get_reg_size,
    x86_reggroup_get_reg,
    x86_reg_print,
    x86_segreg_print,
    yasm_x86__ea_create_expr,
    yasm_x86__ea_destroy,
    yasm_x86__ea_print,
    yasm_x86__create_empty_insn,
    x86_machines,
    "x86",
    16,
    1
};
