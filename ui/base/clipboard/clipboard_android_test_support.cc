// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <string>

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_android.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_javatest_jni_headers/ClipboardAndroidTestSupport_jni.h"

namespace ui {

jboolean JNI_ClipboardAndroidTestSupport_NativeWriteHtml(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_html_text) {
  {
    // Simulate something writing HTML to the clipboard in native.
    // Android requires both a plaintext and HTML version.
    std::u16string html_text;
    base::android::ConvertJavaStringToUTF16(env, j_html_text, &html_text);
    std::string url;
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHTML(html_text, url);
    clipboard_writer.WriteText(html_text);
  }
  auto* clipboard = Clipboard::GetForCurrentThread();
  return clipboard->IsFormatAvailable(ClipboardFormatType::HtmlType(),
                                      ClipboardBuffer::kCopyPaste,
                                      /* data_dst = */ nullptr) &&
         clipboard->IsFormatAvailable(ClipboardFormatType::PlainTextType(),
                                      ClipboardBuffer::kCopyPaste,
                                      /* data_dst = */ nullptr);
}

jboolean JNI_ClipboardAndroidTestSupport_NativeClipboardContains(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_text) {
  // The Java side of the test pretended to be another app using
  // ClipboardManager. This should update the native side of the clipboard as
  // well as the Android side.
  auto* clipboard = Clipboard::GetForCurrentThread();
  if (clipboard->IsFormatAvailable(ClipboardFormatType::HtmlType(),
                                   ClipboardBuffer::kCopyPaste,
                                   /* data_dst = */ nullptr)) {
    LOG(ERROR) << "HTML still in clipboard.";
    return false;
  }

  if (!clipboard->IsFormatAvailable(ClipboardFormatType::PlainTextType(),
                                    ClipboardBuffer::kCopyPaste,
                                    /* data_dst = */ nullptr)) {
    LOG(ERROR) << "Plain text not in clipboard.";
    return false;
  }

  std::string expected_text;
  base::android::ConvertJavaStringToUTF8(env, j_text, &expected_text);

  std::string contents;
  clipboard->ReadAsciiText(ClipboardBuffer::kCopyPaste,
                           /* data_dst = */ nullptr, &contents);
  if (expected_text != contents) {
    LOG(ERROR) << "Clipboard contents do not match. Expected: " << expected_text
               << " Actual: " << contents;
    return false;
  }
  return true;
}

}  // namespace ui
