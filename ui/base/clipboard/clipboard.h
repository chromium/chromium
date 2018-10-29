// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/ui_base_export.h"

#if defined(OS_WIN)
#include <objidl.h>
#endif

class SkBitmap;

#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

namespace ui {
class TestClipboard;
class ScopedClipboardWriter;

class UI_BASE_EXPORT Clipboard : public base::ThreadChecker {
 public:
  // MIME type constants.
  static const char kMimeTypeText[];
  static const char kMimeTypeURIList[];
  static const char kMimeTypeDownloadURL[];
  static const char kMimeTypeMozillaURL[];
  static const char kMimeTypeHTML[];
  static const char kMimeTypeRTF[];
  static const char kMimeTypePNG[];
  static const char kMimeTypeWebCustomData[];
  static const char kMimeTypeWebkitSmartPaste[];
  static const char kMimeTypePepperCustomData[];

  // Platform neutral holder for native data representation of a clipboard type.
  struct UI_BASE_EXPORT FormatType {
    FormatType();
    ~FormatType();

    // Serializes and deserializes a FormatType for use in IPC messages.
    std::string Serialize() const;
    static FormatType Deserialize(const std::string& serialization);

    // FormatType can be used in a set on some platforms.
    bool operator<(const FormatType& other) const;

#if defined(OS_WIN)
    const FORMATETC& ToFormatEtc() const { return data_; }
#elif defined(USE_AURA) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
    const std::string& ToString() const { return data_; }
#elif defined(OS_MACOSX)
    NSString* ToNSString() const { return data_; }
    // Custom copy and assignment constructor to handle NSString.
    FormatType(const FormatType& other);
    FormatType& operator=(const FormatType& other);
#endif

    bool Equals(const FormatType& other) const;

   private:
    friend class base::NoDestructor<FormatType>;
    friend class Clipboard;

    // Platform-specific glue used internally by the Clipboard class. Each
    // plaform should define,at least one of each of the following:
    // 1. A constructor that wraps that native clipboard format descriptor.
    // 2. An accessor to retrieve the wrapped descriptor.
    // 3. A data member to hold the wrapped descriptor.
    //
    // Note that in some cases, the accessor for the wrapped descriptor may be
    // public, as these format types can be used by drag and drop code as well.
#if defined(OS_WIN)
    explicit FormatType(UINT native_format);
    FormatType(UINT native_format, LONG index);
    FORMATETC data_;
#elif defined(USE_AURA) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
    explicit FormatType(const std::string& native_format);
    std::string data_;
#elif defined(OS_MACOSX)
    explicit FormatType(NSString* native_format);
    NSString* data_;
#else
#error No FormatType definition.
#endif

    // Copyable and assignable, since this is essentially an opaque value type.
  };

  static bool IsSupportedClipboardType(int32_t type) {
    switch (type) {
      case CLIPBOARD_TYPE_COPY_PASTE:
        return true;
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
      case CLIPBOARD_TYPE_SELECTION:
        return true;
#endif
    }
    return false;
  }

  // Sets the list of threads that are allowed to access the clipboard.
  static void SetAllowedThreads(
      const std::vector<base::PlatformThreadId>& allowed_threads);

  // Sets the clipboard for the current thread. Previously, there was only
  // one clipboard implementation on a platform; now that mus exists, during
  // mus app startup, we need to specifically initialize mus instead of the
  // current platform clipboard. We take ownership of |platform_clipboard|.
  static void SetClipboardForCurrentThread(
      std::unique_ptr<Clipboard> platform_clipboard);

  // Returns the clipboard object for the current thread.
  //
  // Most implementations will have at most one clipboard which will live on
  // the main UI thread, but Windows has tricky semantics where there have to
  // be two clipboards: one that lives on the UI thread and one that lives on
  // the IO thread.
  static Clipboard* GetForCurrentThread();

  // Does any work necessary prior to Chrome shutdown for the current thread.
  // All platforms but Windows have a single clipboard shared accross all
  // threads. This function is a no-op on Windows. On Desktop Linux, if Chrome
  // has ownership of the clipboard selection this function transfers the
  // clipboard selection to the clipboard manager.
  static void OnPreShutdownForCurrentThread();

  // Destroys the clipboard for the current thread. Usually, this will clean up
  // all clipboards, except on Windows. (Previous code leaks the IO thread
  // clipboard, so it shouldn't be a problem.)
  static void DestroyClipboardForCurrentThread();

  virtual void OnPreShutdown() = 0;

  // Returns a sequence number which uniquely identifies clipboard state.
  // This can be used to version the data on the clipboard and determine
  // whether it has changed.
  virtual uint64_t GetSequenceNumber(ClipboardType type) const = 0;

  // Tests whether the clipboard contains a certain format
  virtual bool IsFormatAvailable(const FormatType& format,
                                 ClipboardType type) const = 0;

  // Clear the clipboard data.
  virtual void Clear(ClipboardType type) = 0;

  virtual void ReadAvailableTypes(ClipboardType type,
                                  std::vector<base::string16>* types,
                                  bool* contains_filenames) const = 0;

  // Reads UNICODE text from the clipboard, if available.
  virtual void ReadText(ClipboardType type, base::string16* result) const = 0;

  // Reads ASCII text from the clipboard, if available.
  virtual void ReadAsciiText(ClipboardType type, std::string* result) const = 0;

  // Reads HTML from the clipboard, if available. If the HTML fragment requires
  // context to parse, |fragment_start| and |fragment_end| are indexes into
  // markup indicating the beginning and end of the actual fragment. Otherwise,
  // they will contain 0 and markup->size().
  virtual void ReadHTML(ClipboardType type,
                        base::string16* markup,
                        std::string* src_url,
                        uint32_t* fragment_start,
                        uint32_t* fragment_end) const = 0;

  // Reads RTF from the clipboard, if available. Stores the result as a byte
  // vector.
  virtual void ReadRTF(ClipboardType type, std::string* result) const = 0;

  // Reads an image from the clipboard, if available.
  virtual SkBitmap ReadImage(ClipboardType type) const = 0;

  virtual void ReadCustomData(ClipboardType clipboard_type,
                              const base::string16& type,
                              base::string16* result) const = 0;

  // Reads a bookmark from the clipboard, if available.
  virtual void ReadBookmark(base::string16* title, std::string* url) const = 0;

  // Reads raw data from the clipboard with the given format type. Stores result
  // as a byte vector.
  virtual void ReadData(const FormatType& format,
                        std::string* result) const = 0;

  // Returns an estimate of the time the clipboard was last updated.  If the
  // time is unknown, returns Time::Time().
  virtual base::Time GetLastModifiedTime() const;

  // Resets the clipboard last modified time to Time::Time().
  virtual void ClearLastModifiedTime();

  // Gets the FormatType corresponding to an arbitrary format string,
  // registering it with the system if needed. Due to Windows/Linux
  // limitiations, |format_string| must never be controlled by the user.
  static FormatType GetFormatType(const std::string& format_string);

  // Get format identifiers for various types.
  static const FormatType& GetUrlFormatType();
  static const FormatType& GetUrlWFormatType();
  static const FormatType& GetMozUrlFormatType();
  static const FormatType& GetPlainTextFormatType();
  static const FormatType& GetPlainTextWFormatType();
  static const FormatType& GetFilenameFormatType();
  static const FormatType& GetFilenameWFormatType();
  static const FormatType& GetWebKitSmartPasteFormatType();
  // Win: MS HTML Format, Other: Generic HTML format
  static const FormatType& GetHtmlFormatType();
  static const FormatType& GetRtfFormatType();
  static const FormatType& GetBitmapFormatType();
  // TODO(raymes): Unify web custom data and pepper custom data:
  // crbug.com/158399.
  static const FormatType& GetWebCustomDataFormatType();
  static const FormatType& GetPepperCustomDataFormatType();

#if defined(OS_WIN)
  // Firefox text/html
  static const FormatType& GetTextHtmlFormatType();
  static const FormatType& GetCFHDropFormatType();
  static const FormatType& GetFileDescriptorFormatType();
  static const FormatType& GetFileContentZeroFormatType();
  static const FormatType& GetIDListFormatType();
#endif

 protected:
  static Clipboard* Create();

  Clipboard() {}
  virtual ~Clipboard() {}

  // ObjectType designates the type of data to be stored in the clipboard. This
  // designation is shared across all OSes. The system-specific designation
  // is defined by FormatType. A single ObjectType might be represented by
  // several system-specific FormatTypes. For example, on Linux the CBF_TEXT
  // ObjectType maps to "text/plain", "STRING", and several other formats. On
  // windows it maps to CF_UNICODETEXT.
  //
  // The order below is the order in which data will be written to the
  // clipboard, so more specific types must be listed before less specific
  // types. For example, placing an image on the clipboard might cause the
  // clipboard to contain a bitmap, HTML markup representing the image, a URL to
  // the image, and the image's alt text. Having the types follow this order
  // maximizes the amount of data that can be extracted by various programs.
  enum ObjectType {
    CBF_SMBITMAP,  // Bitmap from shared memory.
    CBF_HTML,
    CBF_RTF,
    CBF_BOOKMARK,
    CBF_TEXT,
    CBF_WEBKIT,
    CBF_DATA,      // Arbitrary block of bytes.
  };

  // ObjectMap is a map from ObjectType to associated data.
  // The data is organized differently for each ObjectType. The following
  // table summarizes what kind of data is stored for each key.
  // * indicates an optional argument.
  //
  // Key           Arguments    Type
  // -------------------------------------
  // CBF_SMBITMAP  bitmap       A pointer to a SkBitmap. The caller must ensure
  //                            the SkBitmap remains live for the duration of
  //                            the WriteObjects call.
  // CBF_HTML      html         char array
  //               url*         char array
  // CBF_RTF       data         byte array
  // CBF_BOOKMARK  html         char array
  //               url          char array
  // CBF_TEXT      text         char array
  // CBF_WEBKIT    none         empty vector
  // CBF_DATA      format       char array
  //               data         byte array
  using ObjectMapParam = std::vector<char>;
  using ObjectMapParams = std::vector<ObjectMapParam>;
  using ObjectMap = base::flat_map<int /* ObjectType */, ObjectMapParams>;

  // Write a bunch of objects to the system clipboard. Copies are made of the
  // contents of |objects|.
  virtual void WriteObjects(ClipboardType type, const ObjectMap& objects) = 0;

  void DispatchObject(ObjectType type, const ObjectMapParams& params);

  virtual void WriteText(const char* text_data, size_t text_len) = 0;

  virtual void WriteHTML(const char* markup_data,
                         size_t markup_len,
                         const char* url_data,
                         size_t url_len) = 0;

  virtual void WriteRTF(const char* rtf_data, size_t data_len) = 0;

  virtual void WriteBookmark(const char* title_data,
                             size_t title_len,
                             const char* url_data,
                             size_t url_len) = 0;

  virtual void WriteWebSmartPaste() = 0;

  virtual void WriteBitmap(const SkBitmap& bitmap) = 0;

  virtual void WriteData(const FormatType& format,
                         const char* data_data,
                         size_t data_len) = 0;

 private:
  // For access to WriteObjects().
  friend class ForwardingTestingClipboard;
  friend class ScopedClipboardWriter;
  friend class TestClipboard;
  // For SetClipboardForCurrentThread's argument.
  friend struct std::default_delete<Clipboard>;

  static base::PlatformThreadId GetAndValidateThreadID();

  // A list of allowed threads. By default, this is empty and no thread checking
  // is done (in the unit test case), but a user (like content) can set which
  // threads are allowed to call this method.
  using AllowedThreadsVector = std::vector<base::PlatformThreadId>;
  static base::LazyInstance<AllowedThreadsVector>::DestructorAtExit
      allowed_threads_;

  // Mapping from threads to clipboard objects.
  using ClipboardMap =
      base::flat_map<base::PlatformThreadId, std::unique_ptr<Clipboard>>;
  static base::LazyInstance<ClipboardMap>::DestructorAtExit clipboard_map_;

  // Mutex that controls access to |g_clipboard_map|.
  static base::LazyInstance<base::Lock>::Leaky clipboard_map_lock_;

  DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_H_
