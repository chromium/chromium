// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "node_modules/piex/src/piex.h"

#include <assert.h>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <stdint.h>
#include <string.h>

class PiexStreamReader : public piex::StreamInterface {
 public:
  PiexStreamReader(const char* data, size_t size) : data_(data), size_(size) {}

  piex::Error GetData(const size_t offset, const size_t length, uint8_t* out) {
    if (!EnsureData(offset, length))
      return piex::kFail;
    memcpy(out, data_ + offset, length);
    return piex::kOk;
  }

 private:
  bool EnsureData(const size_t offset, const size_t length) const {
    if (offset > size_ || (size_ - offset) < length)
      return false;
    return true;
  }

 private:
  const char* data_;
  const size_t size_;
};

class PiexReader {
 public:
  static emscripten::val ReadImage(const char* data, size_t size) {
    assert(data);

    auto result = emscripten::val::object();
    PiexStreamReader reader(data, size);
    piex::PreviewImageData image;

    switch (piex::GetPreviewImageData(&reader, &image)) {
      case piex::kFail:
        result.set("error", emscripten::val("failed to extract preview"));
        break;
      case piex::kUnsupported:
        result.set("error", emscripten::val("unsupported preview type"));
        break;
      case piex::kOk:
        result = GetProperties(image);
        break;
      default:
        result.set("error", emscripten::val("unknown error"));
        break;
    }

    return result;
  }

 private:
  static emscripten::val GetProperties(const piex::PreviewImageData& image) {
    auto result = emscripten::val::object();

    result.set("details", GetDetails(image));
    result.set("preview", GetPreview(image));
    result.set("thumbnail", GetThumbnail(image));

    return result;
  }

  static emscripten::val GetDetails(const piex::PreviewImageData& image) {
    auto object = emscripten::val::object();

    object.set("cameraMaker", emscripten::val(image.maker));
    object.set("cameraModel", emscripten::val(image.model));

    object.set("aperture", GetRational(image.fnumber));
    object.set("focalLength", GetRational(image.focal_length));
    object.set("exposureTime", GetRational(image.exposure_time));
    object.set("isoSpeed", GetNonZeroValue(image.iso));

    object.set("width", emscripten::val(image.full_width));
    object.set("height", emscripten::val(image.full_height));
    object.set("orientation", emscripten::val(image.exif_orientation));
    object.set("colorSpace", emscripten::val("sRGB"));
    const auto space = static_cast<uint32_t>(image.color_space);
    if (space == piex::PreviewImageData::kAdobeRgb)
      object.set("colorSpace", emscripten::val("AdobeRGB1998"));
    object.set("date", emscripten::val(image.date_time));

    return object;
  }

  static emscripten::val GetPreview(const piex::PreviewImageData& image) {
    const auto undefined = emscripten::val::undefined();

    const auto format = static_cast<uint32_t>(image.preview.format);
    if (format != piex::Image::Format::kJpegCompressed)
      return undefined;
    if (!image.preview.offset || !image.preview.length)
      return undefined;

    auto object = emscripten::val::object();
    object.set("colorSpace", GetColorSpace(image));
    object.set("orientation", emscripten::val(image.exif_orientation));
    object.set("format", emscripten::val(format));
    object.set("offset", emscripten::val(image.preview.offset));
    object.set("length", emscripten::val(image.preview.length));
    object.set("width", emscripten::val(image.preview.width));
    object.set("height", emscripten::val(image.preview.height));

    return object;
  }

  static emscripten::val GetThumbnail(const piex::PreviewImageData& image) {
    const auto undefined = emscripten::val::undefined();

    const auto format = static_cast<uint32_t>(image.thumbnail.format);
    if (format > piex::Image::Format::kUncompressedRgb)
      return undefined;
    if (!image.thumbnail.offset || !image.thumbnail.length)
      return undefined;

    auto object = emscripten::val::object();
    object.set("colorSpace", GetColorSpace(image));
    object.set("orientation", emscripten::val(image.exif_orientation));
    object.set("format", emscripten::val(format));
    object.set("offset", emscripten::val(image.thumbnail.offset));
    object.set("length", emscripten::val(image.thumbnail.length));
    object.set("width", emscripten::val(image.thumbnail.width));
    object.set("height", emscripten::val(image.thumbnail.height));

    return object;
  }

  static emscripten::val GetColorSpace(const piex::PreviewImageData& image) {
    const auto space = static_cast<uint32_t>(image.color_space);
    if (space == piex::PreviewImageData::kAdobeRgb)
      return emscripten::val("adobeRgb");
    return emscripten::val("sRgb");
  }

  static emscripten::val GetNonZeroValue(const uint32_t value) {
    return value > 0 ? emscripten::val(value) : emscripten::val::undefined();
  }

  static float GetRational(const piex::PreviewImageData::Rational& number) {
    if (number.denominator != 0)
      return float(number.numerator) / number.denominator;
    return 0.0f;
  }
};

emscripten::val image(int data, size_t size) {
  return PiexReader::ReadImage(reinterpret_cast<char*>(data), size);
}

EMSCRIPTEN_BINDINGS(PiexWasmModule) {
  emscripten::function("image", &image);
}
