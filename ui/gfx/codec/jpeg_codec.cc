// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/jpeg_codec.h"

#include <setjmp.h>

#include <memory>

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/codec/vector_wstream.h"

extern "C" {
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#elif defined(USE_LIBJPEG_TURBO)
#include "third_party/libjpeg_turbo/jpeglib.h"
#else
#include "third_party/libjpeg/jpeglib.h"
#endif
}

namespace gfx {

// Encoder/decoder shared stuff ------------------------------------------------

namespace {

// used to pass error info through the JPEG library
struct CoderErrorMgr {
  jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

void ErrorExit(jpeg_common_struct* cinfo) {
  CoderErrorMgr *err = reinterpret_cast<CoderErrorMgr*>(cinfo->err);

  // Return control to the setjmp point.
  longjmp(err->setjmp_buffer, false);
}

}  // namespace

// Encoder ---------------------------------------------------------------------

bool JPEGCodec::Encode(const SkPixmap& input,
                       int quality,
                       SkJpegEncoder::Downsample downsample,
                       std::vector<unsigned char>* output) {
  output->clear();
  VectorWStream dst(output);

  SkJpegEncoder::Options options;
  options.fQuality = quality;
  options.fDownsample = downsample;
  return SkJpegEncoder::Encode(&dst, input, options);
}

bool JPEGCodec::Encode(const SkPixmap& input,
                       int quality,
                       std::vector<unsigned char>* output) {
  return Encode(input, quality, SkJpegEncoder::Downsample::k420, output);
}

bool JPEGCodec::Encode(const SkBitmap& src,
                       int quality,
                       std::vector<unsigned char>* output) {
  SkPixmap pixmap;
  if (!src.peekPixels(&pixmap)) {
    return false;
  }

  return JPEGCodec::Encode(pixmap, quality, output);
}

// Decoder --------------------------------------------------------------------

namespace {

struct JpegDecoderState {
  JpegDecoderState(const unsigned char* in, size_t len)
      : input_buffer(in), input_buffer_length(len) {
  }

  const unsigned char* input_buffer;
  size_t input_buffer_length;
};

// Callback to initialize the source.
//
// From the JPEG library:
//  "Initialize source. This is called by jpeg_read_header() before any data is
//   actually read. May leave bytes_in_buffer set to 0 (in which case a
//   fill_input_buffer() call will occur immediately)."
void InitSource(j_decompress_ptr cinfo) {
  JpegDecoderState* state = static_cast<JpegDecoderState*>(cinfo->client_data);
  cinfo->src->next_input_byte = state->input_buffer;
  cinfo->src->bytes_in_buffer = state->input_buffer_length;
}

// Callback to fill the buffer. Since our buffer already contains all the data,
// we should never need to provide more data. If libjpeg thinks it needs more
// data, our input is probably corrupt.
//
// From the JPEG library:
//  "This is called whenever bytes_in_buffer has reached zero and more data is
//   wanted. In typical applications, it should read fresh data into the buffer
//   (ignoring the current state of next_input_byte and bytes_in_buffer), reset
//   the pointer & count to the start of the buffer, and return TRUE indicating
//   that the buffer has been reloaded. It is not necessary to fill the buffer
//   entirely, only to obtain at least one more byte. bytes_in_buffer MUST be
//   set to a positive value if TRUE is returned. A FALSE return should only
//   be used when I/O suspension is desired."
boolean FillInputBuffer(j_decompress_ptr cinfo) {
  return false;
}

// Skip data in the buffer. Since we have all the data at once, this operation
// is easy. It is not clear if this ever gets called because the JPEG library
// should be able to do the skip itself (it has all the data).
//
// From the JPEG library:
//  "Skip num_bytes worth of data. The buffer pointer and count should be
//   advanced over num_bytes input bytes, refilling the buffer as needed. This
//   is used to skip over a potentially large amount of uninteresting data
//   (such as an APPn marker). In some applications it may be possible to
//   optimize away the reading of the skipped data, but it's not clear that
//   being smart is worth much trouble; large skips are uncommon.
//   bytes_in_buffer may be zero on return. A zero or negative skip count
//   should be treated as a no-op."
void SkipInputData(j_decompress_ptr cinfo, long num_bytes) {
  if (num_bytes > static_cast<long>(cinfo->src->bytes_in_buffer)) {
    // Since all our data should be in the buffer, trying to skip beyond it
    // means that there is some kind of error or corrupt input data. A 0 for
    // bytes left means it will call FillInputBuffer which will then fail.
    cinfo->src->next_input_byte += cinfo->src->bytes_in_buffer;
    cinfo->src->bytes_in_buffer = 0;
  } else if (num_bytes > 0) {
    cinfo->src->bytes_in_buffer -= static_cast<size_t>(num_bytes);
    cinfo->src->next_input_byte += num_bytes;
  }
}

// Our source doesn't need any cleanup, so this is a NOP.
//
// From the JPEG library:
//  "Terminate source --- called by jpeg_finish_decompress() after all data has
//   been read to clean up JPEG source manager. NOT called by jpeg_abort() or
//   jpeg_destroy()."
void TermSource(j_decompress_ptr cinfo) {}

// jpeg_decompress_struct Deleter.
struct JpegDecompressStructDeleter {
  void operator()(jpeg_decompress_struct* ptr) {
    jpeg_destroy_decompress(ptr);
    delete ptr;
  }
};

}  // namespace

bool JPEGCodec::Decode(const unsigned char* input, size_t input_size,
                       ColorFormat format, std::vector<unsigned char>* output,
                       int* w, int* h) {
  std::unique_ptr<jpeg_decompress_struct, JpegDecompressStructDeleter> cinfo(
      new jpeg_decompress_struct);
  output->clear();

  // We set up the normal JPEG error routines, then override error_exit.
  // This must be done before the call to jpeg_create_decompress.
  CoderErrorMgr errmgr;
  cinfo->err = jpeg_std_error(&errmgr.pub);
  errmgr.pub.error_exit = ErrorExit;
  // Establish the setjmp return context for ErrorExit to use.
  if (setjmp(errmgr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error.
    // Release |cinfo| by hand to avoid use-after-free of |errmgr|.
    cinfo.reset();
    return false;
  }

  // The destroyer will destroy() cinfo on exit.  We don't want to set the
  // destroyer's object until cinfo is initialized.
  jpeg_create_decompress(cinfo.get());

  // set up the source manager
  jpeg_source_mgr srcmgr;
  srcmgr.init_source = InitSource;
  srcmgr.fill_input_buffer = FillInputBuffer;
  srcmgr.skip_input_data = SkipInputData;
  srcmgr.resync_to_restart = jpeg_resync_to_restart;  // use default routine
  srcmgr.term_source = TermSource;
  cinfo->src = &srcmgr;

  JpegDecoderState state(input, input_size);
  cinfo->client_data = &state;

  // fill the file metadata into our buffer
  if (jpeg_read_header(cinfo.get(), true) != JPEG_HEADER_OK)
    return false;

  // we want to always get RGB data out
  switch (cinfo->jpeg_color_space) {
    case JCS_GRAYSCALE:
    case JCS_RGB:
    case JCS_YCbCr:
      // Choose an output colorspace and return if it is an unsupported one.
      // Same as JPEGCodec::Encode(), libjpeg-turbo supports all input formats
      // used by Chromium (i.e. RGBA and BGRA) and we just map the input
      // parameters to a colorspace.
      if (format == FORMAT_RGBA ||
          (format == FORMAT_SkBitmap && SK_R32_SHIFT == 0)) {
        cinfo->out_color_space = JCS_EXT_RGBX;
        cinfo->output_components = 4;
      } else if (format == FORMAT_BGRA ||
                 (format == FORMAT_SkBitmap && SK_B32_SHIFT == 0)) {
        cinfo->out_color_space = JCS_EXT_BGRX;
        cinfo->output_components = 4;
      } else {
        NOTREACHED() << "Invalid pixel format";
        return false;
      }
      break;
    case JCS_CMYK:
    case JCS_YCCK:
    default:
      // Mozilla errors out on these color spaces, so I presume that the jpeg
      // library can't do automatic color space conversion for them. We don't
      // care about these anyway.
      return false;
  }

  jpeg_calc_output_dimensions(cinfo.get());
  *w = cinfo->output_width;
  *h = cinfo->output_height;

  jpeg_start_decompress(cinfo.get());

  // FIXME(brettw) we may want to allow the capability for callers to request
  // how to align row lengths as we do for the compressor.
  int row_read_stride = cinfo->output_width * cinfo->output_components;

  // Create memory for a decoded image and write decoded lines to the memory
  // without conversions same as JPEGCodec::Encode().
  int row_write_stride = row_read_stride;
  output->resize(row_write_stride * cinfo->output_height);

  for (int row = 0; row < static_cast<int>(cinfo->output_height); row++) {
    unsigned char* rowptr = &(*output)[row * row_write_stride];
    if (!jpeg_read_scanlines(cinfo.get(), &rowptr, 1))
      return false;
  }

  jpeg_finish_decompress(cinfo.get());
  return true;
}

// static
std::unique_ptr<SkBitmap> JPEGCodec::Decode(const unsigned char* input,
                                            size_t input_size) {
  int w, h;
  std::vector<unsigned char> data_vector;
  if (!Decode(input, input_size, FORMAT_SkBitmap, &data_vector, &w, &h))
    return nullptr;

  // Skia only handles 32 bit images.
  int data_length = w * h * 4;

  std::unique_ptr<SkBitmap> bitmap(new SkBitmap());
  bitmap->allocN32Pixels(w, h);
  memcpy(bitmap->getAddr32(0, 0), &data_vector[0], data_length);

  return bitmap;
}

}  // namespace gfx
