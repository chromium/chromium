if(__opus_sources)
  return()
endif()
set(__opus_sources INCLUDED)

include(OpusFunctions)

get_opus_sources(SILK_HEAD silk_headers.mk silk_headers)
get_opus_sources(SILK_SOURCES silk_sources.mk silk_sources)
get_opus_sources(SILK_SOURCES_FLOAT silk_sources.mk silk_sources_float)
get_opus_sources(SILK_SOURCES_FIXED silk_sources.mk silk_sources_fixed)
get_opus_sources(SILK_SOURCES_SSE4_1 silk_sources.mk silk_sources_sse4_1)
get_opus_sources(SILK_SOURCES_FIXED_SSE4_1 silk_sources.mk
                 silk_sources_fixed_sse4_1)
get_opus_sources(SILK_SOURCES_ARM_NEON_INTR silk_sources.mk
                 silk_sources_arm_neon_intr)
get_opus_sources(SILK_SOURCES_FIXED_ARM_NEON_INTR silk_sources.mk
                 silk_sources_fixed_arm_neon_intr)

get_opus_sources(OPUS_HEAD opus_headers.mk opus_headers)
get_opus_sources(OPUS_SOURCES opus_sources.mk opus_sources)
get_opus_sources(OPUS_SOURCES_FLOAT opus_sources.mk opus_sources_float)

get_opus_sources(CELT_HEAD celt_headers.mk celt_headers)
get_opus_sources(CELT_SOURCES celt_sources.mk celt_sources)
get_opus_sources(CELT_SOURCES_SSE celt_sources.mk celt_sources_sse)
get_opus_sources(CELT_SOURCES_SSE2 celt_sources.mk celt_sources_sse2)
get_opus_sources(CELT_SOURCES_SSE4_1 celt_sources.mk celt_sources_sse4_1)
get_opus_sources(CELT_SOURCES_ARM celt_sources.mk celt_sources_arm)
get_opus_sources(CELT_SOURCES_ARM_ASM celt_sources.mk celt_sources_arm_asm)
get_opus_sources(CELT_AM_SOURCES_ARM_ASM celt_sources.mk
                 celt_am_sources_arm_asm)
get_opus_sources(CELT_SOURCES_ARM_NEON_INTR celt_sources.mk
                 celt_sources_arm_neon_intr)
get_opus_sources(CELT_SOURCES_ARM_NE10 celt_sources.mk celt_sources_arm_ne10)

get_opus_sources(opus_demo_SOURCES Makefile.am opus_demo_sources)
get_opus_sources(opus_custom_demo_SOURCES Makefile.am opus_custom_demo_sources)
get_opus_sources(opus_compare_SOURCES Makefile.am opus_compare_sources)
get_opus_sources(tests_test_opus_api_SOURCES Makefile.am test_opus_api_sources)
get_opus_sources(tests_test_opus_encode_SOURCES Makefile.am
                 test_opus_encode_sources)
get_opus_sources(tests_test_opus_decode_SOURCES Makefile.am
                 test_opus_decode_sources)
get_opus_sources(tests_test_opus_padding_SOURCES Makefile.am
                 test_opus_padding_sources)
