// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

#include "./fuzz_utils.h"
#include "src/webp/encode.h"
#include "src/webp/mux.h"

namespace {

const VP8CPUInfo default_VP8GetCPUInfo = VP8GetCPUInfo;

int AddFrame(WebPAnimEncoder** const enc,
             const WebPAnimEncoderOptions& anim_config, int* const width,
             int* const height, int timestamp_ms, const uint8_t data[],
             size_t size, uint32_t* const bit_pos) {
  if (enc == nullptr || width == nullptr || height == nullptr) {
    fprintf(stderr, "NULL parameters.\n");
    if (enc != nullptr) WebPAnimEncoderDelete(*enc);
    abort();
  }

  // Init the source picture.
  WebPPicture pic;
  if (!WebPPictureInit(&pic)) {
    fprintf(stderr, "WebPPictureInit failed.\n");
    WebPAnimEncoderDelete(*enc);
    abort();
  }
  pic.use_argb = Extract(1, data, size, bit_pos);

  // Read the source picture.
  if (!ExtractSourcePicture(&pic, data, size, bit_pos)) {
    const WebPEncodingError error_code = pic.error_code;
    WebPPictureFree(&pic);
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
    fprintf(stderr, "Can't read input image. Error code: %d\n", error_code);
    abort();
  }

  // Crop and scale.
  if (*enc == nullptr) {  // First frame will set canvas width and height.
    if (!ExtractAndCropOrScale(&pic, data, size, bit_pos)) {
      const WebPEncodingError error_code = pic.error_code;
      WebPPictureFree(&pic);
      if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
      fprintf(stderr, "ExtractAndCropOrScale failed. Error code: %d\n",
              error_code);
      abort();
    }
  } else {  // Other frames will be resized to the first frame's dimensions.
    if (!WebPPictureRescale(&pic, *width, *height)) {
      const WebPEncodingError error_code = pic.error_code;
      WebPAnimEncoderDelete(*enc);
      WebPPictureFree(&pic);
      if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
      fprintf(stderr,
              "WebPPictureRescale failed. Size: %d,%d. Error code: %d\n",
              *width, *height, error_code);
      abort();
    }
  }

  // Create encoder if it doesn't exist.
  if (*enc == nullptr) {
    *width = pic.width;
    *height = pic.height;
    *enc = WebPAnimEncoderNew(*width, *height, &anim_config);
    if (*enc == nullptr) {
      WebPPictureFree(&pic);
      return 0;
    }
  }

  // Create frame encoding config.
  WebPConfig config;
  if (!ExtractWebPConfig(&config, data, size, bit_pos)) {
    fprintf(stderr, "ExtractWebPConfig failed.\n");
    WebPAnimEncoderDelete(*enc);
    WebPPictureFree(&pic);
    abort();
  }
  // Skip slow settings on big images, it's likely to timeout.
  if (pic.width * pic.height > 32 * 32) {
    config.method = (config.method > 4) ? 4 : config.method;
    config.quality = (config.quality > 99.0f) ? 99.0f : config.quality;
    config.alpha_quality =
        (config.alpha_quality > 99) ? 99 : config.alpha_quality;
  }

  // Encode.
  if (!WebPAnimEncoderAdd(*enc, &pic, timestamp_ms, &config)) {
    const WebPEncodingError error_code = pic.error_code;
    WebPAnimEncoderDelete(*enc);
    WebPPictureFree(&pic);
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
    fprintf(stderr, "WebPEncode failed. Error code: %d\n", error_code);
    abort();
  }

  WebPPictureFree(&pic);
  return 1;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* const data, size_t size) {
  WebPAnimEncoder* enc = nullptr;
  int width = 0, height = 0, timestamp_ms = 0;
  uint32_t bit_pos = 0;

  ExtractAndDisableOptimizations(default_VP8GetCPUInfo, data, size, &bit_pos);

  // Extract a configuration from the packed bits.
  WebPAnimEncoderOptions anim_config;
  if (!WebPAnimEncoderOptionsInit(&anim_config)) {
    fprintf(stderr, "WebPAnimEncoderOptionsInit failed.\n");
    abort();
  }
  anim_config.minimize_size = Extract(1, data, size, &bit_pos);
  anim_config.kmax = Extract(15, data, size, &bit_pos);
  const int min_kmin = (anim_config.kmax > 1) ? (anim_config.kmax / 2) : 0;
  const int max_kmin = (anim_config.kmax > 1) ? (anim_config.kmax - 1) : 0;
  anim_config.kmin =
      min_kmin + Extract((uint32_t)(max_kmin - min_kmin), data, size, &bit_pos);
  anim_config.allow_mixed = Extract(1, data, size, &bit_pos);
  anim_config.verbose = 0;

  const int nb_frames = 1 + Extract(15, data, size, &bit_pos);

  // For each frame.
  for (int i = 0; i < nb_frames; ++i) {
    if (!AddFrame(&enc, anim_config, &width, &height, timestamp_ms, data, size,
                  &bit_pos)) {
      return 0;
    }

    timestamp_ms += (1 << (2 + Extract(15, data, size, &bit_pos))) +
                    Extract(1, data, size, &bit_pos);  // [1..131073], arbitrary
  }

  // Assemble.
  if (!WebPAnimEncoderAdd(enc, nullptr, timestamp_ms, nullptr)) {
    fprintf(stderr, "Last WebPAnimEncoderAdd failed: %s.\n",
            WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    abort();
  }
  WebPData webp_data;
  WebPDataInit(&webp_data);
  if (!WebPAnimEncoderAssemble(enc, &webp_data)) {
    fprintf(stderr, "WebPAnimEncoderAssemble failed: %s.\n",
            WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    WebPDataClear(&webp_data);
    abort();
  }

  WebPAnimEncoderDelete(enc);
  WebPDataClear(&webp_data);
  return 0;
}
