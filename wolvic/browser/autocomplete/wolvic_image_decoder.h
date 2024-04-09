// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_IMAGE_DECODER_H_
#define WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_IMAGE_DECODER_H_

#include <string>

#include "base/functional/bind.h"
#include "components/image_fetcher/core/image_decoder.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace wolvic {

// image_fetcher::ImageDecoder implementation.
class WolvicImageDecoder : public image_fetcher::ImageDecoder {
 public:
  WolvicImageDecoder();

  WolvicImageDecoder(const WolvicImageDecoder&) = delete;
  WolvicImageDecoder& operator=(const WolvicImageDecoder&) = delete;

  ~WolvicImageDecoder() override;

  void DecodeImage(const std::string& image_data,
                   const gfx::Size& desired_image_frame_size,
                   data_decoder::DataDecoder* data_decoder,
                   image_fetcher::ImageDecodedCallback callback) override;

 private:
  class DecodeImageRequest;

  // Removes the passed image decode |request| from the internal request queue.
  void RemoveDecodeImageRequest(DecodeImageRequest* request);

  // All active image decoding requests.
  std::vector<std::unique_ptr<DecodeImageRequest>> decode_image_requests_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_AUTOCOMPLETE_WOLVIC_IMAGE_DECODER_H_
