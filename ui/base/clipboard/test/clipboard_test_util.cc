// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/test/clipboard_test_util.h"

#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "base/types/optional_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui::clipboard_test_util {

std::vector<std::u16string> ReadAvailableTypes(
    Clipboard* clipboard,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::vector<std::u16string>> future;
  clipboard->ReadAvailableTypes(buffer, base::OptionalFromPtr(data_dst),
                                future.GetCallback());
  return future.Take();
}

std::u16string ReadText(Clipboard* clipboard,
                        ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::u16string> future;
  clipboard->ReadText(buffer, base::OptionalFromPtr(data_dst),
                      future.GetCallback());
  return future.Take();
}

std::string ReadAsciiText(Clipboard* clipboard,
                          ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::string> future;
  clipboard->ReadAsciiText(buffer, base::OptionalFromPtr(data_dst),
                           future.GetCallback());
  return future.Take();
}

void ReadHTML(Clipboard* clipboard,
              ClipboardBuffer buffer,
              const DataTransferEndpoint* data_dst,
              std::u16string* markup,
              std::string* src_url,
              uint32_t* fragment_start,
              uint32_t* fragment_end) {
  base::test::TestFuture<std::u16string, GURL, uint32_t, uint32_t> future;
  clipboard->ReadHTML(buffer, base::OptionalFromPtr(data_dst),
                      future.GetCallback());
  auto [m, url, start, end] = future.Take();
  if (markup) {
    *markup = std::move(m);
  }
  if (src_url) {
    *src_url = url.spec();
  }
  if (fragment_start) {
    *fragment_start = start;
  }
  if (fragment_end) {
    *fragment_end = end;
  }
}

std::u16string ReadSvg(Clipboard* clipboard,
                       ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::u16string> future;
  clipboard->ReadSvg(buffer, base::OptionalFromPtr(data_dst),
                     future.GetCallback());
  return future.Take();
}

std::string ReadRTF(Clipboard* clipboard,
                    ClipboardBuffer buffer,
                    const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::string> future;
  clipboard->ReadRTF(buffer, base::OptionalFromPtr(data_dst),
                     future.GetCallback());
  return future.Take();
}

std::vector<uint8_t> ReadPng(Clipboard* clipboard,
                             ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<const std::vector<uint8_t>&> future;
  clipboard->ReadPng(buffer, base::OptionalFromPtr(data_dst),
                     future.GetCallback());
  return future.Take();
}

std::u16string ReadDataTransferCustomData(
    Clipboard* clipboard,
    ClipboardBuffer buffer,
    const std::u16string& type,
    const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::u16string> future;
  clipboard->ReadDataTransferCustomData(
      buffer, type, base::OptionalFromPtr(data_dst), future.GetCallback());
  return future.Take();
}

std::vector<FileInfo> ReadFilenames(Clipboard* clipboard,
                                    ClipboardBuffer buffer,
                                    const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::vector<FileInfo>> future;
  clipboard->ReadFilenames(buffer, base::OptionalFromPtr(data_dst),
                           future.GetCallback());
  return future.Take();
}

void ReadBookmark(Clipboard* clipboard,
                  const DataTransferEndpoint* data_dst,
                  std::u16string* title,
                  std::string* url) {
  base::test::TestFuture<std::u16string, GURL> future;
  clipboard->ReadBookmark(base::OptionalFromPtr(data_dst),
                          future.GetCallback());
  auto [t, u] = future.Take();
  if (title) {
    *title = std::move(t);
  }
  if (url) {
    *url = u.spec();
  }
}

std::string ReadData(Clipboard* clipboard,
                     const ClipboardFormatType& format,
                     const DataTransferEndpoint* data_dst) {
  base::test::TestFuture<std::string> future;
  clipboard->ReadData(format, base::OptionalFromPtr(data_dst),
                      future.GetCallback());
  return future.Take();
}

}  // namespace ui::clipboard_test_util
