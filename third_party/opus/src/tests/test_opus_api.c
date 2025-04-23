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

/* This tests the API presented by the libopus system.
   It does not attempt to extensively exercise the codec internals.
   The strategy here is to simply the API interface invariants:
   That sane options are accepted, insane options are rejected,
   and that nothing blows up. In particular we don't actually test
   that settings are heeded by the codec (though we do check that
   get after set returns a sane value when it should). Other
   tests check the actual codec behavior.
   In cases where its reasonable to do so we test exhaustively,
   but its not reasonable to do so in all cases.
   Although these tests are simple they found several library bugs
   when they were initially developed. */

/* These tests are more sensitive if compiled with -DVALGRIND and
   run inside valgrind. Malloc failure testing requires glibc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "arch.h"
#include "opus_multistream.h"
#include "opus.h"
#include "test_opus_common.h"

#ifdef VALGRIND
#include <valgrind/memcheck.h>
#define VG_UNDEF(x,y) VALGRIND_MAKE_MEM_UNDEFINED((x),(y))
#define VG_CHECK(x,y) VALGRIND_CHECK_MEM_IS_DEFINED((x),(y))
#else
#define VG_UNDEF(x,y)
#define VG_CHECK(x,y)
#endif

#if defined(HAVE___MALLOC_HOOK)
#define MALLOC_FAIL
#include "os_support.h"
#include <malloc.h>

static const opus_int32 opus_apps[3] = {OPUS_APPLICATION_VOIP,
       OPUS_APPLICATION_AUDIO,OPUS_APPLICATION_RESTRICTED_LOWDELAY};

void *malloc_hook(__attribute__((unused)) size_t size,
                  __attribute__((unused)) const void *caller)
{
   return 0;
}
#endif

opus_int32 *null_int_ptr = (opus_int32 *)NULL;
opus_uint32 *null_uint_ptr = (opus_uint32 *)NULL;

static const opus_int32 opus_rates[5] = {48000,24000,16000,12000,8000};

opus_int32 test_dec_api(void)
{
   opus_uint32 dec_final_range;
   OpusDecoder *dec;
   OpusDecoder *dec2;
   opus_int32 i,j,cfgs;
   unsigned char packet[1276];
#ifndef DISABLE_FLOAT_API
   float fbuf[960*2];
#endif
   short sbuf[960*2];
   int c,err;

   cfgs=0;
   /*First test invalid configurations which should fail*/
   fprintf(stdout,"\n  Decoder basic API tests\n");
   fprintf(stdout,"  ---------------------------------------------------\n");
   for(c=0;c<4;c++)
   {
      i=opus_decoder_get_size(c);
      if(((c==1||c==2)&&(i<=2048||i>1<<18))||((c!=1&&c!=2)&&i!=0))test_failed();
      fprintf(stdout,"    opus_decoder_get_size(%d)=%d ...............%s OK.\n",c,i,i>0?"":"....");
      cfgs++;
   }

   /*Test with unsupported sample rates*/
   for(c=0;c<4;c++)
   {
      for(i=-7;i<=96000;i++)
      {
         int fs;
         if((i==8000||i==12000||i==16000||i==24000||i==48000)&&(c==1||c==2))continue;
         switch(i)
         {
           case(-5):fs=-8000;break;
           case(-6):fs=INT32_MAX;break;
           case(-7):fs=INT32_MIN;break;
           default:fs=i;
         }
         err = OPUS_OK;
         VG_UNDEF(&err,sizeof(err));
         dec = opus_decoder_create(fs, c, &err);
         if(err!=OPUS_BAD_ARG || dec!=NULL)test_failed();
         cfgs++;
         dec = opus_decoder_create(fs, c, 0);
         if(dec!=NULL)test_failed();
         cfgs++;
         dec=malloc(opus_decoder_get_size(2));
         if(dec==NULL)test_failed();
         err = opus_decoder_init(dec,fs,c);
         if(err!=OPUS_BAD_ARG)test_failed();
         cfgs++;
         free(dec);
      }
   }

   VG_UNDEF(&err,sizeof(err));
   dec = opus_decoder_create(48000, 2, &err);
   if(err!=OPUS_OK || dec==NULL)test_failed();
   VG_CHECK(dec,opus_decoder_get_size(2));
   cfgs++;

   fprintf(stdout,"    opus_decoder_create() ........................ OK.\n");
   fprintf(stdout,"    opus_decoder_init() .......................... OK.\n");

   err=opus_decoder_ctl(dec, OPUS_GET_FINAL_RANGE(null_uint_ptr));
   if(err != OPUS_BAD_ARG)test_failed();
   VG_UNDEF(&dec_final_range,sizeof(dec_final_range));
   err=opus_decoder_ctl(dec, OPUS_GET_FINAL_RANGE(&dec_final_range));
   if(err!=OPUS_OK)test_failed();
   VG_CHECK(&dec_final_range,sizeof(dec_final_range));
   fprintf(stdout,"    OPUS_GET_FINAL_RANGE ......................... OK.\n");
   cfgs++;

   err=opus_decoder_ctl(dec,OPUS_UNIMPLEMENTED);
   if(err!=OPUS_UNIMPLEMENTED)test_failed();
   fprintf(stdout,"    OPUS_UNIMPLEMENTED ........................... OK.\n");
   cfgs++;

   err=opus_decoder_ctl(dec, OPUS_GET_BANDWIDTH(null_int_ptr));
   if(err != OPUS_BAD_ARG)test_failed();
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_BANDWIDTH(&i));
   if(err != OPUS_OK || i!=0)test_failed();
   fprintf(stdout,"    OPUS_GET_BANDWIDTH ........................... OK.\n");
   cfgs++;

   err=opus_decoder_ctl(dec, OPUS_GET_SAMPLE_RATE(null_int_ptr));
   if(err != OPUS_BAD_ARG)test_failed();
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_SAMPLE_RATE(&i));
   if(err != OPUS_OK || i!=48000)test_failed();
   fprintf(stdout,"    OPUS_GET_SAMPLE_RATE ......................... OK.\n");
   cfgs++;

   /*GET_PITCH has different execution paths depending on the previously decoded frame.*/
   err=opus_decoder_ctl(dec, OPUS_GET_PITCH(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_PITCH(&i));
   if(err != OPUS_OK || i>0 || i<-1)test_failed();
   cfgs++;
   VG_UNDEF(packet,sizeof(packet));
   packet[0]=63<<2;packet[1]=packet[2]=0;
   if(opus_decode(dec, packet, 3, sbuf, 960, 0)!=960)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_PITCH(&i));
   if(err != OPUS_OK || i>0 || i<-1)test_failed();
   cfgs++;
   packet[0]=1;
   if(opus_decode(dec, packet, 1, sbuf, 960, 0)!=960)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_PITCH(&i));
   if(err != OPUS_OK || i>0 || i<-1)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_PITCH ............................... OK.\n");

   err=opus_decoder_ctl(dec, OPUS_GET_LAST_PACKET_DURATION(null_int_ptr));
   if(err != OPUS_BAD_ARG)test_failed();
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_LAST_PACKET_DURATION(&i));
   if(err != OPUS_OK || i!=960)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_LAST_PACKET_DURATION ................ OK.\n");

   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_GAIN(&i));
   VG_CHECK(&i,sizeof(i));
   if(err != OPUS_OK || i!=0)test_failed();
   cfgs++;
   err=opus_decoder_ctl(dec, OPUS_GET_GAIN(null_int_ptr));
   if(err != OPUS_BAD_ARG)test_failed();
   cfgs++;
   err=opus_decoder_ctl(dec, OPUS_SET_GAIN(-32769));
   if(err != OPUS_BAD_ARG)test_failed();
   cfgs++;
   err=opus_decoder_ctl(dec, OPUS_SET_GAIN(32768));
   if(err != OPUS_BAD_ARG)test_failed();
   cfgs++;
   err=opus_decoder_ctl(dec, OPUS_SET_GAIN(-15));
   if(err != OPUS_OK)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_decoder_ctl(dec, OPUS_GET_GAIN(&i));
   VG_CHECK(&i,sizeof(i));
   if(err != OPUS_OK || i!=-15)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_SET_GAIN ................................ OK.\n");
   fprintf(stdout,"    OPUS_GET_GAIN ................................ OK.\n");

   /*Reset the decoder*/
   dec2=malloc(opus_decoder_get_size(2));
   memcpy(dec2,dec,opus_decoder_get_size(2));
   if(opus_decoder_ctl(dec, OPUS_RESET_STATE)!=OPUS_OK)test_failed();
   if(memcmp(dec2,dec,opus_decoder_get_size(2))==0)test_failed();
   free(dec2);
   fprintf(stdout,"    OPUS_RESET_STATE ............................. OK.\n");
   cfgs++;

   VG_UNDEF(packet,sizeof(packet));
   packet[0]=0;
   if(opus_decoder_get_nb_samples(dec,packet,1)!=480)test_failed();
   if(opus_packet_get_nb_samples(packet,1,48000)!=480)test_failed();
   if(opus_packet_get_nb_samples(packet,1,96000)!=960)test_failed();
   if(opus_packet_get_nb_samples(packet,1,32000)!=320)test_failed();
   if(opus_packet_get_nb_samples(packet,1,8000)!=80)test_failed();
   packet[0]=3;
   if(opus_packet_get_nb_samples(packet,1,24000)!=OPUS_INVALID_PACKET)test_failed();
   packet[0]=(63<<2)|3;
   packet[1]=63;
   if(opus_packet_get_nb_samples(packet,0,24000)!=OPUS_BAD_ARG)test_failed();
   if(opus_packet_get_nb_samples(packet,2,48000)!=OPUS_INVALID_PACKET)test_failed();
   if(opus_decoder_get_nb_samples(dec,packet,2)!=OPUS_INVALID_PACKET)test_failed();
   fprintf(stdout,"    opus_{packet,decoder}_get_nb_samples() ....... OK.\n");
   cfgs+=9;

   if(OPUS_BAD_ARG!=opus_packet_get_nb_frames(packet,0))test_failed();
   for(i=0;i<256;i++) {
     int l1res[4]={1,2,2,OPUS_INVALID_PACKET};
     packet[0]=i;
     if(l1res[packet[0]&3]!=opus_packet_get_nb_frames(packet,1))test_failed();
     cfgs++;
     for(j=0;j<256;j++) {
       packet[1]=j;
       if(((packet[0]&3)!=3?l1res[packet[0]&3]:packet[1]&63)!=opus_packet_get_nb_frames(packet,2))test_failed();
       cfgs++;
     }
   }
   fprintf(stdout,"    opus_packet_get_nb_frames() .................. OK.\n");

   for(i=0;i<256;i++) {
     int bw;
     packet[0]=i;
     bw=packet[0]>>4;
     bw=OPUS_BANDWIDTH_NARROWBAND+(((((bw&7)*9)&(63-(bw&8)))+2+12*((bw&8)!=0))>>4);
     if(bw!=opus_packet_get_bandwidth(packet))test_failed();
     cfgs++;
   }
   fprintf(stdout,"    opus_packet_get_bandwidth() .................. OK.\n");

   for(i=0;i<256;i++) {
     int fp3s,rate;
     packet[0]=i;
     fp3s=packet[0]>>3;
     fp3s=((((3-(fp3s&3))*13&119)+9)>>2)*((fp3s>13)*(3-((fp3s&3)==3))+1)*25;
     for(rate=0;rate<5;rate++) {
       if((opus_rates[rate]*3/fp3s)!=opus_packet_get_samples_per_frame(packet,opus_rates[rate]))test_failed();
       cfgs++;
     }
   }
   fprintf(stdout,"    opus_packet_get_samples_per_frame() .......... OK.\n");

   packet[0]=(63<<2)+3;
   packet[1]=49;
   for(j=2;j<51;j++)packet[j]=0;
   VG_UNDEF(sbuf,sizeof(sbuf));
   if(opus_decode(dec, packet, 51, sbuf, 960, 0)!=OPUS_INVALID_PACKET)test_failed();
   cfgs++;
   packet[0]=(63<<2);
   packet[1]=packet[2]=0;
   if(opus_decode(dec, packet, -1, sbuf, 960, 0)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_decode(dec, packet, 3, sbuf, 60, 0)!=OPUS_BUFFER_TOO_SMALL)test_failed();
   cfgs++;
   if(opus_decode(dec, packet, 3, sbuf, 480, 0)!=OPUS_BUFFER_TOO_SMALL)test_failed();
   cfgs++;
   if(opus_decode(dec, packet, 3, sbuf, 960, 0)!=960)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_decode() ................................ OK.\n");
#ifndef DISABLE_FLOAT_API
   VG_UNDEF(fbuf,sizeof(fbuf));
   if(opus_decode_float(dec, packet, 3, fbuf, 960, 0)!=960)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_decode_float() .......................... OK.\n");
#endif

#if 0
   /*These tests are disabled because the library crashes with null states*/
   if(opus_decoder_ctl(0,OPUS_RESET_STATE)         !=OPUS_INVALID_STATE)test_failed();
   if(opus_decoder_init(0,48000,1)                 !=OPUS_INVALID_STATE)test_failed();
   if(opus_decode(0,packet,1,outbuf,2880,0)        !=OPUS_INVALID_STATE)test_failed();
   if(opus_decode_float(0,packet,1,0,2880,0)       !=OPUS_INVALID_STATE)test_failed();
   if(opus_decoder_get_nb_samples(0,packet,1)      !=OPUS_INVALID_STATE)test_failed();
   if(opus_packet_get_nb_frames(NULL,1)            !=OPUS_BAD_ARG)test_failed();
   if(opus_packet_get_bandwidth(NULL)              !=OPUS_BAD_ARG)test_failed();
   if(opus_packet_get_samples_per_frame(NULL,48000)!=OPUS_BAD_ARG)test_failed();
#endif
   opus_decoder_destroy(dec);
   cfgs++;
   fprintf(stdout,"                   All decoder interface tests passed\n");
   fprintf(stdout,"                             (%6d API invocations)\n",cfgs);
   return cfgs;
}

opus_int32 test_msdec_api(void)
{
   opus_uint32 dec_final_range;
   OpusMSDecoder *dec;
   OpusDecoder *streamdec;
   opus_int32 i,j,cfgs;
   unsigned char packet[1276];
   unsigned char mapping[256];
#ifndef DISABLE_FLOAT_API
   float fbuf[960*2];
#endif
   short sbuf[960*2];
   int a,b,c,err;

   mapping[0]=0;
   mapping[1]=1;
   for(i=2;i<256;i++)VG_UNDEF(&mapping[i],sizeof(unsigned char));

   cfgs=0;
   /*First test invalid configurations which should fail*/
   fprintf(stdout,"\n  Multistream decoder basic API tests\n");
   fprintf(stdout,"  ---------------------------------------------------\n");
   for(a=-1;a<4;a++)
   {
      for(b=-1;b<4;b++)
      {
         i=opus_multistream_decoder_get_size(a,b);
         if(((a>0&&b<=a&&b>=0)&&(i<=2048||i>((1<<18)*a)))||((a<1||b>a||b<0)&&i!=0))test_failed();
         fprintf(stdout,"    opus_multistream_decoder_get_size(%2d,%2d)=%d %sOK.\n",a,b,i,i>0?"":"... ");
         cfgs++;
      }
   }

   /*Test with unsupported sample rates*/
   for(c=1;c<3;c++)
   {
      for(i=-7;i<=96000;i++)
      {
         int fs;
         if((i==8000||i==12000||i==16000||i==24000||i==48000)&&(c==1||c==2))continue;
         switch(i)
         {
           case(-5):fs=-8000;break;
           case(-6):fs=INT32_MAX;break;
           case(-7):fs=INT32_MIN;break;
           default:fs=i;
         }
         err = OPUS_OK;
         VG_UNDEF(&err,sizeof(err));
         dec = opus_multistream_decoder_create(fs, c, 1, c-1, mapping, &err);
         if(err!=OPUS_BAD_ARG || dec!=NULL)test_failed();
         cfgs++;
         dec = opus_multistream_decoder_create(fs, c, 1, c-1, mapping, 0);
         if(dec!=NULL)test_failed();
         cfgs++;
         dec=malloc(opus_multistream_decoder_get_size(1,1));
         if(dec==NULL)test_failed();
         err = opus_multistream_decoder_init(dec,fs,c,1,c-1, mapping);
         if(err!=OPUS_BAD_ARG)test_failed();
         cfgs++;
         free(dec);
      }
   }

   for(c=0;c<2;c++)
   {
      int *ret_err;
      ret_err = c?0:&err;

      mapping[0]=0;
      mapping[1]=1;
      for(i=2;i<256;i++)VG_UNDEF(&mapping[i],sizeof(unsigned char));

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 2, 1, 0, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      mapping[0]=mapping[1]=0;
      dec = opus_multistream_decoder_create(48000, 2, 1, 0, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_OK) || dec==NULL)test_failed();
      cfgs++;
      opus_multistream_decoder_destroy(dec);
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 1, 4, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_OK) || dec==NULL)test_failed();
      cfgs++;

      err = opus_multistream_decoder_init(dec,48000, 1, 0, 0, mapping);
      if(err!=OPUS_BAD_ARG)test_failed();
      cfgs++;

      err = opus_multistream_decoder_init(dec,48000, 1, 1, -1, mapping);
      if(err!=OPUS_BAD_ARG)test_failed();
      cfgs++;

      opus_multistream_decoder_destroy(dec);
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 2, 1, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_OK) || dec==NULL)test_failed();
      cfgs++;
      opus_multistream_decoder_destroy(dec);
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 255, 255, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, -1, 1, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 0, 1, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 1, -1, 2, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 1, -1, -1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 256, 255, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      dec = opus_multistream_decoder_create(48000, 256, 255, 0, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      mapping[0]=255;
      mapping[1]=1;
      mapping[2]=2;
      dec = opus_multistream_decoder_create(48000, 3, 2, 0, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      mapping[0]=0;
      mapping[1]=0;
      mapping[2]=0;
      dec = opus_multistream_decoder_create(48000, 3, 2, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_OK) || dec==NULL)test_failed();
      cfgs++;
      opus_multistream_decoder_destroy(dec);
      cfgs++;

      VG_UNDEF(ret_err,sizeof(*ret_err));
      mapping[0]=0;
      mapping[1]=255;
      mapping[2]=1;
      mapping[3]=2;
      mapping[4]=3;
      dec = opus_multistream_decoder_create(48001, 5, 4, 1, mapping, ret_err);
      if(ret_err){VG_CHECK(ret_err,sizeof(*ret_err));}
      if((ret_err && *ret_err!=OPUS_BAD_ARG) || dec!=NULL)test_failed();
      cfgs++;
   }

   VG_UNDEF(&err,sizeof(err));
   mapping[0]=0;
   mapping[1]=255;
   mapping[2]=1;
   mapping[3]=2;
   dec = opus_multistream_decoder_create(48000, 4, 2, 1, mapping, &err);
   VG_CHECK(&err,sizeof(err));
   if(err!=OPUS_OK || dec==NULL)test_failed();
   cfgs++;

   fprintf(stdout,"    opus_multistream_decoder_create() ............ OK.\n");
   fprintf(stdout,"    opus_multistream_decoder_init() .............. OK.\n");

   VG_UNDEF(&dec_final_range,sizeof(dec_final_range));
   err=opus_multistream_decoder_ctl(dec, OPUS_GET_FINAL_RANGE(&dec_final_range));
   if(err!=OPUS_OK)test_failed();
   VG_CHECK(&dec_final_range,sizeof(dec_final_range));
   fprintf(stdout,"    OPUS_GET_FINAL_RANGE ......................... OK.\n");
   cfgs++;

   streamdec=0;
   VG_UNDEF(&streamdec,sizeof(streamdec));
   err=opus_multistream_decoder_ctl(dec, OPUS_MULTISTREAM_GET_DECODER_STATE(-1,&streamdec));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   err=opus_multistream_decoder_ctl(dec, OPUS_MULTISTREAM_GET_DECODER_STATE(1,&streamdec));
   if(err!=OPUS_OK||streamdec==NULL)test_failed();
   VG_CHECK(streamdec,opus_decoder_get_size(1));
   cfgs++;
   err=opus_multistream_decoder_ctl(dec, OPUS_MULTISTREAM_GET_DECODER_STATE(2,&streamdec));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   err=opus_multistream_decoder_ctl(dec, OPUS_MULTISTREAM_GET_DECODER_STATE(0,&streamdec));
   if(err!=OPUS_OK||streamdec==NULL)test_failed();
   VG_CHECK(streamdec,opus_decoder_get_size(1));
   fprintf(stdout,"    OPUS_MULTISTREAM_GET_DECODER_STATE ........... OK.\n");
   cfgs++;

   for(j=0;j<2;j++)
   {
      OpusDecoder *od;
      err=opus_multistream_decoder_ctl(dec,OPUS_MULTISTREAM_GET_DECODER_STATE(j,&od));
      if(err != OPUS_OK)test_failed();
      VG_UNDEF(&i,sizeof(i));
      err=opus_decoder_ctl(od, OPUS_GET_GAIN(&i));
      VG_CHECK(&i,sizeof(i));
      if(err != OPUS_OK || i!=0)test_failed();
      cfgs++;
   }
   err=opus_multistream_decoder_ctl(dec,OPUS_SET_GAIN(15));
   if(err!=OPUS_OK)test_failed();
   fprintf(stdout,"    OPUS_SET_GAIN ................................ OK.\n");
   for(j=0;j<2;j++)
   {
      OpusDecoder *od;
      err=opus_multistream_decoder_ctl(dec,OPUS_MULTISTREAM_GET_DECODER_STATE(j,&od));
      if(err != OPUS_OK)test_failed();
      VG_UNDEF(&i,sizeof(i));
      err=opus_decoder_ctl(od, OPUS_GET_GAIN(&i));
      VG_CHECK(&i,sizeof(i));
      if(err != OPUS_OK || i!=15)test_failed();
      cfgs++;
   }
   fprintf(stdout,"    OPUS_GET_GAIN ................................ OK.\n");

   VG_UNDEF(&i,sizeof(i));
   err=opus_multistream_decoder_ctl(dec, OPUS_GET_BANDWIDTH(&i));
   if(err != OPUS_OK || i!=0)test_failed();
   fprintf(stdout,"    OPUS_GET_BANDWIDTH ........................... OK.\n");
   cfgs++;

   err=opus_multistream_decoder_ctl(dec,OPUS_UNIMPLEMENTED);
   if(err!=OPUS_UNIMPLEMENTED)test_failed();
   fprintf(stdout,"    OPUS_UNIMPLEMENTED ........................... OK.\n");
   cfgs++;

#if 0
   /*Currently unimplemented for multistream*/
   /*GET_PITCH has different execution paths depending on the previously decoded frame.*/
   err=opus_multistream_decoder_ctl(dec, OPUS_GET_PITCH(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_multistream_decoder_ctl(dec, OPUS_GET_PITCH(&i));
   if(err != OPUS_OK || i>0 || i<-1)test_failed();
   cfgs++;
   VG_UNDEF(packet,sizeof(packet));
   packet[0]=63<<2;packet[1]=packet[2]=0;
   if(opus_multistream_decode(dec, packet, 3, sbuf, 960, 0)!=960)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_multistream_decoder_ctl(dec, OPUS_GET_PITCH(&i));
   if(err != OPUS_OK || i>0 || i<-1)test_failed();
   cfgs++;
   packet[0]=1;
   if(opus_multistream_decode(dec, packet, 1, sbuf, 960, 0)!=960)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   err=opus_multistream_decoder_ctl(dec, OPUS_GET_PITCH(&i));
   if(err != OPUS_OK || i>0 || i<-1)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_PITCH ............................... OK.\n");
#endif

   /*Reset the decoder*/
   if(opus_multistream_decoder_ctl(dec, OPUS_RESET_STATE)!=OPUS_OK)test_failed();
   fprintf(stdout,"    OPUS_RESET_STATE ............................. OK.\n");
   cfgs++;

   opus_multistream_decoder_destroy(dec);
   cfgs++;
   VG_UNDEF(&err,sizeof(err));
   dec = opus_multistream_decoder_create(48000, 2, 1, 1, mapping, &err);
   if(err!=OPUS_OK || dec==NULL)test_failed();
   cfgs++;

   packet[0]=(63<<2)+3;
   packet[1]=49;
   for(j=2;j<51;j++)packet[j]=0;
   VG_UNDEF(sbuf,sizeof(sbuf));
   if(opus_multistream_decode(dec, packet, 51, sbuf, 960, 0)!=OPUS_INVALID_PACKET)test_failed();
   cfgs++;
   packet[0]=(63<<2);
   packet[1]=packet[2]=0;
   if(opus_multistream_decode(dec, packet, -1, sbuf, 960, 0)!=OPUS_BAD_ARG){printf("%d\n",opus_multistream_decode(dec, packet, -1, sbuf, 960, 0));test_failed();}
   cfgs++;
   if(opus_multistream_decode(dec, packet, 3, sbuf, -960, 0)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_multistream_decode(dec, packet, 3, sbuf, 60, 0)!=OPUS_BUFFER_TOO_SMALL)test_failed();
   cfgs++;
   if(opus_multistream_decode(dec, packet, 3, sbuf, 480, 0)!=OPUS_BUFFER_TOO_SMALL)test_failed();
   cfgs++;
   if(opus_multistream_decode(dec, packet, 3, sbuf, 960, 0)!=960)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_multistream_decode() .................... OK.\n");
#ifndef DISABLE_FLOAT_API
   VG_UNDEF(fbuf,sizeof(fbuf));
   if(opus_multistream_decode_float(dec, packet, 3, fbuf, 960, 0)!=960)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_multistream_decode_float() .............. OK.\n");
#endif

#if 0
   /*These tests are disabled because the library crashes with null states*/
   if(opus_multistream_decoder_ctl(0,OPUS_RESET_STATE)         !=OPUS_INVALID_STATE)test_failed();
   if(opus_multistream_decoder_init(0,48000,1)                 !=OPUS_INVALID_STATE)test_failed();
   if(opus_multistream_decode(0,packet,1,outbuf,2880,0)        !=OPUS_INVALID_STATE)test_failed();
   if(opus_multistream_decode_float(0,packet,1,0,2880,0)       !=OPUS_INVALID_STATE)test_failed();
   if(opus_multistream_decoder_get_nb_samples(0,packet,1)      !=OPUS_INVALID_STATE)test_failed();
#endif
   opus_multistream_decoder_destroy(dec);
   cfgs++;
   fprintf(stdout,"       All multistream decoder interface tests passed\n");
   fprintf(stdout,"                             (%6d API invocations)\n",cfgs);
   return cfgs;
}

#ifdef VALGRIND
#define UNDEFINE_FOR_PARSE  toc=-1; \
   frames[0]=(unsigned char *)0; \
   frames[1]=(unsigned char *)0; \
   payload_offset=-1; \
   VG_UNDEF(&toc,sizeof(toc)); \
   VG_UNDEF(frames,sizeof(frames));\
   VG_UNDEF(&payload_offset,sizeof(payload_offset));
#else
#define UNDEFINE_FOR_PARSE  toc=-1; \
   frames[0]=(unsigned char *)0; \
   frames[1]=(unsigned char *)0; \
   payload_offset=-1;
#endif

/* This test exercises the heck out of the libopus parser.
   It is much larger than the parser itself in part because
   it tries to hit a lot of corner cases that could never
   fail with the libopus code, but might be problematic for
   other implementations. */
opus_int32 test_parse(void)
{
   opus_int32 i,j,jj,sz;
   unsigned char packet[1276];
   opus_int32 cfgs,cfgs_total;
   unsigned char toc;
   const unsigned char *frames[48];
   short size[48];
   int payload_offset, ret;
   fprintf(stdout,"\n  Packet header parsing tests\n");
   fprintf(stdout,"  ---------------------------------------------------\n");
   memset(packet,0,sizeof(char)*1276);
   packet[0]=63<<2;
   if(opus_packet_parse(packet,1,&toc,frames,0,&payload_offset)!=OPUS_BAD_ARG)test_failed();
   cfgs_total=cfgs=1;
   /*code 0*/
   for(i=0;i<64;i++)
   {
      packet[0]=i<<2;
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,4,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=1)test_failed();
      if(size[0]!=3)test_failed();
      if(frames[0]!=packet+1)test_failed();
   }
   fprintf(stdout,"    code 0 (%2d cases) ............................ OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   /*code 1, two frames of the same size*/
   for(i=0;i<64;i++)
   {
      packet[0]=(i<<2)+1;
      for(jj=0;jj<=1275*2+3;jj++)
      {
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,jj,&toc,frames,size,&payload_offset);
         cfgs++;
         if((jj&1)==1 && jj<=2551)
         {
            /* Must pass if payload length even (packet length odd) and
               size<=2551, must fail otherwise. */
            if(ret!=2)test_failed();
            if(size[0]!=size[1] || size[0]!=((jj-1)>>1))test_failed();
            if(frames[0]!=packet+1)test_failed();
            if(frames[1]!=frames[0]+size[0])test_failed();
            if((toc>>2)!=i)test_failed();
         } else if(ret!=OPUS_INVALID_PACKET)test_failed();
      }
   }
   fprintf(stdout,"    code 1 (%6d cases) ........................ OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      /*code 2, length code overflow*/
      packet[0]=(i<<2)+2;
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,1,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=OPUS_INVALID_PACKET)test_failed();
      packet[1]=252;
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,2,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=OPUS_INVALID_PACKET)test_failed();
      for(j=0;j<1275;j++)
      {
         if(j<252)packet[1]=j;
         else{packet[1]=252+(j&3);packet[2]=(j-252)>>2;}
         /*Code 2, one too short*/
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,j+(j<252?2:3)-1,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=OPUS_INVALID_PACKET)test_failed();
         /*Code 2, one too long*/
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,j+(j<252?2:3)+1276,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=OPUS_INVALID_PACKET)test_failed();
         /*Code 2, second zero*/
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,j+(j<252?2:3),&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=2)test_failed();
         if(size[0]!=j||size[1]!=0)test_failed();
         if(frames[1]!=frames[0]+size[0])test_failed();
         if((toc>>2)!=i)test_failed();
         /*Code 2, normal*/
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,(j<<1)+4,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=2)test_failed();
         if(size[0]!=j||size[1]!=(j<<1)+3-j-(j<252?1:2))test_failed();
         if(frames[1]!=frames[0]+size[0])test_failed();
         if((toc>>2)!=i)test_failed();
      }
   }
   fprintf(stdout,"    code 2 (%6d cases) ........................ OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      packet[0]=(i<<2)+3;
      /*code 3, length code overflow*/
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,1,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=OPUS_INVALID_PACKET)test_failed();
   }
   fprintf(stdout,"    code 3 m-truncation (%2d cases) ............... OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      /*code 3, m is zero or 49-63*/
      packet[0]=(i<<2)+3;
      for(jj=49;jj<=64;jj++)
      {
        packet[1]=0+(jj&63); /*CBR, no padding*/
        UNDEFINE_FOR_PARSE
        ret=opus_packet_parse(packet,1275,&toc,frames,size,&payload_offset);
        cfgs++;
        if(ret!=OPUS_INVALID_PACKET)test_failed();
        packet[1]=128+(jj&63); /*VBR, no padding*/
        UNDEFINE_FOR_PARSE
        ret=opus_packet_parse(packet,1275,&toc,frames,size,&payload_offset);
        cfgs++;
        if(ret!=OPUS_INVALID_PACKET)test_failed();
        packet[1]=64+(jj&63); /*CBR, padding*/
        UNDEFINE_FOR_PARSE
        ret=opus_packet_parse(packet,1275,&toc,frames,size,&payload_offset);
        cfgs++;
        if(ret!=OPUS_INVALID_PACKET)test_failed();
        packet[1]=128+64+(jj&63); /*VBR, padding*/
        UNDEFINE_FOR_PARSE
        ret=opus_packet_parse(packet,1275,&toc,frames,size,&payload_offset);
        cfgs++;
        if(ret!=OPUS_INVALID_PACKET)test_failed();
      }
   }
   fprintf(stdout,"    code 3 m=0,49-64 (%2d cases) ................ OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      packet[0]=(i<<2)+3;
      /*code 3, m is one, cbr*/
      packet[1]=1;
      for(j=0;j<1276;j++)
      {
        UNDEFINE_FOR_PARSE
        ret=opus_packet_parse(packet,j+2,&toc,frames,size,&payload_offset);
        cfgs++;
        if(ret!=1)test_failed();
        if(size[0]!=j)test_failed();
        if((toc>>2)!=i)test_failed();
      }
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,1276+2,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=OPUS_INVALID_PACKET)test_failed();
   }
   fprintf(stdout,"    code 3 m=1 CBR (%2d cases) ................. OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      int frame_samp;
      /*code 3, m>1 CBR*/
      packet[0]=(i<<2)+3;
      frame_samp=opus_packet_get_samples_per_frame(packet,48000);
      for(j=2;j<49;j++)
      {
         packet[1]=j;
         for(sz=2;sz<((j+2)*1275);sz++)
         {
            UNDEFINE_FOR_PARSE
            ret=opus_packet_parse(packet,sz,&toc,frames,size,&payload_offset);
            cfgs++;
            /*Must be <=120ms, must be evenly divisible, can't have frames>1275 bytes*/
            if(frame_samp*j<=5760 && (sz-2)%j==0 && (sz-2)/j<1276)
            {
               if(ret!=j)test_failed();
               for(jj=1;jj<ret;jj++)if(frames[jj]!=frames[jj-1]+size[jj-1])test_failed();
               if((toc>>2)!=i)test_failed();
            } else if(ret!=OPUS_INVALID_PACKET)test_failed();
         }
      }
      /*Super jumbo packets*/
      packet[1]=5760/frame_samp;
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,1275*packet[1]+2,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=packet[1])test_failed();
      for(jj=0;jj<ret;jj++)if(size[jj]!=1275)test_failed();
   }
   fprintf(stdout,"    code 3 m=1-48 CBR (%2d cases) .......... OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      int frame_samp;
      /*Code 3 VBR, m one*/
      packet[0]=(i<<2)+3;
      packet[1]=128+1;
      frame_samp=opus_packet_get_samples_per_frame(packet,48000);
      for(jj=0;jj<1276;jj++)
      {
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,2+jj,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=1)test_failed();
         if(size[0]!=jj)test_failed();
         if((toc>>2)!=i)test_failed();
      }
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,2+1276,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=OPUS_INVALID_PACKET)test_failed();
      for(j=2;j<49;j++)
      {
         packet[1]=128+j;
         /*Length code overflow*/
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,2+j-2,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=OPUS_INVALID_PACKET)test_failed();
         packet[2]=252;
         packet[3]=0;
         for(jj=4;jj<2+j;jj++)packet[jj]=0;
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,2+j,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=OPUS_INVALID_PACKET)test_failed();
         /*One byte too short*/
         for(jj=2;jj<2+j;jj++)packet[jj]=0;
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,2+j-2,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=OPUS_INVALID_PACKET)test_failed();
         /*One byte too short thanks to length coding*/
         packet[2]=252;
         packet[3]=0;
         for(jj=4;jj<2+j;jj++)packet[jj]=0;
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,2+j+252-1,&toc,frames,size,&payload_offset);
         cfgs++;
         if(ret!=OPUS_INVALID_PACKET)test_failed();
         /*Most expensive way of coding zeros*/
         for(jj=2;jj<2+j;jj++)packet[jj]=0;
         UNDEFINE_FOR_PARSE
         ret=opus_packet_parse(packet,2+j-1,&toc,frames,size,&payload_offset);
         cfgs++;
         if(frame_samp*j<=5760){
            if(ret!=j)test_failed();
            for(jj=0;jj<j;jj++)if(size[jj]!=0)test_failed();
            if((toc>>2)!=i)test_failed();
         } else if(ret!=OPUS_INVALID_PACKET)test_failed();
         /*Quasi-CBR use of mode 3*/
         for(sz=0;sz<8;sz++)
         {
            const int tsz[8]={50,201,403,700,1472,5110,20400,61298};
            int pos=0;
            int as=(tsz[sz]+i-j-2)/j;
            for(jj=0;jj<j-1;jj++)
            {
              if(as<252){packet[2+pos]=as;pos++;}
              else{packet[2+pos]=252+(as&3);packet[3+pos]=(as-252)>>2;pos+=2;}
            }
            UNDEFINE_FOR_PARSE
            ret=opus_packet_parse(packet,tsz[sz]+i,&toc,frames,size,&payload_offset);
            cfgs++;
            if(frame_samp*j<=5760 && as<1276 && (tsz[sz]+i-2-pos-as*(j-1))<1276){
               if(ret!=j)test_failed();
               for(jj=0;jj<j-1;jj++)if(size[jj]!=as)test_failed();
               if(size[j-1]!=(tsz[sz]+i-2-pos-as*(j-1)))test_failed();
               if((toc>>2)!=i)test_failed();
            } else if(ret!=OPUS_INVALID_PACKET)test_failed();
         }
      }
   }
   fprintf(stdout,"    code 3 m=1-48 VBR (%2d cases) ............. OK.\n",cfgs);
   cfgs_total+=cfgs;cfgs=0;

   for(i=0;i<64;i++)
   {
      packet[0]=(i<<2)+3;
      /*Padding*/
      packet[1]=128+1+64;
      /*Overflow the length coding*/
      for(jj=2;jj<127;jj++)packet[jj]=255;
      UNDEFINE_FOR_PARSE
      ret=opus_packet_parse(packet,127,&toc,frames,size,&payload_offset);
      cfgs++;
      if(ret!=OPUS_INVALID_PACKET)test_failed();

      for(sz=0;sz<4;sz++)
      {
         const int tsz[4]={0,72,512,1275};
         for(jj=sz;jj<65025;jj+=11)
         {
            int pos;
            for(pos=0;pos<jj/254;pos++)packet[2+pos]=255;
            packet[2+pos]=jj%254;
            pos++;
            if(sz==0&&i==63)
            {
               /*Code more padding than there is room in the packet*/
               UNDEFINE_FOR_PARSE
               ret=opus_packet_parse(packet,2+jj+pos-1,&toc,frames,size,&payload_offset);
               cfgs++;
               if(ret!=OPUS_INVALID_PACKET)test_failed();
            }
            UNDEFINE_FOR_PARSE
            ret=opus_packet_parse(packet,2+jj+tsz[sz]+i+pos,&toc,frames,size,&payload_offset);
            cfgs++;
            if(tsz[sz]+i<1276)
            {
               if(ret!=1)test_failed();
               if(size[0]!=tsz[sz]+i)test_failed();
               if((toc>>2)!=i)test_failed();
            } else if (ret!=OPUS_INVALID_PACKET)test_failed();
         }
      }
   }
   fprintf(stdout,"    code 3 padding (%2d cases) ............... OK.\n",cfgs);
   cfgs_total+=cfgs;
   fprintf(stdout,"    opus_packet_parse ............................ OK.\n");
   fprintf(stdout,"                      All packet parsing tests passed\n");
   fprintf(stdout,"                          (%d API invocations)\n",cfgs_total);
   return cfgs_total;
}

/* This is a helper macro for the encoder tests.
   The encoder api tests all have a pattern of set-must-fail, set-must-fail,
   set-must-pass, get-and-compare, set-must-pass, get-and-compare. */
#define CHECK_SETGET(setcall,getcall,badv,badv2,goodv,goodv2,sok,gok) \
   i=(badv);\
   if(opus_encoder_ctl(enc,setcall)==OPUS_OK)test_failed();\
   i=(badv2);\
   if(opus_encoder_ctl(enc,setcall)==OPUS_OK)test_failed();\
   j=i=(goodv);\
   if(opus_encoder_ctl(enc,setcall)!=OPUS_OK)test_failed();\
   i=-12345;\
   VG_UNDEF(&i,sizeof(i)); \
   err=opus_encoder_ctl(enc,getcall);\
   if(err!=OPUS_OK || i!=j)test_failed();\
   j=i=(goodv2);\
   if(opus_encoder_ctl(enc,setcall)!=OPUS_OK)test_failed();\
   fprintf(stdout,sok);\
   i=-12345;\
   VG_UNDEF(&i,sizeof(i)); \
   err=opus_encoder_ctl(enc,getcall);\
   if(err!=OPUS_OK || i!=j)test_failed();\
   fprintf(stdout,gok);\
   cfgs+=6;

opus_int32 test_enc_api(void)
{
   opus_uint32 enc_final_range;
   OpusEncoder *enc;
   opus_int32 i,j;
   unsigned char packet[1276];
#ifndef DISABLE_FLOAT_API
   float fbuf[960*2];
#endif
   short sbuf[960*2];
   int c,err,cfgs;

   cfgs=0;
   /*First test invalid configurations which should fail*/
   fprintf(stdout,"\n  Encoder basic API tests\n");
   fprintf(stdout,"  ---------------------------------------------------\n");
   for(c=0;c<4;c++)
   {
      i=opus_encoder_get_size(c);
      if(((c==1||c==2)&&(i<=2048||i>1<<18))||((c!=1&&c!=2)&&i!=0))test_failed();
      fprintf(stdout,"    opus_encoder_get_size(%d)=%d ...............%s OK.\n",c,i,i>0?"":"....");
      cfgs++;
   }

   /*Test with unsupported sample rates, channel counts*/
   for(c=0;c<4;c++)
   {
      for(i=-7;i<=96000;i++)
      {
         int fs;
         if((i==8000||i==12000||i==16000||i==24000||i==48000)&&(c==1||c==2))continue;
         switch(i)
         {
           case(-5):fs=-8000;break;
           case(-6):fs=INT32_MAX;break;
           case(-7):fs=INT32_MIN;break;
           default:fs=i;
         }
         err = OPUS_OK;
         VG_UNDEF(&err,sizeof(err));
         enc = opus_encoder_create(fs, c, OPUS_APPLICATION_VOIP, &err);
         if(err!=OPUS_BAD_ARG || enc!=NULL)test_failed();
         cfgs++;
         enc = opus_encoder_create(fs, c, OPUS_APPLICATION_VOIP, 0);
         if(enc!=NULL)test_failed();
         cfgs++;
         opus_encoder_destroy(enc);
         enc=malloc(opus_encoder_get_size(2));
         if(enc==NULL)test_failed();
         err = opus_encoder_init(enc, fs, c, OPUS_APPLICATION_VOIP);
         if(err!=OPUS_BAD_ARG)test_failed();
         cfgs++;
         free(enc);
      }
   }

   enc = opus_encoder_create(48000, 2, OPUS_AUTO, NULL);
   if(enc!=NULL)test_failed();
   cfgs++;

   VG_UNDEF(&err,sizeof(err));
   enc = opus_encoder_create(48000, 2, OPUS_AUTO, &err);
   if(err!=OPUS_BAD_ARG || enc!=NULL)test_failed();
   cfgs++;

   VG_UNDEF(&err,sizeof(err));
   enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, NULL);
   if(enc==NULL)test_failed();
   opus_encoder_destroy(enc);
   cfgs++;

   VG_UNDEF(&err,sizeof(err));
   enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &err);
   if(err!=OPUS_OK || enc==NULL)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_LOOKAHEAD(&i));
   if(err!=OPUS_OK || i<0 || i>32766)test_failed();
   cfgs++;
   opus_encoder_destroy(enc);

   VG_UNDEF(&err,sizeof(err));
   enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &err);
   if(err!=OPUS_OK || enc==NULL)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_LOOKAHEAD(&i));
   if(err!=OPUS_OK || i<0 || i>32766)test_failed();
   opus_encoder_destroy(enc);
   cfgs++;

   VG_UNDEF(&err,sizeof(err));
   enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &err);
   if(err!=OPUS_OK || enc==NULL)test_failed();
   cfgs++;

   fprintf(stdout,"    opus_encoder_create() ........................ OK.\n");
   fprintf(stdout,"    opus_encoder_init() .......................... OK.\n");

   i=-12345;
   VG_UNDEF(&i,sizeof(i));
   err=opus_encoder_ctl(enc,OPUS_GET_LOOKAHEAD(&i));
   if(err!=OPUS_OK || i<0 || i>32766)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_LOOKAHEAD(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_LOOKAHEAD ........................... OK.\n");

   err=opus_encoder_ctl(enc,OPUS_GET_SAMPLE_RATE(&i));
   if(err!=OPUS_OK || i!=48000)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_SAMPLE_RATE(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_SAMPLE_RATE ......................... OK.\n");

   if(opus_encoder_ctl(enc,OPUS_UNIMPLEMENTED)!=OPUS_UNIMPLEMENTED)test_failed();
   fprintf(stdout,"    OPUS_UNIMPLEMENTED ........................... OK.\n");
   cfgs++;

   err=opus_encoder_ctl(enc,OPUS_GET_APPLICATION(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_APPLICATION(i),OPUS_GET_APPLICATION(&i),-1,OPUS_AUTO,
     OPUS_APPLICATION_AUDIO,OPUS_APPLICATION_RESTRICTED_LOWDELAY,
     "    OPUS_SET_APPLICATION ......................... OK.\n",
     "    OPUS_GET_APPLICATION ......................... OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_BITRATE(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_encoder_ctl(enc,OPUS_SET_BITRATE(1073741832))!=OPUS_OK)test_failed();
   cfgs++;
   VG_UNDEF(&i,sizeof(i));
   if(opus_encoder_ctl(enc,OPUS_GET_BITRATE(&i))!=OPUS_OK)test_failed();
   if(i>700000||i<256000)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_BITRATE(i),OPUS_GET_BITRATE(&i),-12345,0,
     500,256000,
     "    OPUS_SET_BITRATE ............................. OK.\n",
     "    OPUS_GET_BITRATE ............................. OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_FORCE_CHANNELS(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_FORCE_CHANNELS(i),OPUS_GET_FORCE_CHANNELS(&i),-1,3,
     1,OPUS_AUTO,
     "    OPUS_SET_FORCE_CHANNELS ...................... OK.\n",
     "    OPUS_GET_FORCE_CHANNELS ...................... OK.\n")

   i=-2;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(i))==OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_FULLBAND+1;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(i))==OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_NARROWBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_FULLBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_WIDEBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_MEDIUMBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_SET_BANDWIDTH ........................... OK.\n");
   /*We don't test if the bandwidth has actually changed.
     because the change may be delayed until the encoder is advanced.*/
   i=-12345;
   VG_UNDEF(&i,sizeof(i));
   err=opus_encoder_ctl(enc,OPUS_GET_BANDWIDTH(&i));
   if(err!=OPUS_OK || (i!=OPUS_BANDWIDTH_NARROWBAND&&
      i!=OPUS_BANDWIDTH_MEDIUMBAND&&i!=OPUS_BANDWIDTH_WIDEBAND&&
      i!=OPUS_BANDWIDTH_FULLBAND&&i!=OPUS_AUTO))test_failed();
   cfgs++;
   if(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(OPUS_AUTO))!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_BANDWIDTH(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_BANDWIDTH ........................... OK.\n");

   i=-2;
   if(opus_encoder_ctl(enc,OPUS_SET_MAX_BANDWIDTH(i))==OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_FULLBAND+1;
   if(opus_encoder_ctl(enc,OPUS_SET_MAX_BANDWIDTH(i))==OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_NARROWBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_MAX_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_FULLBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_MAX_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_WIDEBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_MAX_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   i=OPUS_BANDWIDTH_MEDIUMBAND;
   if(opus_encoder_ctl(enc,OPUS_SET_MAX_BANDWIDTH(i))!=OPUS_OK)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_SET_MAX_BANDWIDTH ....................... OK.\n");
   /*We don't test if the bandwidth has actually changed.
     because the change may be delayed until the encoder is advanced.*/
   i=-12345;
   VG_UNDEF(&i,sizeof(i));
   err=opus_encoder_ctl(enc,OPUS_GET_MAX_BANDWIDTH(&i));
   if(err!=OPUS_OK || (i!=OPUS_BANDWIDTH_NARROWBAND&&
      i!=OPUS_BANDWIDTH_MEDIUMBAND&&i!=OPUS_BANDWIDTH_WIDEBAND&&
      i!=OPUS_BANDWIDTH_FULLBAND))test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_MAX_BANDWIDTH(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_MAX_BANDWIDTH ....................... OK.\n");

   err=opus_encoder_ctl(enc,OPUS_GET_DTX(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_DTX(i),OPUS_GET_DTX(&i),-1,2,
     1,0,
     "    OPUS_SET_DTX ................................. OK.\n",
     "    OPUS_GET_DTX ................................. OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_COMPLEXITY(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_COMPLEXITY(i),OPUS_GET_COMPLEXITY(&i),-1,11,
     0,10,
     "    OPUS_SET_COMPLEXITY .......................... OK.\n",
     "    OPUS_GET_COMPLEXITY .......................... OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_INBAND_FEC(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_INBAND_FEC(i),OPUS_GET_INBAND_FEC(&i),-1,3,
     1,0,
     "    OPUS_SET_INBAND_FEC .......................... OK.\n",
     "    OPUS_GET_INBAND_FEC .......................... OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_PACKET_LOSS_PERC(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_PACKET_LOSS_PERC(i),OPUS_GET_PACKET_LOSS_PERC(&i),-1,101,
     100,0,
     "    OPUS_SET_PACKET_LOSS_PERC .................... OK.\n",
     "    OPUS_GET_PACKET_LOSS_PERC .................... OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_VBR(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_VBR(i),OPUS_GET_VBR(&i),-1,2,
     1,0,
     "    OPUS_SET_VBR ................................. OK.\n",
     "    OPUS_GET_VBR ................................. OK.\n")

/*   err=opus_encoder_ctl(enc,OPUS_GET_VOICE_RATIO(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_VOICE_RATIO(i),OPUS_GET_VOICE_RATIO(&i),-2,101,
     0,50,
     "    OPUS_SET_VOICE_RATIO ......................... OK.\n",
     "    OPUS_GET_VOICE_RATIO ......................... OK.\n")*/

   err=opus_encoder_ctl(enc,OPUS_GET_VBR_CONSTRAINT(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_VBR_CONSTRAINT(i),OPUS_GET_VBR_CONSTRAINT(&i),-1,2,
     1,0,
     "    OPUS_SET_VBR_CONSTRAINT ...................... OK.\n",
     "    OPUS_GET_VBR_CONSTRAINT ...................... OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_SIGNAL(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_SIGNAL(i),OPUS_GET_SIGNAL(&i),-12345,0x7FFFFFFF,
     OPUS_SIGNAL_MUSIC,OPUS_AUTO,
     "    OPUS_SET_SIGNAL .............................. OK.\n",
     "    OPUS_GET_SIGNAL .............................. OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_LSB_DEPTH(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_LSB_DEPTH(i),OPUS_GET_LSB_DEPTH(&i),7,25,16,24,
     "    OPUS_SET_LSB_DEPTH ........................... OK.\n",
     "    OPUS_GET_LSB_DEPTH ........................... OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_PREDICTION_DISABLED(&i));
   if(i!=0)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_GET_PREDICTION_DISABLED(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_PREDICTION_DISABLED(i),OPUS_GET_PREDICTION_DISABLED(&i),-1,2,1,0,
     "    OPUS_SET_PREDICTION_DISABLED ................. OK.\n",
     "    OPUS_GET_PREDICTION_DISABLED ................. OK.\n")

   err=opus_encoder_ctl(enc,OPUS_GET_EXPERT_FRAME_DURATION(null_int_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_2_5_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_5_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_40_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_60_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_80_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_100_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   err=opus_encoder_ctl(enc,OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_120_MS));
   if(err!=OPUS_OK)test_failed();
   cfgs++;
   CHECK_SETGET(OPUS_SET_EXPERT_FRAME_DURATION(i),OPUS_GET_EXPERT_FRAME_DURATION(&i),0,-1,
         OPUS_FRAMESIZE_60_MS,OPUS_FRAMESIZE_ARG,
     "    OPUS_SET_EXPERT_FRAME_DURATION ............... OK.\n",
     "    OPUS_GET_EXPERT_FRAME_DURATION ............... OK.\n")

   /*OPUS_SET_FORCE_MODE is not tested here because it's not a public API, however the encoder tests use it*/

   err=opus_encoder_ctl(enc,OPUS_GET_FINAL_RANGE(null_uint_ptr));
   if(err!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_encoder_ctl(enc,OPUS_GET_FINAL_RANGE(&enc_final_range))!=OPUS_OK)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_GET_FINAL_RANGE ......................... OK.\n");

   /*Reset the encoder*/
   if(opus_encoder_ctl(enc, OPUS_RESET_STATE)!=OPUS_OK)test_failed();
   cfgs++;
   fprintf(stdout,"    OPUS_RESET_STATE ............................. OK.\n");

   memset(sbuf,0,sizeof(short)*2*960);
   VG_UNDEF(packet,sizeof(packet));
   i=opus_encode(enc, sbuf, 960, packet, sizeof(packet));
   if(i<1 || (i>(opus_int32)sizeof(packet)))test_failed();
   VG_CHECK(packet,i);
   cfgs++;
   fprintf(stdout,"    opus_encode() ................................ OK.\n");
#ifndef DISABLE_FLOAT_API
   memset(fbuf,0,sizeof(float)*2*960);
   VG_UNDEF(packet,sizeof(packet));
   i=opus_encode_float(enc, fbuf, 960, packet, sizeof(packet));
   if(i<1 || (i>(opus_int32)sizeof(packet)))test_failed();
   VG_CHECK(packet,i);
   cfgs++;
   fprintf(stdout,"    opus_encode_float() .......................... OK.\n");
#endif

#if 0
   /*These tests are disabled because the library crashes with null states*/
   if(opus_encoder_ctl(0,OPUS_RESET_STATE)               !=OPUS_INVALID_STATE)test_failed();
   if(opus_encoder_init(0,48000,1,OPUS_APPLICATION_VOIP) !=OPUS_INVALID_STATE)test_failed();
   if(opus_encode(0,sbuf,960,packet,sizeof(packet))      !=OPUS_INVALID_STATE)test_failed();
   if(opus_encode_float(0,fbuf,960,packet,sizeof(packet))!=OPUS_INVALID_STATE)test_failed();
#endif
   opus_encoder_destroy(enc);
   cfgs++;
   fprintf(stdout,"                   All encoder interface tests passed\n");
   fprintf(stdout,"                             (%d API invocations)\n",cfgs);
   return cfgs;
}

#define max_out (1276*48+48*2+2)
int test_repacketizer_api(void)
{
   int ret,cfgs,i,j,k;
   OpusRepacketizer *rp;
   unsigned char *packet;
   unsigned char *po;
   cfgs=0;
   fprintf(stdout,"\n  Repacketizer tests\n");
   fprintf(stdout,"  ---------------------------------------------------\n");

   packet=malloc(max_out);
   if(packet==NULL)test_failed();
   memset(packet,0,max_out);
   po=malloc(max_out+256);
   if(po==NULL)test_failed();

   i=opus_repacketizer_get_size();
   if(i<=0)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_repacketizer_get_size()=%d ............. OK.\n",i);

   rp=malloc(i);
   rp=opus_repacketizer_init(rp);
   if(rp==NULL)test_failed();
   cfgs++;
   free(rp);
   fprintf(stdout,"    opus_repacketizer_init ....................... OK.\n");

   rp=opus_repacketizer_create();
   if(rp==NULL)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_repacketizer_create ..................... OK.\n");

   if(opus_repacketizer_get_nb_frames(rp)!=0)test_failed();
   cfgs++;
   fprintf(stdout,"    opus_repacketizer_get_nb_frames .............. OK.\n");

   /*Length overflows*/
   VG_UNDEF(packet,4);
   if(opus_repacketizer_cat(rp,packet,0)!=OPUS_INVALID_PACKET)test_failed(); /* Zero len */
   cfgs++;
   packet[0]=1;
   if(opus_repacketizer_cat(rp,packet,2)!=OPUS_INVALID_PACKET)test_failed(); /* Odd payload code 1 */
   cfgs++;
   packet[0]=2;
   if(opus_repacketizer_cat(rp,packet,1)!=OPUS_INVALID_PACKET)test_failed(); /* Code 2 overflow one */
   cfgs++;
   packet[0]=3;
   if(opus_repacketizer_cat(rp,packet,1)!=OPUS_INVALID_PACKET)test_failed(); /* Code 3 no count */
   cfgs++;
   packet[0]=2;
   packet[1]=255;
   if(opus_repacketizer_cat(rp,packet,2)!=OPUS_INVALID_PACKET)test_failed(); /* Code 2 overflow two */
   cfgs++;
   packet[0]=2;
   packet[1]=250;
   if(opus_repacketizer_cat(rp,packet,251)!=OPUS_INVALID_PACKET)test_failed(); /* Code 2 overflow three */
   cfgs++;
   packet[0]=3;
   packet[1]=0;
   if(opus_repacketizer_cat(rp,packet,2)!=OPUS_INVALID_PACKET)test_failed(); /* Code 3 m=0 */
   cfgs++;
   packet[1]=49;
   if(opus_repacketizer_cat(rp,packet,100)!=OPUS_INVALID_PACKET)test_failed(); /* Code 3 m=49 */
   cfgs++;
   packet[0]=0;
   if(opus_repacketizer_cat(rp,packet,3)!=OPUS_OK)test_failed();
   cfgs++;
   packet[0]=1<<2;
   if(opus_repacketizer_cat(rp,packet,3)!=OPUS_INVALID_PACKET)test_failed(); /* Change in TOC */
   cfgs++;

   /* Code 0,1,3 CBR -> Code 0,1,3 CBR */
   opus_repacketizer_init(rp);
   for(j=0;j<32;j++)
   {
      /* TOC types, test half with stereo */
      int maxi;
      packet[0]=((j<<1)+(j&1))<<2;
      maxi=960/opus_packet_get_samples_per_frame(packet,8000);
      for(i=1;i<=maxi;i++)
      {
         /* Number of CBR frames in the input packets */
         int maxp;
         packet[0]=((j<<1)+(j&1))<<2;
         if(i>1)packet[0]+=i==2?1:3;
         packet[1]=i>2?i:0;
         maxp=960/(i*opus_packet_get_samples_per_frame(packet,8000));
         for(k=0;k<=(1275+75);k+=3)
         {
            /*Payload size*/
            opus_int32 cnt,rcnt;
            if(k%i!=0)continue; /* Only testing CBR here, payload must be a multiple of the count */
            for(cnt=0;cnt<maxp+2;cnt++)
            {
               if(cnt>0)
               {
                  ret=opus_repacketizer_cat(rp,packet,k+(i>2?2:1));
                  if((cnt<=maxp&&k<=(1275*i))?ret!=OPUS_OK:ret!=OPUS_INVALID_PACKET)test_failed();
                  cfgs++;
               }
               rcnt=k<=(1275*i)?(cnt<maxp?cnt:maxp):0;
               if(opus_repacketizer_get_nb_frames(rp)!=rcnt*i)test_failed();
               cfgs++;
               ret=opus_repacketizer_out_range(rp,0,rcnt*i,po,max_out);
               if(rcnt>0)
               {
                  int len;
                  len=k*rcnt+((rcnt*i)>2?2:1);
                  if(ret!=len)test_failed();
                  if((rcnt*i)<2&&(po[0]&3)!=0)test_failed();                      /* Code 0 */
                  if((rcnt*i)==2&&(po[0]&3)!=1)test_failed();                     /* Code 1 */
                  if((rcnt*i)>2&&(((po[0]&3)!=3)||(po[1]!=rcnt*i)))test_failed(); /* Code 3 CBR */
                  cfgs++;
                  if(opus_repacketizer_out(rp,po,len)!=len)test_failed();
                  cfgs++;
                  if(opus_packet_unpad(po,len)!=len)test_failed();
                  cfgs++;
                  if(opus_packet_pad(po,len,len+1)!=OPUS_OK)test_failed();
                  cfgs++;
                  if(opus_packet_pad(po,len+1,len+256)!=OPUS_OK)test_failed();
                  cfgs++;
                  if(opus_packet_unpad(po,len+256)!=len)test_failed();
                  cfgs++;
                  if(opus_multistream_packet_unpad(po,len,1)!=len)test_failed();
                  cfgs++;
                  if(opus_multistream_packet_pad(po,len,len+1,1)!=OPUS_OK)test_failed();
                  cfgs++;
                  if(opus_multistream_packet_pad(po,len+1,len+256,1)!=OPUS_OK)test_failed();
                  cfgs++;
                  if(opus_multistream_packet_unpad(po,len+256,1)!=len)test_failed();
                  cfgs++;
                  if(opus_repacketizer_out(rp,po,len-1)!=OPUS_BUFFER_TOO_SMALL)test_failed();
                  cfgs++;
                  if(len>1)
                  {
                     if(opus_repacketizer_out(rp,po,1)!=OPUS_BUFFER_TOO_SMALL)test_failed();
                     cfgs++;
                  }
                  if(opus_repacketizer_out(rp,po,0)!=OPUS_BUFFER_TOO_SMALL)test_failed();
                  cfgs++;
               } else if (ret!=OPUS_BAD_ARG)test_failed();                        /* M must not be 0 */
            }
            opus_repacketizer_init(rp);
         }
      }
   }

   /*Change in input count code, CBR out*/
   opus_repacketizer_init(rp);
   packet[0]=0;
   if(opus_repacketizer_cat(rp,packet,5)!=OPUS_OK)test_failed();
   cfgs++;
   packet[0]+=1;
   if(opus_repacketizer_cat(rp,packet,9)!=OPUS_OK)test_failed();
   cfgs++;
   i=opus_repacketizer_out(rp,po,max_out);
   if((i!=(4+8+2))||((po[0]&3)!=3)||((po[1]&63)!=3)||((po[1]>>7)!=0))test_failed();
   cfgs++;
   i=opus_repacketizer_out_range(rp,0,1,po,max_out);
   if(i!=5||(po[0]&3)!=0)test_failed();
   cfgs++;
   i=opus_repacketizer_out_range(rp,1,2,po,max_out);
   if(i!=5||(po[0]&3)!=0)test_failed();
   cfgs++;

   /*Change in input count code, VBR out*/
   opus_repacketizer_init(rp);
   packet[0]=1;
   if(opus_repacketizer_cat(rp,packet,9)!=OPUS_OK)test_failed();
   cfgs++;
   packet[0]=0;
   if(opus_repacketizer_cat(rp,packet,3)!=OPUS_OK)test_failed();
   cfgs++;
   i=opus_repacketizer_out(rp,po,max_out);
   if((i!=(2+8+2+2))||((po[0]&3)!=3)||((po[1]&63)!=3)||((po[1]>>7)!=1))test_failed();
   cfgs++;

   /*VBR in, VBR out*/
   opus_repacketizer_init(rp);
   packet[0]=2;
   packet[1]=4;
   if(opus_repacketizer_cat(rp,packet,8)!=OPUS_OK)test_failed();
   cfgs++;
   if(opus_repacketizer_cat(rp,packet,8)!=OPUS_OK)test_failed();
   cfgs++;
   i=opus_repacketizer_out(rp,po,max_out);
   if((i!=(2+1+1+1+4+2+4+2))||((po[0]&3)!=3)||((po[1]&63)!=4)||((po[1]>>7)!=1))test_failed();
   cfgs++;

   /*VBR in, CBR out*/
   opus_repacketizer_init(rp);
   packet[0]=2;
   packet[1]=4;
   if(opus_repacketizer_cat(rp,packet,10)!=OPUS_OK)test_failed();
   cfgs++;
   if(opus_repacketizer_cat(rp,packet,10)!=OPUS_OK)test_failed();
   cfgs++;
   i=opus_repacketizer_out(rp,po,max_out);
   if((i!=(2+4+4+4+4))||((po[0]&3)!=3)||((po[1]&63)!=4)||((po[1]>>7)!=0))test_failed();
   cfgs++;

   /*Count 0 in, VBR out*/
   for(j=0;j<32;j++)
   {
      /* TOC types, test half with stereo */
      int maxi,sum,rcnt;
      packet[0]=((j<<1)+(j&1))<<2;
      maxi=960/opus_packet_get_samples_per_frame(packet,8000);
      sum=0;
      rcnt=0;
      opus_repacketizer_init(rp);
      for(i=1;i<=maxi+2;i++)
      {
         int len;
         ret=opus_repacketizer_cat(rp,packet,i);
         if(rcnt<maxi)
         {
            if(ret!=OPUS_OK)test_failed();
            rcnt++;
            sum+=i-1;
         } else if (ret!=OPUS_INVALID_PACKET)test_failed();
         cfgs++;
         len=sum+(rcnt<2?1:rcnt<3?2:2+rcnt-1);
         if(opus_repacketizer_out(rp,po,max_out)!=len)test_failed();
         if(rcnt>2&&(po[1]&63)!=rcnt)test_failed();
         if(rcnt==2&&(po[0]&3)!=2)test_failed();
         if(rcnt==1&&(po[0]&3)!=0)test_failed();
         cfgs++;
         if(opus_repacketizer_out(rp,po,len)!=len)test_failed();
         cfgs++;
         if(opus_packet_unpad(po,len)!=len)test_failed();
         cfgs++;
         if(opus_packet_pad(po,len,len+1)!=OPUS_OK)test_failed();
         cfgs++;
         if(opus_packet_pad(po,len+1,len+256)!=OPUS_OK)test_failed();
         cfgs++;
         if(opus_packet_unpad(po,len+256)!=len)test_failed();
         cfgs++;
         if(opus_multistream_packet_unpad(po,len,1)!=len)test_failed();
         cfgs++;
         if(opus_multistream_packet_pad(po,len,len+1,1)!=OPUS_OK)test_failed();
         cfgs++;
         if(opus_multistream_packet_pad(po,len+1,len+256,1)!=OPUS_OK)test_failed();
         cfgs++;
         if(opus_multistream_packet_unpad(po,len+256,1)!=len)test_failed();
         cfgs++;
         if(opus_repacketizer_out(rp,po,len-1)!=OPUS_BUFFER_TOO_SMALL)test_failed();
         cfgs++;
         if(len>1)
         {
            if(opus_repacketizer_out(rp,po,1)!=OPUS_BUFFER_TOO_SMALL)test_failed();
            cfgs++;
         }
         if(opus_repacketizer_out(rp,po,0)!=OPUS_BUFFER_TOO_SMALL)test_failed();
         cfgs++;
      }
   }

   po[0]='O';
   po[1]='p';
   if(opus_packet_pad(po,4,4)!=OPUS_OK)test_failed();
   cfgs++;
   if(opus_multistream_packet_pad(po,4,4,1)!=OPUS_OK)test_failed();
   cfgs++;
   if(opus_packet_pad(po,4,5)!=OPUS_INVALID_PACKET)test_failed();
   cfgs++;
   if(opus_multistream_packet_pad(po,4,5,1)!=OPUS_INVALID_PACKET)test_failed();
   cfgs++;
   if(opus_packet_pad(po,0,5)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_multistream_packet_pad(po,0,5,1)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_packet_unpad(po,0)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_multistream_packet_unpad(po,0,1)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_packet_unpad(po,4)!=OPUS_INVALID_PACKET)test_failed();
   cfgs++;
   if(opus_multistream_packet_unpad(po,4,1)!=OPUS_INVALID_PACKET)test_failed();
   cfgs++;
   po[0]=0;
   po[1]=0;
   po[2]=0;
   if(opus_packet_pad(po,5,4)!=OPUS_BAD_ARG)test_failed();
   cfgs++;
   if(opus_multistream_packet_pad(po,5,4,1)!=OPUS_BAD_ARG)test_failed();
   cfgs++;

   fprintf(stdout,"    opus_repacketizer_cat ........................ OK.\n");
   fprintf(stdout,"    opus_repacketizer_out ........................ OK.\n");
   fprintf(stdout,"    opus_repacketizer_out_range .................. OK.\n");
   fprintf(stdout,"    opus_packet_pad .............................. OK.\n");
   fprintf(stdout,"    opus_packet_unpad ............................ OK.\n");
   fprintf(stdout,"    opus_multistream_packet_pad .................. OK.\n");
   fprintf(stdout,"    opus_multistream_packet_unpad ................ OK.\n");

   opus_repacketizer_destroy(rp);
   cfgs++;
   free(packet);
   free(po);
   fprintf(stdout,"                        All repacketizer tests passed\n");
   fprintf(stdout,"                            (%7d API invocations)\n",cfgs);

   return cfgs;
}

#ifdef MALLOC_FAIL
/* GLIBC 2.14 declares __malloc_hook as deprecated, generating a warning
 * under GCC. However, this is the cleanest way to test malloc failure
 * handling in our codebase, and the lack of thread safety isn't an
 * issue here. We therefore disable the warning for this function.
 */
#if OPUS_GNUC_PREREQ(4,6)
/* Save the current warning settings */
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

typedef void *(*mhook)(size_t __size, __const void *);
#endif

int test_malloc_fail(void)
{
#ifdef MALLOC_FAIL
   OpusDecoder *dec;
   OpusEncoder *enc;
   OpusRepacketizer *rp;
   unsigned char mapping[256] = {0,1};
   OpusMSDecoder *msdec;
   OpusMSEncoder *msenc;
   int rate,c,app,cfgs,err,useerr;
   int *ep;
   mhook orig_malloc;
   cfgs=0;
#endif
   fprintf(stdout,"\n  malloc() failure tests\n");
   fprintf(stdout,"  ---------------------------------------------------\n");
#ifdef MALLOC_FAIL
   orig_malloc=__malloc_hook;
   __malloc_hook=malloc_hook;
   ep=(int *)opus_alloc(sizeof(int));
   if(ep!=NULL)
   {
      if(ep)free(ep);
      __malloc_hook=orig_malloc;
#endif
      fprintf(stdout,"    opus_decoder_create() ................... SKIPPED.\n");
      fprintf(stdout,"    opus_encoder_create() ................... SKIPPED.\n");
      fprintf(stdout,"    opus_repacketizer_create() .............. SKIPPED.\n");
      fprintf(stdout,"    opus_multistream_decoder_create() ....... SKIPPED.\n");
      fprintf(stdout,"    opus_multistream_encoder_create() ....... SKIPPED.\n");
      fprintf(stdout,"(Test only supported with GLIBC and without valgrind)\n");
      return 0;
#ifdef MALLOC_FAIL
   }
   for(useerr=0;useerr<2;useerr++)
   {
      ep=useerr?&err:0;
      for(rate=0;rate<5;rate++)
      {
        for(c=1;c<3;c++)
        {
           err=1;
           if(useerr)
           {
              VG_UNDEF(&err,sizeof(err));
           }
           dec=opus_decoder_create(opus_rates[rate], c, ep);
           if(dec!=NULL||(useerr&&err!=OPUS_ALLOC_FAIL))
           {
              __malloc_hook=orig_malloc;
              test_failed();
           }
           cfgs++;
           msdec=opus_multistream_decoder_create(opus_rates[rate], c, 1, c-1, mapping, ep);
           if(msdec!=NULL||(useerr&&err!=OPUS_ALLOC_FAIL))
           {
              __malloc_hook=orig_malloc;
              test_failed();
           }
           cfgs++;
           for(app=0;app<3;app++)
           {
              if(useerr)
              {
                 VG_UNDEF(&err,sizeof(err));
              }
              enc=opus_encoder_create(opus_rates[rate], c, opus_apps[app],ep);
              if(enc!=NULL||(useerr&&err!=OPUS_ALLOC_FAIL))
              {
                 __malloc_hook=orig_malloc;
                 test_failed();
              }
              cfgs++;
              msenc=opus_multistream_encoder_create(opus_rates[rate], c, 1, c-1, mapping, opus_apps[app],ep);
              if(msenc!=NULL||(useerr&&err!=OPUS_ALLOC_FAIL))
              {
                 __malloc_hook=orig_malloc;
                 test_failed();
              }
              cfgs++;
           }
        }
     }
   }
   rp=opus_repacketizer_create();
   if(rp!=NULL)
   {
      __malloc_hook=orig_malloc;
      test_failed();
   }
   cfgs++;
   __malloc_hook=orig_malloc;
   fprintf(stdout,"    opus_decoder_create() ........................ OK.\n");
   fprintf(stdout,"    opus_encoder_create() ........................ OK.\n");
   fprintf(stdout,"    opus_repacketizer_create() ................... OK.\n");
   fprintf(stdout,"    opus_multistream_decoder_create() ............ OK.\n");
   fprintf(stdout,"    opus_multistream_encoder_create() ............ OK.\n");
   fprintf(stdout,"                      All malloc failure tests passed\n");
   fprintf(stdout,"                                 (%2d API invocations)\n",cfgs);
   return cfgs;
#endif
}

#ifdef MALLOC_FAIL
#if __GNUC_PREREQ(4,6)
#pragma GCC diagnostic pop /* restore -Wdeprecated-declarations */
#endif
#endif

int main(int _argc, char **_argv)
{
   opus_int32 total;
   const char * oversion;
   if(_argc>1)
   {
      fprintf(stderr,"Usage: %s\n",_argv[0]);
      return 1;
   }
   iseed=0;

   oversion=opus_get_version_string();
   if(!oversion)test_failed();
   fprintf(stderr,"Testing the %s API deterministically\n", oversion);
   if(opus_strerror(-32768)==NULL)test_failed();
   if(opus_strerror(32767)==NULL)test_failed();
   if(strlen(opus_strerror(0))<1)test_failed();
   total=4;

   total+=test_dec_api();
   total+=test_msdec_api();
   total+=test_parse();
   total+=test_enc_api();
   total+=test_repacketizer_api();
   total+=test_malloc_fail();

   fprintf(stderr,"\nAll API tests passed.\nThe libopus API was invoked %d times.\n",total);

   return 0;
}
