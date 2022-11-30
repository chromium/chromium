/* Copyright (c) 2014, Cisco Systems, INC
   Written by XiangMingZhu WeiZhou MinPeng YanWang

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

#if !defined(X86CPU_H)
# define X86CPU_H

# if defined(OPUS_X86_MAY_HAVE_SSE)
#  define MAY_HAVE_SSE(name) name ## _sse
# else
#  define MAY_HAVE_SSE(name) name ## _c
# endif

# if defined(OPUS_X86_MAY_HAVE_SSE2)
#  define MAY_HAVE_SSE2(name) name ## _sse2
# else
#  define MAY_HAVE_SSE2(name) name ## _c
# endif

# if defined(OPUS_X86_MAY_HAVE_SSE4_1)
#  define MAY_HAVE_SSE4_1(name) name ## _sse4_1
# else
#  define MAY_HAVE_SSE4_1(name) name ## _c
# endif

# if defined(OPUS_X86_MAY_HAVE_AVX)
#  define MAY_HAVE_AVX(name) name ## _avx
# else
#  define MAY_HAVE_AVX(name) name ## _c
# endif

# if defined(OPUS_HAVE_RTCD)
int opus_select_arch(void);
# endif

/*MOVD should not impose any alignment restrictions, but the C standard does,
   and UBSan will report errors if we actually make unaligned accesses.
  Use this to work around those restrictions (which should hopefully all get
   optimized to a single MOVD instruction).*/
#define OP_LOADU_EPI32(x) \
  (int)((*(unsigned char *)(x) | *((unsigned char *)(x) + 1) << 8U |\
   *((unsigned char *)(x) + 2) << 16U | (opus_uint32)*((unsigned char *)(x) + 3) << 24U))

#define OP_CVTEPI8_EPI32_M32(x) \
 (_mm_cvtepi8_epi32(_mm_cvtsi32_si128(OP_LOADU_EPI32(x))))

#define OP_CVTEPI16_EPI32_M64(x) \
 (_mm_cvtepi16_epi32(_mm_loadl_epi64((__m128i *)(x))))

#endif
