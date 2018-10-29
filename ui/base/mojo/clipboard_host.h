// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJO_CLIPBOARD_HOST_H_
#define UI_BASE_MOJO_CLIPBOARD_HOST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/base/mojo/clipboard.mojom.h"

namespace ui {

class Clipboard;
class ScopedClipboardWriter;

// A ClipboardHost interface impl backed by the local Clipboard instance.
// The host and client are both tested in ui/views/mus/clipboard_unittest.cc.
class ClipboardHost : public mojom::ClipboardHost {
 public:
  ClipboardHost();
  ~ClipboardHost() override;

  void AddBinding(mojom::ClipboardHostRequest request);

  // mojom::ClipboardHost:
  void GetSequenceNumber(ClipboardType type,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(const std::string& format,
                         ClipboardType type,
                         IsFormatAvailableCallback callback) override;
  void Clear(ClipboardType type) override;
  void ReadAvailableTypes(ClipboardType type,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(ClipboardType type, ReadTextCallback callback) override;
  void ReadAsciiText(ClipboardType type,
                     ReadAsciiTextCallback callback) override;
  void ReadHTML(ClipboardType type, ReadHTMLCallback callback) override;
  void ReadRTF(ClipboardType type, ReadRTFCallback callback) override;
  void ReadImage(ClipboardType type, ReadImageCallback callback) override;
  void ReadCustomData(ClipboardType clipboard_type,
                      const base::string16& type,
                      ReadCustomDataCallback callback) override;
  void ReadBookmark(ReadBookmarkCallback callback) override;
  void ReadData(const std::string& format, ReadDataCallback callback) override;
  void GetLastModifiedTime(GetLastModifiedTimeCallback callback) override;
  void ClearLastModifiedTime() override;
  void WriteText(const base::string16& text) override;
  void WriteHTML(const base::string16& markup, const std::string& url) override;
  void WriteRTF(const std::string& rtf) override;
  void WriteBookmark(const std::string& url,
                     const base::string16& title) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const std::string& type, const std::string& data) override;
  void CommitWrite(ClipboardType type) override;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  void WriteStringToFindPboard(const base::string16& text) override;
#endif

  Clipboard* clipboard_;  // Not owned

  // Used to store pending written data until CommitWrite is called.
  std::unique_ptr<ScopedClipboardWriter> clipboard_writer_;

  mojo::BindingSet<mojom::ClipboardHost> bindings_;
};

}  // namespace ui

#endif  // UI_BASE_MOJO_CLIPBOARD_HOST_H_
