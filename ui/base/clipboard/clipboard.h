// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_content_type.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

class GURL;
class SkBitmap;

namespace ui {
class TestClipboard;
class ScopedClipboardWriter;
class DataTransferEndpoint;

// Clipboard:
// - reads from and writes to the system clipboard.
// - specifies an ordering in which to write types to the clipboard
//   (see PortableFormat).
// - is generalized for all targets/operating systems.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) Clipboard
    : public base::ThreadChecker {
 public:
  using ReadAvailableTypesCallback =
      base::OnceCallback<void(std::vector<std::u16string> result)>;
  using ReadTextCallback = base::OnceCallback<void(std::u16string result)>;
  using ReadAsciiTextCallback = base::OnceCallback<void(std::string result)>;
  using ReadHtmlCallback = base::OnceCallback<void(std::u16string markup,
                                                   GURL src_url,
                                                   uint32_t fragment_start,
                                                   uint32_t fragment_end)>;
  using ReadSvgCallback = base::OnceCallback<void(std::u16string result)>;
  using ReadRTFCallback = base::OnceCallback<void(std::string result)>;
  using ReadPngCallback =
      base::OnceCallback<void(const std::vector<uint8_t>& result)>;
  using ReadCustomDataCallback =
      base::OnceCallback<void(std::u16string result)>;
  using ReadFilenamesCallback =
      base::OnceCallback<void(std::vector<ui::FileInfo> result)>;
  using ReadBookmarkCallback =
      base::OnceCallback<void(std::u16string title, GURL url)>;
  using ReadDataCallback = base::OnceCallback<void(std::string result)>;

  Clipboard(const Clipboard&) = delete;
  Clipboard& operator=(const Clipboard&) = delete;

  static bool IsSupportedClipboardBuffer(ClipboardBuffer buffer);

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
  //
  // The return value should not be cached.
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

  // Gets the source of the current clipboard buffer contents.
  virtual const DataTransferEndpoint* GetSource(
      ClipboardBuffer buffer) const = 0;

  // Returns a token which uniquely identifies clipboard state.
  // ClipboardSequenceNumberTokens are used since there may be multiple
  // ui::Clipboard instances that have the same sequence number.
  virtual const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const = 0;

  // Returns all the standard MIME types that are present on the clipboard.
  // The standard MIME types are the formats that are well defined by the
  // Clipboard API
  // spec(https://w3c.github.io/clipboard-apis/#mandatory-data-types-x).
  // Currently we support text/html, text/plain, text/rtf, image/png &
  // text/uri-list.
  // TODO(snianu): Create a more generalized function for standard formats that
  // can be shared by all platforms.
  virtual std::vector<std::u16string> GetStandardFormats(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const = 0;

  // Tests whether the clipboard contains a certain format.
  virtual bool IsFormatAvailable(
      const ClipboardFormatType& format,
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const = 0;

  // Returns whether the clipboard has data that is marked by its originator as
  // confidential. This is available for opt-in checking by the user of this API
  // as confidential information, like passwords, might legitimately need to be
  // manipulated.
  virtual bool IsMarkedByOriginatorAsConfidential() const;

  // Mark the data on the clipboard as being confidential. This isn't
  // implemented for all platforms yet, but this call should be made on every
  // platform so that when it is implemented on other platforms it is picked up.
  virtual void MarkAsConfidential();

  // Clear the clipboard data.
  virtual void Clear(ClipboardBuffer buffer) = 0;

  // TODO(huangdarwin): Rename to ReadAvailablePortableFormatNames().
  // Includes all sanitized types.
  // Also, includes pickled types by splitting them out of the pickled format.
  virtual void ReadAvailableTypes(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  ReadAvailableTypesCallback callback) const;

  // Reads Unicode text from the clipboard, if available.
  virtual void ReadText(ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst,
                        ReadTextCallback callback) const;

  // Reads ASCII text from the clipboard, if available.
  virtual void ReadAsciiText(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             ReadAsciiTextCallback callback) const;

  // Reads HTML from the clipboard, if available. If the HTML fragment requires
  // context to parse, |fragment_start| and |fragment_end| are indexes into
  // markup indicating the beginning and end of the actual fragment. Otherwise,
  // they will contain 0 and markup->size().
  virtual void ReadHTML(ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst,
                        ReadHtmlCallback callback) const;

  // Reads an SVG image from the clipboard, if available.
  virtual void ReadSvg(ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst,
                       ReadSvgCallback callback) const;

  // Reads RTF from the clipboard, if available. Stores the result as a byte
  // vector.
  virtual void ReadRTF(ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst,
                       ReadRTFCallback callback) const;

  // Reads a png from the clipboard, if available.
  virtual void ReadPng(ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst,
                       ReadPngCallback callback) const = 0;

  virtual void ReadCustomData(ClipboardBuffer buffer,
                              const std::u16string& type,
                              const DataTransferEndpoint* data_dst,
                              ReadCustomDataCallback callback) const;

  // Reads filenames from the clipboard, if available.
  virtual void ReadFilenames(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             ReadFilenamesCallback callback) const;

  // Reads a bookmark from the clipboard, if available.
  // |title| or |url| may be null.
  virtual void ReadBookmark(const DataTransferEndpoint* data_dst,
                            ReadBookmarkCallback callback) const;

  // Reads data from the clipboard with the given format type. Stores result
  // as a byte vector.
  virtual void ReadData(const ClipboardFormatType& format,
                        const DataTransferEndpoint* data_dst,
                        ReadDataCallback callback) const;

  // Synchronous reads are deprecated (https://crbug.com/443355). Please use the
  // equivalent functions that take callbacks above.
  virtual void ReadAvailableTypes(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  std::vector<std::u16string>* types) const = 0;
  virtual void ReadText(ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst,
                        std::u16string* result) const = 0;
  virtual void ReadAsciiText(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             std::string* result) const = 0;
  virtual void ReadHTML(ClipboardBuffer buffer,
                        const DataTransferEndpoint* data_dst,
                        std::u16string* markup,
                        std::string* src_url,
                        uint32_t* fragment_start,
                        uint32_t* fragment_end) const = 0;
  virtual void ReadSvg(ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst,
                       std::u16string* result) const = 0;
  virtual void ReadRTF(ClipboardBuffer buffer,
                       const DataTransferEndpoint* data_dst,
                       std::string* result) const = 0;
  virtual void ReadCustomData(ClipboardBuffer buffer,
                              const std::u16string& type,
                              const DataTransferEndpoint* data_dst,
                              std::u16string* result) const = 0;
  virtual void ReadFilenames(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             std::vector<ui::FileInfo>* result) const = 0;
  virtual void ReadBookmark(const DataTransferEndpoint* data_dst,
                            std::u16string* title,
                            std::string* url) const = 0;
  virtual void ReadData(const ClipboardFormatType& format,
                        const DataTransferEndpoint* data_dst,
                        std::string* result) const = 0;

  // Returns an estimate of the time the clipboard was last updated.  If the
  // time is unknown, returns Time::Time().
  virtual base::Time GetLastModifiedTime() const;

  // Resets the clipboard last modified time to Time::Time().
  virtual void ClearLastModifiedTime();

  // Reads the web custom format map (which is in JSON format) from the
  // clipboard if it's available. Parses the JSON string that has the mapping of
  // MIME type to custom format name and fetches the list of custom MIME types.
  // e.g. on Windows, the mapping is represented as "text/html":"Web Custom
  // Format(0-99)".
  std::map<std::string, std::string> ExtractCustomPlatformNames(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const;

  std::vector<std::u16string> ReadAvailableStandardAndCustomFormatNames(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const;

 protected:
  // PortableFormat designates the type of data to be stored in the clipboard.
  // This designation is shared across all OSes. The system-specific designation
  // is defined by ClipboardFormatType. A single PortableFormat might be
  // represented by several system-specific ClipboardFormatTypes. For example,
  // on Linux the kText PortableFormat maps to "text/plain", "STRING", and
  // several other formats. On Windows it maps to CF_UNICODETEXT.
  //
  // The order below is the order in which data will be written to the
  // clipboard, so more specific types must be listed before less specific
  // types. For example, placing an image on the clipboard might cause the
  // clipboard to contain a bitmap, HTML markup representing the image, a URL to
  // the image, and the image's alt text. Having the types follow this order
  // maximizes the amount of data that can be extracted by various programs.
  // Documentation on motivation for format ordering is also available here:
  // https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats#multiple-clipboard-formats
  enum class PortableFormat {
    kBitmap,  // Bitmap from shared memory.
    kHtml,
    kRtf,
    kBookmark,
    kText,
    kWebkit,
    kData,  // Arbitrary block of bytes.
    kSvg,
    kFilenames,
    kWebCustomFormatMap,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    kEncodedDataTransferEndpoint,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  };

  // TODO (https://crbug.com/994928): Rename ObjectMap-related types.
  // ObjectMap is a map from PortableFormat to associated data.
  // The data is organized differently for each PortableFormat. The following
  // table summarizes what kind of data is stored for each key.
  // * indicates an optional argument.
  //
  // Key        Arguments     Type
  // -------------------------------------
  // kBitmap    bitmap             A pointer to a SkBitmap. The caller must
  //                               ensure the SkBitmap remains live for the
  //                               duration of the WritePortableRepresentations
  //                               call.
  // kHtml      html               char array
  //            url*               char array
  // kRtf       data               byte array
  // kFilenames text/uri-list      char array
  // kBookmark  html               char array
  //            url                char array
  // kText      text               char array
  // kWebkit    none               empty vector
  // kData      format             char array
  //            data               byte array
  // kWebCustomFormatMap           char array
  // kEncodedDataTransferEndpoint  char array
  using ObjectMapParam = std::vector<char>;
  struct ObjectMapParams {
    ObjectMapParams(std::vector<ObjectMapParam> data,
                    ClipboardContentType content_type);
    ObjectMapParams(const ObjectMapParams& other);
    ObjectMapParams();
    ~ObjectMapParams();
    std::vector<ObjectMapParam> data;
    ClipboardContentType content_type;
  };
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

  // Write platform & portable formats, in the order of their appearance in
  // `platform_representations` & `ObjectMap`. Also, adds the source of the data
  // to the clipboard, which can be used when we need to restrict the clipboard
  // data between a set of confidential documents. The data source maybe passed
  // as nullptr.
  virtual void WritePortableAndPlatformRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src) = 0;

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

  virtual void WriteUnsanitizedHTML(const char* markup_data,
                                    size_t markup_len,
                                    const char* url_data,
                                    size_t url_len) = 0;

  virtual void WriteSvg(const char* markup_data, size_t markup_len) = 0;

  virtual void WriteRTF(const char* rtf_data, size_t data_len) = 0;

  virtual void WriteFilenames(std::vector<ui::FileInfo> filenames) = 0;

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

#if BUILDFLAG(IS_OZONE)
  // Returns whether the selection buffer is available.  This is true for some
  // Linux platforms.
  virtual bool IsSelectionBufferAvailable() const = 0;
#endif  // BUILDFLAG(IS_OZONE)

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
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_H_
