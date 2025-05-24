/* Copyright (c) 2023 Amazon
   Written by Michael Klingbeil */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

/* including sources directly to test internal APIs */
#define CELT_C /* to make celt_assert work */
#include "opus.h"
#include "test_opus_common.h"



#define NB_RANDOM_EXTENSIONS 10000000
#define MAX_EXTENSION_SIZE 200
#define MAX_NB_EXTENSIONS 100

void test_random_dred(void)
{
   int error;
   int i;
   OpusDREDDecoder *dred_dec;
   OpusDRED *dred;
   dred_dec = opus_dred_decoder_create(&error);
   expect_true(error == OPUS_OK, "opus_dred_decoder_create() failed");
   dred = opus_dred_alloc(&error);
   expect_true(error == OPUS_OK, "opus_dred_create() failed");
   for (i=0;i<NB_RANDOM_EXTENSIONS;i++)
   {
      unsigned char payload[MAX_EXTENSION_SIZE];
      int len;
      int j;
      int res1, res2;
      int dred_end;
      len = fast_rand()%(MAX_EXTENSION_SIZE+1);
      for (j=0;j<len;j++)
         payload[j] = fast_rand()&0xFF;
      res1 = opus_dred_parse(dred_dec, dred, payload, len, 48000, 48000, &dred_end, fast_rand()&0x1);
      if (res1 > 0)
      {
         res2 = opus_dred_process(dred_dec, dred, dred);
         expect_true(res2 == OPUS_OK, "process should succeed if parse succeeds");
         expect_true(res1 >= dred_end, "end before beginning");
      }
   }
   opus_dred_free(dred);
   opus_dred_decoder_destroy(dred_dec);
}

int main(int argc, char **argv)
{
   int env_used;
   char *env_seed;
   env_used=0;
   env_seed=getenv("SEED");
   if(argc>1)iseed=atoi(argv[1]);
   else if(env_seed)
   {
      iseed=atoi(env_seed);
      env_used=1;
   }
   else iseed=(opus_uint32)time(NULL)^(((opus_uint32)getpid()&65535)<<16);
   Rw=Rz=iseed;

   fprintf(stderr,"Testing dred. Random seed: %u (%.4X)\n", iseed, fast_rand() % 65535);
   if(env_used)fprintf(stderr,"  Random seed set from the environment (SEED=%s).\n", env_seed);

   test_random_dred();
   fprintf(stderr,"Tests completed successfully.\n");
   return 0;
}
