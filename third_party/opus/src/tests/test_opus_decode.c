/* Copyright (c) 2011-2013 Xiph.Org Foundation
   Written by Gregory Maxwell */
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
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif
#include "opus.h"
#include "test_opus_common.h"

#define MAX_PACKET (1500)
#define MAX_FRAME_SAMP (5760)

int test_decoder_code0(int no_fuzz)
{
   static const opus_int32 fsv[5]={48000,24000,16000,12000,8000};
   int err,skip,plen;
   int out_samples,fec;
   int t;
   opus_int32 i;
   OpusDecoder *dec[5*2];
   opus_int32 decsize;
   OpusDecoder *decbak;
   opus_uint32 dec_final_range1,dec_final_range2,dec_final_acc;
   unsigned char *packet;
   unsigned char modes[4096];
   short *outbuf_int;
   short *outbuf;

   dec_final_range1=dec_final_range2=2;

   packet=(unsigned char *)malloc(sizeof(unsigned char)*MAX_PACKET);
   if(packet==NULL)test_failed();

   outbuf_int=(short *)malloc(sizeof(short)*(MAX_FRAME_SAMP+16)*2);
   for(i=0;i<(MAX_FRAME_SAMP+16)*2;i++)outbuf_int[i]=32749;
   outbuf=&outbuf_int[8*2];

   fprintf(stdout,"  Starting %d decoders...\n",5*2);
   for(t=0;t<5*2;t++)
   {
      int fs=fsv[t>>1];
      int c=(t&1)+1;
      err=OPUS_INTERNAL_ERROR;
      dec[t] = opus_decoder_create(fs, c, &err);
      if(err!=OPUS_OK || dec[t]==NULL)test_failed();
      fprintf(stdout,"    opus_decoder_create(%5d,%d) OK. Copy ",fs,c);
      {
         OpusDecoder *dec2;
         /*The opus state structures contain no pointers and can be freely copied*/
         dec2=(OpusDecoder *)malloc(opus_decoder_get_size(c));
         if(dec2==NULL)test_failed();
         memcpy(dec2,dec[t],opus_decoder_get_size(c));
         memset(dec[t],255,opus_decoder_get_size(c));
         opus_decoder_destroy(dec[t]);
         printf("OK.\n");
         dec[t]=dec2;
      }
   }

   decsize=opus_decoder_get_size(1);
   decbak=(OpusDecoder *)malloc(decsize);
   if(decbak==NULL)test_failed();

   for(t=0;t<5*2;t++)
   {
      int factor=48000/fsv[t>>1];
      for(fec=0;fec<2;fec++)
      {
         opus_int32 dur;
#if defined(ENABLE_OSCE) || defined(ENABLE_DEEP_PLC)
         opus_decoder_ctl(dec[t], OPUS_SET_COMPLEXITY(fast_rand()%11));
#endif
         /*Test PLC on a fresh decoder*/
         out_samples = opus_decode(dec[t], 0, 0, outbuf, 120/factor, fec);
         if(out_samples!=120/factor)test_failed();
         if(opus_decoder_ctl(dec[t], OPUS_GET_LAST_PACKET_DURATION(&dur))!=OPUS_OK)test_failed();
         if(dur!=120/factor)test_failed();

         /*Test on a size which isn't a multiple of 2.5ms*/
         out_samples = opus_decode(dec[t], 0, 0, outbuf, 120/factor+2, fec);
         if(out_samples!=OPUS_BAD_ARG)test_failed();

         /*Test null pointer input*/
         out_samples = opus_decode(dec[t], 0, -1, outbuf, 120/factor, fec);
         if(out_samples!=120/factor)test_failed();
         out_samples = opus_decode(dec[t], 0, 1, outbuf, 120/factor, fec);
         if(out_samples!=120/factor)test_failed();
         out_samples = opus_decode(dec[t], 0, 10, outbuf, 120/factor, fec);
         if(out_samples!=120/factor)test_failed();
         out_samples = opus_decode(dec[t], 0, fast_rand(), outbuf, 120/factor, fec);
         if(out_samples!=120/factor)test_failed();
         if(opus_decoder_ctl(dec[t], OPUS_GET_LAST_PACKET_DURATION(&dur))!=OPUS_OK)test_failed();
         if(dur!=120/factor)test_failed();

         /*Zero lengths*/
         out_samples = opus_decode(dec[t], packet, 0, outbuf, 120/factor, fec);
         if(out_samples!=120/factor)test_failed();

         /*Zero buffer*/
         outbuf[0]=32749;
         out_samples = opus_decode(dec[t], packet, 0, outbuf, 0, fec);
         if(out_samples>0)test_failed();
#if !defined(OPUS_BUILD) && (OPUS_GNUC_PREREQ(4, 6) || (defined(__clang_major__) && __clang_major__ >= 3))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
#endif
         out_samples = opus_decode(dec[t], packet, 0, 0, 0, fec);
#if !defined(OPUS_BUILD) && (OPUS_GNUC_PREREQ(4, 6) || (defined(__clang_major__) && __clang_major__ >= 3))
#pragma GCC diagnostic pop
#endif
         if(out_samples>0)test_failed();
         if(outbuf[0]!=32749)test_failed();

         /*Invalid lengths*/
         out_samples = opus_decode(dec[t], packet, -1, outbuf, MAX_FRAME_SAMP, fec);
         if(out_samples>=0)test_failed();
         out_samples = opus_decode(dec[t], packet, INT_MIN, outbuf, MAX_FRAME_SAMP, fec);
         if(out_samples>=0)test_failed();
         out_samples = opus_decode(dec[t], packet, -1, outbuf, -1, fec);
         if(out_samples>=0)test_failed();

         /*Crazy FEC values*/
         out_samples = opus_decode(dec[t], packet, 1, outbuf, MAX_FRAME_SAMP, fec?-1:2);
         if(out_samples>=0)test_failed();

         /*Reset the decoder*/
         if(opus_decoder_ctl(dec[t], OPUS_RESET_STATE)!=OPUS_OK)test_failed();
      }
   }
   fprintf(stdout,"  dec[all] initial frame PLC OK.\n");

   /*Count code 0 tests*/
   for(i=0;i<64;i++)
   {
      opus_int32 dur;
      int j,expected[5*2];
      packet[0]=i<<2;
      packet[1]=255;
      packet[2]=255;
      err=opus_packet_get_nb_channels(packet);
      if(err!=(i&1)+1)test_failed();

      for(t=0;t<5*2;t++){
         expected[t]=opus_decoder_get_nb_samples(dec[t],packet,1);
         if(expected[t]>2880)test_failed();
      }

      for(j=0;j<256;j++)
      {
         packet[1]=j;
         for(t=0;t<5*2;t++)
         {
#if defined(ENABLE_OSCE) || defined(ENABLE_DEEP_PLC)
            opus_decoder_ctl(dec[t], OPUS_SET_COMPLEXITY(fast_rand()%11));
#endif
            out_samples = opus_decode(dec[t], packet, 3, outbuf, MAX_FRAME_SAMP, 0);
            if(out_samples!=expected[t])test_failed();
            if(opus_decoder_ctl(dec[t], OPUS_GET_LAST_PACKET_DURATION(&dur))!=OPUS_OK)test_failed();
            if(dur!=out_samples)test_failed();
            opus_decoder_ctl(dec[t], OPUS_GET_FINAL_RANGE(&dec_final_range1));
            if(t==0)dec_final_range2=dec_final_range1;
            else if(dec_final_range1!=dec_final_range2)test_failed();
         }
      }

      for(t=0;t<5*2;t++){
         int factor=48000/fsv[t>>1];
         /* The PLC is run for 6 frames in order to get better PLC coverage. */
         for(j=0;j<6;j++)
         {
#if defined(ENABLE_OSCE) || defined(ENABLE_DEEP_PLC)
            opus_decoder_ctl(dec[t], OPUS_SET_COMPLEXITY(fast_rand()%11));
#endif
            out_samples = opus_decode(dec[t], 0, 0, outbuf, expected[t], 0);
            if(out_samples!=expected[t])test_failed();
            if(opus_decoder_ctl(dec[t], OPUS_GET_LAST_PACKET_DURATION(&dur))!=OPUS_OK)test_failed();
            if(dur!=out_samples)test_failed();
         }
         /* Run the PLC once at 2.5ms, as a simulation of someone trying to
            do small drift corrections. */
         if(expected[t]!=120/factor)
         {
            out_samples = opus_decode(dec[t], 0, 0, outbuf, 120/factor, 0);
            if(out_samples!=120/factor)test_failed();
            if(opus_decoder_ctl(dec[t], OPUS_GET_LAST_PACKET_DURATION(&dur))!=OPUS_OK)test_failed();
            if(dur!=out_samples)test_failed();
         }
         out_samples = opus_decode(dec[t], packet, 2, outbuf, expected[t]-1, 0);
         if(out_samples>0)test_failed();
      }
   }
   fprintf(stdout,"  dec[all] all 2-byte prefix for length 3 and PLC, all modes (64) OK.\n");

   if(no_fuzz)
   {
      fprintf(stdout,"  Skipping many tests which fuzz the decoder as requested.\n");
      free(decbak);
      for(t=0;t<5*2;t++)opus_decoder_destroy(dec[t]);
      printf("  Decoders stopped.\n");

      err=0;
      for(i=0;i<8*2;i++)err|=outbuf_int[i]!=32749;
      for(i=MAX_FRAME_SAMP*2;i<(MAX_FRAME_SAMP+8)*2;i++)err|=outbuf[i]!=32749;
      if(err)test_failed();

      free(outbuf_int);
      free(packet);
      return 0;
   }

   {
     /*We only test a subset of the modes here simply because the longer
       durations end up taking a long time.*/
      static const int cmodes[4]={16,20,24,28};
      static const opus_uint32 cres[4]={116290185,2172123586u,2172123586u,2172123586u};
      static const opus_uint32 lres[3]={3285687739u,1481572662,694350475};
      static const int lmodes[3]={0,4,8};
      int mode=fast_rand()%4;

      packet[0]=cmodes[mode]<<3;
      dec_final_acc=0;
      t=fast_rand()%10;

      for(i=0;i<65536;i++)
      {
         int factor=48000/fsv[t>>1];
         packet[1]=i>>8;
         packet[2]=i&255;
         packet[3]=255;
         out_samples = opus_decode(dec[t], packet, 4, outbuf, MAX_FRAME_SAMP, 0);
         if(out_samples!=120/factor)test_failed();
         opus_decoder_ctl(dec[t], OPUS_GET_FINAL_RANGE(&dec_final_range1));
         dec_final_acc+=dec_final_range1;
      }
      if(dec_final_acc!=cres[mode])test_failed();
      fprintf(stdout,"  dec[%3d] all 3-byte prefix for length 4, mode %2d OK.\n",t,cmodes[mode]);

      mode=fast_rand()%3;
      packet[0]=lmodes[mode]<<3;
      dec_final_acc=0;
      t=fast_rand()%10;
      for(i=0;i<65536;i++)
      {
         int factor=48000/fsv[t>>1];
         packet[1]=i>>8;
         packet[2]=i&255;
         packet[3]=255;
         out_samples = opus_decode(dec[t], packet, 4, outbuf, MAX_FRAME_SAMP, 0);
         if(out_samples!=480/factor)test_failed();
         opus_decoder_ctl(dec[t], OPUS_GET_FINAL_RANGE(&dec_final_range1));
         dec_final_acc+=dec_final_range1;
      }
      if(dec_final_acc!=lres[mode])test_failed();
      fprintf(stdout,"  dec[%3d] all 3-byte prefix for length 4, mode %2d OK.\n",t,lmodes[mode]);
   }

   skip=fast_rand()%7;
   for(i=0;i<64;i++)
   {
      int j,expected[5*2];
      packet[0]=i<<2;
      for(t=0;t<5*2;t++)expected[t]=opus_decoder_get_nb_samples(dec[t],packet,1);
      for(j=2+skip;j<1275;j+=4)
      {
         int jj;
         for(jj=0;jj<j;jj++)packet[jj+1]=fast_rand()&255;
         for(t=0;t<5*2;t++)
         {
#if defined(ENABLE_OSCE) || defined(ENABLE_DEEP_PLC)
            opus_decoder_ctl(dec[t], OPUS_SET_COMPLEXITY(fast_rand()%11));
#endif
            out_samples = opus_decode(dec[t], packet, j+1, outbuf, MAX_FRAME_SAMP, 0);
            if(out_samples!=expected[t])test_failed();
            opus_decoder_ctl(dec[t], OPUS_GET_FINAL_RANGE(&dec_final_range1));
            if(t==0)dec_final_range2=dec_final_range1;
            else if(dec_final_range1!=dec_final_range2)test_failed();
         }
      }
   }
   fprintf(stdout,"  dec[all] random packets, all modes (64), every 8th size from from %d bytes to maximum OK.\n",2+skip);

   debruijn2(64,modes);
   plen=(fast_rand()%18+3)*8+skip+3;
   for(i=0;i<4096;i++)
   {
      int j,expected[5*2];
      packet[0]=modes[i]<<2;
      for(t=0;t<5*2;t++)expected[t]=opus_decoder_get_nb_samples(dec[t],packet,plen);
      for(j=0;j<plen;j++)packet[j+1]=(fast_rand()|fast_rand())&255;
      memcpy(decbak,dec[0],decsize);
      if(opus_decode(decbak, packet, plen+1, outbuf, expected[0], 1)!=expected[0])test_failed();
      memcpy(decbak,dec[0],decsize);
      if(opus_decode(decbak,  0, 0, outbuf, MAX_FRAME_SAMP, 1)<20)test_failed();
      memcpy(decbak,dec[0],decsize);
      if(opus_decode(decbak,  0, 0, outbuf, MAX_FRAME_SAMP, 0)<20)test_failed();
      for(t=0;t<5*2;t++)
      {
         opus_int32 dur;
#if defined(ENABLE_OSCE) || defined(ENABLE_DEEP_PLC)
         opus_decoder_ctl(dec[t], OPUS_SET_COMPLEXITY(fast_rand()%11));
#endif
         out_samples = opus_decode(dec[t], packet, plen+1, outbuf, MAX_FRAME_SAMP, 0);
         if(out_samples!=expected[t])test_failed();
         if(t==0)dec_final_range2=dec_final_range1;
         else if(dec_final_range1!=dec_final_range2)test_failed();
         if(opus_decoder_ctl(dec[t], OPUS_GET_LAST_PACKET_DURATION(&dur))!=OPUS_OK)test_failed();
         if(dur!=out_samples)test_failed();
      }
   }
   fprintf(stdout,"  dec[all] random packets, all mode pairs (4096), %d bytes/frame OK.\n",plen+1);

   plen=(fast_rand()%18+3)*8+skip+3;
   t=rand()&3;
   for(i=0;i<4096;i++)
   {
      int count,j,expected;
      packet[0]=modes[i]<<2;
      expected=opus_decoder_get_nb_samples(dec[t],packet,plen);
      for(count=0;count<10;count++)
      {
#if defined(ENABLE_OSCE) || defined(ENABLE_DEEP_PLC)
         opus_decoder_ctl(dec[t], OPUS_SET_COMPLEXITY(fast_rand()%11));
#endif
         for(j=0;j<plen;j++)packet[j+1]=(fast_rand()|fast_rand())&255;
         out_samples = opus_decode(dec[t], packet, plen+1, outbuf, MAX_FRAME_SAMP, 0);
         if(out_samples!=expected)test_failed();
      }
   }
   fprintf(stdout,"  dec[%3d] random packets, all mode pairs (4096)*10, %d bytes/frame OK.\n",t,plen+1);

   {
      int tmodes[1]={25<<2};
      opus_uint32 tseeds[1]={140441};
      int tlen[1]={157};
      opus_int32 tret[1]={480};
      t=fast_rand()&1;
      for(i=0;i<1;i++)
      {
         int j;
         packet[0]=tmodes[i];
         Rw=Rz=tseeds[i];
         for(j=1;j<tlen[i];j++)packet[j]=fast_rand()&255;
         out_samples=opus_decode(dec[t], packet, tlen[i], outbuf, MAX_FRAME_SAMP, 0);
         if(out_samples!=tret[i])test_failed();
      }
      fprintf(stdout,"  dec[%3d] pre-selected random packets OK.\n",t);
   }

   free(decbak);
   for(t=0;t<5*2;t++)opus_decoder_destroy(dec[t]);
   printf("  Decoders stopped.\n");

   err=0;
   for(i=0;i<8*2;i++)err|=outbuf_int[i]!=32749;
   for(i=MAX_FRAME_SAMP*2;i<(MAX_FRAME_SAMP+8)*2;i++)err|=outbuf[i]!=32749;
   if(err)test_failed();

   free(outbuf_int);
   free(packet);
   return 0;
}

#ifndef DISABLE_FLOAT_API
void test_soft_clip(void)
{
   int i,j;
   float x[1024];
   float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
   fprintf(stdout,"  Testing opus_pcm_soft_clip... ");
   for(i=0;i<1024;i++)
   {
      for (j=0;j<1024;j++)
      {
        x[j]=(j&255)*(1/32.f)-4.f;
      }
      opus_pcm_soft_clip(&x[i],1024-i,1,s);
      for (j=i;j<1024;j++)
      {
        if(x[j]>1.f)test_failed();
        if(x[j]<-1.f)test_failed();
      }
   }
   for(i=1;i<9;i++)
   {
      for (j=0;j<1024;j++)
      {
        x[j]=(j&255)*(1/32.f)-4.f;
      }
      opus_pcm_soft_clip(x,1024/i,i,s);
      for (j=0;j<(1024/i)*i;j++)
      {
        if(x[j]>1.f)test_failed();
        if(x[j]<-1.f)test_failed();
      }
   }
   opus_pcm_soft_clip(x,0,1,s);
   opus_pcm_soft_clip(x,1,0,s);
   opus_pcm_soft_clip(x,1,1,0);
   opus_pcm_soft_clip(x,1,-1,s);
   opus_pcm_soft_clip(x,-1,1,s);
   opus_pcm_soft_clip(0,1,1,s);
   printf("OK.\n");
}
#endif

int main(int _argc, char **_argv)
{
   const char * oversion;
   const char * env_seed;
   int env_used;

   if(_argc>2)
   {
      fprintf(stderr,"Usage: %s [<seed>]\n",_argv[0]);
      return 1;
   }

   env_used=0;
   env_seed=getenv("SEED");
   if(_argc>1)iseed=atoi(_argv[1]);
   else if(env_seed)
   {
      iseed=atoi(env_seed);
      env_used=1;
   }
   else iseed=(opus_uint32)time(NULL)^(((opus_uint32)getpid()&65535)<<16);
   Rw=Rz=iseed;

   oversion=opus_get_version_string();
   if(!oversion)test_failed();
   fprintf(stderr,"Testing %s decoder. Random seed: %u (%.4X)\n", oversion, iseed, fast_rand() % 65535);
   if(env_used)fprintf(stderr,"  Random seed set from the environment (SEED=%s).\n", env_seed);

   /*Setting TEST_OPUS_NOFUZZ tells the tool not to send garbage data
     into the decoders. This is helpful because garbage data
     may cause the decoders to clip, which angers CLANG IOC.*/
   test_decoder_code0(getenv("TEST_OPUS_NOFUZZ")!=NULL);
#ifndef DISABLE_FLOAT_API
   test_soft_clip();
#endif

   return 0;
}
