// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Many of these functions are based on those found in
// webkit/port/platform/PasteboardWin.cpp

#include "ui/base/clipboard/clipboard_win.h"

#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdint>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/win/message_window.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_hglobal.h"
#include "net/base/filename_util.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ui {

namespace {

// A scoper to impersonate the anonymous token and revert when leaving scope
class AnonymousImpersonator {
 public:
  AnonymousImpersonator() {
    must_revert_ = ::ImpersonateAnonymousToken(::GetCurrentThread());
  }
  AnonymousImpersonator(const AnonymousImpersonator&) = delete;
  AnonymousImpersonator& operator=(const AnonymousImpersonator&) = delete;
  ~AnonymousImpersonator() {
    if (must_revert_)
      ::RevertToSelf();
  }

 private:
  BOOL must_revert_;
};

// A scoper to manage acquiring and automatically releasing the clipboard.
class ScopedClipboard {
 public:
  ScopedClipboard() : opened_(false) { }

  ~ScopedClipboard() {
    if (opened_)
      Release();
  }

  bool Acquire(HWND owner) {
    const int kMaxAttemptsToOpenClipboard = 5;

    if (opened_) {
      NOTREACHED();
      return false;
    }

    // Attempt to open the clipboard, which will acquire the Windows clipboard
    // lock.  This may fail if another process currently holds this lock.
    // We're willing to try a few times in the hopes of acquiring it.
    //
    // This turns out to be an issue when using remote desktop because the
    // rdpclip.exe process likes to read what we've written to the clipboard and
    // send it to the RDP client.  If we open and close the clipboard in quick
    // succession, we might be trying to open it while rdpclip.exe has it open,
    // See Bug 815425.
    //
    // In fact, we believe we'll only spin this loop over remote desktop.  In
    // normal situations, the user is initiating clipboard operations and there
    // shouldn't be contention.

    for (int attempts = 0; attempts < kMaxAttemptsToOpenClipboard; ++attempts) {
      if (::OpenClipboard(owner)) {
        opened_ = true;
        return true;
      }

      // If we didn't manage to open the clipboard, sleep a bit and be hopeful.
      ::Sleep(5);
    }

    // We failed to acquire the clipboard.
    return false;
  }

  void Release() {
    if (opened_) {
      // Impersonate the anonymous token during the call to CloseClipboard
      // This prevents Windows 8+ capturing the broker's access token which
      // could be accessed by lower-privileges chrome processes leading to
      // a risk of EoP
      AnonymousImpersonator impersonator;
      ::CloseClipboard();
      opened_ = false;
    } else {
      NOTREACHED();
    }
  }

 private:
  bool opened_;
};

bool ClipboardOwnerWndProc(UINT message,
                           WPARAM wparam,
                           LPARAM lparam,
                           LRESULT* result) {
  switch (message) {
  case WM_RENDERFORMAT:
    // This message comes when SetClipboardData was sent a null data handle
    // and now it's come time to put the data on the clipboard.
    // We always set data, so there isn't a need to actually do anything here.
    break;
  case WM_RENDERALLFORMATS:
    // This message comes when SetClipboardData was sent a null data handle
    // and now this application is about to quit, so it must put data on
    // the clipboard before it exits.
    // We always set data, so there isn't a need to actually do anything here.
    break;
  case WM_DRAWCLIPBOARD:
    break;
  case WM_DESTROY:
    break;
  case WM_CHANGECBCHAIN:
    break;
  default:
    return false;
  }

  *result = 0;
  return true;
}

template <typename charT>
HGLOBAL CreateGlobalData(const std::basic_string<charT>& str) {
  HGLOBAL data =
    ::GlobalAlloc(GMEM_MOVEABLE, ((str.size() + 1) * sizeof(charT)));
  if (data) {
    charT* raw_data = static_cast<charT*>(::GlobalLock(data));
    memcpy(raw_data, str.data(), str.size() * sizeof(charT));
    raw_data[str.size()] = '\0';
    ::GlobalUnlock(data);
  }
  return data;
}

bool BitmapHasInvalidPremultipliedColors(const SkPixmap& pixmap) {
  for (int x = 0; x < pixmap.width(); ++x) {
    for (int y = 0; y < pixmap.height(); ++y) {
      uint32_t pixel = *pixmap.addr32(x, y);
      if (SkColorGetR(pixel) > SkColorGetA(pixel) ||
          SkColorGetG(pixel) > SkColorGetA(pixel) ||
          SkColorGetB(pixel) > SkColorGetA(pixel))
        return true;
    }
  }
  return false;
}

void MakeBitmapOpaque(SkPixmap* pixmap) {
  for (int x = 0; x < pixmap->width(); ++x) {
    for (int y = 0; y < pixmap->height(); ++y) {
      *pixmap->writable_addr32(x, y) =
          SkColorSetA(*pixmap->addr32(x, y), SK_AlphaOPAQUE);
    }
  }
}

template <typename StringType>
void TrimAfterNull(StringType* result) {
  // Text copied to the clipboard may explicitly contain null characters that
  // should be ignored, depending on the application that does the copying.
  constexpr typename StringType::value_type kNull = 0;
  size_t pos = result->find_first_of(kNull);
  if (pos != StringType::npos)
    result->resize(pos);
}

bool ReadFilenamesAvailable() {
  return ::IsClipboardFormatAvailable(
             ClipboardFormatType::CFHDropType().ToFormatEtc().cfFormat) ||
         ::IsClipboardFormatAvailable(
             ClipboardFormatType::FilenameType().ToFormatEtc().cfFormat) ||
         ::IsClipboardFormatAvailable(
             ClipboardFormatType::FilenameAType().ToFormatEtc().cfFormat);
}

}  // namespace

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardWin;
}

// ClipboardWin implementation.
ClipboardWin::ClipboardWin() {
  if (base::CurrentUIThread::IsSet())
    clipboard_owner_ = std::make_unique<base::win::MessageWindow>();
}

ClipboardWin::~ClipboardWin() {
}

void ClipboardWin::OnPreShutdown() {}

// DataTransferEndpoint is not used on this platform.
DataTransferEndpoint* ClipboardWin::GetSource(ClipboardBuffer buffer) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  return nullptr;
}

const ClipboardSequenceNumberToken& ClipboardWin::GetSequenceNumber(
    ClipboardBuffer buffer) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  DWORD sequence_number = ::GetClipboardSequenceNumber();
  if (sequence_number != clipboard_sequence_.sequence_number) {
    // Generate a unique token associated with the current sequence number.
    clipboard_sequence_ = {sequence_number, ClipboardSequenceNumberToken()};
  }
  return clipboard_sequence_.token;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardWin::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  if (format == ClipboardFormatType::FilenameType())
    return ReadFilenamesAvailable();
  // Chrome can retrieve an image from the clipboard as either a bitmap or PNG.
  if (format == ClipboardFormatType::PngType() ||
      format == ClipboardFormatType::BitmapType()) {
    return ::IsClipboardFormatAvailable(
               ClipboardFormatType::PngType().ToFormatEtc().cfFormat) !=
               FALSE ||
           ::IsClipboardFormatAvailable(
               ClipboardFormatType::BitmapType().ToFormatEtc().cfFormat) !=
               FALSE;
  }

  return ::IsClipboardFormatAvailable(format.ToFormatEtc().cfFormat) != FALSE;
}

void ClipboardWin::Clear(ClipboardBuffer buffer) {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  ::EmptyClipboard();
}

std::vector<std::u16string> ClipboardWin::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  if (::IsClipboardFormatAvailable(
          ClipboardFormatType::PlainTextAType().ToFormatEtc().cfFormat))
    types.push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (::IsClipboardFormatAvailable(
          ClipboardFormatType::HtmlType().ToFormatEtc().cfFormat))
    types.push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (::IsClipboardFormatAvailable(
          ClipboardFormatType::SvgType().ToFormatEtc().cfFormat))
    types.push_back(base::UTF8ToUTF16(kMimeTypeSvg));
  if (::IsClipboardFormatAvailable(
          ClipboardFormatType::RtfType().ToFormatEtc().cfFormat))
    types.push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (::IsClipboardFormatAvailable(CF_DIB))
    types.push_back(base::UTF8ToUTF16(kMimeTypePNG));
  if (ReadFilenamesAvailable())
    types.push_back(base::UTF8ToUTF16(kMimeTypeURIList));
  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(types);

  types->clear();
  *types = GetStandardFormats(buffer, data_dst);

  // Acquire the clipboard to read WebCustomDataType types.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE hdata = ::GetClipboardData(
      ClipboardFormatType::WebCustomDataType().ToFormatEtc().cfFormat);
  if (!hdata)
    return;

  ReadCustomDataTypes(::GlobalLock(hdata), ::GlobalSize(hdata), types);
  ::GlobalUnlock(hdata);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadText(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* result) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
  if (!data)
    return;

  result->assign(static_cast<const char16_t*>(::GlobalLock(data)),
                 ::GlobalSize(data) / sizeof(char16_t));
  ::GlobalUnlock(data);
  TrimAfterNull(result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadAsciiText(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(CF_TEXT);
  if (!data)
    return;

  result->assign(static_cast<const char*>(::GlobalLock(data)),
                 ::GlobalSize(data));
  ::GlobalUnlock(data);
  TrimAfterNull(result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadHTML(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* markup,
                            std::string* src_url,
                            uint32_t* fragment_start,
                            uint32_t* fragment_end) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kHtml);

  markup->clear();
  // TODO(dcheng): Remove these checks, I don't think they should be optional.
  DCHECK(src_url);
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(
      ClipboardFormatType::HtmlType().ToFormatEtc().cfFormat);
  if (!data)
    return;

  std::string cf_html(static_cast<const char*>(::GlobalLock(data)),
                      ::GlobalSize(data));
  ::GlobalUnlock(data);
  TrimAfterNull(&cf_html);

  size_t html_start = std::string::npos;
  size_t start_index = std::string::npos;
  size_t end_index = std::string::npos;
  ClipboardUtil::CFHtmlExtractMetadata(cf_html, src_url, &html_start,
                                       &start_index, &end_index);

  // This might happen if the contents of the clipboard changed and CF_HTML is
  // no longer available.
  if (start_index == std::string::npos ||
      end_index == std::string::npos ||
      html_start == std::string::npos)
    return;

  if (start_index < html_start || end_index < start_index)
    return;

  std::vector<size_t> offsets = {start_index - html_start,
                                 end_index - html_start};
  markup->assign(base::UTF8ToUTF16AndAdjustOffsets(cf_html.data() + html_start,
                                                   &offsets));
  // Ensure the Fragment points within the string; see https://crbug.com/607181.
  size_t end = std::min(offsets[1], markup->length());
  *fragment_start = base::checked_cast<uint32_t>(std::min(offsets[0], end));
  *fragment_end = base::checked_cast<uint32_t>(end);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadSvg(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::u16string* result) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kSvg);

  std::string data;
  ReadData(ClipboardFormatType::SvgType(), data_dst, &data);
  result->assign(reinterpret_cast<const char16_t*>(data.data()),
                 data.size() / sizeof(char16_t));

  TrimAfterNull(result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadRTF(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::string* result) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kRtf);

  ReadData(ClipboardFormatType::RtfType(), data_dst, result);
  TrimAfterNull(result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadPng(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           ReadPngCallback callback) const {
  RecordRead(ClipboardFormatMetric::kPng);
  std::vector<uint8_t> data = ReadPngInternal(buffer);
  // On Windows, PNG and bitmap are separate formats. Read PNG if possible,
  // otherwise fall back to reading as a bitmap.
  if (data.empty()) {
    SkBitmap bitmap = ReadBitmapInternal(buffer);
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                      &data);
  }
  std::move(callback).Run(data);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadCustomData(ClipboardBuffer buffer,
                                  const std::u16string& type,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* result) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kCustomData);

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE hdata = ::GetClipboardData(
      ClipboardFormatType::WebCustomDataType().ToFormatEtc().cfFormat);
  if (!hdata)
    return;

  ReadCustomDataForType(::GlobalLock(hdata), ::GlobalSize(hdata), type, result);
  ::GlobalUnlock(hdata);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadFilenames(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::vector<ui::FileInfo>* result) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  DCHECK(result);
  RecordRead(ClipboardFormatMetric::kFilenames);

  result->clear();
  if (!ReadFilenamesAvailable())
    return;

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  // TODO(crbug.com/1178671): Refactor similar code in clipboard_utils_win:
  // ClipboardUtil::GetFilenames() and reuse rather than duplicate.
  HANDLE data = ::GetClipboardData(
      ClipboardFormatType::CFHDropType().ToFormatEtc().cfFormat);
  if (data) {
    {
      base::win::ScopedHGlobal<HDROP> hdrop(data);
      if (!hdrop.get())
        return;

      const int kMaxFilenameLen = 4096;
      const unsigned num_files = DragQueryFileW(hdrop.get(), 0xffffffff, 0, 0);
      for (unsigned int i = 0; i < num_files; ++i) {
        wchar_t filename[kMaxFilenameLen];
        if (!DragQueryFileW(hdrop.get(), i, filename, kMaxFilenameLen))
          continue;
        base::FilePath path(filename);
        result->push_back(ui::FileInfo(path, base::FilePath()));
      }
    }
    return;
  }

  data = ::GetClipboardData(
      ClipboardFormatType::FilenameType().ToFormatEtc().cfFormat);
  if (data) {
    {
      // filename using Unicode
      base::win::ScopedHGlobal<wchar_t*> filename(data);
      if (filename.get() && filename.get()[0]) {
        base::FilePath path(filename.get());
        result->push_back(ui::FileInfo(path, base::FilePath()));
      }
    }
    return;
  }

  data = ::GetClipboardData(
      ClipboardFormatType::FilenameAType().ToFormatEtc().cfFormat);
  if (data) {
    {
      // filename using ASCII
      base::win::ScopedHGlobal<char*> filename(data);
      if (filename.get() && filename.get()[0]) {
        base::FilePath path(base::SysNativeMBToWide(filename.get()));
        result->push_back(ui::FileInfo(path, base::FilePath()));
      }
    }
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadBookmark(const DataTransferEndpoint* data_dst,
                                std::u16string* title,
                                std::string* url) const {
  RecordRead(ClipboardFormatMetric::kBookmark);
  if (title)
    title->clear();

  if (url)
    url->clear();

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data =
      ::GetClipboardData(ClipboardFormatType::UrlType().ToFormatEtc().cfFormat);
  if (!data)
    return;

  std::u16string bookmark(static_cast<const char16_t*>(::GlobalLock(data)),
                          ::GlobalSize(data) / sizeof(char16_t));
  ::GlobalUnlock(data);
  TrimAfterNull(&bookmark);

  *url = base::UTF16ToUTF8(bookmark);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::ReadData(const ClipboardFormatType& format,
                            const DataTransferEndpoint* data_dst,
                            std::string* result) const {
  RecordRead(ClipboardFormatMetric::kData);
  if (!result) {
    NOTREACHED();
    return;
  }

  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;

  HANDLE data = ::GetClipboardData(format.ToFormatEtc().cfFormat);
  if (!data)
    return;

  result->assign(static_cast<const char*>(::GlobalLock(data)),
                 ::GlobalSize(data));
  ::GlobalUnlock(data);
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardWin::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return;
  ::EmptyClipboard();

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
}

void ClipboardWin::WriteText(const char* text_data, size_t text_len) {
  std::u16string text;
  base::UTF8ToUTF16(text_data, text_len, &text);
  HGLOBAL glob = CreateGlobalData(text);

  WriteToClipboard(ClipboardFormatType::PlainTextType(), glob);
}

void ClipboardWin::WriteHTML(const char* markup_data,
                             size_t markup_len,
                             const char* url_data,
                             size_t url_len) {
  std::string markup(markup_data, markup_len);
  std::string url;

  if (url_len > 0)
    url.assign(url_data, url_len);

  std::string html_fragment = ClipboardUtil::HtmlToCFHtml(
      markup, url, ClipboardContentType::kSanitized);
  HGLOBAL glob = CreateGlobalData(html_fragment);

  WriteToClipboard(ClipboardFormatType::HtmlType(), glob);
}

void ClipboardWin::WriteSvg(const char* markup_data, size_t markup_len) {
  std::u16string markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  HGLOBAL glob = CreateGlobalData(markup);

  WriteToClipboard(ClipboardFormatType::SvgType(), glob);
}

void ClipboardWin::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(ClipboardFormatType::RtfType(), rtf_data, data_len);
}

void ClipboardWin::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  STGMEDIUM storage = ClipboardUtil::CreateStorageForFileNames(filenames);
  if (storage.tymed == TYMED_NULL)
    return;
  WriteToClipboard(ClipboardFormatType::CFHDropType(), storage.hGlobal);
}

void ClipboardWin::WriteBookmark(const char* title_data,
                                 size_t title_len,
                                 const char* url_data,
                                 size_t url_len) {
  // On Windows, CFSTR_INETURLW is expected to only contain the URL & not the
  // title separated by a newline.
  // https://docs.microsoft.com/en-us/windows/win32/shell/clipboard#cfstr_ineturl.
  std::string bookmark(url_data, url_len);
  std::u16string wide_bookmark = base::UTF8ToUTF16(bookmark);
  HGLOBAL glob = CreateGlobalData(wide_bookmark);

  WriteToClipboard(ClipboardFormatType::UrlType(), glob);
}

void ClipboardWin::WriteWebSmartPaste() {
  DCHECK_NE(clipboard_owner_->hwnd(), nullptr);
  ::SetClipboardData(
      ClipboardFormatType::WebKitSmartPasteType().ToFormatEtc().cfFormat,
      nullptr);
}

void ClipboardWin::WriteBitmap(const SkBitmap& bitmap) {
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  // On Windows there are 2 ways of writing a transparent image to the
  // clipboard: Using the DIBV5 format with correct header and format or using
  // the PNG format. Some programs only support one or the other. In particular,
  // Word support for DIBV5 is buggy and PNG format is needed for it. Writing
  // order is also important as some programs will use the first compatible
  // format that is available on the clipboard, and we want Word to choose the
  // PNG format.

  std::vector<unsigned char> png_encoded_bitmap;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                        &png_encoded_bitmap)) {
    HGLOBAL png_hglobal = skia::CreateHGlobalForByteArray(png_encoded_bitmap);
    if (png_hglobal)
      WriteToClipboard(ClipboardFormatType::PngType(), png_hglobal);
  }
  HGLOBAL dibv5_hglobal = skia::CreateDIBV5ImageDataFromN32SkBitmap(bitmap);
  if (dibv5_hglobal)
    WriteToClipboard(ClipboardFormatType::BitmapType(), dibv5_hglobal);
}

void ClipboardWin::WriteData(const ClipboardFormatType& format,
                             const char* data_data,
                             size_t data_len) {
  HGLOBAL hdata = ::GlobalAlloc(GMEM_MOVEABLE, data_len);
  if (!hdata)
    return;

  char* data = static_cast<char*>(::GlobalLock(hdata));
  memcpy(data, data_data, data_len);
  ::GlobalUnlock(data);
  WriteToClipboard(format, hdata);
}

std::vector<uint8_t> ClipboardWin::ReadPngInternal(
    ClipboardBuffer buffer) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return std::vector<uint8_t>();

  HANDLE data =
      ::GetClipboardData(ClipboardFormatType::PngType().ToFormatEtc().cfFormat);

  if (!data)
    return std::vector<uint8_t>();

  std::string result(static_cast<const char*>(::GlobalLock(data)),
                     ::GlobalSize(data));
  ::GlobalUnlock(data);
  return std::vector<uint8_t>(result.begin(), result.end());
}

SkBitmap ClipboardWin::ReadBitmapInternal(ClipboardBuffer buffer) const {
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);

  // Acquire the clipboard.
  ScopedClipboard clipboard;
  if (!clipboard.Acquire(GetClipboardWindow()))
    return SkBitmap();

  // We use a DIB rather than a DDB here since ::GetObject() with the
  // HBITMAP returned from ::GetClipboardData(CF_BITMAP) always reports a color
  // depth of 32bpp.
  BITMAPINFO* bitmap = static_cast<BITMAPINFO*>(::GetClipboardData(CF_DIB));
  if (!bitmap)
    return SkBitmap();
  int color_table_length = 0;

  // Image is too large, and may cause an allocation failure.
  // See https://crbug.com/1164680.
  constexpr size_t kMaxImageSizeBytes = 1 << 27;  // 128 MiB
  size_t image_size_bytes;
  // Estimate the number of bytes per pixel. For images with fewer than one byte
  // pixel we will over-estimate the size. For compressed images we will
  // over-estimate the size, but some overestimating of the storage size is okay
  // and the calculation will be a good estimate of the decompressed size. The
  // reported bug was for uncompressed 32-bit images where this code will give
  // correct results.
  size_t bytes_per_pixel = bitmap->bmiHeader.biBitCount / 8;
  if (bytes_per_pixel == 0)
    bytes_per_pixel = 1;
  // Calculate the size of the bitmap. This is not an exact calculation but that
  // doesn't matter for this purpose. If the calculation overflows then the
  // image is too big. Return an empty image.
  if (!base::CheckMul(
           bitmap->bmiHeader.biWidth,
           base::CheckMul(bitmap->bmiHeader.biHeight, bytes_per_pixel))
           .AssignIfValid(&image_size_bytes))
    return SkBitmap();
  // If the image size is too big then return an empty image.
  if (image_size_bytes > kMaxImageSizeBytes)
    return SkBitmap();

  // For more information on BITMAPINFOHEADER and biBitCount definition,
  // see https://docs.microsoft.com/en-us/windows/win32/wmdm/-bitmapinfoheader
  switch (bitmap->bmiHeader.biBitCount) {
    case 1:
    case 4:
    case 8:
      color_table_length = bitmap->bmiHeader.biClrUsed
                               ? bitmap->bmiHeader.biClrUsed
                               : 1 << bitmap->bmiHeader.biBitCount;
      break;
    case 16:
    case 32:
      if (bitmap->bmiHeader.biCompression == BI_BITFIELDS)
        color_table_length = 3;
      break;
    case 24:
      break;
    default:
      NOTREACHED();
  }
  const void* bitmap_bits = reinterpret_cast<const char*>(bitmap) +
                            bitmap->bmiHeader.biSize +
                            color_table_length * sizeof(RGBQUAD);

  void* dst_bits;
  // dst_hbitmap is freed by the release_proc in skia_bitmap (below)
  base::win::ScopedBitmap dst_hbitmap = skia::CreateHBitmapXRGB8888(
      bitmap->bmiHeader.biWidth, bitmap->bmiHeader.biHeight, 0, &dst_bits);

  {
    base::win::ScopedCreateDC hdc(CreateCompatibleDC(nullptr));
    HBITMAP old_hbitmap =
        static_cast<HBITMAP>(SelectObject(hdc.Get(), dst_hbitmap.get()));
    ::SetDIBitsToDevice(hdc.Get(), 0, 0, bitmap->bmiHeader.biWidth,
                        bitmap->bmiHeader.biHeight, 0, 0, 0,
                        bitmap->bmiHeader.biHeight, bitmap_bits, bitmap,
                        DIB_RGB_COLORS);
    SelectObject(hdc.Get(), old_hbitmap);
  }
  // Windows doesn't really handle alpha channels well in many situations. When
  // the source image is < 32 bpp, we force the bitmap to be opaque. When the
  // source image is 32 bpp, the alpha channel might still contain garbage data.
  // Since Windows uses premultiplied alpha, we scan for instances where
  // (R, G, B) > A. If there are any invalid premultiplied colors in the image,
  // we assume the alpha channel contains garbage and force the bitmap to be
  // opaque as well. This heuristic will fail on a transparent bitmap
  // containing only black pixels...
  SkPixmap device_pixels(SkImageInfo::MakeN32Premul(bitmap->bmiHeader.biWidth,
                                                    bitmap->bmiHeader.biHeight),
                         dst_bits, bitmap->bmiHeader.biWidth * 4);

  {
    bool has_invalid_alpha_channel =
        bitmap->bmiHeader.biBitCount < 32 ||
        BitmapHasInvalidPremultipliedColors(device_pixels);
    if (has_invalid_alpha_channel) {
      MakeBitmapOpaque(&device_pixels);
    }
  }

  SkBitmap skia_bitmap;
  skia_bitmap.installPixels(
      device_pixels.info(), device_pixels.writable_addr(),
      device_pixels.rowBytes(),
      [](void* pixels, void* hbitmap) {
        DeleteObject(static_cast<HBITMAP>(hbitmap));
      },
      dst_hbitmap.release());
  return skia_bitmap;
}

void ClipboardWin::WriteToClipboard(ClipboardFormatType format, HANDLE handle) {
  UINT cf_format = format.ToFormatEtc().cfFormat;
  DCHECK_NE(clipboard_owner_->hwnd(), nullptr);
  if (handle && !::SetClipboardData(cf_format, handle)) {
    DCHECK_NE(GetLastError(),
              static_cast<unsigned long>(ERROR_CLIPBOARD_NOT_OPEN));
    ::GlobalFree(handle);
  }
}

HWND ClipboardWin::GetClipboardWindow() const {
  if (!clipboard_owner_)
    return nullptr;

  if (clipboard_owner_->hwnd() == nullptr)
    clipboard_owner_->Create(base::BindRepeating(&ClipboardOwnerWndProc));

  return clipboard_owner_->hwnd();
}

}  // namespace ui
