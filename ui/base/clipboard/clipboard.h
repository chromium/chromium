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
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"

class SkBitmap;

namespace ui {
class TestClipboard;
class ScopedClipboardWriter;

// Clipboard:
// - reads from and writes to the system clipboard.
// - specifies an ordering in which to write types to the clipboard
//   (see PortableFormat).
// - is generalized for all targets/operating systems.
class COMPONENT_EXPORT(BASE_CLIPBOARD) Clipboard : public base::ThreadChecker {
 public:
  static bool IsSupportedClipboardBuffer(ClipboardBuffer buffer) {
    switch (buffer) {
      case ClipboardBuffer::kCopyPaste:
        return true;
      case ClipboardBuffer::kSelection:
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
        return true;
#else
        return false;
#endif
      case ClipboardBuffer::kDrag:
        return false;
    }
    NOTREACHED();
  }

  // Sets the list of threads that are allowed to access the clipboard.
  static void SetAllowedThreads(
      const std::vector<base::PlatformThreadId>& allowed_threads);

  // Sets the clipboard for the current thread, and take ownership of
  // |platform_clipboard|.
  // TODO(huangdarwin): In the past, mus allowed >1 clipboard implementation per
  // platform. Now that mus is removed, only 1 clipboard implementation exists
  // per platform. Evaluate whether we can or should remove functions like
  // SetClipboardForCurrentThread, as only one clipboard should exist now.
  static void SetClipboardForCurrentThread(
      std::unique_ptr<Clipboard> platform_clipboard);

  // Returns the clipboard object for the current thread.
  //
  // Most implementations will have at most one clipboard which will live on
  // the main UI thread, but Windows has tricky semantics where there have to
  // be two clipboards: one that lives on the UI thread and one that lives on
  // the IO thread.
  static Clipboard* GetForCurrentThread();

  // Removes and transfers ownership of the current thread's clipboard to the
  // caller. If the clipboard was never initialized, returns nullptr.
  static std::unique_ptr<Clipboard> TakeForCurrentThread();

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
  virtual uint64_t GetSequenceNumber(ClipboardBuffer buffer) const = 0;

  // Tests whether the clipboard contains a certain format
  virtual bool IsFormatAvailable(const ClipboardFormatType& format,
                                 ClipboardBuffer buffer) const = 0;

  // Clear the clipboard data.
  virtual void Clear(ClipboardBuffer buffer) = 0;

  virtual void ReadAvailableTypes(ClipboardBuffer buffer,
                                  std::vector<base::string16>* types,
                                  bool* contains_filenames) const = 0;

  // Reads Unicode text from the clipboard, if available.
  virtual void ReadText(ClipboardBuffer buffer,
                        base::string16* result) const = 0;

  // Reads ASCII text from the clipboard, if available.
  virtual void ReadAsciiText(ClipboardBuffer buffer,
                             std::string* result) const = 0;

  // Reads HTML from the clipboard, if available. If the HTML fragment requires
  // context to parse, |fragment_start| and |fragment_end| are indexes into
  // markup indicating the beginning and end of the actual fragment. Otherwise,
  // they will contain 0 and markup->size().
  virtual void ReadHTML(ClipboardBuffer buffer,
                        base::string16* markup,
                        std::string* src_url,
                        uint32_t* fragment_start,
                        uint32_t* fragment_end) const = 0;

  // Reads RTF from the clipboard, if available. Stores the result as a byte
  // vector.
  virtual void ReadRTF(ClipboardBuffer buffer, std::string* result) const = 0;

  // Reads an image from the clipboard, if available.
  virtual SkBitmap ReadImage(ClipboardBuffer buffer) const = 0;

  virtual void ReadCustomData(ClipboardBuffer buffer,
                              const base::string16& type,
                              base::string16* result) const = 0;

  // Reads a bookmark from the clipboard, if available.
  // |title| or |url| may be null.
  virtual void ReadBookmark(base::string16* title, std::string* url) const = 0;

  // Reads raw data from the clipboard with the given format type. Stores result
  // as a byte vector.
  virtual void ReadData(const ClipboardFormatType& format,
                        std::string* result) const = 0;

  // Returns an estimate of the time the clipboard was last updated.  If the
  // time is unknown, returns Time::Time().
  virtual base::Time GetLastModifiedTime() const;

  // Resets the clipboard last modified time to Time::Time().
  virtual void ClearLastModifiedTime();

 protected:
  // PortableFormat designates the type of data to be stored in the clipboard.
  // This designation is shared across all OSes. The system-specific designation
  // is defined by ClipboardFormatType. A single PortableFormat might be
  // represented by several system-specific ClipboardFormatTypes. For example,
  // on Linux the kText PortableFormat maps to "text/plain", "STRING", and
  // several other formats. On windows it maps to CF_UNICODETEXT.
  //
  // The order below is the order in which data will be written to the
  // clipboard, so more specific types must be listed before less specific
  // types. For example, placing an image on the clipboard might cause the
  // clipboard to contain a bitmap, HTML markup representing the image, a URL to
  // the image, and the image's alt text. Having the types follow this order
  // maximizes the amount of data that can be extracted by various programs.
  enum class PortableFormat {
    kBitmap,  // Bitmap from shared memory.
    kHtml,
    kRtf,
    kBookmark,
    kText,
    kWebkit,
    kData,  // Arbitrary block of bytes.
  };

  // TODO (https://crbug.com/994928): Rename ObjectMap-related types.
  // ObjectMap is a map from PortableFormat to associated data.
  // The data is organized differently for each PortableFormat. The following
  // table summarizes what kind of data is stored for each key.
  // * indicates an optional argument.
  //
  // Key        Arguments    Type
  // -------------------------------------
  // kBitmap    bitmap       A pointer to a SkBitmap. The caller must ensure
  //                         the SkBitmap remains live for the duration of
  //                         the WritePortableRepresentations call.
  // kHtml      html         char array
  //            url*         char array
  // kRtf       data         byte array
  // kBookmark  html         char array
  //            url          char array
  // kText      text         char array
  // kWebkit    none         empty vector
  // kData      format       char array
  //            data         byte array
  using ObjectMapParam = std::vector<char>;
  using ObjectMapParams = std::vector<ObjectMapParam>;
  using ObjectMap = base::flat_map<PortableFormat, ObjectMapParams>;

  // PlatformRepresentation is used for DispatchPlatformRepresentations, and
  // supports writing directly to the system clipboard, without custom type
  // mapping per platform.
  struct PlatformRepresentation {
    std::string format;
    // BigBuffer shared memory is still writable from the renderer when backed
    // by shared memory, so PlatformRepresentation's data.data() must not be
    // branched on, and *data.data() must not be accessed, except to copy it
    // into private memory.
    mojo_base::BigBuffer data;
  };

  static Clipboard* Create();

  Clipboard();
  virtual ~Clipboard();

  // Write a bunch of objects to the system clipboard. Copies are made of the
  // contents of |objects|.
  virtual void WritePortableRepresentations(ClipboardBuffer buffer,
                                            const ObjectMap& objects) = 0;
  // Write |platform_representations|, in the order of their appearance in
  // |platform_representations|.
  virtual void WritePlatformRepresentations(
      ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation>
          platform_representations) = 0;

  void DispatchPortableRepresentation(PortableFormat format,
                                      const ObjectMapParams& params);

  // Write directly to the system clipboard.
  void DispatchPlatformRepresentations(
      std::vector<Clipboard::PlatformRepresentation> platform_representations);

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

  // |data_data| is shared memory, and is still writable from the renderer.
  // Therefore, |data_data| must not be branched on, and *|data_data| must not
  // be accessed, except to copy it into private memory.
  virtual void WriteData(const ClipboardFormatType& format,
                         const char* data_data,
                         size_t data_len) = 0;

 private:
  // For access to WritePortableRepresentations().
  friend class ForwardingTestingClipboard;
  friend class ScopedClipboardWriter;
  friend class TestClipboard;
  // For SetClipboardForCurrentThread's argument.
  friend struct std::default_delete<Clipboard>;

  static base::PlatformThreadId GetAndValidateThreadID();

  // A list of allowed threads. By default, this is empty and no thread checking
  // is done (in the unit test case), but a user (like content) can set which
  // threads are allowed to call this method.
  static std::vector<base::PlatformThreadId>& AllowedThreads();

  // Mapping from threads to clipboard objects.
  using ClipboardMap =
      base::flat_map<base::PlatformThreadId, std::unique_ptr<Clipboard>>;
  static ClipboardMap* ClipboardMapPtr();

  // Mutex that controls access to |g_clipboard_map|.
  static base::Lock& ClipboardMapLock();

  DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_H_
