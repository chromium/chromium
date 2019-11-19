// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_ANDROID_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_ANDROID_H_

#include "ui/base/clipboard/clipboard.h"

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ui {

class ClipboardAndroid : public Clipboard {
 public:
  // Callback called whenever the clipboard is modified.  The parameter
  // represents the time of the modification.
  using ModifiedCallback = base::Callback<void(base::Time)>;

  // Called by Java when the Java Clipboard is notified that the clipboard has
  // changed.
  void OnPrimaryClipChanged(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  // Called by Java when the Java Clipboard is notified that the window focus
  // has changed. Since Chrome will not receive OnPrimaryClipChanged call from
  // Android if Chrome is in background,Clipboard handler needs to check the
  // content of clipboard didn't change, when Chrome is back in foreground.
  void OnPrimaryClipTimestampInvalidated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jlong j_timestamp_ms);

  // Called by Java side.
  int64_t GetLastModifiedTimeToJavaTime(JNIEnv* env);

  // Sets the callback called whenever the clipboard is modified.
  COMPONENT_EXPORT(BASE_CLIPBOARD)
  void SetModifiedCallback(ModifiedCallback cb);

  // Sets the last modified time without calling the above callback.
  COMPONENT_EXPORT(BASE_CLIPBOARD)
  void SetLastModifiedTimeWithoutRunningCallback(base::Time time);

 private:
  friend class Clipboard;

  ClipboardAndroid();
  ~ClipboardAndroid() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  uint64_t GetSequenceNumber(ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          std::vector<base::string16>* types,
                          bool* contains_filenames) const override;
  void ReadText(ClipboardBuffer buffer, base::string16* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadRTF(ClipboardBuffer buffer, std::string* result) const override;
  SkBitmap ReadImage(ClipboardBuffer buffer) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const base::string16& type,
                      base::string16* result) const override;
  void ReadBookmark(base::string16* title, std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                std::string* result) const override;
  base::Time GetLastModifiedTime() const override;
  void ClearLastModifiedTime() override;
  void WritePortableRepresentations(ClipboardBuffer buffer,
                                    const ObjectMap& objects) override;
  void WritePlatformRepresentations(
      ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation> platform_representations)
      override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 const char* data_data,
                 size_t data_len) override;

  DISALLOW_COPY_AND_ASSIGN(ClipboardAndroid);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_ANDROID_H_
