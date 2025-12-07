// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_DO_NOT_REWRITE_THIRD_PARTY_API_H_
#define TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_DO_NOT_REWRITE_THIRD_PARTY_API_H_

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
};

struct hb_glyph_position_t {};
struct hb_buffer_t {};
hb_glyph_position_t* hb_buffer_get_glyph_positions(hb_buffer_t* buffer,
                                                   unsigned int* length);

#endif  // TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_DO_NOT_REWRITE_THIRD_PARTY_API_H_
