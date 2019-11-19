// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include <algorithm>
#include <utility>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/ui_base_jni_headers/Clipboard_jni.h"
#include "ui/gfx/geometry/size.h"

// TODO:(andrewhayden) Support additional formats in Android: Bitmap, URI, HTML,
// HTML+text now that Android's clipboard system supports them, then nuke the
// legacy implementation note below.

// Legacy implementation note:
// The Android clipboard system used to only support text format. So we used the
// Android system when some text was added or retrieved from the system. For
// anything else, we STILL store the value in some process wide static
// variable protected by a lock. So the (non-text) clipboard will only work
// within the same process.

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

namespace {

class ClipboardMap {
 public:
  ClipboardMap();
  void SetModifiedCallback(ClipboardAndroid::ModifiedCallback cb);
  void SetJavaSideNativePtr(Clipboard* clipboard);
  std::string Get(const std::string& format);
  uint64_t GetSequenceNumber() const;
  base::Time GetLastModifiedTime() const;
  void ClearLastModifiedTime();
  bool HasFormat(const std::string& format);
  void OnPrimaryClipboardChanged();
  void OnPrimaryClipTimestampInvalidated(int64_t timestamp_ms);
  void Set(const std::string& format, const std::string& data);
  void CommitToAndroidClipboard();
  void Clear();

  // Unlike the functions above, does not call |modified_cb_|.
  void SetLastModifiedTimeWithoutRunningCallback(base::Time time);

 private:
  enum class MapState {
    kOutOfDate,
    kUpToDate,
    kPreparingCommit,
  };

  // Updates |last_modified_time_| to |time| and writes it to |local_state_|.
  void UpdateLastModifiedTime(base::Time time);

  // Updates |map_| and |map_state_| if necessary by fetching data from Java.
  void UpdateFromAndroidClipboard();

  std::map<std::string, std::string> map_;
  MapState map_state_;
  base::Lock lock_;

  uint64_t sequence_number_;
  base::Time last_modified_time_;

  ClipboardAndroid::ModifiedCallback modified_cb_;

  // Java class and methods for the Android ClipboardManager.
  ScopedJavaGlobalRef<jobject> clipboard_manager_;
};
base::LazyInstance<ClipboardMap>::Leaky g_map = LAZY_INSTANCE_INITIALIZER;

ClipboardMap::ClipboardMap() : map_state_(MapState::kOutOfDate) {
  clipboard_manager_.Reset(Java_Clipboard_getInstance(AttachCurrentThread()));
  DCHECK(clipboard_manager_.obj());
}

void ClipboardMap::SetModifiedCallback(ClipboardAndroid::ModifiedCallback cb) {
  modified_cb_ = std::move(cb);
}

void ClipboardMap::SetJavaSideNativePtr(Clipboard* clipboard) {
  JNIEnv* env = AttachCurrentThread();
  Java_Clipboard_setNativePtr(env, clipboard_manager_,
                              reinterpret_cast<intptr_t>(clipboard));
}

std::string ClipboardMap::Get(const std::string& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  std::map<std::string, std::string>::const_iterator it = map_.find(format);
  return it == map_.end() ? std::string() : it->second;
}

uint64_t ClipboardMap::GetSequenceNumber() const {
  return sequence_number_;
}

base::Time ClipboardMap::GetLastModifiedTime() const {
  return last_modified_time_;
}

void ClipboardMap::ClearLastModifiedTime() {
  UpdateLastModifiedTime(base::Time());
}

bool ClipboardMap::HasFormat(const std::string& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  return base::Contains(map_, format);
}

void ClipboardMap::OnPrimaryClipboardChanged() {
  sequence_number_++;
  UpdateLastModifiedTime(base::Time::Now());
  map_state_ = MapState::kOutOfDate;
}

void ClipboardMap::OnPrimaryClipTimestampInvalidated(int64_t timestamp_ms) {
  base::Time timestamp = base::Time::FromJavaTime(timestamp_ms);
  if (GetLastModifiedTime() < timestamp) {
    sequence_number_++;
    UpdateLastModifiedTime(timestamp);
    map_state_ = MapState::kOutOfDate;
  }
}

void ClipboardMap::Set(const std::string& format, const std::string& data) {
  base::AutoLock lock(lock_);
  map_[format] = data;
  map_state_ = MapState::kPreparingCommit;
}

void ClipboardMap::CommitToAndroidClipboard() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  if (base::Contains(map_, ClipboardFormatType::GetHtmlType().ToString())) {
    // Android's API for storing HTML content on the clipboard requires a plain-
    // text representation to be available as well.
    if (!base::Contains(map_,
                        ClipboardFormatType::GetPlainTextType().ToString()))
      return;

    ScopedJavaLocalRef<jstring> html = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::GetHtmlType().ToString()]);
    ScopedJavaLocalRef<jstring> text = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::GetPlainTextType().ToString()]);

    DCHECK(html.obj() && text.obj());
    Java_Clipboard_setHTMLText(env, clipboard_manager_, html, text);
  } else if (base::Contains(
                 map_, ClipboardFormatType::GetPlainTextType().ToString())) {
    ScopedJavaLocalRef<jstring> str = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::GetPlainTextType().ToString()]);
    DCHECK(str.obj());
    Java_Clipboard_setText(env, clipboard_manager_, str);
  } else {
    Java_Clipboard_clear(env, clipboard_manager_);
    // TODO(huangdarwin): Implement raw clipboard support for arbitrary formats.
    NOTIMPLEMENTED();
  }
  map_state_ = MapState::kUpToDate;
  sequence_number_++;
  UpdateLastModifiedTime(base::Time::Now());
}

void ClipboardMap::Clear() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  map_.clear();
  Java_Clipboard_clear(env, clipboard_manager_);
  map_state_ = MapState::kUpToDate;
  sequence_number_++;
  UpdateLastModifiedTime(base::Time::Now());
}

void ClipboardMap::SetLastModifiedTimeWithoutRunningCallback(base::Time time) {
  last_modified_time_ = time;
}

// Add a key:jstr pair to map, if jstr is null or is empty, then remove that
// entry.
void JNI_Clipboard_AddMapEntry(JNIEnv* env,
                               std::map<std::string, std::string>* map,
                               const char* key,
                               const ScopedJavaLocalRef<jstring>& jstr) {
  if (jstr.is_null()) {
    map->erase(key);
    return;
  }
  std::string str = ConvertJavaStringToUTF8(env, jstr.obj());
  if (!str.empty()) {
    (*map)[key] = str;
  } else {
    map->erase(key);
  }
}

void ClipboardMap::UpdateLastModifiedTime(base::Time time) {
  last_modified_time_ = time;
  // |modified_callback_| may be null in tests.
  if (modified_cb_)
    modified_cb_.Run(time);
}

void ClipboardMap::UpdateFromAndroidClipboard() {
  DCHECK_NE(MapState::kPreparingCommit, map_state_);
  if (map_state_ == MapState::kUpToDate)
    return;

  // Fetch the current Android clipboard state.
  lock_.AssertAcquired();
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> jtext =
      Java_Clipboard_getCoercedText(env, clipboard_manager_);
  ScopedJavaLocalRef<jstring> jhtml =
      Java_Clipboard_getHTMLText(env, clipboard_manager_);

  JNI_Clipboard_AddMapEntry(
      env, &map_, ClipboardFormatType::GetPlainTextType().ToString().c_str(),
      jtext);
  JNI_Clipboard_AddMapEntry(
      env, &map_, ClipboardFormatType::GetHtmlType().ToString().c_str(), jhtml);

  map_state_ = MapState::kUpToDate;
}

}  // namespace

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardAndroid;
}

// ClipboardAndroid implementation.

void ClipboardAndroid::OnPrimaryClipChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  g_map.Get().OnPrimaryClipboardChanged();
}

void ClipboardAndroid::OnPrimaryClipTimestampInvalidated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jlong j_timestamp_ms) {
  g_map.Get().OnPrimaryClipTimestampInvalidated(j_timestamp_ms);
}

int64_t ClipboardAndroid::GetLastModifiedTimeToJavaTime(JNIEnv* env) {
  return GetLastModifiedTime().ToJavaTime();
}

void ClipboardAndroid::SetModifiedCallback(ModifiedCallback cb) {
  g_map.Get().SetModifiedCallback(std::move(cb));
}

void ClipboardAndroid::SetLastModifiedTimeWithoutRunningCallback(
    base::Time time) {
  g_map.Get().SetLastModifiedTimeWithoutRunningCallback(time);
}

ClipboardAndroid::ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
  g_map.Get().SetJavaSideNativePtr(this);
}

ClipboardAndroid::~ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
}

void ClipboardAndroid::OnPreShutdown() {}

uint64_t ClipboardAndroid::GetSequenceNumber(
    ClipboardBuffer /* buffer */) const {
  DCHECK(CalledOnValidThread());
  return g_map.Get().GetSequenceNumber();
}

bool ClipboardAndroid::IsFormatAvailable(const ClipboardFormatType& format,
                                         ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  return g_map.Get().HasFormat(format.ToString());
}

void ClipboardAndroid::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();
}

void ClipboardAndroid::ReadAvailableTypes(ClipboardBuffer buffer,
                                          std::vector<base::string16>* types,
                                          bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  types->clear();

  // would be nice to ask the ClipboardMap to enumerate the types it supports,
  // rather than hardcode the list here.
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));

  // these formats aren't supported by the ClipboardMap currently, but might
  // be one day?
  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (IsFormatAvailable(ClipboardFormatType::GetBitmapType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;
}

void ClipboardAndroid::ReadText(ClipboardBuffer buffer,
                                base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::string utf8;
  ReadAsciiText(buffer, &utf8);
  *result = base::UTF8ToUTF16(utf8);
}

void ClipboardAndroid::ReadAsciiText(ClipboardBuffer buffer,
                                     std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  *result = g_map.Get().Get(ClipboardFormatType::GetPlainTextType().ToString());
}

// Note: |src_url| isn't really used. It is only implemented in Windows
void ClipboardAndroid::ReadHTML(ClipboardBuffer buffer,
                                base::string16* markup,
                                std::string* src_url,
                                uint32_t* fragment_start,
                                uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  if (src_url)
    src_url->clear();

  std::string input =
      g_map.Get().Get(ClipboardFormatType::GetHtmlType().ToString());
  *markup = base::UTF8ToUTF16(input);

  *fragment_start = 0;
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardAndroid::ReadRTF(ClipboardBuffer buffer,
                               std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

SkBitmap ClipboardAndroid::ReadImage(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::string input =
      g_map.Get().Get(ClipboardFormatType::GetBitmapType().ToString());

  SkBitmap bmp;
  if (!input.empty()) {
    DCHECK_LE(sizeof(gfx::Size), input.size());
    const gfx::Size* size = reinterpret_cast<const gfx::Size*>(input.data());

    bmp.allocN32Pixels(size->width(), size->height());

    DCHECK_EQ(sizeof(gfx::Size) + bmp.computeByteSize(), input.size());

    memcpy(bmp.getPixels(), input.data() + sizeof(gfx::Size),
           bmp.computeByteSize());
  }
  return bmp;
}

void ClipboardAndroid::ReadCustomData(ClipboardBuffer buffer,
                                      const base::string16& type,
                                      base::string16* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardAndroid::ReadBookmark(base::string16* title,
                                    std::string* url) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardAndroid::ReadData(const ClipboardFormatType& format,
                                std::string* result) const {
  DCHECK(CalledOnValidThread());
  *result = g_map.Get().Get(format.ToString());
}

base::Time ClipboardAndroid::GetLastModifiedTime() const {
  DCHECK(CalledOnValidThread());
  return g_map.Get().GetLastModifiedTime();
}

void ClipboardAndroid::ClearLastModifiedTime() {
  DCHECK(CalledOnValidThread());
  g_map.Get().ClearLastModifiedTime();
}

// Main entry point used to write several values in the clipboard.
void ClipboardAndroid::WritePortableRepresentations(ClipboardBuffer buffer,
                                                    const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();

  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);

  g_map.Get().CommitToAndroidClipboard();
}

void ClipboardAndroid::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();

  DispatchPlatformRepresentations(std::move(platform_representations));

  g_map.Get().CommitToAndroidClipboard();
}

void ClipboardAndroid::WriteText(const char* text_data, size_t text_len) {
  g_map.Get().Set(ClipboardFormatType::GetPlainTextType().ToString(),
                  std::string(text_data, text_len));
}

void ClipboardAndroid::WriteHTML(const char* markup_data,
                                 size_t markup_len,
                                 const char* url_data,
                                 size_t url_len) {
  g_map.Get().Set(ClipboardFormatType::GetHtmlType().ToString(),
                  std::string(markup_data, markup_len));
}

void ClipboardAndroid::WriteRTF(const char* rtf_data, size_t data_len) {
  NOTIMPLEMENTED();
}

// Note: according to other platforms implementations, this really writes the
// URL spec.
void ClipboardAndroid::WriteBookmark(const char* title_data,
                                     size_t title_len,
                                     const char* url_data,
                                     size_t url_len) {
  g_map.Get().Set(ClipboardFormatType::GetBookmarkType().ToString(),
                  std::string(url_data, url_len));
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardAndroid::WriteWebSmartPaste() {
  g_map.Get().Set(ClipboardFormatType::GetWebKitSmartPasteType().ToString(),
                  std::string());
}

// Note: we implement this to pass all unit tests but it is currently unclear
// how some code would consume this.
void ClipboardAndroid::WriteBitmap(const SkBitmap& bitmap) {
  gfx::Size size(bitmap.width(), bitmap.height());

  std::string packed(reinterpret_cast<const char*>(&size), sizeof(size));
  packed += std::string(static_cast<const char*>(bitmap.getPixels()),
                        bitmap.computeByteSize());
  g_map.Get().Set(ClipboardFormatType::GetBitmapType().ToString(), packed);
}

void ClipboardAndroid::WriteData(const ClipboardFormatType& format,
                                 const char* data_data,
                                 size_t data_len) {
  g_map.Get().Set(format.ToString(), std::string(data_data, data_len));
}

}  // namespace ui
