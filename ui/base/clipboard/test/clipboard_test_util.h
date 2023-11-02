// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_
#define UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_

#include <cstdint>
#include <vector>

class SkBitmap;

namespace ui {

class Clipboard;

namespace clipboard_test_util {

// Helper functions to read images from clipboard synchronously.
std::vector<uint8_t> ReadPng(Clipboard* clipboard);
SkBitmap ReadImage(Clipboard* clipboard);

}  // namespace clipboard_test_util

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_
