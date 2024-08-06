/*
 *  Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <cstring>
#include <variant>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_image.h"

using fuzztest::Arbitrary;
using fuzztest::ElementOf;
using fuzztest::FlatMap;
using fuzztest::InRange;
using fuzztest::Just;
using fuzztest::StructOf;
using fuzztest::VariantOf;
using fuzztest::VectorOf;

auto AnyBitDepth() {
  vpx_codec_iface_t* const iface = vpx_codec_vp9_cx();
  return (vpx_codec_get_caps(iface) & VPX_CODEC_CAP_HIGHBITDEPTH)
             ? ElementOf({VPX_BITS_8, VPX_BITS_10, VPX_BITS_12})
             : Just(VPX_BITS_8);
}

// Represents a VideoEncoder::configure() method call.
// Parameters:
//   VideoEncoderConfig config
struct Configure {
  unsigned int threads;  // Not part of VideoEncoderConfig
  unsigned int width;    // Nonzero
  unsigned int height;   // Nonzero
  // TODO(wtc): displayWidth, displayHeight, bitrate, framerate,
  // scalabilityMode.
  vpx_rc_mode end_usage;        // Implies bitrateMode: constant, variable.
                                // TODO(wtc): quantizer.
  vpx_enc_deadline_t deadline;  // Implies LatencyMode: quality, realtime.
  // TODO(wtc): contentHint.
};

auto AnyConfigureWithSize(unsigned int width, unsigned int height) {
  return StructOf<Configure>(
      // Chrome's WebCodecs uses at most 16 threads.
      /*threads=*/InRange(0u, 16u), /*width=*/Just(width),
      /*height=*/Just(height),
      /*end_usage=*/ElementOf({VPX_VBR, VPX_CBR}),
      /*deadline=*/
      ElementOf({VPX_DL_GOOD_QUALITY, VPX_DL_REALTIME}));
}

auto AnyConfigureWithMaxSize(unsigned int max_width, unsigned int max_height) {
  return StructOf<Configure>(
      // Chrome's WebCodecs uses at most 16 threads.
      /*threads=*/InRange(0u, 16u),
      /*width=*/InRange(1u, max_width),
      /*height=*/InRange(1u, max_height),
      /*end_usage=*/ElementOf({VPX_VBR, VPX_CBR}),
      /*deadline=*/
      ElementOf({VPX_DL_GOOD_QUALITY, VPX_DL_REALTIME}));
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

void* Memset16(void* dest, int val, size_t length) {
  uint16_t* dest16 = reinterpret_cast<uint16_t*>(dest);
  for (size_t i = 0; i < length; i++) {
    *dest16++ = val;
  }
  return dest;
}

vpx_image_t* CreateGrayImage(vpx_bit_depth_t bit_depth,
                             vpx_img_fmt_t fmt,
                             unsigned int width,
                             unsigned int height) {
  if (bit_depth > VPX_BITS_8) {
    fmt = static_cast<vpx_img_fmt_t>(fmt | VPX_IMG_FMT_HIGHBITDEPTH);
  }
  vpx_image_t* image = vpx_img_alloc(nullptr, fmt, width, height, 1);
  if (!image) {
    return image;
  }

  const int val = 1 << (bit_depth - 1);
  const unsigned int uv_h =
      (image->d_h + image->y_chroma_shift) >> image->y_chroma_shift;
  const unsigned int uv_w =
      (image->d_w + image->x_chroma_shift) >> image->x_chroma_shift;
  if (bit_depth > VPX_BITS_8) {
    for (unsigned int i = 0; i < image->d_h; ++i) {
      Memset16(image->planes[0] + i * image->stride[0], val, image->d_w);
    }
    for (unsigned int i = 0; i < uv_h; ++i) {
      Memset16(image->planes[1] + i * image->stride[1], val, uv_w);
      Memset16(image->planes[2] + i * image->stride[2], val, uv_w);
    }
  } else {
    for (unsigned int i = 0; i < image->d_h; ++i) {
      memset(image->planes[0] + i * image->stride[0], val, image->d_w);
    }
    for (unsigned int i = 0; i < uv_h; ++i) {
      memset(image->planes[1] + i * image->stride[1], val, uv_w);
      memset(image->planes[2] + i * image->stride[2], val, uv_w);
    }
  }

  return image;
}

void VP9EncodeArbitraryCallSequenceSucceeds(int speed,
                                            unsigned int row_mt,
                                            vpx_bit_depth_t bit_depth,
                                            vpx_img_fmt_t fmt,
                                            const CallSequence& call_sequence) {
  ASSERT_EQ(fmt & VPX_IMG_FMT_HIGHBITDEPTH, 0);
  const bool high_bit_depth = bit_depth > VPX_BITS_8;
  const bool is_420 = fmt == VPX_IMG_FMT_I420;
  vpx_codec_iface_t* const iface = vpx_codec_vp9_cx();
  vpx_codec_enc_cfg_t cfg;
  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, /*usage=*/0),
            VPX_CODEC_OK);
  cfg.g_threads = call_sequence.initialize.threads;
  // In profiles 0 and 2, only 4:2:0 format is allowed. In profiles 1 and 3,
  // all other subsampling formats are allowed. In profiles 0 and 1, only bit
  // depth 8 is allowed. In profiles 2 and 3, only bit depths 10 and 12 are
  // allowed.
  cfg.g_profile = 2 * high_bit_depth + !is_420;
  cfg.g_w = call_sequence.initialize.width;
  cfg.g_h = call_sequence.initialize.height;
  cfg.g_bit_depth = bit_depth;
  cfg.g_input_bit_depth = bit_depth;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000 * 1000;  // microseconds
  cfg.g_pass = VPX_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = call_sequence.initialize.end_usage;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 58;

  vpx_codec_ctx_t enc;
  ASSERT_EQ(vpx_codec_enc_init(&enc, iface, &cfg,
                               high_bit_depth ? VPX_CODEC_USE_HIGHBITDEPTH : 0),
            VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_CPUUSED, speed), VPX_CODEC_OK);
  ASSERT_EQ(vpx_codec_control(&enc, VP9E_SET_ROW_MT, row_mt), VPX_CODEC_OK);

  vpx_enc_deadline_t deadline = call_sequence.initialize.deadline;
  const vpx_codec_cx_pkt_t* pkt;

  int frame_index = 0;
  for (const auto& call : call_sequence.method_calls) {
    if (std::holds_alternative<Configure>(call)) {
      const Configure& configure = std::get<Configure>(call);
      cfg.g_threads = configure.threads;
      cfg.g_w = configure.width;
      cfg.g_h = configure.height;
      cfg.rc_end_usage = configure.end_usage;
      ASSERT_EQ(vpx_codec_enc_config_set(&enc, &cfg), VPX_CODEC_OK)
          << vpx_codec_error_detail(&enc);
      deadline = configure.deadline;
    } else {
      // Encode a frame.
      const Encode& encode = std::get<Encode>(call);
      vpx_image_t* const image =
          CreateGrayImage(bit_depth, fmt, cfg.g_w, cfg.g_h);
      ASSERT_NE(image, nullptr);

      const vpx_enc_frame_flags_t flags =
          encode.key_frame ? VPX_EFLAG_FORCE_KF : 0;
      ASSERT_EQ(vpx_codec_encode(&enc, image, frame_index, 1, flags, deadline),
                VPX_CODEC_OK);
      frame_index++;
      vpx_codec_iter_t iter = nullptr;
      while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
        ASSERT_EQ(pkt->kind, VPX_CODEC_CX_FRAME_PKT);
        if (encode.key_frame) {
          ASSERT_EQ(pkt->data.frame.flags & VPX_FRAME_IS_KEY, VPX_FRAME_IS_KEY);
        }
      }
      vpx_img_free(image);
    }
  }

  // Flush the encoder.
  bool got_data;
  do {
    ASSERT_EQ(vpx_codec_encode(&enc, nullptr, 0, 1, 0, deadline), VPX_CODEC_OK);
    got_data = false;
    vpx_codec_iter_t iter = nullptr;
    while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, VPX_CODEC_CX_FRAME_PKT);
      got_data = true;
    }
  } while (got_data);

  ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
}

FUZZ_TEST(VP9EncodeFuzzTest, VP9EncodeArbitraryCallSequenceSucceeds)
    .WithDomains(
        /*speed=*/InRange(-9, 9),
        /*row_mt=*/InRange(0u, 1u),
        /*bit_depth=*/AnyBitDepth(),
        // Only generate image formats that don't have the
        // VPX_IMG_FMT_HIGHBITDEPTH bit set. If bit_depth > 8, we will set the
        // VPX_IMG_FMT_HIGHBITDEPTH bit before passing the image format to
        // vpx_img_alloc(). To reduce the search space, don't generate
        // VPX_IMG_FMT_I440 (not used) and VPX_IMG_FMT_NV12 (which is converted
        // to I420 in vp9_copy_and_extend_frame()).
        /*fmt=*/
        ElementOf({VPX_IMG_FMT_I420, VPX_IMG_FMT_I422, VPX_IMG_FMT_I444}),
        /*call_sequence=*/AnyCallSequence());

// speed: range -9..9
// end_usage: Rate control mode
void VP9EncodeSucceeds(unsigned int threads,
                       int speed,
                       vpx_rc_mode end_usage,
                       unsigned int width,
                       unsigned int height,
                       int num_frames) {
  vpx_codec_iface_t* const iface = vpx_codec_vp9_cx();
  vpx_codec_enc_cfg_t cfg;
  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, /*usage=*/0),
            VPX_CODEC_OK);
  cfg.g_threads = threads;
  cfg.g_w = width;
  cfg.g_h = height;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000 * 1000;  // microseconds
  cfg.g_pass = VPX_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = end_usage;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 58;

  vpx_codec_ctx_t enc;
  ASSERT_EQ(vpx_codec_enc_init(&enc, iface, &cfg, 0), VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_CPUUSED, speed), VPX_CODEC_OK);

  vpx_image_t* const image =
      CreateGrayImage(VPX_BITS_8, VPX_IMG_FMT_I420, cfg.g_w, cfg.g_h);
  ASSERT_NE(image, nullptr);

  // Encode frames.
  const vpx_enc_deadline_t deadline = VPX_DL_REALTIME;
  const vpx_codec_cx_pkt_t* pkt;
  for (int i = 0; i < num_frames; ++i) {
    ASSERT_EQ(vpx_codec_encode(&enc, image, i, 1, 0, deadline), VPX_CODEC_OK);
    vpx_codec_iter_t iter = nullptr;
    while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, VPX_CODEC_CX_FRAME_PKT);
    }
  }

  // Flush the encoder.
  bool got_data;
  do {
    ASSERT_EQ(vpx_codec_encode(&enc, nullptr, 0, 1, 0, deadline), VPX_CODEC_OK);
    got_data = false;
    vpx_codec_iter_t iter = nullptr;
    while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
      ASSERT_EQ(pkt->kind, VPX_CODEC_CX_FRAME_PKT);
      got_data = true;
    }
  } while (got_data);

  vpx_img_free(image);
  ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
}

// Chrome's WebCodecs uses at most 16 threads.
FUZZ_TEST(VP9EncodeFuzzTest, VP9EncodeSucceeds)
    .WithDomains(/*threads=*/InRange(0u, 16u),
                 /*speed=*/InRange(-9, 9),
                 /*end_usage=*/ElementOf({VPX_VBR, VPX_CBR}),
                 /*width=*/InRange(1u, 1920u),
                 /*height=*/InRange(1u, 1080u),
                 /*num_frames=*/InRange(1, 10));
