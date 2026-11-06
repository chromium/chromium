// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_CHROME_THIRD_PARTY_DO_NOT_REWRITE_THIRD_PARTY_API_H_
#define TOOLS_CLANG_SPANIFY_TESTS_CHROME_THIRD_PARTY_DO_NOT_REWRITE_THIRD_PARTY_API_H_

#include <cstddef>
#include <cstdint>

extern int* GetThirdPartyBuffer();

// Implemented in first party.
class ThirdPartyInterface {
 public:
  virtual void ToBeImplemented(int arg[3]) = 0;
};

class SkBitmap {
 public:
  uint32_t* NoArgForTesting() const;

  uint32_t* getAddr32(int x, int y) const;
  int width() const;
};

struct hb_glyph_info_t {};
struct hb_glyph_position_t {};
struct hb_buffer_t {};
hb_glyph_info_t* hb_buffer_get_glyph_infos(hb_buffer_t* buffer,
                                           unsigned int* length);
hb_glyph_position_t* hb_buffer_get_glyph_positions(hb_buffer_t* buffer,
                                                   unsigned int* length);
unsigned int hb_buffer_get_length(const hb_buffer_t* buffer);

struct CRYPTO_BUFFER;
const uint8_t* CRYPTO_BUFFER_data(const CRYPTO_BUFFER* buf);
size_t CRYPTO_BUFFER_len(const CRYPTO_BUFFER* buf);

#endif  // TOOLS_CLANG_SPANIFY_TESTS_CHROME_THIRD_PARTY_DO_NOT_REWRITE_THIRD_PARTY_API_H_
