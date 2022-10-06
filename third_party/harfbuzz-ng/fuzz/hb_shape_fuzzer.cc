// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// clang-format off
#include <hb.h>
#include <hb-cplusplus.hh>
#include <hb-ot.h>
// clang-format on

#include <iterator>

constexpr size_t kMaxInputLength = 16800;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > kMaxInputLength)
    return 0;

  const char* data_ptr = reinterpret_cast<const char*>(data);
  hb::unique_ptr<hb_blob_t> blob(hb_blob_create(
      data_ptr, size, HB_MEMORY_MODE_READONLY, nullptr, nullptr));
  hb::unique_ptr<hb_face_t> face(hb_face_create(blob.get(), 0));
  hb::unique_ptr<hb_font_t> font(hb_font_create(face.get()));
  hb_ot_font_set_funcs(font.get());
  hb_font_set_scale(font.get(), 12, 12);

  {
    const char text[] = "ABCDEXYZ123@_%&)*$!";
    hb::unique_ptr<hb_buffer_t> buffer(hb_buffer_create());
    hb_buffer_add_utf8(buffer.get(), text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer.get());
    hb_shape(font.get(), buffer.get(), nullptr, 0);
  }

  uint32_t text32[16] = {0};
  if (size > sizeof(text32)) {
    memcpy(text32, data + size - sizeof(text32), sizeof(text32));
    hb::unique_ptr<hb_buffer_t> buffer(hb_buffer_create());
    hb_buffer_add_utf32(buffer.get(), text32, std::size(text32), 0, -1);
    hb_buffer_guess_segment_properties(buffer.get());
    hb_shape(font.get(), buffer.get(), nullptr, 0);
  }

  return 0;
}
