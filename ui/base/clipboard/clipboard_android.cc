// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/ui_base_jni_headers/Clipboard_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// TODO:(andrewhayden) Support additional formats in Android: URI, HTML,
// HTML+text now that Android's clipboard system supports them, then nuke the
// legacy implementation note below.

// Legacy implementation note:
// The Android clipboard, and hence `ClipboardAndroid` as well, used to only
// support text. Since then, bitmap support has been added, but not support
// for any other formats.
//
// Therefore, Clipboard data is stored:
// - on the Android system, for text and bitmaps.
// - in a process-wide static variable protected by a lock, for other formats.
//
// These "other formats" only work within the same process, and can't be copied
// between Android applications.

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace ui {

namespace {

constexpr char kPngExtension[] = ".png";

using ReadImageCallback = ClipboardAndroid::ReadImageCallback;

// Fetching image data from Java.
SkBitmap GetImageData(
    const base::android::ScopedJavaGlobalRef<jobject>& clipboard_manager) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmap =
      Java_Clipboard_getImage(env, clipboard_manager);
  if (jbitmap.is_null()) {
    return SkBitmap();
  }

  gfx::JavaBitmap java_bitmap(jbitmap);
  if (java_bitmap.size().IsEmpty() || java_bitmap.stride() == 0U ||
      java_bitmap.pixels() == nullptr) {
    return SkBitmap();
  }

  return gfx::CreateSkBitmapFromJavaBitmap(java_bitmap);
}

class ClipboardMap {
 public:
  ClipboardMap();
  void SetModifiedCallback(ClipboardAndroid::ModifiedCallback cb);
  void SetJavaSideNativePtr(Clipboard* clipboard);
  std::string Get(const ClipboardFormatType& format);
  void GetImage(ReadImageCallback callback);
  uint64_t GetSequenceNumber() const;
  base::Time GetLastModifiedTime() const;
  void ClearLastModifiedTime();
  bool HasFormat(const ClipboardFormatType& format);
  std::vector<ClipboardFormatType> GetFormats();
  void OnPrimaryClipboardChanged();
  void OnPrimaryClipTimestampInvalidated(int64_t timestamp_ms);
  void Set(const ClipboardFormatType& format, const std::string& data);
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

  std::map<ClipboardFormatType, std::string> map_ GUARDED_BY(lock_);
  MapState map_state_;

  // This lock is for read/write |map_|.
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

std::string ClipboardMap::Get(const ClipboardFormatType& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  auto it = map_.find(format);
  return it == map_.end() ? std::string() : it->second;
}

void ClipboardMap::GetImage(ReadImageCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetImageData, clipboard_manager_), std::move(callback));
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

bool ClipboardMap::HasFormat(const ClipboardFormatType& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  return base::Contains(map_, format);
}

std::vector<ClipboardFormatType> ClipboardMap::GetFormats() {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  std::vector<ClipboardFormatType> formats;
  formats.reserve(map_.size());
  for (const auto& it : map_)
    formats.push_back(it.first);
  return formats;
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

void ClipboardMap::Set(const ClipboardFormatType& format,
                       const std::string& data) {
  base::AutoLock lock(lock_);
  map_[format] = data;
  map_state_ = MapState::kPreparingCommit;
}

void ClipboardMap::CommitToAndroidClipboard() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  if (base::Contains(map_, ClipboardFormatType::GetHtmlType())) {
    // Android's API for storing HTML content on the clipboard requires a plain-
    // text representation to be available as well.
    if (!base::Contains(map_, ClipboardFormatType::GetPlainTextType()))
      return;

    ScopedJavaLocalRef<jstring> html =
        ConvertUTF8ToJavaString(env, map_[ClipboardFormatType::GetHtmlType()]);
    ScopedJavaLocalRef<jstring> text = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::GetPlainTextType()]);

    DCHECK(html.obj() && text.obj());
    Java_Clipboard_setHTMLText(env, clipboard_manager_, html, text);
  } else if (base::Contains(map_, ClipboardFormatType::GetPlainTextType())) {
    ScopedJavaLocalRef<jstring> str = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::GetPlainTextType()]);
    DCHECK(str.obj());
    Java_Clipboard_setText(env, clipboard_manager_, str);
  } else if (base::Contains(map_, ClipboardFormatType::GetBitmapType())) {
    ScopedJavaLocalRef<jbyteArray> image_data =
        ToJavaByteArray(env, map_[ClipboardFormatType::GetBitmapType()]);
    ScopedJavaLocalRef<jstring> image_extension =
        ConvertUTF8ToJavaString(env, kPngExtension);
    DCHECK(image_data.obj());
    Java_Clipboard_setImage(env, clipboard_manager_, image_data,
                            image_extension);
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

// Add a format:jstr pair to map, if jstr is null or is empty, then remove that
// entry.
void JNI_Clipboard_AddMapEntry(JNIEnv* env,
                               std::map<ClipboardFormatType, std::string>* map,
                               const ClipboardFormatType& format,
                               const ScopedJavaLocalRef<jstring>& jstr) {
  if (jstr.is_null()) {
    map->erase(format);
    return;
  }

  std::string str = ConvertJavaStringToUTF8(env, jstr.obj());
  if (!str.empty()) {
    (*map)[format] = str;
  } else {
    map->erase(format);
  }
}

void ClipboardMap::UpdateLastModifiedTime(base::Time time) {
  last_modified_time_ = time;
  // |modified_cb_| may be null in tests.
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
  ScopedJavaLocalRef<jstring> jimageuri =
      Java_Clipboard_getImageUriString(env, clipboard_manager_);

  JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::GetPlainTextType(),
                            jtext);
  JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::GetHtmlType(),
                            jhtml);
  JNI_Clipboard_AddMapEntry(
      env, &map_, ClipboardFormatType::GetType(kMimeTypeImageURI), jimageuri);

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

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardAndroid::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const ClipboardDataEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  if (format == ClipboardFormatType::GetBitmapType()) {
    return g_map.Get().HasFormat(
        ClipboardFormatType::GetType(kMimeTypeImageURI));
  }
  return g_map.Get().HasFormat(format);
}

void ClipboardAndroid::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const ClipboardDataEndpoint* data_dst,
    std::vector<base::string16>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  DCHECK(types);

  types->clear();

  // would be nice to ask the ClipboardMap to enumerate the types it supports,
  // rather than hardcode the list here.
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));

  // these formats aren't supported by the ClipboardMap currently, but might
  // be one day?
  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (IsFormatAvailable(ClipboardFormatType::GetBitmapType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
std::vector<base::string16>
ClipboardAndroid::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const ClipboardDataEndpoint* data_dst) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::vector<ClipboardFormatType> formats = g_map.Get().GetFormats();

  std::vector<base::string16> types;
  types.reserve(formats.size());
  for (const ClipboardFormatType& format : formats)
    types.push_back(base::UTF8ToUTF16(format.GetName()));

  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadText(ClipboardBuffer buffer,
                                const ClipboardDataEndpoint* data_dst,
                                base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::string utf8;
  ReadAsciiText(buffer, data_dst, &utf8);
  *result = base::UTF8ToUTF16(utf8);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadAsciiText(ClipboardBuffer buffer,
                                     const ClipboardDataEndpoint* data_dst,
                                     std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);
  *result = g_map.Get().Get(ClipboardFormatType::GetPlainTextType());
}

// Note: |src_url| isn't really used. It is only implemented in Windows.
// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadHTML(ClipboardBuffer buffer,
                                const ClipboardDataEndpoint* data_dst,
                                base::string16* markup,
                                std::string* src_url,
                                uint32_t* fragment_start,
                                uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kHtml);
  if (src_url)
    src_url->clear();

  std::string input = g_map.Get().Get(ClipboardFormatType::GetHtmlType());
  *markup = base::UTF8ToUTF16(input);

  *fragment_start = 0;
  *fragment_end = static_cast<uint32_t>(markup->length());
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadSvg(ClipboardBuffer buffer,
                               const ClipboardDataEndpoint* data_dst,
                               base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::string utf8 = g_map.Get().Get(ClipboardFormatType::GetSvgType());
  *result = base::UTF8ToUTF16(utf8);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadRTF(ClipboardBuffer buffer,
                               const ClipboardDataEndpoint* data_dst,
                               std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadImage(ClipboardBuffer buffer,
                                 const ClipboardDataEndpoint* data_dst,
                                 ReadImageCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kImage);
  g_map.Get().GetImage(std::move(callback));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadCustomData(ClipboardBuffer buffer,
                                      const base::string16& type,
                                      const ClipboardDataEndpoint* data_dst,
                                      base::string16* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadBookmark(const ClipboardDataEndpoint* data_dst,
                                    base::string16* title,
                                    std::string* url) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadData(const ClipboardFormatType& format,
                                const ClipboardDataEndpoint* data_dst,
                                std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);
  *result = g_map.Get().Get(format);
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
// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::WritePortableRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<ClipboardDataEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();

  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);

  g_map.Get().CommitToAndroidClipboard();
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<ClipboardDataEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();

  DispatchPlatformRepresentations(std::move(platform_representations));

  g_map.Get().CommitToAndroidClipboard();
}

void ClipboardAndroid::WriteText(const char* text_data, size_t text_len) {
  g_map.Get().Set(ClipboardFormatType::GetPlainTextType(),
                  std::string(text_data, text_len));
}

void ClipboardAndroid::WriteHTML(const char* markup_data,
                                 size_t markup_len,
                                 const char* url_data,
                                 size_t url_len) {
  g_map.Get().Set(ClipboardFormatType::GetHtmlType(),
                  std::string(markup_data, markup_len));
}

void ClipboardAndroid::WriteSvg(const char* markup_data, size_t markup_len) {
  g_map.Get().Set(ClipboardFormatType::GetSvgType(),
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
  g_map.Get().Set(ClipboardFormatType::GetUrlType(),
                  std::string(url_data, url_len));
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardAndroid::WriteWebSmartPaste() {
  g_map.Get().Set(ClipboardFormatType::GetWebKitSmartPasteType(),
                  std::string());
}

// Encoding SkBitmap to PNG data. Then, |g_map| can commit the PNG data to
// Android system clipboard without encode/decode.
void ClipboardAndroid::WriteBitmap(const SkBitmap& sk_bitmap) {
  scoped_refptr<base::RefCountedMemory> image_memory =
      gfx::Image::CreateFrom1xBitmap(sk_bitmap).As1xPNGBytes();
  std::string packed(image_memory->front_as<char>(), image_memory->size());

  g_map.Get().Set(ClipboardFormatType::GetBitmapType(), packed);
}

void ClipboardAndroid::WriteData(const ClipboardFormatType& format,
                                 const char* data_data,
                                 size_t data_len) {
  g_map.Get().Set(format, std::string(data_data, data_len));
}

}  // namespace ui
