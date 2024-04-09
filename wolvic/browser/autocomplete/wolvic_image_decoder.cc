// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/autocomplete/wolvic_image_decoder.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

// Referred to image_decoder_impl.cc & image_decoder.cc in //chrome
namespace wolvic {

namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

void RunDecodeCallbackOnTaskRunner(
    data_decoder::DecodeImageCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const SkBitmap& image) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), image));
}

// Note that this is always called on the thread which initiated the
// corresponding data_decoder::DecodeImage request.
void OnDecodeImageDone(
    base::OnceCallback<void()> fail_callback,
    base::OnceCallback<void(const SkBitmap&)> success_callback,
    const SkBitmap& image) {
  if (!image.isNull() && !image.empty())
    std::move(success_callback).Run(image);
  else
    std::move(fail_callback).Run();
}

}  // namespace

// A request for decoding an image.
class WolvicImageDecoder::DecodeImageRequest {
 public:
  DecodeImageRequest(WolvicImageDecoder* decoder,
                     const std::string& image_data,
                     data_decoder::DataDecoder* data_decoder,
                     const gfx::Size& desired_image_frame_size,
                     image_fetcher::ImageDecodedCallback callback)
      : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        data_decoder_(data_decoder),
        callback_(std::move(callback)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::span<const uint8_t> image_data_span(
        base::as_bytes(base::make_span(image_data)));

  auto decode_callback =
      base::BindOnce(&OnDecodeImageDone,
                     base::BindOnce(&DecodeImageRequest::OnDecodeImageFailed,
                                    base::Unretained(this)),
                     base::BindOnce(&DecodeImageRequest::OnDecodeImageSucceeded,
                                    base::Unretained(this)));

    data_decoder::DecodeImage(
        data_decoder, image_data_span,
        data_decoder::mojom::ImageCodec::kDefault, /*shrink_to_fit=*/false,
        kMaxImageSizeInBytes, desired_image_frame_size,
        base::BindOnce(&RunDecodeCallbackOnTaskRunner, std::move(decode_callback),
                      std::move(task_runner_)));
  }

  DecodeImageRequest(const DecodeImageRequest&) = delete;
  DecodeImageRequest& operator=(const DecodeImageRequest&) = delete;

  ~DecodeImageRequest() = default;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Runs the callback and remove the request from the internal request queue.
  void RunCallbackAndRemoveRequest(const gfx::Image& image) {
    std::move(callback_).Run(image);

    // This must be the last line in the method body.
    decoder_->RemoveDecodeImageRequest(this);
  }

  void OnDecodeImageSucceeded(const SkBitmap& decoded_image) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    gfx::Image image(gfx::Image::CreateFrom1xBitmap(decoded_image));
    RunCallbackAndRemoveRequest(image);
  }

  void OnDecodeImageFailed() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    RunCallbackAndRemoveRequest(gfx::Image());
  }

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ptr<WolvicImageDecoder> decoder_;
  raw_ptr<data_decoder::DataDecoder> data_decoder_;
  // The callback to call after the request completed.
  image_fetcher::ImageDecodedCallback callback_;
};

WolvicImageDecoder::WolvicImageDecoder() = default;

WolvicImageDecoder::~WolvicImageDecoder() = default;

void WolvicImageDecoder::DecodeImage(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    data_decoder::DataDecoder* data_decoder,
    image_fetcher::ImageDecodedCallback callback) {
  auto decode_image_request = std::make_unique<DecodeImageRequest>(
      this, image_data, data_decoder, desired_image_frame_size, std::move(callback));

  decode_image_requests_.push_back(std::move(decode_image_request));
}

void WolvicImageDecoder::RemoveDecodeImageRequest(DecodeImageRequest* request) {
  // Remove the finished request from the request queue.
  auto request_it =
      base::ranges::find(decode_image_requests_, request,
                         &std::unique_ptr<DecodeImageRequest>::get);
  DCHECK(request_it != decode_image_requests_.end());
  decode_image_requests_.erase(request_it);
}

}  // namespace wolvic
