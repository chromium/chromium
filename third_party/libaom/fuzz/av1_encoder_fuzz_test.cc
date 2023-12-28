/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstring>
#include <variant>
#include <vector>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "third_party/libaom/source/libaom/aom/aom_codec.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aom_image.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"

using fuzztest::Arbitrary;
using fuzztest::ElementOf;
using fuzztest::FlatMap;
using fuzztest::InRange;
using fuzztest::Just;
using fuzztest::StructOf;
using fuzztest::VariantOf;
using fuzztest::VectorOf;

// Represents a VideoEncoder::configure() method call.
// Parameters:
//   VideoEncoderConfig config
struct Configure {
  unsigned int threads;  // Not part of VideoEncoderConfig
  unsigned int width;    // Nonzero
  unsigned int height;   // Nonzero
  // TODO(wtc): displayWidth, displayHeight, bitrate, framerate,
  // scalabilityMode.
  aom_rc_mode end_usage;  // Implies bitrateMode: constant, variable.
                          // TODO(wtc): quantizer.
  unsigned int usage;     // Implies LatencyMode: quality, realtime.
  // TODO(wtc): contentHint.
};

auto AnyConfigureWithSize(unsigned int width, unsigned int height) {
  return StructOf<Configure>(
      // Chrome's WebCodecs uses at most 16 threads.
      /*threads=*/InRange(0u, 16u), /*width=*/Just(width),
      /*height=*/Just(height),
      /*end_usage=*/ElementOf({AOM_VBR, AOM_CBR}),
      /*usage=*/
      Just(AOM_USAGE_REALTIME));
}

auto AnyConfigureWithMaxSize(unsigned int max_width, unsigned int max_height) {
  return StructOf<Configure>(
      // Chrome's WebCodecs uses at most 16 threads.
      /*threads=*/InRange(0u, 16u),
      /*width=*/InRange(1u, max_width),
      /*height=*/InRange(1u, max_height),
      /*end_usage=*/ElementOf({AOM_VBR, AOM_CBR}),
      /*usage=*/
      Just(AOM_USAGE_REALTIME));
}

// Represents a VideoEncoder::encode() method call.
// Parameters:
//   VideoFrame frame
//   optional VideoEncoderEncodeOptions options = {}
struct Encode {
  bool key_frame;
  // TODO(wtc): quantizer.
};

auto AnyEncode() {
  return StructOf<Encode>(Arbitrary<bool>());
}

using MethodCall = std::variant<Configure, Encode>;

auto AnyMethodCallWithMaxSize(unsigned int max_width, unsigned int max_height) {
  return VariantOf(AnyConfigureWithMaxSize(max_width, max_height), AnyEncode());
}

struct CallSequence {
  Configure initialize;
  std::vector<MethodCall> method_calls;
};

auto AnyCallSequenceWithMaxSize(unsigned int max_width,
                                unsigned int max_height) {
  return StructOf<CallSequence>(
      /*initialize=*/AnyConfigureWithSize(max_width, max_height),
      /*method_calls=*/VectorOf(AnyMethodCallWithMaxSize(max_width, max_height))
          .WithMaxSize(20));
}

auto AnyCallSequence() {
  return FlatMap(AnyCallSequenceWithMaxSize,
                 /*max_width=*/InRange(1u, 1920u),
                 /*max_height=*/InRange(1u, 1080u));
}

void AV1EncodeArbitraryCallSequenceSucceeds(int speed,
                                            const CallSequence& call_sequence) {
  aom_codec_iface_t* const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  const unsigned int usage = call_sequence.initialize.usage;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_threads = call_sequence.initialize.threads;
  cfg.g_w = call_sequence.initialize.width;
  cfg.g_h = call_sequence.initialize.height;
  cfg.g_forced_max_frame_width = cfg.g_w;
  cfg.g_forced_max_frame_height = cfg.g_h;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000 * 1000;  // microseconds
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = call_sequence.initialize.end_usage;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 58;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);

  const aom_codec_cx_pkt_t* pkt;

  int frame_index = 0;
  for (const auto& call : call_sequence.method_calls) {
    if (std::holds_alternative<Configure>(call)) {
      const Configure& configure = std::get<Configure>(call);
      cfg.g_threads = configure.threads;
      cfg.g_w = configure.width;
      cfg.g_h = configure.height;
      cfg.rc_end_usage = configure.end_usage;
      ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_OK)
          << aom_codec_error_detail(&enc);
    } else {
      // Encode a frame.
      const Encode& encode = std::get<Encode>(call);
      // TODO(wtc): Support high bit depths and other YUV formats.
      aom_image_t* const image =
          aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
      ASSERT_NE(image, nullptr);

      for (unsigned int i = 0; i < image->d_h; ++i) {
        memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
      }
      const unsigned int uv_h = (image->d_h + 1) / 2;
      const unsigned int uv_w = (image->d_w + 1) / 2;
      for (unsigned int i = 0; i < uv_h; ++i) {
        memset(image->planes[1] + i * image->stride[1], 128, uv_w);
        memset(image->planes[2] + i * image->stride[2], 128, uv_w);
      }

      const aom_enc_frame_flags_t flags =
          encode.key_frame ? AOM_EFLAG_FORCE_KF : 0;
      ASSERT_EQ(aom_codec_encode(&enc, image, frame_index, 1, flags),
                AOM_CODEC_OK);
      frame_index++;
      aom_codec_iter_t iter = nullptr;
      while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
        ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
        if (encode.key_frame) {
          ASSERT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
        }
      }
      aom_img_free(image);
    }
  }

  // Flush the encoder.
  bool got_data;
  do {
    ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
    got_data = false;
    aom_codec_iter_t iter = nullptr;
    while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      got_data = true;
    }
  } while (got_data);

  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

FUZZ_TEST(AV1EncodeFuzzTest, AV1EncodeArbitraryCallSequenceSucceeds)
    .WithDomains(/*speed=*/InRange(5, 11),
                 /*call_sequence=*/AnyCallSequence());

// speed: range 5..11
// end_usage: Rate control mode
void AV1EncodeSucceeds(unsigned int threads,
                       int speed,
                       aom_rc_mode end_usage,
                       unsigned int width,
                       unsigned int height,
                       int num_frames) {
  aom_codec_iface_t* const iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  const unsigned int usage = AOM_USAGE_REALTIME;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  cfg.g_threads = threads;
  cfg.g_w = width;
  cfg.g_h = height;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000 * 1000;  // microseconds
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = end_usage;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 58;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);

  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, speed), AOM_CODEC_OK);

  // TODO(wtc): Support high bit depths and other YUV formats.
  aom_image_t* const image =
      aom_img_alloc(nullptr, AOM_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(image, nullptr);

  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  const unsigned int uv_h = (image->d_h + 1) / 2;
  const unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  // Encode frames.
  const aom_codec_cx_pkt_t* pkt;
  for (int i = 0; i < num_frames; ++i) {
    ASSERT_EQ(aom_codec_encode(&enc, image, i, 1, 0), AOM_CODEC_OK);
    aom_codec_iter_t iter = nullptr;
    while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
    }
  }

  // Flush the encoder.
  bool got_data;
  do {
    ASSERT_EQ(aom_codec_encode(&enc, nullptr, 0, 0, 0), AOM_CODEC_OK);
    got_data = false;
    aom_codec_iter_t iter = nullptr;
    while ((pkt = aom_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
      got_data = true;
    }
  } while (got_data);

  aom_img_free(image);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// Chrome's WebCodecs uses at most 16 threads.
FUZZ_TEST(AV1EncodeFuzzTest, AV1EncodeSucceeds)
    .WithDomains(/*threads=*/InRange(0u, 16u),
                 /*speed=*/InRange(5, 11),
                 /*end_usage=*/ElementOf({AOM_VBR, AOM_CBR}),
                 /*width=*/InRange(1u, 1920u),
                 /*height=*/InRange(1u, 1080u),
                 /*num_frames=*/InRange(1, 10));
