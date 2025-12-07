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

#include "../src/opus_private.h"
#include "test_opus_common.h"

void test_extensions_generate_success(void)
{
   static const opus_extension_data ext[] = {
      {3, 0, (const unsigned char *)"a", 1},
      {32, 10, (const unsigned char *)"DRED", 4},
      {33, 1, (const unsigned char *)"NOT DRED", 8},
      {4, 4, (const unsigned char *)NULL, 0}
   };

   int result;
   unsigned char packet[32];
   const unsigned char *p = packet;
   result = opus_packet_extensions_generate(packet, 23+4, ext, 4, 11, 1);
   expect_true(result == 23+4, "expected length 23+4");

   /* expect padding */
   expect_true(p[0] == 1 && p[1] == 1 && p[2] == 1 && p[3] == 1, "expected padding");
   p += 4;

   /* extension ID=3 */
   expect_true((p[0] >> 1) == 3, "expected extension id 3");
   /* For extension IDs 1 through 31, L=0 means that no data follows the
      extension, whereas L=1 means that exactly one byte of extension data follows. */
   expect_true((p[0] & 0x01) == 1, "expected L-bit set");
   /* content */
   expect_true(p[1] == 'a', "expected extension content");
   p += 2;

   /* next byte should increment the frame count, ID=1, L=0 */
   expect_true(p[0] == 0x02, "bad frame separator");
   p += 1;
   /* extension ID=33 */
   expect_true((p[0] >> 1) == 33, "expected extension id 33");
   /* For IDs 32 to 127, L=0 signals that the extension data takes up the
      rest of the padding, and L=1 signals that a length indicator follows. */
   expect_true((p[0] & 0x01) == 1, "expected L-bit set");
   /* content */
   expect_true(p[1] == ext[2].len, "expected length");
   p += 2;
   expect_true(0 == memcmp(p, ext[2].data, ext[2].len), "expected extension content");
   p += ext[2].len;

   /* advance to frame 4, increment by 3 */
   /* next byte should increment the frame count, ID=1, L=1 */
   expect_true(p[0] == 0x03, "bad frame separator");
   expect_true(p[1] == 0x03, "bad frame increment");
   p += 2;
   /* extension ID=4 */
   expect_true((p[0] >> 1) == 4, "expected extension id 4");
   /* For extension IDs 1 through 31, L=0 means that no data follows the
      extension, whereas L=1 means that exactly one byte of extension data follows. */
   expect_true((p[0] & 0x01) == 0, "expected L-bit unset");
   p += 1;

   /* advance to frame 10, increment by 6 */
   /* next byte should increment the frame count, ID=1, L=1 */
   expect_true(p[0] == 0x03, "bad frame separator");
   expect_true(p[1] == 0x06, "bad frame increment");
   p += 2;
   /* extension ID=32 */
   expect_true((p[0] >> 1) == 32, "expected extension id 32");
   /* For IDs 32 to 127, L=0 signals that the extension data takes up the
      rest of the padding */
   expect_true((p[0] & 0x01) == 0, "expected L-bit unset");
   p += 1;
   expect_true(0 == memcmp(p, ext[1].data, ext[1].len), "expected extension content");
}

void test_extensions_generate_zero(void)
{
   int result;
   unsigned char packet[32];

   /* zero length packet, zero extensions */
   result = opus_packet_extensions_generate(packet, 0, NULL, 0, 0, 1);
   expect_true(result == 0, "expected length 0");
}

void test_extensions_generate_no_padding(void)
{
   static const opus_extension_data ext[] = {
      {3, 0, (const unsigned char *)"a", 1},
      {32, 10, (const unsigned char *)"DRED", 4},
      {33, 1, (const unsigned char *)"NOT DRED", 8},
      {4, 4, (const unsigned char *)NULL, 0}
   };

   int result;
   unsigned char packet[32];
   result =
    opus_packet_extensions_generate(packet, sizeof(packet), ext, 4, 11, 0);
   expect_true(result == 23, "expected length 23");
}

void test_extensions_generate_fail(void)
{
   static const opus_extension_data ext[] = {
      {3, 0, (const unsigned char *)"a", 1},
      {32, 10, (const unsigned char *)"DRED", 4},
      {33, 1, (const unsigned char *)"NOT DRED", 8},
      {4, 4, (const unsigned char *)NULL, 0}
   };

   int result;
   unsigned char packet[100];

   /* buffer too small */
   {
      opus_int32 len;
      /* this failure can occur at lots of points, so iterate to check as many
          as possible */
      for (len=0;len<23;len++)
      {
         size_t i;
         for (i=len;i<sizeof(packet);i++) packet[i] = 0xFE;
         result = opus_packet_extensions_generate(packet, len, ext, 4, 11, 1);
         expect_true(result == OPUS_BUFFER_TOO_SMALL,
          "expected OPUS_BUFFER_TOO_SMALL");
         for (i=len;i<sizeof(packet);i++)
         {
            expect_true(packet[i] == 0xFE,
             "expected 0xFE padding to be undisturbed");
         }
      }
   }

   /* invalid id */
   {
      static const opus_extension_data id_too_big[] = {
         {256, 0, (const unsigned char *)"a", 1},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       id_too_big, 1, 11, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* invalid id */
   {
      static const opus_extension_data id_too_small[] = {
         {2, 0, (const unsigned char *)"a", 1},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       id_too_small, 1, 11, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* frame count too big */
   {
      static const opus_extension_data frame_too_big[] = {
         {33, 11, (const unsigned char *)"a", 1},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       frame_too_big, 1, 49, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* frame index too small */
   {
      static const opus_extension_data frame_too_big[] = {
         {33, -1, (const unsigned char *)"a", 1},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       frame_too_big, 1, 11, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* frame index too big */
   {
      static const opus_extension_data frame_too_big[] = {
         {33, 11, (const unsigned char *)"a", 1},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       frame_too_big, 1, 11, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* size too big for extension IDs 1 through 31 */
   {
      static const opus_extension_data size_too_big[] = {
         {3, 0, (const unsigned char *)"abcd", 4},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       size_too_big, 1, 1, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* negative size for extension IDs 1 through 31 */
   {
      static const opus_extension_data neg_size[] = {
         {3, 0, NULL, -4},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       neg_size, 1, 1, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }

   /* negative size for extension IDs 32 through 127 */
   {
      static const opus_extension_data neg_size_33[] = {
         {33, 0, NULL, -4},
      };
      result = opus_packet_extensions_generate(packet, sizeof(packet),
       neg_size_33, 1, 1, 1);
      expect_true(result == OPUS_BAD_ARG, "expected OPUS_BAD_ARG");
   }
}

void test_extensions_parse_success(void)
{
   static const opus_extension_data ext[] = {
      {3, 0, (const unsigned char *)"a", 1},
      {32, 10, (const unsigned char *)"DRED", 4},
      {33, 1, (const unsigned char *)"NOT DRED", 8},
      {4, 4, (const unsigned char *)NULL, 0}
   };
   opus_extension_data ext_out[10];
   int nb_ext;
   int nb_frames;
   int len, result;
   unsigned char packet[32];

   nb_ext = 10;
   nb_frames = 11;
   len = opus_packet_extensions_generate(packet, 32, ext, 4, 11, 1);
   expect_true(len == 32, "expected length 32");
   result = opus_packet_extensions_count(packet, len, nb_frames);
   expect_true(result == 4, "expected opus_packet_extensions_count 4");
   result = opus_packet_extensions_parse(packet, len, ext_out, &nb_ext,
    nb_frames);
   expect_true(nb_ext == 4, "expected 4 extensions");

   expect_true(ext_out[0].id == 3, "expected id 3");
   expect_true(ext_out[0].frame == 0, "expected frame 0");
   expect_true(ext_out[0].len == 1, "expected len 1");
   expect_true(0 == memcmp(ext_out[0].data, ext[0].data, 1), "expected data");

   expect_true(ext_out[1].id == 33, "expected id 33");
   expect_true(ext_out[1].frame == 1, "expected frame 1");
   expect_true(ext_out[1].len == 8, "expected len 8");
   expect_true(0 == memcmp(ext_out[1].data, ext[2].data, 8), "expected data");

   expect_true(ext_out[2].id == 4, "expected id 4");
   expect_true(ext_out[2].frame == 4, "expected frame 4");
   expect_true(ext_out[2].len == 0, "expected len 0");

   expect_true(ext_out[3].id == 32, "expected id 32");
   expect_true(ext_out[3].frame == 10, "expected frame 10");
   expect_true(ext_out[3].len == 4, "expected len 4");
   expect_true(0 == memcmp(ext_out[3].data, ext[1].data, 4), "expected data");
}

void test_extensions_parse_zero(void)
{
   static const opus_extension_data ext[] = {
      {32, 1, (const unsigned char *)"DRED", 4},
   };
   int nb_ext;
   int nb_frames;
   int len, result;
   unsigned char packet[32];

   len = opus_packet_extensions_generate(packet, 32, ext, 1, 2, 1);
   expect_true(len == 32, "expected length 32");

   nb_ext = 0;
   nb_frames = 2;
   result = opus_packet_extensions_parse(packet, len, NULL, &nb_ext, nb_frames);
   expect_true(result == OPUS_BUFFER_TOO_SMALL, "expected OPUS_BUFFER_TOO_SMALL");
}

void test_extensions_parse_fail(void)
{
   static const opus_extension_data ext[] = {
      {3, 0, (const unsigned char *)"a", 1},
      {33, 1, (const unsigned char *)"NOT DRED", 8},
      {4, 4, (const unsigned char *)NULL, 0},
      {32, 10, (const unsigned char *)"DRED", 4},
      {32, 9, (const unsigned char *)"DRED", 4},
      {4, 9, (const unsigned char *)"b", 1},
      {4, 10, (const unsigned char *)"c", 1}
   };
   opus_extension_data ext_out[10];
   int nb_ext;
   int nb_frames;
   int len, result;
   unsigned char packet[32];

   /* create invalid length */
   len = opus_packet_extensions_generate(packet, sizeof(packet), ext, 4, 11, 0);
   packet[4] = 255;
   nb_ext = 10;
   nb_frames = 11;
   result = opus_packet_extensions_parse(packet, len, ext_out, &nb_ext,
    nb_frames);
   expect_true(result == OPUS_INVALID_PACKET, "expected OPUS_INVALID_PACKET");
   /* note, opus_packet_extensions_count stops at the invalid frame increment
      and tells us that we have 1 extension */
   result = opus_packet_extensions_count(packet, len, nb_frames);
   expect_true(result == 1, "expected opus_packet_extensions_count to return 1");

   /* create invalid frame increment */
   nb_ext = 10;
   len = opus_packet_extensions_generate(packet, sizeof(packet), ext, 4, 11, 0);
   /* first by reducing the number of frames */
   result = opus_packet_extensions_parse(packet, len, ext_out, &nb_ext, 5);
   expect_true(result == OPUS_INVALID_PACKET, "expected OPUS_INVALID_PACKET");

   /* note, opus_packet_extensions_count stops at the invalid frame increment
      and tells us that we have 3 extensions */
   result = opus_packet_extensions_count(packet, len, 5);
   expect_true(result == 3, "expected opus_packet_extensions_count to return 3");

   /* then by increasing the increment */
   packet[14] = 255;
   result = opus_packet_extensions_parse(packet, len, ext_out, &nb_ext,
    nb_frames);
   expect_true(result == OPUS_INVALID_PACKET, "expected OPUS_INVALID_PACKET");

   /* note, opus_packet_extensions_count stops at the invalid frame increment
      and tells us that we have 2 extensions */
   result = opus_packet_extensions_count(packet, len, nb_frames);
   expect_true(result == 2, "expected opus_packet_extensions_count to return 2");

   /* not enough space */
   nb_ext = 1;
   len = opus_packet_extensions_generate(packet, sizeof(packet), ext, 4, 11, 0);
   result = opus_packet_extensions_parse(packet, len, ext_out, &nb_ext,
    nb_frames);
   expect_true(result == OPUS_BUFFER_TOO_SMALL, "expected OPUS_BUFFER_TOO_SMALL");

   /* create repeated L=0 long extension without enough room for all of the
       short extension payloads that need to follow it */
   len = opus_packet_extensions_generate(packet, sizeof(packet), ext, 7, 11, 0);
   len -= 5;
   nb_ext = 10;
   nb_frames = 11;
   result = opus_packet_extensions_parse(packet, len, ext_out, &nb_ext,
    nb_frames);
   expect_true(result == OPUS_INVALID_PACKET, "expected OPUS_INVALID_PACKET");
   /* note, opus_packet_extensions_count stops at the invalid long extension
       and tells us that we have 5 extensions */
   result = opus_packet_extensions_count(packet, len, nb_frames);
   expect_true(result == 5, "expected opus_packet_extensions_count to return 5");

   /* overflow for long extension length */
   {
      /* about 8 MB */
#define LENSIZE ((1U<<31)/255 + 1)
      unsigned char *buf = malloc(LENSIZE+1);
      len = LENSIZE+1;
      buf[0] = 33<<1 | 1;
      memset(buf + 1, 0xFF, LENSIZE - 1);
      buf[LENSIZE] = 0xFE;
      result = opus_packet_extensions_parse(buf, len, ext_out, &nb_ext, 1);
      expect_true(result == OPUS_INVALID_PACKET, "expected OPUS_INVALID_PACKET");
      free(buf);
   }
}

/* Check that the same extensions with the same data appear in both ext_in and
    ext_out, with extensions in ext_out appearing in frame order, and the order
    within a frame matching the order within a frame in ext_in. */
static void check_ext_data(const opus_extension_data *ext_in,
 const opus_extension_data *ext_out, int nb_ext)
{
   opus_int32 i;
   opus_int32 j;
   int prev_frame;
   prev_frame = -1;
   j = 0;
   for (i=0;i<nb_ext;i++)
   {
      expect_true(ext_out[i].frame >= prev_frame,
       "expected parsed extensions to be returned in frame order");
      if (ext_out[i].frame > prev_frame) {
         j = 0;
      }
      while (j < nb_ext && ext_in[j].frame != ext_out[i].frame) j++;
      expect_true(j < nb_ext,
       "expected enough extensions matching this frame");
      expect_true(ext_in[j].id == ext_out[i].id,
       "expected extension IDs to match");
      expect_true(ext_in[j].len == ext_out[i].len,
       "expected extension lengths to match");
      /* The len check allows ext_in[j].data to be NULL without triggering
          undefined behavior. */
      expect_true(ext_in[j].len == 0 ||
       memcmp(ext_in[j].data, ext_out[i].data, ext_out[i].len) == 0,
       "expected extension data to match");
      prev_frame = ext_out[i].frame;
      j++;
   }
}

#define NB_EXT (13)

void test_extensions_repeating(void)
{
   static const opus_extension_data ext[] = {
      {3, 0, (const unsigned char *)"a", 1},
      {3, 1, (const unsigned char *)"b", 1},
      {3, 2, (const unsigned char *)"c", 1},
      {4, 0, (const unsigned char *)"d", 1},
      {4, 1, (const unsigned char *)NULL, 0},
      {4, 2, (const unsigned char *)NULL, 0},
      {32, 2, (const unsigned char *)"DRED2", 5},
      {32, 1, (const unsigned char *)"DRED", 4},
      {5, 1, (const unsigned char *)NULL, 0},
      {5, 2, (const unsigned char *)NULL, 0},
      {6, 2, (const unsigned char *)"f", 1},
      {6, 1, (const unsigned char *)"e", 1},
      {32, 2, (const unsigned char *)"DREDthree", 9}
   };
   static const opus_int32 encoded_len[] = {
      0,
      2,
      /* nb_ext = 2: don't try to repeat if the same extension is not used in
          every frame */
      5,
      /* nb_ext = 3: do repeat if the same extension is used in every frame */
      5,
      7,
      9,
      /* nb_ext = 6: do not repeat short extensions if the lengths do not match
          ... but do repeat after the first frame when the lengths do match. */
      10,
      /* nb_ext = 7: code repeated extensions with L=0 to skip a frame
          separator because they are all short extensions. */
      16,
      /* nb_ext = 8: repeat multiple extensions in the same frame.
         code the last repeated extension with L=0 if it is a long
          extension. */
      21,
      23,
      /* nb_ext = 10: code the last repeated long extension with L=0 even if it
          is followed by repeated short extensions with L=0. */
      22,
      /* nb_ext = 11: don't use L=0 to skip a frame separator if repeats end
          on a short extension if there was a preceding L=0 long extension. */
      26,
      /* nb_ext = 12: code the last repeated long extension with L=0 even if
          it is followed by repeated short extensions with L=1. */
      25,
      /* nb_ext = 13: don't use L=0 to skip a frame separator if repeats end
          on a short extension if there was a preceding L=1 long extension. */
      37
   };
   opus_int32 nb_ext;
   for (nb_ext = 0; nb_ext <= NB_EXT; nb_ext++) {
      opus_extension_data ext_out[NB_EXT];
      opus_int32 nb_frame_exts[48];
      opus_int32 nb_ext_out;
      int len, result;
      unsigned char packet[64];
      len = opus_packet_extensions_generate(packet, sizeof(packet), ext,
       nb_ext, 3, 0);
      expect_true(len == encoded_len[nb_ext],
       "expected extension encoding length to match");
      result = opus_packet_extensions_count_ext(packet, len, nb_frame_exts, 3);
      expect_true(result == nb_ext, "expected extension count to match");
      nb_ext_out = NB_EXT;
      result = opus_packet_extensions_parse_ext(packet, len, ext_out,
       &nb_ext_out, nb_frame_exts, 3);
      expect_true(result == 0, "expected extension parsing to succeed");
      expect_true(nb_ext_out == nb_ext, "expected extension count to match");
      check_ext_data(ext, ext_out, nb_ext);
      /* Special case some modifications to test things our generator will
          never produce. */
      if (nb_ext == 6) {
         /* allow a repeat in the last frame, as well as trailing junk after an
             L=0 repeat that MUST be ignored. */
         packet[len++] = 2<<1|0;
         packet[len++] = 3<<1|0;
      }
      else if (nb_ext == 8) {
         /* insert padding before the repeat indicator */
         memmove(packet+16, packet+15, len-15);
         packet[15] = 0x1;
         len++;
         /* don't repeat the padding and continue decoding last extension with
             L=0 */
      }
      else if (nb_ext == 10) {
         /* use a frame separator with an increment of 0 as padding.
            This should _not_ change which extensions get repeated. */
         memmove(packet+17, packet+15, len-15);
         packet[15] = 0x3;
         packet[16] = 0;
         len += 2;
      }
      else if (nb_ext == 13) {
         /* use L=0 to skip a frame separator if a repeat has no extensions at
             all */
         packet[26] = 2<<1|0;
      }
      else continue;
      result = opus_packet_extensions_count_ext(packet, len, nb_frame_exts, 3);
      expect_true(result == nb_ext, "expected extension count to match");
      nb_ext_out = NB_EXT;
      result = opus_packet_extensions_parse_ext(packet, len, ext_out,
       &nb_ext_out, nb_frame_exts, 3);
      expect_true(result == 0, "expected extension parsing to succeed");
      expect_true(nb_ext_out == nb_ext, "expected extension count to match");
      check_ext_data(ext, ext_out, nb_ext);
      if (nb_ext == 8) {
         /* allow multiple repeat indicators in the same frame */
         memmove(packet+10, packet+9, len-9);
         packet[9] = 2<<1|1;
         len++;
         /* even when there are no new extensions to repeat */
         memmove(packet+6, packet+5, len-5);
         packet[5] = 2<<1|1;
         len++;
      }
      else continue;
      result = opus_packet_extensions_count_ext(packet, len, nb_frame_exts, 3);
      expect_true(result == nb_ext, "expected extension count to match");
      nb_ext_out = NB_EXT;
      result = opus_packet_extensions_parse_ext(packet, len, ext_out,
       &nb_ext_out, nb_frame_exts, 3);
      expect_true(result == 0, "expected extension parsing to succeed");
      expect_true(nb_ext_out == nb_ext, "expected extension count to match");
      check_ext_data(ext, ext_out, nb_ext);
   }
}

#define NB_RANDOM_EXTENSIONS 100000000
#define MAX_EXTENSION_SIZE 200
/* The worst case is a 48-frame packet filled entirely with 0-byte short
    extensions followed by an RTE. */
#define MAX_NB_EXTENSIONS ((MAX_EXTENSION_SIZE - 1)*48)

void test_random_extensions_parse(void)
{
   int i;
   for (i=0;i<NB_RANDOM_EXTENSIONS;i++)
   {
      opus_extension_data ext_out[MAX_NB_EXTENSIONS];
      int nb_frames;
      int nb_ext;
      unsigned char payload[MAX_EXTENSION_SIZE];
      int len;
      int j;
      int result;
      len = fast_rand()%(MAX_EXTENSION_SIZE+1);
      for (j=0;j<len;j++)
         payload[j] = fast_rand()&0xFF;
      nb_ext = fast_rand()%(MAX_NB_EXTENSIONS+1);
      /* The success rate is near 60.4% almost independent of the number of
          frames we choose, so pick it randomly.
         nb_frames == 1 is slightly (~0.2%) less likely to succeed, while
          nb_frames == 2 is slightly (~0.1%) more likely to succeed, followed
          by nb_frames == 3 being very slightly (~0.02%) more likely. */
      nb_frames = (fast_rand()%48) + 1;
      result =
       opus_packet_extensions_parse(payload, len, ext_out, &nb_ext, nb_frames);
      expect_true(result == OPUS_OK || result == OPUS_BUFFER_TOO_SMALL || result == OPUS_INVALID_PACKET, "expected OPUS_OK, OPUS_BUFFER_TOO_SMALL or OPUS_INVALID_PACKET");
      /* Even if parsing fails, check that the extensions that got extracted make sense. */
      for (j=0;j<nb_ext;j++)
      {
         expect_true(ext_out[j].frame >= 0 && ext_out[j].frame < nb_frames,
          "expected frame between 0 and nb_frames-1");
         expect_true(ext_out[j].id >= 2 && ext_out[j].id <= 127, "expected id between 2 and 127");
         expect_true(ext_out[j].data >= payload && ext_out[j].data+ext_out[j].len <= payload+len, "expected data to be within packet");
      }
      /* If parsing succeeds, check to see if we can round-trip these
          extensions. */
      if (result == OPUS_OK) {
         opus_extension_data ext_out2[MAX_NB_EXTENSIONS];
         unsigned char payload2[MAX_EXTENSION_SIZE+1];
         opus_int32 nb_frame_exts[48];
         opus_int32 nb_ext_out;
         len = opus_packet_extensions_generate(payload2, sizeof(payload2),
          ext_out, nb_ext, nb_frames, 0);
         expect_true(len >= 0, "expected extension generation to succeed");
         result = opus_packet_extensions_count_ext(payload2, len,
          nb_frame_exts, nb_frames);
         expect_true(result == nb_ext, "expected extension count to match");
         nb_ext_out = MAX_NB_EXTENSIONS;
         result = opus_packet_extensions_parse_ext(payload2, len, ext_out2,
          &nb_ext_out, nb_frame_exts, nb_frames);
         expect_true(result == OPUS_OK,
          "expected extension parsing to succeed");
         expect_true(nb_ext == nb_ext_out,
          "expected extension count to match");
         check_ext_data(ext_out, ext_out2, nb_ext);
      }
   }
}

void test_opus_repacketizer_out_range_impl(void)
{
   OpusRepacketizer rp;
   unsigned char packet[1024];
   unsigned char packet_out[1024];
   opus_int16 size[48];
   const unsigned char *padding;
   opus_int32 padding_len;
   opus_extension_data ext_out[10];
   int i;
   int nb_ext;
   int res, len;
   int first_count = 0, second_count = 0;
   static const opus_extension_data ext[] = {
      {33, 0, (const unsigned char *)"abcdefg", 7},
      {100, 0, (const unsigned char *)"uvwxyz", 6},
   };

   opus_repacketizer_init(&rp);

   memset(packet, 0, sizeof(packet));
   /* Hybrid Packet with 20 msec frames, Code 3 */
   packet[0] = (15 << 3) | 3;
   /* Code 3, padding bit set, 1 frame */
   packet[1] = 1 << 6 | 1;
   packet[2] = 0;
   packet[3] = 0;

   /* generate 2 extensions, id 33 and 100 */
   len = opus_packet_extensions_generate(&packet[4], sizeof(packet)-4, ext, 2,
    1, 0);
   /* update the padding length */
   packet[2] = len;

   /* concatenate 3 frames */
   res = opus_repacketizer_cat(&rp, packet, 4+len);
   /* for the middle frame, no padding, no extensions */
   packet[1] = 1;
   res = opus_repacketizer_cat(&rp, packet, 4);
   /* switch back to extensions for the last frame extensions */
   packet[1] = 1 << 6 | 1;
   res = opus_repacketizer_cat(&rp, packet, 4+len);

   expect_true(rp.nb_frames == 3, "Expected 3 frames");
   res = opus_repacketizer_out_range_impl(&rp,
      0, 3, /* begin, end */
      packet_out, /* unsigned char *data */
      sizeof(packet_out), /* opus_int32 maxlen */
      0, /*int self_delimited */
      0, /* int pad */
      NULL, /* const opus_extension_data *extensions */
      0 /* int nb_extensions */);
   expect_true(res > 0, "expected valid packet length");

   /* now verify that we have the expected extensions */
   res = opus_packet_parse_impl(packet_out, res, 0, NULL, NULL, size,
      NULL, NULL, &padding, &padding_len);
   nb_ext = 10;
   res = opus_packet_extensions_parse(padding, padding_len, ext_out, &nb_ext,
    3);
   expect_true(nb_ext == 4, "Expected 4 extensions");
   for (i = 0 ; i < nb_ext; i++)
   {
      if (ext_out[i].id == 33)
      {
         opus_test_assert(ext_out[i].len == ext[0].len);
         opus_test_assert(0 == memcmp(ext_out[i].data, ext[0].data, ext[0].len));
         first_count++;
      }
      else if (ext_out[i].id == 100)
      {
         opus_test_assert(ext_out[i].len == ext[1].len);
         opus_test_assert(0 == memcmp(ext_out[i].data, ext[1].data, ext[1].len));
         second_count++;
      }
      if (i < 2)
         opus_test_assert(ext_out[i].frame == 0)
      else
         opus_test_assert(ext_out[i].frame == 2)
   }
   opus_test_assert(first_count == 2);
   opus_test_assert(second_count == 2);
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

   fprintf(stderr,"Testing extensions. Random seed: %u (%.4X)\n", iseed, fast_rand() % 65535);
   if(env_used)fprintf(stderr,"  Random seed set from the environment (SEED=%s).\n", env_seed);

   test_extensions_generate_success();
   test_extensions_generate_zero();
   test_extensions_generate_no_padding();
   test_extensions_generate_fail();
   test_extensions_parse_success();
   test_extensions_parse_zero();
   test_extensions_parse_fail();
   test_extensions_repeating();
   test_random_extensions_parse();
   test_opus_repacketizer_out_range_impl();
   fprintf(stderr,"Tests completed successfully.\n");
   return 0;
}
