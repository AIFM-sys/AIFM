/*
 * GAS-compatible re2c lexer
 *
 *  Copyright (C) 2005-2007  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
RCSID("$Id: gas-token.re,v 1.1.1.1 2012/03/29 17:20:59 uid42307 Exp $");

#include <libyasm.h>

#include "modules/parsers/gas/gas-parser.h"


#define BSIZE   8192

#define YYCURSOR        cursor
#define YYLIMIT         (s->lim)
#define YYMARKER        (s->ptr)
#define YYFILL(n)       {cursor = fill(parser_gas, cursor);}

#define RETURN(i)       do {s->cur = cursor; parser_gas->tokch = s->tok[0]; \
                         return i;} while (0)

#define SCANINIT()      {s->tok = cursor;}

#define TOK             ((char *)s->tok)
#define TOKLEN          (size_t)(cursor-s->tok)

static size_t
rept_input(yasm_parser_gas *parser_gas, /*@out@*/ YYCTYPE *buf,
           size_t max_size)
{
    gas_rept *rept = parser_gas->rept;
    size_t numleft = max_size;
    YYCTYPE *bufp = buf;

    /* If numrept is 0, copy out just the line end characters */
    if (rept->numrept == 0) {
        /* Skip first line, which contains .line */
        rept->line = STAILQ_NEXT(rept->line, link);
        if (!rept->line) {
            rept->numrept = 1;
            rept->numdone = 1;
        }
        while (rept->numrept == 0 && numleft > 0) {
            *bufp++ = rept->line->data[rept->line->len-1];
            rept->line = STAILQ_NEXT(rept->line, link);
            if (!rept->line) {
                rept->numrept = 1;
                rept->numdone = 1;
            }
        }
    }

    /* Copy out the previous fill buffer until we're *really* done */
    if (rept->numdone == rept->numrept) {
        size_t numcopy = rept->oldbuflen - rept->oldbufpos;
        if (numcopy > numleft)
            numcopy = numleft;
        memcpy(bufp, &rept->oldbuf[rept->oldbufpos], numcopy);
        numleft -= numcopy;
        bufp += numcopy;
        rept->oldbufpos += numcopy;

        if (rept->oldbufpos == rept->oldbuflen) {
            /* Delete lines, then delete rept and clear rept state */
            gas_rept_line *cur, *next;
            cur = STAILQ_FIRST(&rept->lines);
            while (cur) {
                next = STAILQ_NEXT(cur, link);
                yasm_xfree(cur->data);
                yasm_xfree(cur);
                cur = next;
            }
            yasm_xfree(rept->oldbuf);
            yasm_xfree(rept);
            parser_gas->rept = NULL;
        }
    }

    while (numleft > 0 && rept->numdone < rept->numrept) {
        /* Copy from line data to buf */
        size_t numcopy = rept->line->len - rept->linepos;
        if (numcopy > numleft)
            numcopy = numleft;
        memcpy(bufp, &rept->line->data[rept->linepos], numcopy);
        numleft -= numcopy;
        bufp += numcopy;
        rept->linepos += numcopy;

        /* Update locations if needed */
        if (rept->linepos == rept->line->len) {
            rept->line = STAILQ_NEXT(rept->line, link);
            rept->linepos = 0;
        }
        if (rept->line == NULL) {
            rept->numdone++;
            rept->line = STAILQ_FIRST(&rept->lines);
        }
    }

    return (max_size-numleft);
}

/* Bridge function to convert byte-oriented parser with line-oriented
 * preprocessor.
 */
static size_t
preproc_input(yasm_parser_gas *parser_gas, /*@out@*/ YYCTYPE *buf,
              size_t max_size)
{
    size_t tot=0;
    while (max_size > 0) {
        size_t n;

        if (!parser_gas->line) {
            parser_gas->line = yasm_preproc_get_line(parser_gas->preproc);
            if (!parser_gas->line)
                return tot; /* EOF */
            parser_gas->linepos = parser_gas->line;
            parser_gas->lineleft = strlen(parser_gas->line) + 1;
            parser_gas->line[parser_gas->lineleft-1] = '\n';
        }

        n = parser_gas->lineleft<max_size ? parser_gas->lineleft : max_size;
        strncpy((char *)buf+tot, parser_gas->linepos, n);

        if (n == parser_gas->lineleft) {
            yasm_xfree(parser_gas->line);
            parser_gas->line = NULL;
        } else {
            parser_gas->lineleft -= n;
            parser_gas->linepos += n;
        }

        tot += n;
        max_size -= n;
    }
    return tot;
}
#if 0
static size_t
fill_input(void *d, unsigned char *buf, size_t max)
{
    return yasm_preproc_input((yasm_preproc *)d, (char *)buf, max);
}
#endif
static YYCTYPE *
fill(yasm_parser_gas *parser_gas, YYCTYPE *cursor)
{
    yasm_scanner *s = &parser_gas->s;
    int first = 0;
    if(!s->eof){
        size_t cnt = s->tok - s->bot;
        if(cnt){
            memmove(s->bot, s->tok, (size_t)(s->lim - s->tok));
            s->tok = s->bot;
            s->ptr -= cnt;
            cursor -= cnt;
            s->lim -= cnt;
        }
        if (!s->bot)
            first = 1;
        if((s->top - s->lim) < BSIZE){
            YYCTYPE *buf = yasm_xmalloc((size_t)(s->lim - s->bot) + BSIZE);
            memcpy(buf, s->tok, (size_t)(s->lim - s->tok));
            s->tok = buf;
            s->ptr = &buf[s->ptr - s->bot];
            cursor = &buf[cursor - s->bot];
            s->lim = &buf[s->lim - s->bot];
            s->top = &s->lim[BSIZE];
            if (s->bot)
                yasm_xfree(s->bot);
            s->bot = buf;
        }
        if (parser_gas->rept && parser_gas->rept->ended) {
            /* Pull from rept lines instead of preproc */
            cnt = rept_input(parser_gas, s->lim, BSIZE);
        } else if((cnt = preproc_input(parser_gas, s->lim, BSIZE)) == 0) {
            s->eof = &s->lim[cnt]; *s->eof++ = '\n';
        }
        s->lim += cnt;
        if (first && parser_gas->save_input) {
            int i;
            YYCTYPE *saveline;
            parser_gas->save_last ^= 1;
            saveline = parser_gas->save_line[parser_gas->save_last];
            /* save next line into cur_line */
            for (i=0; i<79 && &s->tok[i] < s->lim && s->tok[i] != '\n'; i++)
                saveline[i] = s->tok[i];
            saveline[i] = '\0';
        }
    }
    return cursor;
}

static YYCTYPE *
save_line(yasm_parser_gas *parser_gas, YYCTYPE *cursor)
{
    yasm_scanner *s = &parser_gas->s;
    int i = 0;
    YYCTYPE *saveline;

    parser_gas->save_last ^= 1;
    saveline = parser_gas->save_line[parser_gas->save_last];

    /* save next line into cur_line */
    if ((YYLIMIT - YYCURSOR) < 80)
        YYFILL(80);
    for (i=0; i<79 && &cursor[i] < s->lim && cursor[i] != '\n'; i++)
        saveline[i] = cursor[i];
    saveline[i] = '\0';
    return cursor;
}

/* starting size of string buffer */
#define STRBUF_ALLOC_SIZE       128

/* string buffer used when parsing strings/character constants */
static YYCTYPE *strbuf = NULL;

/* length of strbuf (including terminating NULL character) */
static size_t strbuf_size = 0;

static void
strbuf_append(size_t count, YYCTYPE *cursor, yasm_scanner *s, int ch)
{
    if (count >= strbuf_size) {
        strbuf = yasm_xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
        strbuf_size += STRBUF_ALLOC_SIZE;
    }
    strbuf[count] = ch;
}

/*!re2c
  any = [\000-\377];
  digit = [0-9];
  iletter = [a-zA-Z];
  bindigit = [01];
  octdigit = [0-7];
  hexdigit = [0-9a-fA-F];
  ws = [ \t\r];
  dquot = ["];
*/


int
gas_parser_lex(YYSTYPE *lvalp, yasm_parser_gas *parser_gas)
{
    /*@null@*/ gas_rept *rept = parser_gas->rept;
    yasm_scanner *s = &parser_gas->s;
    YYCTYPE *cursor = s->cur;
    size_t count;
    YYCTYPE savech;
    int linestart;
    gas_rept_line *new_line;

    /* Handle one token of lookahead */
    if (parser_gas->peek_token != NONE) {
        int tok = parser_gas->peek_token;
        *lvalp = parser_gas->peek_tokval;  /* structure copy */
        parser_gas->tokch = parser_gas->peek_tokch;
        parser_gas->peek_token = NONE;
        return tok;
    }

    /* Catch EOF */
    if (s->eof && cursor == s->eof)
        return 0;

    /* Handle rept */
    if (rept && !rept->ended)
        goto rept_directive;

    /* Jump to proper "exclusive" states */
    switch (parser_gas->state) {
        case COMMENT:
            goto comment;
        case SECTION_DIRECTIVE:
            goto section_directive;
        default:
            break;
    }

scan:
    SCANINIT();

    /*!re2c
        /* standard decimal integer */
        ([1-9] digit*) | "0" {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->intn = yasm_intnum_create_dec(TOK);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        /* 0b10010011 - binary number */
        '0b' bindigit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->intn = yasm_intnum_create_bin(TOK+2);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        /* 0777 - octal number */
        "0" octdigit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->intn = yasm_intnum_create_oct(TOK);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        /* 0xAA - hexidecimal number */
        '0x' hexdigit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            /* skip 0 and x */
            lvalp->intn = yasm_intnum_create_hex(TOK+2);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        /* floating point value */
        "0" [DdEeFfTt] [-+]? (digit+)? ("." digit*)? ('e' [-+]? digit+)? {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->flt = yasm_floatnum_create(TOK+2);
            s->tok[TOKLEN] = savech;
            RETURN(FLTNUM);
        }

        /* character constant values */
        ['] {
            goto charconst;
        }

        /* string constant values */
        dquot {
            goto stringconst;
        }

        /* operators */
        "<<"                    { RETURN(LEFT_OP); }
        ">>"                    { RETURN(RIGHT_OP); }
        "<"                     { RETURN(LEFT_OP); }
        ">"                     { RETURN(RIGHT_OP); }
        [-+|^!*&/~$():@=,]      { RETURN(s->tok[0]); }
        ";"     {
            parser_gas->state = INITIAL;
            RETURN(s->tok[0]);
        }

        /* label or maybe directive */
        [_.][a-zA-Z0-9_$.]* {
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(ID);
        }

        /* register or segment register */
        [%][a-zA-Z0-9]+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            switch (yasm_arch_parse_check_regtmod
                    (p_object->arch, TOK+1, TOKLEN-1, &lvalp->arch_data)) {
                case YASM_ARCH_REG:
                    s->tok[TOKLEN] = savech;
                    RETURN(REG);
                case YASM_ARCH_REGGROUP:
                    s->tok[TOKLEN] = savech;
                    RETURN(REGGROUP);
                case YASM_ARCH_SEGREG:
                    s->tok[TOKLEN] = savech;
                    RETURN(SEGREG);
                default:
                    break;
            }
            yasm_error_set(YASM_ERROR_GENERAL,
                           N_("Unrecognized register name `%s'"), s->tok);
            s->tok[TOKLEN] = savech;
            lvalp->arch_data = 0;
            RETURN(REG);
        }

        /* label */
        [a-zA-Z][a-zA-Z0-9_$.]* ws* ':' {
            /* strip off colon and any whitespace */
            count = TOKLEN-1;
            while (s->tok[count] == ' ' || s->tok[count] == '\t'
                   || s->tok[count] == '\r')
                count--;
            /* Just an identifier, return as such. */
            lvalp->str_val = yasm__xstrndup(TOK, count);
            RETURN(LABEL);
        }

        /* local label */
        [0-9] ':' {
            /* increment label index */
            parser_gas->local[s->tok[0]-'0']++;
            /* build local label name */
            lvalp->str_val = yasm_xmalloc(30);
            sprintf(lvalp->str_val, "L%c\001%lu", s->tok[0],
                    parser_gas->local[s->tok[0]-'0']);
            RETURN(LABEL);
        }

        /* local label forward reference */
        [0-9] 'f' {
            /* build local label name */
            lvalp->str_val = yasm_xmalloc(30);
            sprintf(lvalp->str_val, "L%c\001%lu", s->tok[0],
                    parser_gas->local[s->tok[0]-'0']+1);
            RETURN(ID);
        }

        /* local label backward reference */
        [0-9] 'b' {
            /* build local label name */
            lvalp->str_val = yasm_xmalloc(30);
            sprintf(lvalp->str_val, "L%c\001%lu", s->tok[0],
                    parser_gas->local[s->tok[0]-'0']);
            RETURN(ID);
        }

        /* identifier that may be an instruction, etc. */
        [a-zA-Z][a-zA-Z0-9_$.]* {
            /* Can only be an instruction/prefix when not inside an
             * instruction or directive.
             */
            if (parser_gas->state != INSTDIR) {
                uintptr_t prefix;
                savech = s->tok[TOKLEN];
                s->tok[TOKLEN] = '\0';
                switch (yasm_arch_parse_check_insnprefix
                        (p_object->arch, TOK, TOKLEN, cur_line, &lvalp->bc,
                         &prefix)) {
                    case YASM_ARCH_INSN:
                        s->tok[TOKLEN] = savech;
                        parser_gas->state = INSTDIR;
                        RETURN(INSN);
                    case YASM_ARCH_PREFIX:
                        lvalp->arch_data = prefix;
                        s->tok[TOKLEN] = savech;
                        RETURN(PREFIX);
                    default:
                        s->tok[TOKLEN] = savech;
                }
            }
            /* Propagate errors in case we got a warning from the arch */
            yasm_errwarn_propagate(parser_gas->errwarns, cur_line);
            /* Just an identifier, return as such. */
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(ID);
        }

        "/*"                    { parser_gas->state = COMMENT; goto comment; }
        "#"                     {
            if (strcmp(((yasm_preproc_base*)parser_gas->preproc)->module->keyword,
                 "cpp") == 0)
            {
                RETURN(LINE_MARKER);
            } else
                goto line_comment;
        }

        ws+                     { goto scan; }

        "\n"                    {
            if (parser_gas->save_input)
                cursor = save_line(parser_gas, cursor);
            parser_gas->state = INITIAL;
            RETURN(s->tok[0]);
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto scan;
        }
    */

    /* C-style comment; nesting not supported */
comment:
    SCANINIT();

    /*!re2c
        /* End of comment */
        "*/"    { parser_gas->state = INITIAL; goto scan; }

        "\n"                    {
            if (parser_gas->save_input)
                cursor = save_line(parser_gas, cursor);
            RETURN(s->tok[0]);
        }

        any     {
            if (cursor == s->eof)
                return 0;
            goto comment;
        }
    */

    /* Single line comment. */
line_comment:
    /*!re2c
        (any \ [\n])*   { goto scan; }
    */

    /* .section directive (the section name portion thereof) */
section_directive:
    SCANINIT();

    /*!re2c
        [a-zA-Z0-9_$.-]+ {
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            parser_gas->state = INITIAL;
            RETURN(ID);
        }

        dquot                   { goto stringconst; }

        ws+                     { goto section_directive; }

        ","                     {
            parser_gas->state = INITIAL;
            RETURN(s->tok[0]);
        }

        "\n"                    {
            if (parser_gas->save_input)
                cursor = save_line(parser_gas, cursor);
            parser_gas->state = INITIAL;
            RETURN(s->tok[0]);
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto section_directive;
        }
    */

    /* character constant values */
charconst:
    /*TODO*/

    /* string constant values */
stringconst:
    strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);
    strbuf_size = STRBUF_ALLOC_SIZE;
    count = 0;

stringconst_scan:
    SCANINIT();

    /*!re2c
        /* Handle escaped double-quote by copying and continuing */
        "\\\""      {
            if (cursor == s->eof) {
                yasm_error_set(YASM_ERROR_SYNTAX,
                               N_("unexpected end of file in string"));
                lvalp->str.contents = (char *)strbuf;
                lvalp->str.len = count;
                RETURN(STRING);
            }
            strbuf_append(count++, cursor, s, '"');
            goto stringconst_scan;
        }

        dquot   {
            strbuf_append(count, cursor, s, '\0');
            yasm_unescape_cstring(strbuf, &count);
            lvalp->str.contents = (char *)strbuf;
            lvalp->str.len = count;
            RETURN(STRING);
        }

        any     {
            if (cursor == s->eof) {
                yasm_error_set(YASM_ERROR_SYNTAX,
                               N_("unexpected end of file in string"));
                lvalp->str.contents = (char *)strbuf;
                lvalp->str.len = count;
                RETURN(STRING);
            }
            strbuf_append(count++, cursor, s, s->tok[0]);
            goto stringconst_scan;
        }
    */

rept_directive:
    strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);
    strbuf_size = STRBUF_ALLOC_SIZE;
    count = 0;
    linestart = 1;


rept_scan:
    SCANINIT();

    /*!re2c
        [\n;]   {
            /* Line ending, save in lines */
            new_line = yasm_xmalloc(sizeof(gas_rept_line));
            if (cursor == s->eof) {
                yasm_xfree(strbuf);
                return 0;
            }
            strbuf_append(count++, cursor, s, s->tok[0]);
            new_line->data = strbuf;
            new_line->len = count;
            STAILQ_INSERT_TAIL(&rept->lines, new_line, link);
            /* Allocate new strbuf */
            strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);
            strbuf_size = STRBUF_ALLOC_SIZE;
            count = 0;
            /* Mark start of line */
            linestart = 1;
            goto rept_scan;
        }
        '.rept' {
            int i;
            if (linestart) {
                /* We don't support nested right now, error */
                yasm_error_set(YASM_ERROR_GENERAL,
                               N_("nested rept not supported"));
                yasm_errwarn_propagate(parser_gas->errwarns, cur_line);
            }
            for (i=0; i<6; i++)
                strbuf_append(count++, cursor, s, s->tok[i]);
            goto rept_scan;
        }
        '.endr' {
            if (linestart) {
                /* We're done, kick off the main lexer */
                rept->line = STAILQ_FIRST(&rept->lines);
                if (!rept->line) {
                    /* Didn't get any intervening data?  Empty repeat, so
                     * don't even bother.
                     */
                    yasm_xfree(strbuf);
                    yasm_xfree(rept);
                    parser_gas->rept = NULL;
                } else {
                    rept->ended = 1;

                    /* Add .line as first line to get line numbers correct */
                    new_line = yasm_xmalloc(sizeof(gas_rept_line));
                    new_line->data = yasm_xmalloc(40);
                    sprintf((char *)new_line->data, ".line %lu;",
                            rept->startline+1);
                    new_line->len = strlen((char *)new_line->data);
                    STAILQ_INSERT_HEAD(&rept->lines, new_line, link);

                    /* Save previous fill buffer */
                    rept->oldbuf = parser_gas->s.bot;
                    rept->oldbuflen = s->lim - s->bot;
                    rept->oldbufpos = cursor - s->bot;

                    /* Reset fill */
                    s->bot = NULL;
                    s->tok = NULL;
                    s->ptr = NULL;
                    s->cur = NULL;
                    s->lim = NULL;
                    s->top = NULL;
                    s->eof = NULL;
                    cursor = NULL;
                    YYFILL(1);
                }

                goto scan;
            } else {
                int i;
                for (i=0; i<6; i++)
                    strbuf_append(count++, cursor, s, s->tok[i]);
                goto rept_scan;
            }
        }

        any     {
            if (cursor == s->eof) {
                yasm_xfree(strbuf);
                return 0;
            }
            strbuf_append(count++, cursor, s, s->tok[0]);
            linestart = 0;
            goto rept_scan;
        }
    */

}
