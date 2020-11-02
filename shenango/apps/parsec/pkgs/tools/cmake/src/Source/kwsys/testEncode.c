/*=========================================================================

  Program:   KWSys - Kitware System Library
  Module:    $RCSfile: testEncode.c,v $

  Copyright (c) Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "kwsysPrivate.h"
#include KWSYS_HEADER(MD5.h)

/* Work-around CMake dependency scanning limitation.  This must
   duplicate the above list of headers.  */
#if 0
# include "MD5.h.in"
#endif

#include <stdio.h>
#include <string.h>

static const unsigned char testMD5input1[] =
"  Copyright (c) Kitware, Inc., Insight Consortium.  All rights reserved.\n"
"  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.\n";
static const char testMD5output1[] = "04051e509e81ef0b1612ddf0e52ca89e";

static const int testMD5input2len = 28;
static const unsigned char testMD5input2[] = "the cow jumped over the moon";
static const char testMD5output2[] = "a2ad137b746138fae4e5adca9c85d3ae";

static int testMD5_1(kwsysMD5* md5)
{
  char md5out[33];
  kwsysMD5_Initialize(md5);
  kwsysMD5_Append(md5, testMD5input1, -1);
  kwsysMD5_FinalizeHex(md5, md5out);
  md5out[32] = 0;
  printf("md5sum 1: expected [%s]\n"
         "               got [%s]\n",
         testMD5output1, md5out);
  return (strcmp(md5out, testMD5output1) != 0)? 1:0;
}

static int testMD5_2(kwsysMD5* md5)
{
  unsigned char digest[16];
  char md5out[33];
  kwsysMD5_Initialize(md5);
  kwsysMD5_Append(md5, testMD5input2, testMD5input2len);
  kwsysMD5_Finalize(md5, digest);
  kwsysMD5_DigestToHex(digest, md5out);
  md5out[32] = 0;
  printf("md5sum 2: expected [%s]\n"
         "               got [%s]\n",
         testMD5output2, md5out);
  return (strcmp(md5out, testMD5output2) != 0)? 1:0;
}

int testEncode(int argc, char* argv[])
{
  int result = 0;
  (void)argc;
  (void)argv;

  /* Test MD5 digest.  */
  {
  kwsysMD5* md5 = kwsysMD5_New();
  result |= testMD5_1(md5);
  result |= testMD5_2(md5);
  kwsysMD5_Delete(md5);
  }

  return result;
}
