// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data.h"

#include <objbase.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_hglobal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "url/gurl.h"

namespace ui {

namespace {

const std::vector<DWORD> kStorageMediaTypesForVirtualFiles = {
    TYMED_ISTORAGE,
    TYMED_ISTREAM,
    TYMED_HGLOBAL,
};

class RefCountMockStream : public IStream {
 public:
  RefCountMockStream() = default;
  ~RefCountMockStream() = default;

  ULONG GetRefCount() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ref_count_;
  }

  // Overridden from IUnknown:
  IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (iid == IID_IUnknown || iid == IID_ISequentialStream ||
        iid == IID_IStream) {
      *object = static_cast<IStream*>(this);
      AddRef();
      return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef(void) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ++ref_count_;
  }

  IFACEMETHODIMP_(ULONG) Release(void) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_GT(ref_count_, 0u);
    return --ref_count_;
  }
  // Overridden from ISequentialStream:
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Read,
                             HRESULT(void* pv, ULONG cb, ULONG* pcbRead));

  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Write,
                             HRESULT(void const* pv, ULONG cb, ULONG* pcbW));

  // Overridden from IStream:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetSize,
                             HRESULT(ULARGE_INTEGER));

  MOCK_METHOD4_WITH_CALLTYPE(
      STDMETHODCALLTYPE,
      CopyTo,
      HRESULT(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*));

  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, Commit, HRESULT(DWORD));

  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Revert, HRESULT());

  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             LockRegion,
                             HRESULT(ULARGE_INTEGER, ULARGE_INTEGER, DWORD));

  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             UnlockRegion,
                             HRESULT(ULARGE_INTEGER, ULARGE_INTEGER, DWORD));

  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, Clone, HRESULT(IStream**));

  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Seek,
                             HRESULT(LARGE_INTEGER liDistanceToMove,
                                     DWORD dwOrigin,
                                     ULARGE_INTEGER* lpNewFilePointer));

  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Stat,
                             HRESULT(STATSTG* pStatstg, DWORD grfStatFlag));

 private:
  ULONG ref_count_ = 0u;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

class OSExchangeDataWinTest : public ::testing::Test {
 public:
  OSExchangeDataWinTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void OnGotVirtualFilesAsTempFiles(
      const std::vector<std::pair<base::FilePath, base::FilePath>>&
          filepaths_and_names) {
    on_got_virtual_files_as_temp_files_called_ = true;

    // Clear any previous results and cache a vector of FileInfo objects for
    // verification.
    retrieved_virtual_files_.clear();

    for (const auto& filepath_and_name : filepaths_and_names) {
      retrieved_virtual_files_.push_back(
          FileInfo(filepath_and_name.first, filepath_and_name.second));
    }
  }

 protected:
  class OnGotVirtualFilesAsTempFilesCalledChecker {
   public:
    OnGotVirtualFilesAsTempFilesCalledChecker(OSExchangeDataWinTest* test)
        : test_(test) {
      test_->on_got_virtual_files_as_temp_files_called_ = false;
    }
    ~OnGotVirtualFilesAsTempFilesCalledChecker() {
      EXPECT_TRUE(test_->on_got_virtual_files_as_temp_files_called_);
    }

   private:
    raw_ptr<OSExchangeDataWinTest> test_;
  };

  std::vector<FileInfo> retrieved_virtual_files_;
  base::test::TaskEnvironment task_environment_;
  bool on_got_virtual_files_as_temp_files_called_ = false;
};

// Test getting using the IDataObject COM API
TEST_F(OSExchangeDataWinTest, StringDataAccessViaCOM) {
  OSExchangeData data;
  std::wstring input = L"O hai googlz.";
  data.SetString(base::AsString16(input));
  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));

  FORMATETC format_etc =
      { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&format_etc));

  STGMEDIUM medium;
  EXPECT_EQ(S_OK, com_data->GetData(&format_etc, &medium));
  std::wstring output =
      base::win::ScopedHGlobal<wchar_t*>(medium.hGlobal).data();
  EXPECT_EQ(input, output);
  ReleaseStgMedium(&medium);
}

// Test setting using the IDataObject COM API
TEST_F(OSExchangeDataWinTest, StringDataWritingViaCOM) {
  OSExchangeData data;
  std::wstring input = L"http://www.google.com/";

  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));

  // Store data in the object using the COM SetData API.
  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  STGMEDIUM medium;
  medium.tymed = TYMED_HGLOBAL;
  HGLOBAL glob = GlobalAlloc(GPTR, sizeof(wchar_t) * (input.size() + 1));
  base::win::ScopedHGlobal<wchar_t*> global_lock(glob);
  wchar_t* buffer_handle = global_lock.data();
  wcscpy_s(buffer_handle, input.size() + 1, input.c_str());
  medium.hGlobal = glob;
  medium.pUnkForRelease = NULL;
  EXPECT_EQ(S_OK, com_data->SetData(&format_etc, &medium, TRUE));

  // Construct a new object with the old object so that we can use our access
  // APIs.
  OSExchangeData data2(data.provider().Clone());
  EXPECT_TRUE(data2.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
  std::optional<OSExchangeData::UrlInfo> url_info =
      data2.GetURLAndTitle(FilenameToURLPolicy::CONVERT_FILENAMES);
  ASSERT_TRUE(url_info.has_value());
  GURL reference_url(base::AsStringPiece16(input));
  EXPECT_EQ(reference_url, url_info->url);
}

// Verifies SetData invoked twice with the same data clobbers existing data.
TEST_F(OSExchangeDataWinTest, RemoveData) {
  OSExchangeData data;
  std::wstring input = L"http://www.google.com/";
  std::wstring input2 = L"http://www.google2.com/";

  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));

  // Store data in the object using the COM SetData API.
  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  STGMEDIUM medium;
  medium.tymed = TYMED_HGLOBAL;
  {
    HGLOBAL glob = GlobalAlloc(GPTR, sizeof(wchar_t) * (input.size() + 1));
    base::win::ScopedHGlobal<wchar_t*> global_lock(glob);
    wchar_t* buffer_handle = global_lock.data();
    wcscpy_s(buffer_handle, input.size() + 1, input.c_str());
    medium.hGlobal = glob;
    medium.pUnkForRelease = NULL;
    EXPECT_EQ(S_OK, com_data->SetData(&format_etc, &medium, TRUE));
  }
  // This should clobber the existing data.
  {
    HGLOBAL glob = GlobalAlloc(GPTR, sizeof(wchar_t) * (input2.size() + 1));
    base::win::ScopedHGlobal<wchar_t*> global_lock(glob);
    wchar_t* buffer_handle = global_lock.data();
    wcscpy_s(buffer_handle, input2.size() + 1, input2.c_str());
    medium.hGlobal = glob;
    medium.pUnkForRelease = NULL;
    EXPECT_EQ(S_OK, com_data->SetData(&format_etc, &medium, TRUE));
  }
  EXPECT_EQ(1u, static_cast<DataObjectImpl*>(com_data.Get())->size());

  // Construct a new object with the old object so that we can use our access
  // APIs.
  OSExchangeData data2(data.provider().Clone());
  EXPECT_TRUE(data2.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
  std::optional<OSExchangeData::UrlInfo> url_info =
      data2.GetURLAndTitle(FilenameToURLPolicy::CONVERT_FILENAMES);
  ASSERT_TRUE(url_info.has_value());
  EXPECT_EQ(GURL(base::AsStringPiece16(input2)), url_info->url);
}

TEST_F(OSExchangeDataWinTest, URLDataAccessViaCOM) {
  OSExchangeData data;
  GURL url("http://www.google.com/");
  data.SetURL(url, std::u16string());
  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));

  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&format_etc));

  STGMEDIUM medium;
  EXPECT_EQ(S_OK, com_data->GetData(&format_etc, &medium));
  std::wstring output =
      base::win::ScopedHGlobal<wchar_t*>(medium.hGlobal).data();
  EXPECT_EQ(url.spec(), base::WideToUTF8(output));
  ReleaseStgMedium(&medium);
}

TEST_F(OSExchangeDataWinTest, MultipleFormatsViaCOM) {
  OSExchangeData data;
  std::string url_spec = "http://www.google.com/";
  GURL url(url_spec);
  std::u16string text = u"O hai googlz.";
  data.SetURL(url, u"Google");
  data.SetString(text);

  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));

  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC url_format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&url_format_etc));
  FORMATETC text_format_etc =
      { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&text_format_etc));

  STGMEDIUM medium;
  EXPECT_EQ(S_OK, com_data->GetData(&url_format_etc, &medium));
  std::wstring output_url =
      base::win::ScopedHGlobal<wchar_t*>(medium.hGlobal).data();
  EXPECT_EQ(url.spec(), base::WideToUTF8(output_url));
  ReleaseStgMedium(&medium);

  // The text is supposed to be the raw text of the URL, _NOT_ the value of
  // |text|! This is because the URL is added first and thus takes precedence!
  EXPECT_EQ(S_OK, com_data->GetData(&text_format_etc, &medium));
  std::wstring output_text =
      base::win::ScopedHGlobal<wchar_t*>(medium.hGlobal).data();
  EXPECT_EQ(url_spec, base::WideToUTF8(output_text));
  ReleaseStgMedium(&medium);
}

TEST_F(OSExchangeDataWinTest, EnumerationViaCOM) {
  OSExchangeData data;
  data.SetURL(GURL("http://www.google.com/"), std::u16string());
  data.SetString(u"O hai googlz.");

  CLIPFORMAT cfstr_file_group_descriptor =
      RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
  CLIPFORMAT text_x_moz_url = RegisterClipboardFormat(L"text/x-moz-url");

  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));
  Microsoft::WRL::ComPtr<IEnumFORMATETC> enumerator;
  EXPECT_EQ(S_OK, com_data.Get()->EnumFormatEtc(DATADIR_GET, &enumerator));

  // Test that we can get one item.
  {
    // Explictly don't reset the first time, to verify the creation state is
    // OK.
    ULONG retrieved = 0;
    FORMATETC elements_array[1];
    EXPECT_EQ(S_OK, enumerator->Next(1,
        reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
    EXPECT_EQ(1u, retrieved);
    EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
  }

  // Test that we can get one item with a NULL retrieved value.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    FORMATETC elements_array[1];
    EXPECT_EQ(S_OK, enumerator->Next(1,
        reinterpret_cast<FORMATETC*>(&elements_array), NULL));
    EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
  }

  // Test that we can get two items.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    ULONG retrieved = 0;
    FORMATETC elements_array[2];
    EXPECT_EQ(S_OK, enumerator->Next(2,
        reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
    EXPECT_EQ(2u, retrieved);
    EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
    EXPECT_EQ(cfstr_file_group_descriptor, elements_array[1].cfFormat);
  }

  // Test that we can skip the first item.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    EXPECT_EQ(S_OK, enumerator->Skip(1));
    ULONG retrieved = 0;
    FORMATETC elements_array[1];
    EXPECT_EQ(S_OK, enumerator->Next(1,
        reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
    EXPECT_EQ(1u, retrieved);
    EXPECT_EQ(cfstr_file_group_descriptor, elements_array[0].cfFormat);
  }

  // Test that we can skip the first item, and create a clone that matches in
  // this state, and modify the original without affecting the clone.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    EXPECT_EQ(S_OK, enumerator->Skip(1));
    Microsoft::WRL::ComPtr<IEnumFORMATETC> cloned_enumerator;
    EXPECT_EQ(S_OK, enumerator.Get()->Clone(&cloned_enumerator));
    EXPECT_EQ(S_OK, enumerator.Get()->Reset());

    {
      ULONG retrieved = 0;
      FORMATETC elements_array[1];
      EXPECT_EQ(S_OK, cloned_enumerator->Next(1,
          reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
      EXPECT_EQ(1u, retrieved);
      EXPECT_EQ(cfstr_file_group_descriptor, elements_array[0].cfFormat);
    }

    {
      ULONG retrieved = 0;
      FORMATETC elements_array[1];
      EXPECT_EQ(S_OK, enumerator->Next(1,
          reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
      EXPECT_EQ(1u, retrieved);
      EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
    }
  }
}

TEST_F(OSExchangeDataWinTest, TestURLExchangeFormatsViaCOM) {
  OSExchangeData data;
  std::string url_spec = "http://www.google.com/";
  GURL url(url_spec);
  std::u16string url_title = u"www.google.com";
  data.SetURL(url, url_title);

  // File contents access via COM
  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));
  {
    CLIPFORMAT cfstr_file_contents =
        RegisterClipboardFormat(CFSTR_FILECONTENTS);
    // format_etc.lindex value 0 used for file drop.
    FORMATETC format_etc = {cfstr_file_contents, nullptr, DVASPECT_CONTENT, 0,
                            TYMED_HGLOBAL};
    EXPECT_EQ(S_OK, com_data->QueryGetData(&format_etc));

    STGMEDIUM medium;
    EXPECT_EQ(S_OK, com_data->GetData(&format_etc, &medium));
    base::win::ScopedHGlobal<char*> glob(medium.hGlobal);
    std::string output(glob.data(), glob.size());
    std::string file_contents = "[InternetShortcut]\r\nURL=";
    file_contents += url_spec;
    file_contents += "\r\n";
    EXPECT_EQ(file_contents, output);
    ReleaseStgMedium(&medium);
  }
}

TEST_F(OSExchangeDataWinTest, FileContents) {
  OSExchangeData data;
  std::string file_contents("data\0with\0nulls", 15);
  data.SetFileContents(base::FilePath(L"filename.txt"), file_contents);

  OSExchangeData copy(data.provider().Clone());
  std::optional<OSExchangeData::FileContentsInfo> file_contents_info =
      copy.GetFileContents();
  EXPECT_TRUE(file_contents_info.has_value());
  EXPECT_EQ(L"filename.txt", file_contents_info->filename.value());
  EXPECT_EQ(file_contents, file_contents_info->file_contents);
}

TEST_F(OSExchangeDataWinTest, VirtualFiles) {
  const base::FilePath kPathPlaceholder(FILE_PATH_LITERAL("temp.tmp"));

  const std::vector<std::pair<base::FilePath, std::string>>
      kTestFilenamesAndContents = {
          {base::FilePath(FILE_PATH_LITERAL("filename.txt")),
           std::string("just some data")},
          {base::FilePath(FILE_PATH_LITERAL("another filename.txt")),
           std::string("just some data\0with\0nulls", 25)},
          {base::FilePath(FILE_PATH_LITERAL("and another filename.txt")),
           std::string("just some more data")},
      };

  for (const auto& tymed : kStorageMediaTypesForVirtualFiles) {
    OSExchangeData data;
    data.provider().SetVirtualFileContentsForTesting(kTestFilenamesAndContents,
                                                     tymed);

    OSExchangeData copy(data.provider().Clone());
    std::optional<std::vector<FileInfo>> file_infos =
        copy.GetVirtualFilenames();
    ASSERT_TRUE(file_infos.has_value());
    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < file_infos.value().size(); i++) {
      EXPECT_EQ(kTestFilenamesAndContents[i].first,
                file_infos.value()[i].display_name);
      EXPECT_EQ(kPathPlaceholder, file_infos.value()[i].path);
    }

    base::FilePath temp_dir;
    EXPECT_TRUE(base::GetTempDir(&temp_dir));

    // Callback for GetVirtualFilesAsTempFiles is executed when all virtual
    // files are backed by temp files.
    auto callback =
        base::BindOnce(&OSExchangeDataWinTest::OnGotVirtualFilesAsTempFiles,
                       base::Unretained(this));

    OnGotVirtualFilesAsTempFilesCalledChecker checker(this);
    copy.GetVirtualFilesAsTempFiles(std::move(callback));

    // RunUntilIdle assures all async tasks are run.
    task_environment_.RunUntilIdle();

    EXPECT_EQ(kTestFilenamesAndContents.size(),
              retrieved_virtual_files_.size());
    for (size_t i = 0; i < retrieved_virtual_files_.size(); i++) {
      EXPECT_EQ(kTestFilenamesAndContents[i].first,
                retrieved_virtual_files_[i].display_name);
      // Check if the temp files that back the virtual files are actually
      // created in the temp directory. Need to compare long file paths here
      // because GetTempDir can return a short ("8.3") path if the test is run
      // under a username that is too long.
      EXPECT_EQ(
          base::MakeLongFilePath(temp_dir),
          base::MakeLongFilePath(retrieved_virtual_files_[i].path.DirName()));
      EXPECT_EQ(kTestFilenamesAndContents[i].first.Extension(),
                retrieved_virtual_files_[i].path.Extension());
      std::string read_contents;
      EXPECT_TRUE(base::ReadFileToString(retrieved_virtual_files_[i].path,
                                         &read_contents));
      if (tymed != TYMED_ISTORAGE) {
        EXPECT_EQ(kTestFilenamesAndContents[i].second, read_contents);
      } else {
        // IStorage uses compound files, so temp files won't be flat text files.
        // Just make sure the original contents appears in the compound files.
        EXPECT_TRUE(
            base::Contains(read_contents, kTestFilenamesAndContents[i].second));
      }
    }
  }
}

TEST_F(OSExchangeDataWinTest, VirtualFilesRealFilesPreferred) {
  // Verify that no virtual files retrieved if there is real file data.
  const std::vector<FileInfo> kTestFilenames = {
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file1")),
       base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file2")),
       base::FilePath()},
  };

  const std::vector<std::pair<base::FilePath, std::string>>
      kTestFilenamesAndContents = {
          {base::FilePath(FILE_PATH_LITERAL("filename.txt")),
           std::string("just some data")},
          {base::FilePath(FILE_PATH_LITERAL("another filename.txt")),
           std::string("just some data\0with\0nulls", 25)},
          {base::FilePath(FILE_PATH_LITERAL("and another filename.txt")),
           std::string("just some more data")},
      };

  for (const auto& tymed : kStorageMediaTypesForVirtualFiles) {
    OSExchangeData data;
    data.SetFilenames(kTestFilenames);
    data.provider().SetVirtualFileContentsForTesting(kTestFilenamesAndContents,
                                                     tymed);

    OSExchangeData copy(data.provider().Clone());

    std::optional<std::vector<FileInfo>> real_filenames = copy.GetFilenames();
    ASSERT_TRUE(real_filenames.has_value());
    EXPECT_EQ(kTestFilenames.size(), real_filenames.value().size());
    EXPECT_EQ(kTestFilenames, real_filenames.value());

    std::optional<std::vector<FileInfo>> file_infos =
        copy.GetVirtualFilenames();
    EXPECT_FALSE(file_infos.has_value());

    // Callback for GetVirtualFilesAsTempFiles is executed when all virtual
    // files are backed by temp files.
    auto callback =
        base::BindOnce(&OSExchangeDataWinTest::OnGotVirtualFilesAsTempFiles,
                       base::Unretained(this));

    OnGotVirtualFilesAsTempFilesCalledChecker checker(this);
    copy.GetVirtualFilesAsTempFiles(std::move(callback));

    // RunUntilIdle assures all async tasks are run.
    task_environment_.RunUntilIdle();

    EXPECT_EQ(static_cast<size_t>(0), retrieved_virtual_files_.size());
  }
}

TEST_F(OSExchangeDataWinTest, VirtualFilesDuplicateNames) {
  const std::vector<std::pair<base::FilePath, std::string>>
      kTestFilenamesAndContents = {
          {base::FilePath(FILE_PATH_LITERAL("A (1) (2).txt")),
           std::string("just some data")},
          {base::FilePath(FILE_PATH_LITERAL("A.txt")),
           std::string("just some more data")},
          {base::FilePath(FILE_PATH_LITERAL("A (1).txt")),
           std::string("just some more more data")},
          {base::FilePath(FILE_PATH_LITERAL("A.txt")),
           std::string("just some more more more data")},
      };

  for (const auto& tymed : kStorageMediaTypesForVirtualFiles) {
    OSExchangeData data;
    data.provider().SetVirtualFileContentsForTesting(kTestFilenamesAndContents,
                                                     tymed);

    OSExchangeData copy(data.provider().Clone());
    std::optional<std::vector<FileInfo>> file_infos =
        copy.GetVirtualFilenames();
    ASSERT_TRUE(file_infos.has_value());
    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < file_infos.value().size(); i++) {
      // Check that display name is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            file_infos.value()[j].display_name.value(),
            file_infos.value()[i].display_name.value()));
      }
    }

    base::FilePath temp_dir;
    EXPECT_TRUE(base::GetTempDir(&temp_dir));

    // Callback for GetVirtualFilesAsTempFiles is executed when all virtual
    // files are backed by temp files.
    auto callback =
        base::BindOnce(&OSExchangeDataWinTest::OnGotVirtualFilesAsTempFiles,
                       base::Unretained(this));

    OnGotVirtualFilesAsTempFilesCalledChecker checker(this);
    copy.GetVirtualFilesAsTempFiles(std::move(callback));

    // RunUntilIdle assures all async tasks are run.
    task_environment_.RunUntilIdle();

    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < retrieved_virtual_files_.size(); i++) {
      // Check that display name is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            retrieved_virtual_files_[j].display_name.value(),
            retrieved_virtual_files_[i].display_name.value()));
      }

      // Check that temp file path is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            retrieved_virtual_files_[j].path.value(),
            retrieved_virtual_files_[i].path.value()));
      }

      // Check if the temp files that back the virtual files are actually
      // created in the temp directory. Need to compare long file paths here
      // because GetTempDir can return a short ("8.3") path if the test is run
      // under a username that is too long.
      EXPECT_EQ(
          base::MakeLongFilePath(temp_dir),
          base::MakeLongFilePath(retrieved_virtual_files_[i].path.DirName()));
      EXPECT_EQ(kTestFilenamesAndContents[i].first.Extension(),
                retrieved_virtual_files_[i].path.Extension());
      std::string read_contents;
      EXPECT_TRUE(base::ReadFileToString(retrieved_virtual_files_[i].path,
                                         &read_contents));
      if (tymed != TYMED_ISTORAGE) {
        EXPECT_EQ(kTestFilenamesAndContents[i].second, read_contents);
      } else {
        // IStorage uses compound files, so temp files won't be flat text files.
        // Just make sure the original contents appears in the compound files.
        EXPECT_TRUE(
            base::Contains(read_contents, kTestFilenamesAndContents[i].second));
      }
    }
  }
}  // namespace ui

TEST_F(OSExchangeDataWinTest, VirtualFilesDuplicateNamesCaseInsensitivity) {
  const std::vector<std::pair<base::FilePath, std::string>>
      kTestFilenamesAndContents = {
          {base::FilePath(FILE_PATH_LITERAL("a.txt")),
           std::string("just some data")},
          {base::FilePath(FILE_PATH_LITERAL("B.txt")),
           std::string("just some more data")},
          {base::FilePath(FILE_PATH_LITERAL("A.txt")),
           std::string("just some more more data")},
      };

  for (const auto& tymed : kStorageMediaTypesForVirtualFiles) {
    OSExchangeData data;
    data.provider().SetVirtualFileContentsForTesting(kTestFilenamesAndContents,
                                                     tymed);

    OSExchangeData copy(data.provider().Clone());
    std::optional<std::vector<FileInfo>> file_infos =
        copy.GetVirtualFilenames();
    ASSERT_TRUE(file_infos.has_value());
    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < file_infos.value().size(); i++) {
      // Check that display name is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            file_infos.value()[j].display_name.value(),
            file_infos.value()[i].display_name.value()));
      }
    }

    base::FilePath temp_dir;
    EXPECT_TRUE(base::GetTempDir(&temp_dir));

    // Callback for GetVirtualFilesAsTempFiles is executed when all virtual
    // files are backed by temp files.
    auto callback =
        base::BindOnce(&OSExchangeDataWinTest::OnGotVirtualFilesAsTempFiles,
                       base::Unretained(this));

    OnGotVirtualFilesAsTempFilesCalledChecker checker(this);
    copy.GetVirtualFilesAsTempFiles(std::move(callback));

    // RunUntilIdle assures all async tasks are run.
    task_environment_.RunUntilIdle();

    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < retrieved_virtual_files_.size(); i++) {
      // Check that display name is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            retrieved_virtual_files_[j].display_name.value(),
            retrieved_virtual_files_[i].display_name.value()));
      }

      // Check that temp file path is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            retrieved_virtual_files_[j].path.value(),
            retrieved_virtual_files_[i].path.value()));
      }

      // Check if the temp files that back the virtual files are actually
      // created in the temp directory. Need to compare long file paths here
      // because GetTempDir can return a short ("8.3") path if the test is run
      // under a username that is too long.
      EXPECT_EQ(
          base::MakeLongFilePath(temp_dir),
          base::MakeLongFilePath(retrieved_virtual_files_[i].path.DirName()));
      EXPECT_EQ(kTestFilenamesAndContents[i].first.Extension(),
                retrieved_virtual_files_[i].path.Extension());
      std::string read_contents;
      EXPECT_TRUE(base::ReadFileToString(retrieved_virtual_files_[i].path,
                                         &read_contents));
      if (tymed != TYMED_ISTORAGE) {
        EXPECT_EQ(kTestFilenamesAndContents[i].second, read_contents);
      } else {
        // IStorage uses compound files, so temp files won't be flat text files.
        // Just make sure the original contents appears in the compound files.
        EXPECT_TRUE(
            base::Contains(read_contents, kTestFilenamesAndContents[i].second));
      }
    }
  }
}

TEST_F(OSExchangeDataWinTest, VirtualFilesInvalidAndDuplicateNames) {
  const std::wstring kInvalidFileNameCharacters(
      FILE_PATH_LITERAL("\\/:*?\"<>|"));
  const std::wstring kInvalidFilePathCharacters(FILE_PATH_LITERAL("/*?\"<>|"));
  const base::FilePath kPathWithInvalidFileNameCharacters =
      base::FilePath(kInvalidFileNameCharacters)
          .AddExtension(FILE_PATH_LITERAL("txt"));
  const base::FilePath kEmptyDisplayName(FILE_PATH_LITERAL(""));
  const base::FilePath kMaxPathDisplayName =
      base::FilePath(std::wstring(MAX_PATH - 5, L'a'))
          .AddExtension(FILE_PATH_LITERAL("txt"));

  const std::vector<std::pair<base::FilePath, std::string>>
      kTestFilenamesAndContents = {
          {kPathWithInvalidFileNameCharacters, std::string("just some data")},
          {kPathWithInvalidFileNameCharacters,
           std::string("just some data\0with\0nulls", 25)},
          {// Test that still get a unique name if a previous uniquified
           // name is a duplicate of this one.
           kPathWithInvalidFileNameCharacters.InsertBeforeExtension(
               FILE_PATH_LITERAL(" (1)")),
           std::string("just some more data")},
          // Expect a default display name to be generated ("download" if it
          // matters).
          {kEmptyDisplayName, std::string("data for an empty display name")},
          {kEmptyDisplayName,
           std::string("data for another empty display name")},
          // Expect good behavior if the display name length exceeds MAX_PATH.
          {kMaxPathDisplayName,
           std::string("data for a >MAX_PATH display name")},
          {kMaxPathDisplayName,
           std::string("data for another >MAX_PATH display name")},
      };

  for (const auto& tymed : kStorageMediaTypesForVirtualFiles) {
    OSExchangeData data;
    data.provider().SetVirtualFileContentsForTesting(kTestFilenamesAndContents,
                                                     tymed);

    OSExchangeData copy(data.provider().Clone());
    std::optional<std::vector<FileInfo>> file_infos =
        copy.GetVirtualFilenames();
    ASSERT_TRUE(file_infos.has_value());
    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < file_infos.value().size(); i++) {
      // Check that display name does not contain invalid characters.
      EXPECT_EQ(std::wstring::npos,
                file_infos.value()[i].display_name.value().find_first_of(
                    kInvalidFileNameCharacters));
      // Check that display name is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            file_infos.value()[j].display_name.value(),
            file_infos.value()[i].display_name.value()));
      }
    }

    base::FilePath temp_dir;
    EXPECT_TRUE(base::GetTempDir(&temp_dir));

    // Callback for GetVirtualFilesAsTempFiles is executed when all virtual
    // files are backed by temp files.
    auto callback =
        base::BindOnce(&OSExchangeDataWinTest::OnGotVirtualFilesAsTempFiles,
                       base::Unretained(this));

    OnGotVirtualFilesAsTempFilesCalledChecker checker(this);
    copy.GetVirtualFilesAsTempFiles(std::move(callback));

    // RunUntilIdle assures all async tasks are run.
    task_environment_.RunUntilIdle();

    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < retrieved_virtual_files_.size(); i++) {
      // Check that display name does not contain invalid characters.
      EXPECT_EQ(std::wstring::npos,
                retrieved_virtual_files_[i].display_name.value().find_first_of(
                    kInvalidFileNameCharacters));
      // Check that display name is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            retrieved_virtual_files_[j].display_name.value(),
            retrieved_virtual_files_[i].display_name.value()));
      }
      // Check that temp file path does not contain invalid characters (except
      // for separator).
      EXPECT_EQ(std::wstring::npos,
                retrieved_virtual_files_[i].path.value().find_first_of(
                    kInvalidFilePathCharacters));
      // Check that temp file path is unique.
      for (size_t j = 0; j < i; j++) {
        EXPECT_FALSE(base::FilePath::CompareEqualIgnoreCase(
            retrieved_virtual_files_[j].path.value(),
            retrieved_virtual_files_[i].path.value()));
      }

      // Check if the temp files that back the virtual files are actually
      // created in the temp directory. Need to compare long file paths here
      // because GetTempDir can return a short ("8.3") path if the test is run
      // under a username that is too long.
      EXPECT_EQ(
          base::MakeLongFilePath(temp_dir),
          base::MakeLongFilePath(retrieved_virtual_files_[i].path.DirName()));
      EXPECT_EQ(kTestFilenamesAndContents[i].first.Extension(),
                retrieved_virtual_files_[i].path.Extension());
      std::string read_contents;
      // Ability to read the contents implies a temp file was successfully
      // created on the file system even though the original suggested display
      // name had invalid filename characters.
      EXPECT_TRUE(base::ReadFileToString(retrieved_virtual_files_[i].path,
                                         &read_contents));
      if (tymed != TYMED_ISTORAGE) {
        EXPECT_EQ(kTestFilenamesAndContents[i].second, read_contents);
      } else {
        // IStorage uses compound files, so temp files won't be flat text files.
        // Just make sure the original contents appears in the compound files.
        EXPECT_TRUE(
            base::Contains(read_contents, kTestFilenamesAndContents[i].second));
      }
    }
  }
}

TEST_F(OSExchangeDataWinTest, VirtualFilesEmptyContents) {
  const std::vector<std::pair<base::FilePath, std::string>>
      kTestFilenamesAndContents = {
          {base::FilePath(FILE_PATH_LITERAL("file_with_no_contents.txt")),
           std::string()},
      };

  for (const auto& tymed : kStorageMediaTypesForVirtualFiles) {
    OSExchangeData data;
    data.provider().SetVirtualFileContentsForTesting(kTestFilenamesAndContents,
                                                     tymed);

    OSExchangeData copy(data.provider().Clone());
    std::optional<std::vector<FileInfo>> file_infos =
        copy.GetVirtualFilenames();
    ASSERT_TRUE(file_infos.has_value());
    EXPECT_EQ(kTestFilenamesAndContents.size(), file_infos.value().size());
    for (size_t i = 0; i < file_infos.value().size(); i++) {
      EXPECT_EQ(kTestFilenamesAndContents[i].first,
                file_infos.value()[i].display_name);
    }

    base::FilePath temp_dir;
    EXPECT_TRUE(base::GetTempDir(&temp_dir));

    // Callback for GetVirtualFilesAsTempFiles is executed when all virtual
    // files are backed by temp files.
    auto callback =
        base::BindOnce(&OSExchangeDataWinTest::OnGotVirtualFilesAsTempFiles,
                       base::Unretained(this));

    OnGotVirtualFilesAsTempFilesCalledChecker checker(this);
    copy.GetVirtualFilesAsTempFiles(std::move(callback));

    // RunUntilIdle assures all async tasks are run.
    task_environment_.RunUntilIdle();

    EXPECT_EQ(kTestFilenamesAndContents.size(),
              retrieved_virtual_files_.size());
    for (size_t i = 0; i < retrieved_virtual_files_.size(); i++) {
      EXPECT_EQ(kTestFilenamesAndContents[i].first,
                retrieved_virtual_files_[i].display_name);

      // Check if the temp files that back the virtual files are actually
      // created in the temp directory. Need to compare long file paths here
      // because GetTempDir can return a short ("8.3") path if the test is run
      // under a username that is too long.
      EXPECT_EQ(
          base::MakeLongFilePath(temp_dir),
          base::MakeLongFilePath(retrieved_virtual_files_[i].path.DirName()));
      EXPECT_EQ(kTestFilenamesAndContents[i].first.Extension(),
                retrieved_virtual_files_[i].path.Extension());
      std::string read_contents;
      EXPECT_TRUE(base::ReadFileToString(retrieved_virtual_files_[i].path,
                                         &read_contents));
      // IStorage uses compound files, so temp files won't be flat text files.
      // Just make sure the original contents appear in the compound files.
      if (tymed != TYMED_ISTORAGE) {
        EXPECT_EQ(kTestFilenamesAndContents[i].second, read_contents);
        EXPECT_EQ(static_cast<size_t>(0), read_contents.length());
      }
    }
  }
}

TEST_F(OSExchangeDataWinTest, CFHtml) {
  OSExchangeData data;
  GURL url("http://www.google.com/");
  std::u16string html(
      u"<HTML>\n<BODY>\n"
      u"<b>bold.</b> <i><b>This is bold italic.</b></i>\n"
      u"</BODY>\n</HTML>");
  data.SetHtml(html, url);

  // Check the CF_HTML too.
  std::string expected_cf_html(
      "Version:0.9\r\nStartHTML:0000000139\r\nEndHTML:0000000288\r\n"
      "StartFragment:0000000175\r\nEndFragment:0000000252\r\n"
      "SourceURL:http://www.google.com/\r\n<html>\r\n<body>\r\n"
      "<!--StartFragment-->");
  expected_cf_html += base::UTF16ToUTF8(html);
  expected_cf_html.append("<!--EndFragment-->\r\n</body>\r\n</html>");

  FORMATETC format = ClipboardFormatType::HtmlType().ToFormatEtc();
  STGMEDIUM medium;
  IDataObject* data_object = OSExchangeDataProviderWin::GetIDataObject(data);
  EXPECT_EQ(S_OK, data_object->GetData(&format, &medium));
  base::win::ScopedHGlobal<char*> glob(medium.hGlobal);
  std::string output(glob.data(), glob.size());
  EXPECT_EQ(expected_cf_html, output);
  ReleaseStgMedium(&medium);
}

TEST_F(OSExchangeDataWinTest, SetURLWithMaxPath) {
  OSExchangeData data;
  std::u16string long_title(MAX_PATH + 1, u'a');
  data.SetURL(GURL("http://google.com"), long_title);
}

TEST_F(OSExchangeDataWinTest, ProvideURLForPlainTextURL) {
  OSExchangeData data;
  data.SetString(u"http://google.com");

  OSExchangeData data2(data.provider().Clone());
  ASSERT_TRUE(data2.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
  std::optional<OSExchangeData::UrlInfo> url_info =
      data2.GetURLAndTitle(FilenameToURLPolicy::CONVERT_FILENAMES);
  ASSERT_TRUE(url_info.has_value());
  EXPECT_EQ(GURL("http://google.com"), url_info->url);
}

class MockDownloadFileProvider : public DownloadFileProvider {
 public:
  MockDownloadFileProvider() = default;
  ~MockDownloadFileProvider() override = default;
  base::WeakPtr<MockDownloadFileProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD1(Start, void(DownloadFileObserver* observer));
  MOCK_METHOD0(Wait, bool());
  MOCK_METHOD0(Stop, void());

 private:
  base::WeakPtrFactory<MockDownloadFileProvider> weak_ptr_factory_{this};
};

// Verifies that DataObjectImpl::OnDownloadCompleted() doesn't delete
// the DownloadFileProvider instance.
TEST_F(OSExchangeDataWinTest, OnDownloadCompleted) {
  OSExchangeData data;
  Microsoft::WRL::ComPtr<IDataObject> com_data(
      OSExchangeDataProviderWin::GetIDataObject(data));

  OSExchangeDataProviderWin provider(com_data.Get());

  auto download_file_provider = std::make_unique<MockDownloadFileProvider>();
  auto weak_ptr = download_file_provider->GetWeakPtr();
  DownloadFileInfo file_info(
      base::FilePath(FILE_PATH_LITERAL("file_with_no_contents.txt")),
      std::move(download_file_provider));
  provider.SetDownloadFileInfo(&file_info);

  OSExchangeDataProviderWin::GetDataObjectImpl(data)->OnDownloadCompleted(
      base::FilePath());
  EXPECT_TRUE(weak_ptr);
}

// Verifies the data set by DataObjectImpl::SetData with |fRelease| is released
// correctly after the DataObjectImpl instance is destroyed.
TEST_F(OSExchangeDataWinTest, SetDataRelease) {
  RefCountMockStream stream;

  ASSERT_EQ(stream.AddRef(), 1u);
  {
    OSExchangeDataProviderWin data_provider;
    IDataObject* data_object = data_provider.data_object();

    ClipboardFormatType format(
        /* cfFormat= */ CF_TEXT, /* lindex= */ -1, /* tymed= */ TYMED_ISTREAM);
    FORMATETC format_etc = format.ToFormatEtc();

    STGMEDIUM medium;
    medium.tymed = TYMED_ISTREAM;
    medium.pstm = &stream;
    medium.pUnkForRelease = nullptr;

    // |stream| should be released when |data_object| is destroyed since it
    // takes responsibility to release |stream| after used.
    EXPECT_EQ(S_OK,
              data_object->SetData(&format_etc, &medium, /* fRelease= */ TRUE));
    ASSERT_EQ(stream.GetRefCount(), 1u);
  }

  EXPECT_EQ(stream.GetRefCount(), 0u);
}

// Verifies the data duplicated by DataObjectImpl::SetData without |fRelease|
// is released correctly after the DataObjectImpl instance destroyed.
TEST_F(OSExchangeDataWinTest, SetDataNoRelease) {
  RefCountMockStream stream;

  ASSERT_EQ(stream.GetRefCount(), 0u);
  {
    OSExchangeDataProviderWin data_provider;
    IDataObject* data_object = data_provider.data_object();

    ClipboardFormatType format(
        /* cfFormat= */ CF_TEXT, /* lindex= */ -1, /* tymed= */ TYMED_ISTREAM);
    FORMATETC format_etc = format.ToFormatEtc();

    STGMEDIUM medium;
    medium.tymed = TYMED_ISTREAM;
    medium.pstm = &stream;
    medium.pUnkForRelease = nullptr;

    EXPECT_EQ(S_OK, data_object->SetData(&format_etc, &medium,
                                         /* fRelease= */ FALSE));
    ASSERT_EQ(stream.GetRefCount(), 1u);
  }

  // Reference count should be the same as before if |data_object| is
  // destroyed.
  EXPECT_EQ(stream.GetRefCount(), 0u);
}

}  // namespace ui
