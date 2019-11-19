// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/chromeos/jpeg_codec_robust_slow.h"

#include <setjmp.h>

#include <memory>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

extern "C" {
// IJG provides robust JPEG decode
#include "third_party/libjpeg/jpeglib.h"
}

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"

namespace gfx {

// Encoder/decoder shared stuff ------------------------------------------------

namespace {

// used to pass error info through the JPEG library
struct IjgCoderErrorMgr {
  jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

void IjgErrorExit(jpeg_common_struct* cinfo) {
  IjgCoderErrorMgr* err = reinterpret_cast<IjgCoderErrorMgr*>(cinfo->err);

  // Return control to the setjmp point.
  longjmp(err->setjmp_buffer, false);
}

}  // namespace

// Decoder --------------------------------------------------------------------

namespace {

// Callback to initialize the source.
//
// From the JPEG library:
//  "Initialize source. This is called by jpeg_read_header() before any data is
//   actually read. May leave bytes_in_buffer set to 0 (in which case a
//   fill_input_buffer() call will occur immediately)."
void IjgInitSource(j_decompress_ptr cinfo) {
  auto* compressed_data =
      static_cast<base::span<const uint8_t>*>(cinfo->client_data);
  cinfo->src->next_input_byte = compressed_data->data();
  cinfo->src->bytes_in_buffer = compressed_data->size();
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
boolean IjgFillInputBuffer(j_decompress_ptr cinfo) {
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
void IjgSkipInputData(j_decompress_ptr cinfo, long num_bytes) {
  if (num_bytes > static_cast<long>(cinfo->src->bytes_in_buffer)) {
    // Since all our data should be in the buffer, trying to skip beyond it
    // means that there is some kind of error or corrupt input data. A 0 for
    // bytes left means it will call IjgFillInputBuffer which will then fail.
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
void IjgTermSource(j_decompress_ptr cinfo) {}

#if !defined(JCS_EXTENSIONS)
// Converts one row of rgb data to rgba data by adding a fully-opaque alpha
// value.
void AddAlpha(const uint8_t* rgb, int pixel_width, uint8_t* rgba) {
  for (int x = 0; x < pixel_width; x++) {
    memcpy(&rgba[x * 4], &rgb[x * 3], 3);
    rgba[x * 4 + 3] = 0xff;
  }
}

// Converts one row of RGB data to BGRA by reordering the color components and
// adding alpha values of 0xff.
void RGBtoBGRA(const uint8_t* bgra, int pixel_width, uint8_t* rgb) {
  for (int x = 0; x < pixel_width; x++) {
    const uint8_t* pixel_in = &bgra[x * 3];
    uint8_t* pixel_out = &rgb[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = 0xff;
  }
}
#endif  // !defined(JCS_EXTENSIONS)

// jpeg_decompress_struct Deleter.
struct JpegRobustDecompressStructDeleter {
  void operator()(jpeg_decompress_struct* ptr) {
    jpeg_destroy_decompress(ptr);
    delete ptr;
  }
};

}  // namespace

bool JPEGCodecRobustSlow::Decode(base::span<const uint8_t> compressed_data,
                                 ColorFormat format,
                                 std::vector<uint8_t>* output,
                                 int* w,
                                 int* h,
                                 base::Optional<size_t> max_decoded_num_bytes) {
  std::unique_ptr<jpeg_decompress_struct, JpegRobustDecompressStructDeleter>
      cinfo(new jpeg_decompress_struct);
  output->clear();

  // We set up the normal JPEG error routines, then override error_exit.
  // This must be done before the call to create_decompress.
  IjgCoderErrorMgr errmgr;
  cinfo->err = jpeg_std_error(&errmgr.pub);
  errmgr.pub.error_exit = IjgErrorExit;
  // Establish the setjmp return context for IjgErrorExit to use.
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
  srcmgr.init_source = IjgInitSource;
  srcmgr.fill_input_buffer = IjgFillInputBuffer;
  srcmgr.skip_input_data = IjgSkipInputData;
  srcmgr.resync_to_restart = jpeg_resync_to_restart;  // use default routine
  srcmgr.term_source = IjgTermSource;
  cinfo->src = &srcmgr;

  cinfo->client_data = &compressed_data;

  // fill the file metadata into our buffer
  if (jpeg_read_header(cinfo.get(), true) != JPEG_HEADER_OK)
    return false;

  // we want to always get RGB data out
  switch (cinfo->jpeg_color_space) {
    case JCS_GRAYSCALE:
    case JCS_RGB:
    case JCS_YCbCr:
#ifdef JCS_EXTENSIONS
      // Choose an output colorspace and return if it is an unsupported one.
      // Same as JPEGCodec::Encode(), libjpeg-turbo supports all input formats
      // used by Chromium (i.e. RGB, RGBA, and BGRA) and we just map the input
      // parameters to a colorspace.
      if (format == FORMAT_RGB) {
        cinfo->out_color_space = JCS_RGB;
        cinfo->output_components = 3;
      } else if (format == FORMAT_RGBA ||
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
#else
      cinfo->out_color_space = JCS_RGB;
#endif
      break;
    case JCS_CMYK:
    case JCS_YCCK:
    default:
      // Mozilla errors out on these color spaces, so I presume that the jpeg
      // library can't do automatic color space conversion for them. We don't
      // care about these anyway.
      return false;
  }
#ifndef JCS_EXTENSIONS
  cinfo->output_components = 3;
#endif

  jpeg_calc_output_dimensions(cinfo.get());
  *w = cinfo->output_width;
  *h = cinfo->output_height;

  // FIXME(brettw) we may want to allow the capability for callers to request
  // how to align row lengths as we do for the compressor.
  const size_t decoded_row_stride =
      cinfo->output_width *
      base::saturated_cast<size_t>(cinfo->output_components);
  void (*converter)(const uint8_t* rgb, int w, uint8_t* out) = nullptr;
  size_t output_row_stride;

#if defined(JCS_EXTENSIONS)
  // Easy case: rows need no pixel format conversion.
  output_row_stride = decoded_row_stride;
#else
  if (format == FORMAT_RGB) {
    // Easy case: rows need no pixel format conversion.
    output_row_stride = decoded_row_stride;
  } else {
    // Rows will need a pixel format conversion to output format.
    if (format == FORMAT_RGBA ||
        (format == FORMAT_SkBitmap && SK_R32_SHIFT == 0)) {
      output_row_stride = cinfo->output_width * 4;
      converter = AddAlpha;
    } else if (format == FORMAT_BGRA ||
               (format == FORMAT_SkBitmap && SK_B32_SHIFT == 0)) {
      output_row_stride = cinfo->output_width * 4;
      converter = RGBtoBGRA;
    } else {
      NOTREACHED() << "Invalid pixel format";
      return false;
    }
  }
#endif

  const size_t output_num_bytes = output_row_stride * cinfo->output_height;
  if (max_decoded_num_bytes && output_num_bytes > *max_decoded_num_bytes)
    return false;

  jpeg_start_decompress(cinfo.get());

  output->resize(output_num_bytes);
  if (converter) {
    // The decoded pixels need to be converted to the output format. We reuse
    // this temporary buffer to decode each row and then write their converted
    // form to the real output buffer.
    auto row_data = std::make_unique<uint8_t[]>(decoded_row_stride);
    uint8_t* rowptr = row_data.get();
    for (size_t row = 0; row < cinfo->output_height; ++row) {
      if (!jpeg_read_scanlines(cinfo.get(), &rowptr, 1))
        return false;
      converter(rowptr, *w, &(*output)[row * output_row_stride]);
    }
  } else {
    for (size_t row = 0; row < cinfo->output_height; ++row) {
      uint8_t* rowptr = &(*output)[row * output_row_stride];
      if (!jpeg_read_scanlines(cinfo.get(), &rowptr, 1))
        return false;
    }
  }

  jpeg_finish_decompress(cinfo.get());
  return true;
}

// static
std::unique_ptr<SkBitmap> JPEGCodecRobustSlow::Decode(
    base::span<const uint8_t> compressed_data,
    base::Optional<size_t> max_decoded_num_bytes) {
  int w, h;
  std::vector<uint8_t> data_vector;
  if (!Decode(compressed_data, FORMAT_SkBitmap, &data_vector, &w, &h,
              max_decoded_num_bytes)) {
    return nullptr;
  }

  // Skia only handles 32 bit images.
  int data_length = w * h * 4;

  auto bitmap = std::make_unique<SkBitmap>();
  bitmap->allocN32Pixels(w, h);
  memcpy(bitmap->getAddr32(0, 0), &data_vector[0], data_length);

  return bitmap;
}

}  // namespace gfx
