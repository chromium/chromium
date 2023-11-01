/*
 *  Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_image.h"

using fuzztest::ElementOf;
using fuzztest::InRange;

// speed: range -16..16
// end_usage: Rate control mode
void VP8EncodeSucceeds(unsigned int threads,
                       int speed,
                       vpx_rc_mode end_usage,
                       unsigned int width,
                       unsigned int height,
                       int num_frames) {
  vpx_codec_iface_t* iface = vpx_codec_vp8_cx();
  vpx_codec_enc_cfg_t cfg;
  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, /*usage=*/0),
            VPX_CODEC_OK);
  cfg.g_threads = threads;
  cfg.g_w = width;
  cfg.g_h = height;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000 * 1000;  // microseconds
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = end_usage;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 58;

  vpx_codec_ctx_t enc;
  ASSERT_EQ(vpx_codec_enc_init(&enc, iface, &cfg, 0), VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_CPUUSED, speed), VPX_CODEC_OK);

  // VP8 supports only one image format: 8-bit YUV 4:2:0.
  vpx_image_t* image =
      vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(image, nullptr);

  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  unsigned int uv_h = (image->d_h + 1) / 2;
  unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  // Encode frames.
  const unsigned long deadline = VPX_DL_REALTIME;  // NOLINT(runtime/int,
                                                   // google-runtime-int)
  const vpx_codec_cx_pkt_t* pkt;
  for (int i = 0; i < num_frames; ++i) {
    ASSERT_EQ(vpx_codec_encode(&enc, image, i, 1, 0, deadline), VPX_CODEC_OK);
    vpx_codec_iter_t iter = nullptr;
    while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
    }
  }

  // Flush the encoder.
  bool got_data;
  do {
    ASSERT_EQ(vpx_codec_encode(&enc, nullptr, 0, 1, 0, deadline), VPX_CODEC_OK);
    got_data = false;
    vpx_codec_iter_t iter = nullptr;
    while ((pkt = vpx_codec_get_cx_data(&enc, &iter)) != nullptr) {
      got_data = true;
    }
  } while (got_data);

  vpx_img_free(image);
  ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
}

// Chrome's WebCodecs uses at most 16 threads.
FUZZ_TEST(VP8EncodeFuzzTest, VP8EncodeSucceeds)
    .WithDomains(/*threads=*/InRange(0, 16),
                 /*speed=*/InRange(-16, 16),
                 /*end_usage=*/ElementOf<vpx_rc_mode>({VPX_VBR, VPX_CBR}),
                 /*width=*/InRange(1, 1920),
                 /*height=*/InRange(1, 1080),
                 /*num_frames=*/InRange(1, 10));
