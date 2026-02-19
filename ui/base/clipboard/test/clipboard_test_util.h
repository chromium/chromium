// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_
#define UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"

namespace ui {

class Clipboard;
class ClipboardFormatType;
class DataTransferEndpoint;

namespace clipboard_test_util {

// Helper functions to read from the clipboard synchronously for use in tests.
std::vector<std::u16string> ReadAvailableTypes(
    Clipboard* clipboard,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst);
std::u16string ReadText(Clipboard* clipboard,
                        ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst);
std::string ReadAsciiText(Clipboard* clipboard,
                          ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst);
void ReadHTML(Clipboard* clipboard,
              ClipboardBuffer buffer,
              const DataTransferEndpoint* data_dst,
              std::u16string* markup,
              std::string* src_url,
              uint32_t* fragment_start,
              uint32_t* fragment_end);
std::u16string ReadSvg(Clipboard* clipboard,
                       ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst);
std::string ReadRTF(Clipboard* clipboard,
                    ClipboardBuffer buffer,
                    const DataTransferEndpoint* data_dst);
std::vector<uint8_t> ReadPng(Clipboard* clipboard,
                             ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst);
std::u16string ReadDataTransferCustomData(Clipboard* clipboard,
                                          ClipboardBuffer buffer,
                                          const std::u16string& type,
                                          const DataTransferEndpoint* data_dst);
std::vector<FileInfo> ReadFilenames(Clipboard* clipboard,
                                    ClipboardBuffer buffer,
                                    const DataTransferEndpoint* data_dst);
void ReadBookmark(Clipboard* clipboard,
                  const DataTransferEndpoint* data_dst,
                  std::u16string* title,
                  std::string* url);
std::string ReadData(Clipboard* clipboard,
                     const ClipboardFormatType& format,
                     const DataTransferEndpoint* data_dst);

}  // namespace clipboard_test_util

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_
