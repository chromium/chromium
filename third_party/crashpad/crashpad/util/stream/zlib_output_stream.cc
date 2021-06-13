// Copyright 2019 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stream/zlib_output_stream.h"

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "util/misc/zlib.h"

namespace crashpad {

ZlibOutputStream::ZlibOutputStream(
    Mode mode,
    std::unique_ptr<OutputStreamInterface> output_stream)
    : output_stream_(std::move(output_stream)),
      mode_(mode),
      initialized_(),
      flush_needed_(false) {}

ZlibOutputStream::~ZlibOutputStream() {
  if (!initialized_.is_valid())
    return;
  DCHECK(!flush_needed_);
  if (mode_ == Mode::kCompress) {
    if (deflateEnd(&zlib_stream_) != Z_OK)
      LOG(ERROR) << "deflateEnd: " << zlib_stream_.msg;
  } else if (mode_ == Mode::kDecompress) {
    if (inflateEnd(&zlib_stream_) != Z_OK)
      LOG(ERROR) << "inflateEnd: " << zlib_stream_.msg;
  }
}

bool ZlibOutputStream::Write(const uint8_t* data, size_t size) {
  if (initialized_.is_uninitialized()) {
    initialized_.set_invalid();

    zlib_stream_.zalloc = Z_NULL;
    zlib_stream_.zfree = Z_NULL;
    zlib_stream_.opaque = Z_NULL;

    if (mode_ == Mode::kDecompress) {
      int result = inflateInit(&zlib_stream_);
      if (result != Z_OK) {
        LOG(ERROR) << "inflateInit: " << ZlibErrorString(result);
        return false;
      }
    } else if (mode_ == Mode::kCompress) {
      int result = deflateInit(&zlib_stream_, Z_BEST_COMPRESSION);
      if (result != Z_OK) {
        LOG(ERROR) << "deflateInit: " << ZlibErrorString(result);
        return false;
      }
    }
    zlib_stream_.next_out = buffer_;
    zlib_stream_.avail_out = base::saturated_cast<uInt>(base::size(buffer_));
    initialized_.set_valid();
  }

  if (!initialized_.is_valid())
    return false;

  zlib_stream_.next_in = data;
  zlib_stream_.avail_in = base::saturated_cast<uInt>(size);
  flush_needed_ = false;
  while (zlib_stream_.avail_in > 0) {
    if (mode_ == Mode::kCompress) {
      if (deflate(&zlib_stream_, Z_NO_FLUSH) != Z_OK) {
        LOG(ERROR) << "deflate: " << zlib_stream_.msg;
        return false;
      }
    } else if (mode_ == Mode::kDecompress) {
      int result = inflate(&zlib_stream_, Z_NO_FLUSH);
      if (result == Z_STREAM_END) {
        if (zlib_stream_.avail_in > 0) {
          LOG(ERROR) << "inflate: unconsumed input";
          return false;
        }
      } else if (result != Z_OK) {
        LOG(ERROR) << "inflate: " << zlib_stream_.msg;
        return false;
      }
    }

    if (!WriteOutputStream())
      return false;
  }
  flush_needed_ = true;
  return true;
}

bool ZlibOutputStream::Flush() {
  if (initialized_.is_valid() && flush_needed_) {
    flush_needed_ = false;
    int result = Z_OK;
    do {
      if (mode_ == Mode::kCompress) {
        result = deflate(&zlib_stream_, Z_FINISH);
        if (result != Z_STREAM_END && result != Z_BUF_ERROR && result != Z_OK) {
          LOG(ERROR) << "deflate: " << zlib_stream_.msg;
          return false;
        }
      } else if (mode_ == Mode::kDecompress) {
        result = inflate(&zlib_stream_, Z_FINISH);
        if (result != Z_STREAM_END && result != Z_BUF_ERROR && result != Z_OK) {
          LOG(ERROR) << "inflate: " << zlib_stream_.msg;
          return false;
        }
      }
      if (!WriteOutputStream())
        return false;
    } while (result != Z_STREAM_END);
  }
  return output_stream_->Flush();
}

bool ZlibOutputStream::WriteOutputStream() {
  auto valid_size = base::size(buffer_) - zlib_stream_.avail_out;
  if (valid_size > 0 && !output_stream_->Write(buffer_, valid_size))
    return false;

  zlib_stream_.next_out = buffer_;
  zlib_stream_.avail_out = base::saturated_cast<uInt>(base::size(buffer_));

  return true;
}

}  // namespace crashpad
