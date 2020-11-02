/* See md5.c for explanation and copyright information.  */

/*
 * $Id: md5.h,v 1.1.1.1 2012/03/29 17:21:05 uid42307 Exp $
 */

#ifndef YASM_MD5_H
#define YASM_MD5_H

/* Unlike previous versions of this code, uint32 need not be exactly
   32 bits, merely 32 bits or more.  Choosing a data type which is 32
   bits instead of 64 is not important; speed is considerably more
   important.  ANSI guarantees that "unsigned long" will be big enough,
   and always using it seems to have few disadvantages.  */

typedef struct yasm_md5_context {
        unsigned long buf[4];
        unsigned long bits[2];
        unsigned char in[64];
} yasm_md5_context;

void yasm_md5_init(yasm_md5_context *context);
void yasm_md5_update(yasm_md5_context *context, unsigned char const *buf,
                     unsigned long len);
void yasm_md5_final(unsigned char digest[16], yasm_md5_context *context);
void yasm_md5_transform(unsigned long buf[4], const unsigned char in[64]);

#endif /* !YASM_MD5_H */
