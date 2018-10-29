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
#include "jni/Clipboard_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
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

// Various formats we support.
const char kURLFormat[] = "url";
const char kPlainTextFormat[] = "text";
const char kHTMLFormat[] = "html";
const char kRTFFormat[] = "rtf";
const char kBitmapFormat[] = "bitmap";
const char kWebKitSmartPasteFormat[] = "webkit_smart";
const char kBookmarkFormat[] = "bookmark";

class ClipboardMap {
 public:
  ClipboardMap();
  void SetModifiedCallback(ClipboardAndroid::ModifiedCallback cb);
  std::string Get(const std::string& format);
  uint64_t GetSequenceNumber() const;
  base::Time GetLastModifiedTime() const;
  void ClearLastModifiedTime();
  bool HasFormat(const std::string& format);
  void OnPrimaryClipboardChanged();
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
  return base::ContainsKey(map_, format);
}

void ClipboardMap::OnPrimaryClipboardChanged() {
  sequence_number_++;
  UpdateLastModifiedTime(base::Time::Now());
  map_state_ = MapState::kOutOfDate;
}

void ClipboardMap::Set(const std::string& format, const std::string& data) {
  base::AutoLock lock(lock_);
  map_[format] = data;
  map_state_ = MapState::kPreparingCommit;
}

void ClipboardMap::CommitToAndroidClipboard() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  if (base::ContainsKey(map_, kHTMLFormat)) {
    // Android's API for storing HTML content on the clipboard requires a plain-
    // text representation to be available as well.
    if (!base::ContainsKey(map_, kPlainTextFormat))
      return;

    ScopedJavaLocalRef<jstring> html =
        ConvertUTF8ToJavaString(env, map_[kHTMLFormat]);
    ScopedJavaLocalRef<jstring> text =
        ConvertUTF8ToJavaString(env, map_[kPlainTextFormat]);

    DCHECK(html.obj() && text.obj());
    Java_Clipboard_setHTMLText(env, clipboard_manager_, html, text);
  } else if (base::ContainsKey(map_, kPlainTextFormat)) {
    ScopedJavaLocalRef<jstring> str =
        ConvertUTF8ToJavaString(env, map_[kPlainTextFormat]);
    DCHECK(str.obj());
    Java_Clipboard_setText(env, clipboard_manager_, str);
  } else {
    Java_Clipboard_clear(env, clipboard_manager_);
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

  JNI_Clipboard_AddMapEntry(env, &map_, kPlainTextFormat, jtext);
  JNI_Clipboard_AddMapEntry(env, &map_, kHTMLFormat, jhtml);

  map_state_ = MapState::kUpToDate;
}

}  // namespace

// Clipboard::FormatType implementation.
Clipboard::FormatType::FormatType() {
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
}

Clipboard::FormatType::~FormatType() {
}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  return data_ < other.data_;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

// Various predefined FormatTypes.
// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  static base::NoDestructor<FormatType> type(kURLFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  static base::NoDestructor<FormatType> type(kPlainTextFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  static base::NoDestructor<FormatType> type(kPlainTextFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  static base::NoDestructor<FormatType> type(kWebKitSmartPasteFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  static base::NoDestructor<FormatType> type(kHTMLFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  static base::NoDestructor<FormatType> type(kRTFFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  static base::NoDestructor<FormatType> type(kBitmapFormat);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeWebCustomData);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypePepperCustomData);
  return *type;
}

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

void ClipboardAndroid::SetModifiedCallback(ModifiedCallback cb) {
  g_map.Get().SetModifiedCallback(std::move(cb));
}

void ClipboardAndroid::SetLastModifiedTimeWithoutRunningCallback(
    base::Time time) {
  g_map.Get().SetLastModifiedTimeWithoutRunningCallback(time);
}

ClipboardAndroid::ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
}

ClipboardAndroid::~ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
}

void ClipboardAndroid::OnPreShutdown() {}

uint64_t ClipboardAndroid::GetSequenceNumber(ClipboardType /* type */) const {
  DCHECK(CalledOnValidThread());
  return g_map.Get().GetSequenceNumber();
}

bool ClipboardAndroid::IsFormatAvailable(const Clipboard::FormatType& format,
                                         ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  return g_map.Get().HasFormat(format.ToString());
}

void ClipboardAndroid::Clear(ClipboardType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  g_map.Get().Clear();
}

void ClipboardAndroid::ReadAvailableTypes(ClipboardType type,
                                          std::vector<base::string16>* types,
                                          bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  types->clear();

  // would be nice to ask the ClipboardMap to enumerate the types it supports,
  // rather than hardcode the list here.
  if (IsFormatAvailable(Clipboard::GetPlainTextFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(Clipboard::GetHtmlFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));

  // these formats aren't supported by the ClipboardMap currently, but might
  // be one day?
  if (IsFormatAvailable(Clipboard::GetRtfFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (IsFormatAvailable(Clipboard::GetBitmapFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;
}

void ClipboardAndroid::ReadText(ClipboardType type,
                                base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  std::string utf8;
  ReadAsciiText(type, &utf8);
  *result = base::UTF8ToUTF16(utf8);
}

void ClipboardAndroid::ReadAsciiText(ClipboardType type,
                                     std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  *result = g_map.Get().Get(kPlainTextFormat);
}

// Note: |src_url| isn't really used. It is only implemented in Windows
void ClipboardAndroid::ReadHTML(ClipboardType type,
                                base::string16* markup,
                                std::string* src_url,
                                uint32_t* fragment_start,
                                uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  if (src_url)
    src_url->clear();

  std::string input = g_map.Get().Get(kHTMLFormat);
  *markup = base::UTF8ToUTF16(input);

  *fragment_start = 0;
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardAndroid::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

SkBitmap ClipboardAndroid::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  std::string input = g_map.Get().Get(kBitmapFormat);

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

void ClipboardAndroid::ReadCustomData(ClipboardType clipboard_type,
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

void ClipboardAndroid::ReadData(const Clipboard::FormatType& format,
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
void ClipboardAndroid::WriteObjects(ClipboardType type,
                                    const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  g_map.Get().Clear();

  for (const auto& object : objects)
    DispatchObject(static_cast<ObjectType>(object.first), object.second);

  g_map.Get().CommitToAndroidClipboard();
}

void ClipboardAndroid::WriteText(const char* text_data, size_t text_len) {
  g_map.Get().Set(kPlainTextFormat, std::string(text_data, text_len));
}

void ClipboardAndroid::WriteHTML(const char* markup_data,
                                 size_t markup_len,
                                 const char* url_data,
                                 size_t url_len) {
  g_map.Get().Set(kHTMLFormat, std::string(markup_data, markup_len));
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
  g_map.Get().Set(kBookmarkFormat, std::string(url_data, url_len));
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardAndroid::WriteWebSmartPaste() {
  g_map.Get().Set(kWebKitSmartPasteFormat, std::string());
}

// Note: we implement this to pass all unit tests but it is currently unclear
// how some code would consume this.
void ClipboardAndroid::WriteBitmap(const SkBitmap& bitmap) {
  gfx::Size size(bitmap.width(), bitmap.height());

  std::string packed(reinterpret_cast<const char*>(&size), sizeof(size));
  packed += std::string(static_cast<const char*>(bitmap.getPixels()),
                        bitmap.computeByteSize());
  g_map.Get().Set(kBitmapFormat, packed);
}

void ClipboardAndroid::WriteData(const Clipboard::FormatType& format,
                                 const char* data_data,
                                 size_t data_len) {
  g_map.Get().Set(format.ToString(), std::string(data_data, data_len));
}

// Returns a pointer to the current ClipboardAndroid object.
static jlong JNI_Clipboard_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(Clipboard::GetForCurrentThread());
}

} // namespace ui
