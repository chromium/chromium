// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_win.h"

#include <shlobj.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/win/scoped_hglobal.h"
#include "base/win/shlwapi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace ui {
namespace {

// Mock IDataObject that simulates ZIP Shell Folder behavior:
// - Advertises CF_HDROP via QueryGetData (returns S_OK)
// - Fails on GetData for CF_HDROP (returns DV_E_FORMATETC)
// - Successfully returns virtual file data (FILEDESCRIPTOR + FILECONTENTS)
class MockZipShellFolderDataObject
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDataObject> {
 public:
  MockZipShellFolderDataObject() = default;

  IFACEMETHODIMP GetData(FORMATETC* format_etc, STGMEDIUM* medium) override {
    if (!format_etc || !medium) {
      return E_INVALIDARG;
    }

    if (format_etc->cfFormat == CF_HDROP) {
      return DV_E_FORMATETC;
    }

    if (format_etc->cfFormat ==
        ClipboardFormatType::FileContentAtIndexType(0).ToFormatEtc().cfFormat) {
      return GetFileContents(format_etc->lindex, medium);
    }

    return DV_E_FORMATETC;
  }

  IFACEMETHODIMP QueryGetData(FORMATETC* format_etc) override {
    if (!format_etc) {
      return E_INVALIDARG;
    }

    if (format_etc->cfFormat == CF_HDROP ||
        format_etc->cfFormat ==
            ClipboardFormatType::FileDescriptorType().ToFormatEtc().cfFormat ||
        format_etc->cfFormat == ClipboardFormatType::FileContentAtIndexType(0)
                                    .ToFormatEtc()
                                    .cfFormat) {
      return S_OK;
    }

    return DV_E_FORMATETC;
  }

  IFACEMETHODIMP GetDataHere(FORMATETC*, STGMEDIUM*) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP SetData(FORMATETC*, STGMEDIUM*, BOOL) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP EnumFormatEtc(DWORD, IEnumFORMATETC**) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP DUnadvise(DWORD) override { return E_NOTIMPL; }

  IFACEMETHODIMP EnumDAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }

 private:
  HRESULT GetFileContents(LONG index, STGMEDIUM* medium) {
    if (index != 0) {
      return DV_E_LINDEX;
    }

    // Return as TYMED_ISTREAM (like real virtual files)
    medium->pstm = ::SHCreateMemStream(
        reinterpret_cast<const BYTE*>(kVirtualFileContents_.data()),
        static_cast<UINT>(kVirtualFileContents_.size()));
    if (!medium->pstm) {
      return E_OUTOFMEMORY;
    }

    // Move stream pointer to end (as per IDataObject contract)
    const LARGE_INTEGER kZero = {};
    medium->pstm->Seek(kZero, STREAM_SEEK_END, nullptr);
    medium->tymed = TYMED_ISTREAM;
    medium->pUnkForRelease = nullptr;
    return S_OK;
  }

  const std::string kVirtualFileContents_ = "Virtual file contents from ZIP";
};

using ClipboardUtilWinTest = PlatformTest;

TEST_F(ClipboardUtilWinTest, EmptyHtmlToCFHtml) {
  const std::string result_cfhtml =
      clipboard_util::HtmlToCFHtml(std::string(), "www.example.com");
  EXPECT_TRUE(result_cfhtml.empty());
  EXPECT_TRUE(result_cfhtml.empty());
}

TEST_F(ClipboardUtilWinTest, ConversionFromWellFormedHtmlToCFHtml) {
  const std::string well_formed_html =
      "<html><head><style>p {color:blue}</style></head><body><p>Hello "
      "World</p></body></html>";
  const std::string url = "www.example.com";
  const std::string expected_cfhtml =
      "Version:0.9\r\n"
      "StartHTML:0000000132\r\n"
      "EndHTML:0000000290\r\n"
      "StartFragment:0000000168\r\n"
      "EndFragment:0000000254\r\n"
      "SourceURL:" +
      url +
      "\r\n"
      "<html>\r\n"
      "<body>\r\n"
      "<!--StartFragment-->" +
      well_formed_html + "<!--EndFragment-->" + "\r\n</body>\r\n</html>";
  const std::string actual_cfhtml =
      clipboard_util::HtmlToCFHtml(well_formed_html, url);
  EXPECT_EQ(expected_cfhtml, actual_cfhtml);
}

// Test the behavior of Windows ZIP Shell Folder, which advertises CF_HDROP but
// cannot actually render real file paths - only virtual file contents.
TEST_F(ClipboardUtilWinTest,
       HasVirtualFilenamesWithAdvertisedButFailingCFHDrop) {
  Microsoft::WRL::ComPtr<IDataObject> data_object =
      Microsoft::WRL::Make<MockZipShellFolderDataObject>();

  // Verify the mock behaves like ZIP Shell Folder:
  FORMATETC cf_hdrop = ClipboardFormatType::CFHDropType().ToFormatEtc();
  EXPECT_EQ(S_OK, data_object->QueryGetData(&cf_hdrop));
  STGMEDIUM medium;
  EXPECT_NE(S_OK, data_object->GetData(&cf_hdrop, &medium));

  // Verify HasVirtualFilenames() returns true despite CF_HDROP being
  // advertised but failing on GetData
  EXPECT_TRUE(clipboard_util::HasFilenames(data_object.Get()));
  EXPECT_FALSE(clipboard_util::HasRealFiles(data_object.Get()));
  EXPECT_TRUE(clipboard_util::HasVirtualFilenames(data_object.Get()));
}
}  // namespace
}  // namespace ui