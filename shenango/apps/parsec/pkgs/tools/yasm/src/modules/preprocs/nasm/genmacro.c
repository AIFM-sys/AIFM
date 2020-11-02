/* $Id: genmacro.c,v 1.1.1.1 2012/03/29 17:21:00 uid42307 Exp $
 *
 * C version of NASM's macros.pl
 *
 *  Copyright (C) 2004-2007  Peter Johnson
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT  "nasm-macros.c"
#define MAXLINE 1024

int
main(int argc, char *argv[])
{
    FILE *in, *out;
    int i;
    char *str;
    char *strp;
    char *charp;
    int fline;
    int line = 0;
    int lindex = 0;
    int tasm_count = -1;
    size_t len;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> [<file> ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    out = fopen(OUTPUT, "wt");

    if (!out) {
        fprintf(stderr, "Could not open `%s'.\n", OUTPUT);
        return EXIT_FAILURE;
    }

    str = malloc(MAXLINE);

    fprintf(out, "/* This file auto-generated from standard.mac by genmacro.c"
                 " - don't edit it */\n\n#include <stddef.h>\n\n"
                 "static const char *stdmac[] = {\n");

    for (i=1; i<argc; i++) {
        in = fopen(argv[i], "rt");
        if (!in) {
            fprintf(stderr, "Could not open `%s'.\n", argv[i]);
            fclose(out);
            remove(OUTPUT);
            return EXIT_FAILURE;
        }

        fline = 0;

        while (fgets(str, MAXLINE, in)) {
            line++;
            fline++;

            strp = str;

            /* check for unterminated quotes and delete comments */
            charp = strp;
            while ((charp = strpbrk(charp, "'\";"))) {
                if (charp[0] == ';') {
                    *charp = '\0';
                    break;
                }
                if ((charp = strchr(charp+1, charp[0])) == NULL) {
                    fprintf(stderr, "%s:%d: error: unterminated quote\n",
                            argv[i], fline);
                    fclose(out);
                    remove(OUTPUT);
                    return EXIT_FAILURE;
                }
                charp++;
            }

            /* strip off leading and trailing whitespace */
            while (*strp == ' ' || *strp == '\t')
                strp++;
            len = strlen(strp);
            while (len > 0 && (strp[len-1] == ' ' || strp[len-1] == '\t' ||
                               strp[len-1] == '\n')) {
                strp[len-1] = '\0';
                len--;
            }

            /* skip blank lines */
            if (len == 0)
                continue;

            /* check for special TASM ending token */
            if (strcmp(strp, "*END*TASM*MACROS*") == 0) {
                tasm_count = lindex;
                continue;
            }

            /* output as string to output file */
            fprintf(out, "    \"");
            while (*strp != '\0') {
                if (*strp == '\\' || *strp == '"')
                    fputc('\\', out);
                fputc(*strp, out);
                strp++;
            }
            fprintf(out, "\",\n");
            lindex++;
        }

        fclose(in);
    }

    fprintf(out, "    NULL\n};\n");
    if (tasm_count == -1)
        tasm_count = lindex;
    fprintf(out, "#define TASM_MACRO_COUNT %d\n", tasm_count);
    fclose(out);

    free(str);

    return EXIT_SUCCESS;
}
