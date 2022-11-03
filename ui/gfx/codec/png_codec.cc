// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/png_codec.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "third_party/libpng/png.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "third_party/zlib/zlib.h"
#include "ui/gfx/codec/vector_wstream.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

// Decoder --------------------------------------------------------------------
//
// This code is based on WebKit libpng interface (PNGImageDecoder), which is
// in turn based on the Mozilla png decoder.

namespace {

// Gamma constants: We assume we're on Windows which uses a gamma of 2.2.
const double kMaxGamma = 21474.83;  // Maximum gamma accepted by png library.
const double kDefaultGamma = 2.2;
const double kInverseGamma = 1.0 / kDefaultGamma;

class PngDecoderState {
 public:
  // Output is a vector<unsigned char>.
  PngDecoderState(PNGCodec::ColorFormat ofmt, std::vector<unsigned char>* o)
      : output_format(ofmt),
        output_channels(0),
        bitmap(nullptr),
        is_opaque(true),
        output(o),
        width(0),
        height(0),
        done(false) {}

  // Output is an SkBitmap.
  explicit PngDecoderState(SkBitmap* skbitmap)
      : output_format(PNGCodec::FORMAT_SkBitmap),
        output_channels(0),
        bitmap(skbitmap),
        is_opaque(true),
        output(nullptr),
        width(0),
        height(0),
        done(false) {}

  PngDecoderState(const PngDecoderState&) = delete;
  PngDecoderState& operator=(const PngDecoderState&) = delete;

  PNGCodec::ColorFormat output_format;
  int output_channels;

  // An incoming SkBitmap to write to. If NULL, we write to output instead.
  raw_ptr<SkBitmap> bitmap;

  // Used during the reading of an SkBitmap. Defaults to true until we see a
  // pixel with anything other than an alpha of 255.
  bool is_opaque;

  // The other way to decode output, where we write into an intermediary buffer
  // instead of directly to an SkBitmap.
  raw_ptr<std::vector<unsigned char>> output;

  // Size of the image, set in the info callback.
  int width;
  int height;

  // Set to true when we've found the end of the data.
  bool done;
};

// User transform (passed to libpng) which converts a row decoded by libpng to
// Skia format. Expects the row to have 4 channels, otherwise there won't be
// enough room in |data|.
void ConvertRGBARowToSkia(png_structp png_ptr,
                          png_row_infop row_info,
                          png_bytep data) {
  const int channels = row_info->channels;
  DCHECK_EQ(channels, 4);

  PngDecoderState* state =
      static_cast<PngDecoderState*>(png_get_user_transform_ptr(png_ptr));
  DCHECK(state) << "LibPNG user transform pointer is NULL";

  unsigned char* const end = data + row_info->rowbytes;
  for (unsigned char* p = data; p < end; p += channels) {
    uint32_t* sk_pixel = reinterpret_cast<uint32_t*>(p);
    const unsigned char alpha = p[channels - 1];
    if (alpha != 255) {
      state->is_opaque = false;
      *sk_pixel = SkPreMultiplyARGB(alpha, p[0], p[1], p[2]);
    } else {
      *sk_pixel = SkPackARGB32(alpha, p[0], p[1], p[2]);
    }
  }
}

// Called when the png header has been read. This code is based on the WebKit
// PNGImageDecoder
void DecodeInfoCallback(png_struct* png_ptr, png_info* info_ptr) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  int bit_depth, color_type, interlace_type, compression_type;
  int filter_type;
  png_uint_32 w, h;
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);

  // Bounds check. When the image is unreasonably big, we'll error out and
  // end up back at the setjmp call when we set up decoding.  "Unreasonably big"
  // means "big enough that w * h * 32bpp might overflow an int"; we choose this
  // threshold to match WebKit and because a number of places in code assume
  // that an image's size (in bytes) fits in a (signed) int.
  unsigned long long total_size =
      static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h);
  if (total_size > ((1 << 29) - 1))
    longjmp(png_jmpbuf(png_ptr), 1);
  state->width = static_cast<int>(w);
  state->height = static_cast<int>(h);

  // The following png_set_* calls have to be done in the order dictated by
  // the libpng docs. Please take care if you have to move any of them. This
  // is also why certain things are done outside of the switch, even though
  // they look like they belong there.

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand(png_ptr);

  // The '!= 0' is for silencing a Windows compiler warning.
  bool input_has_alpha = ((color_type & PNG_COLOR_MASK_ALPHA) != 0);

  // Transparency for paletted images.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_expand(png_ptr);
    input_has_alpha = true;
  }

  // Convert 16-bit to 8-bit.
  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  // Pick our row format converter necessary for this data.
  if (!input_has_alpha) {
    switch (state->output_format) {
      case PNGCodec::FORMAT_RGBA:
        state->output_channels = 4;
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        break;
      case PNGCodec::FORMAT_BGRA:
        state->output_channels = 4;
        png_set_bgr(png_ptr);
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        break;
      case PNGCodec::FORMAT_SkBitmap:
        state->output_channels = 4;
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        break;
    }
  } else {
    switch (state->output_format) {
      case PNGCodec::FORMAT_RGBA:
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_BGRA:
        state->output_channels = 4;
        png_set_bgr(png_ptr);
        break;
      case PNGCodec::FORMAT_SkBitmap:
        state->output_channels = 4;
        break;
    }
  }

  // Expand grayscale to RGB.
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  // Deal with gamma and keep it under our control.
  double gamma;
  if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
    if (gamma <= 0.0 || gamma > kMaxGamma) {
      gamma = kInverseGamma;
      png_set_gAMA(png_ptr, info_ptr, gamma);
    }
    png_set_gamma(png_ptr, kDefaultGamma, gamma);
  } else {
    png_set_gamma(png_ptr, kDefaultGamma, kInverseGamma);
  }

  // Setting the user transforms here (as opposed to inside the switch above)
  // because all png_set_* calls need to be done in the specific order
  // mandated by libpng.
  if (state->output_format == PNGCodec::FORMAT_SkBitmap) {
    png_set_read_user_transform_fn(png_ptr, ConvertRGBARowToSkia);
    png_set_user_transform_info(png_ptr, state, 0, 0);
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  if (state->bitmap) {
    if (!state->bitmap->tryAllocN32Pixels(state->width, state->height)) {
      png_error(png_ptr, "Could not allocate bitmap.");
      NOTREACHED();
      return;
    }
  } else if (state->output) {
    state->output->resize(
        state->width * state->output_channels * state->height);
  }
}

void DecodeRowCallback(png_struct* png_ptr, png_byte* new_row,
                       png_uint_32 row_num, int pass) {
  if (!new_row)
    return;  // Interlaced image; row didn't change this pass.

  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  if (static_cast<int>(row_num) > state->height) {
    NOTREACHED() << "Invalid row";
    return;
  }

  unsigned char* base = NULL;
  if (state->bitmap)
    base = reinterpret_cast<unsigned char*>(state->bitmap->getAddr32(0, 0));
  else if (state->output)
    base = &state->output->front();

  unsigned char* dest = &base[state->width * state->output_channels * row_num];
  png_progressive_combine_row(png_ptr, dest, new_row);
}

void DecodeEndCallback(png_struct* png_ptr, png_info* info) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  // Mark the image as complete, this will tell the Decode function that we
  // have successfully found the end of the data.
  state->done = true;
}

// Holds png struct and info ensuring the proper destruction.
class PngReadStructInfo {
 public:
  PngReadStructInfo(): png_ptr_(nullptr), info_ptr_(nullptr) {
  }

  PngReadStructInfo(const PngReadStructInfo&) = delete;
  PngReadStructInfo& operator=(const PngReadStructInfo&) = delete;

  ~PngReadStructInfo() {
    png_destroy_read_struct(&png_ptr_, &info_ptr_, NULL);
  }

  bool Build(const unsigned char* input, size_t input_size) {
    if (input_size < 8)
      return false;  // Input data too small to be a png

    // Have libpng check the signature, it likes the first 8 bytes.
    if (png_sig_cmp(const_cast<unsigned char*>(input), 0, 8) != 0)
      return false;

    png_ptr_ = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr_)
      return false;

    info_ptr_ = png_create_info_struct(png_ptr_);
    if (!info_ptr_) {
      return false;
    }
    return true;
  }

  png_struct* png_ptr_;
  png_info* info_ptr_;
};

// Holds png struct and info ensuring the proper destruction.
class PngWriteStructInfo {
 public:
  PngWriteStructInfo() : png_ptr_(nullptr), info_ptr_(nullptr) {
  }

  PngWriteStructInfo(const PngWriteStructInfo&) = delete;
  PngWriteStructInfo& operator=(const PngWriteStructInfo&) = delete;

  ~PngWriteStructInfo() {
    png_destroy_write_struct(&png_ptr_, &info_ptr_);
  }

  png_struct* png_ptr_;
  png_info* info_ptr_;
};

// Libpng user error and warning functions which allows us to print libpng
// errors and warnings using Chrome's logging facilities instead of stderr.

void LogLibPNGDecodeError(png_structp png_ptr, png_const_charp error_msg) {
  DLOG(ERROR) << "libpng decode error: " << error_msg;
  longjmp(png_jmpbuf(png_ptr), 1);
}

void LogLibPNGDecodeWarning(png_structp png_ptr, png_const_charp warning_msg) {
  DLOG(ERROR) << "libpng decode warning: " << warning_msg;
}

}  // namespace

// static
bool PNGCodec::Decode(const unsigned char* input, size_t input_size,
                      ColorFormat format, std::vector<unsigned char>* output,
                      int* w, int* h) {
  PngReadStructInfo si;
  if (!si.Build(input, input_size))
    return false;

  if (setjmp(png_jmpbuf(si.png_ptr_))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }

  PngDecoderState state(format, output);

  png_set_error_fn(si.png_ptr_, NULL,
                   LogLibPNGDecodeError, LogLibPNGDecodeWarning);
  png_set_progressive_read_fn(si.png_ptr_, &state, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_process_data(si.png_ptr_,
                   si.info_ptr_,
                   const_cast<unsigned char*>(input),
                   input_size);

  if (!state.done) {
    // Fed it all the data but the library didn't think we got all the data, so
    // this file must be truncated.
    output->clear();
    return false;
  }

  *w = state.width;
  *h = state.height;
  return true;
}

// static
bool PNGCodec::Decode(const unsigned char* input, size_t input_size,
                      SkBitmap* bitmap) {
  DCHECK(bitmap);
  PngReadStructInfo si;
  if (!si.Build(input, input_size))
    return false;

  if (setjmp(png_jmpbuf(si.png_ptr_))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }

  PngDecoderState state(bitmap);

  png_set_progressive_read_fn(si.png_ptr_, &state, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_process_data(si.png_ptr_,
                   si.info_ptr_,
                   const_cast<unsigned char*>(input),
                   input_size);

  if (!state.done) {
    return false;
  }

  // Set the bitmap's opaqueness based on what we saw.
  bitmap->setAlphaType(state.is_opaque ?
                       kOpaque_SkAlphaType : kPremul_SkAlphaType);

  return true;
}

// Encoder --------------------------------------------------------------------

namespace {

void AddComments(SkPngEncoder::Options& options,
                 const std::vector<PNGCodec::Comment>& comments) {
  std::vector<const char*> comment_pointers;
  std::vector<size_t> comment_sizes;
  for (const auto& comment : comments) {
    comment_pointers.push_back(comment.key.c_str());
    comment_pointers.push_back(comment.text.c_str());
    comment_sizes.push_back(comment.key.length() + 1);
    comment_sizes.push_back(comment.text.length() + 1);
  }
  options.fComments = SkDataTable::MakeCopyArrays(
      (void const* const*)comment_pointers.data(), comment_sizes.data(),
      static_cast<int>(comment_pointers.size()));
}

bool EncodeSkPixmap(const SkPixmap& src,
                    const std::vector<PNGCodec::Comment>& comments,
                    std::vector<unsigned char>* output,
                    int zlib_level,
                    bool disable_filters) {
  output->clear();
  VectorWStream dst(output);

  SkPngEncoder::Options options;
  AddComments(options, comments);
  options.fZLibLevel = zlib_level;
  if (disable_filters)
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;
  return SkPngEncoder::Encode(&dst, src, options);
}

bool EncodeSkPixmap(const SkPixmap& src,
                    bool discard_transparency,
                    const std::vector<PNGCodec::Comment>& comments,
                    std::vector<unsigned char>* output,
                    int zlib_level,
                    bool disable_filters) {
  if (discard_transparency) {
    SkImageInfo opaque_info = src.info().makeAlphaType(kOpaque_SkAlphaType);
    SkBitmap copy;
    if (!copy.tryAllocPixels(opaque_info)) {
      return false;
    }
    SkPixmap opaque_pixmap;
    bool success = copy.peekPixels(&opaque_pixmap);
    DCHECK(success);
    // The following step does the unpremul as we set the dst alpha type to be
    // kUnpremul_SkAlphaType. Later, because opaque_pixmap has
    // kOpaque_SkAlphaType, we'll discard the transparency as required.
    success =
        src.readPixels(opaque_info.makeAlphaType(kUnpremul_SkAlphaType),
                       opaque_pixmap.writable_addr(), opaque_pixmap.rowBytes());
    DCHECK(success);
    return EncodeSkPixmap(opaque_pixmap, comments, output, zlib_level,
                          disable_filters);
  }
  return EncodeSkPixmap(src, comments, output, zlib_level, disable_filters);
}

bool EncodeSkBitmap(const SkBitmap& input,
                    bool discard_transparency,
                    std::vector<unsigned char>* output,
                    int zlib_level,
                    bool disable_filters) {
  SkPixmap src;
  if (!input.peekPixels(&src)) {
    return false;
  }
  return EncodeSkPixmap(src, discard_transparency,
                        std::vector<PNGCodec::Comment>(), output, zlib_level,
                        disable_filters);
}

}  // namespace

// static
bool PNGCodec::Encode(const unsigned char* input,
                      ColorFormat format,
                      const Size& size,
                      int row_byte_width,
                      bool discard_transparency,
                      const std::vector<Comment>& comments,
                      std::vector<unsigned char>* output) {
  // Initialization required for Windows although the switch covers all cases.
  SkColorType colorType = kN32_SkColorType;
  switch (format) {
    case FORMAT_RGBA:
      colorType = kRGBA_8888_SkColorType;
      break;
    case FORMAT_BGRA:
      colorType = kBGRA_8888_SkColorType;
      break;
    case FORMAT_SkBitmap:
      colorType = kN32_SkColorType;
      break;
  }
  auto alphaType =
      format == FORMAT_SkBitmap ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
  SkImageInfo info =
      SkImageInfo::Make(size.width(), size.height(), colorType, alphaType);
  SkPixmap src(info, input, row_byte_width);
  return EncodeSkPixmap(src, discard_transparency, comments, output,
                        DEFAULT_ZLIB_COMPRESSION, /* disable_filters= */ false);
}

// static
bool PNGCodec::EncodeBGRASkBitmap(const SkBitmap& input,
                                  bool discard_transparency,
                                  std::vector<unsigned char>* output) {
  return EncodeSkBitmap(input, discard_transparency, output,
                        DEFAULT_ZLIB_COMPRESSION, /* disable_filters= */ false);
}

// static
bool PNGCodec::EncodeA8SkBitmap(const SkBitmap& input,
                                std::vector<unsigned char>* output) {
  DCHECK_EQ(input.colorType(), kAlpha_8_SkColorType);
  auto info = input.info()
                  .makeColorType(kGray_8_SkColorType)
                  .makeAlphaType(kOpaque_SkAlphaType);
  SkPixmap src(info, input.getAddr(0, 0), input.rowBytes());
  return EncodeSkPixmap(src, std::vector<PNGCodec::Comment>(), output,
                        DEFAULT_ZLIB_COMPRESSION, /* disable_filters= */ false);
}

// static
bool PNGCodec::FastEncodeBGRASkBitmap(const SkBitmap& input,
                                      bool discard_transparency,
                                      std::vector<unsigned char>* output) {
  return EncodeSkBitmap(input, discard_transparency, output, Z_BEST_SPEED,
                        /* disable_filters= */ true);
}

PNGCodec::Comment::Comment(const std::string& k, const std::string& t)
    : key(k), text(t) {
}

PNGCodec::Comment::~Comment() {
}

}  // namespace gfx
