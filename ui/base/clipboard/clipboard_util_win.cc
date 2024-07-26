// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/clipboard/clipboard_util_win.h"

#include <shellapi.h>
#include <wininet.h>  // For INTERNET_MAX_URL_LENGTH.
#include <wrl/client.h>

#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_hglobal.h"
#include "base/win/shlwapi.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "url/gurl.h"

namespace ui {

namespace {

constexpr STGMEDIUM kNullStorageMedium = {.tymed = TYMED_NULL,
                                          .pUnkForRelease = nullptr};

bool HasData(IDataObject* data_object, const ClipboardFormatType& format) {
  FORMATETC format_etc = format.ToFormatEtc();
  return SUCCEEDED(data_object->QueryGetData(&format_etc));
}

bool GetData(IDataObject* data_object,
             const ClipboardFormatType& format,
             STGMEDIUM* medium) {
  FORMATETC format_etc = format.ToFormatEtc();
  return SUCCEEDED(data_object->GetData(&format_etc, medium));
}

bool GetUrlFromHDrop(IDataObject* data_object,
                     GURL* url,
                     std::u16string* title) {
  DCHECK(data_object && url && title);

  bool success = false;
  STGMEDIUM medium;
  if (!GetData(data_object, ClipboardFormatType::CFHDropType(), &medium))
    return false;

  {
    base::win::ScopedHGlobal<HDROP> hdrop(medium.hGlobal);

    if (!hdrop.data()) {
      return false;
    }

    wchar_t filename[MAX_PATH];
    if (DragQueryFileW(hdrop.data(), 0, filename, std::size(filename))) {
      wchar_t url_buffer[INTERNET_MAX_URL_LENGTH];
      if (0 == _wcsicmp(PathFindExtensionW(filename), L".url") &&
          GetPrivateProfileStringW(L"InternetShortcut", L"url", 0, url_buffer,
                                   std::size(url_buffer), filename)) {
        *url = GURL(base::AsStringPiece16(url_buffer));
        PathRemoveExtension(filename);
        title->assign(base::as_u16cstr(PathFindFileName(filename)));
        success = url->is_valid();
      }
    }
  }

  ReleaseStgMedium(&medium);
  return success;
}

void SplitUrlAndTitle(const std::u16string& str,
                      GURL* url,
                      std::u16string* title) {
  DCHECK(url && title);
  size_t newline_pos = str.find('\n');
  if (newline_pos != std::u16string::npos) {
    *url = GURL(std::u16string(str, 0, newline_pos));
    title->assign(str, newline_pos + 1, std::u16string::npos);
  } else {
    *url = GURL(str);
    title->assign(str);
  }
}

// Performs a case-insensitive search for a file path in a vector of existing
// filepaths. Case-insensivity is needed for file systems such as Windows where
// A.txt and a.txt are considered the same file name.
bool ContainsFilePathCaseInsensitive(
    const std::vector<base::FilePath>& existing_filenames,
    const base::FilePath& candidate_path) {
  return base::ranges::any_of(existing_filenames,
                              [&candidate_path](const base::FilePath& elem) {
                                return base::FilePath::CompareEqualIgnoreCase(
                                    elem.value(), candidate_path.value());
                              });
}

// Returns a unique display name for a virtual file, as it is possible that the
// filenames found in the file group descriptor are not unique (e.g. multiple
// emails with the same subject line are dragged out of Outlook.exe).
// |uniquifier| is incremented on encountering a non-unique file name.
base::FilePath GetUniqueVirtualFilename(
    const std::wstring& candidate_name,
    const std::vector<base::FilePath>& existing_filenames,
    unsigned int* uniquifier) {
  // Remove any possible filepath components/separators that drag source may
  // have included in virtual file name.
  base::FilePath unique_name = base::FilePath(candidate_name).BaseName();

  // To mitigate against running up against MAX_PATH limitations (temp files
  // failing to be created), truncate the display name.
  const size_t kTruncatedDisplayNameLength = 128;
  const std::wstring extension = unique_name.Extension();
  unique_name = unique_name.RemoveExtension();
  std::wstring truncated = unique_name.value();
  if (truncated.length() > kTruncatedDisplayNameLength) {
    truncated.erase(kTruncatedDisplayNameLength);
    unique_name = base::FilePath(truncated);
  }
  unique_name = unique_name.AddExtension(extension);

  // Replace any file name illegal characters.
  unique_name = net::GenerateFileName(GURL(), std::string(), std::string(),
                                      base::WideToUTF8(unique_name.value()),
                                      std::string(), std::string());

  // Make the file name unique. This is more involved than just marching through
  // |existing_filenames|, finding the first match, uniquifying, then breaking
  // out of the loop. For example, consider an array of candidate display names
  // {"A (1) (2)", "A", "A (1) ", "A"}. In the first three iterations of the
  // outer loop in GetVirtualFilenames, the candidate names are already unique
  // and so simply pushed to the vector of |filenames|. On the fourth iteration
  // of the outer loop and second iteration of the inner loop (that in
  // GetUniqueVirtualFilename), the duplicate name is encountered and the fourth
  // item is tentatively uniquified to "A (1)". If this inner loop were exited
  // now, the final |filenames| would be {"A (1) (2)", "A", "A (1) ", "A (1)"}
  // and would contain duplicate entries. So try not breaking out of the
  // inner loop. In that case on the third iteration of the inner loop, the
  // tentative unique name encounters another duplicate, so now gets uniquefied
  // to "A (1) (2)" and if we then don't restart the loop, we would end up with
  // the final |filenames| being {"A (1) (2)", "A", "A (1) ", "A (1) (2)"} and
  // we still have duplicate entries. Instead we need to test against the
  // entire collection of existing names on each uniquification attempt.

  // Same value used in base::GetUniquePathNumber.
  static const int kMaxUniqueFiles = 100;
  int count = 1;
  for (; count <= kMaxUniqueFiles; ++count) {
    if (!ContainsFilePathCaseInsensitive(existing_filenames, unique_name))
      break;

    unique_name = unique_name.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", (*uniquifier)++));
  }
  if (count > kMaxUniqueFiles)
    unique_name = base::FilePath();

  return unique_name;
}

// Creates a uniquely-named temporary file based on the suggested filename, or
// an empty path on error. The file will be empty and all handles closed after
// this function returns.
base::FilePath CreateTemporaryFileWithSuggestedName(
    const base::FilePath& suggested_name) {
  base::FilePath temp_path1;
  if (!base::CreateTemporaryFile(&temp_path1))
    return base::FilePath();

  base::FilePath temp_path2 = temp_path1.DirName().Append(suggested_name);

  // Make filename unique.
  temp_path2 = base::GetUniquePath(temp_path2);
  if (temp_path2.empty())
    return base::FilePath();  // Failed to make a unique path.

  base::File::Error replace_file_error = base::File::FILE_OK;
  if (!ReplaceFile(temp_path1, temp_path2, &replace_file_error))
    return base::FilePath();

  return temp_path2;
}

// This method performs file I/O and thus is executed on a worker thread. An
// empty FilePath for the temp file is returned on failure.
base::FilePath WriteFileContentsToTempFile(const base::FilePath& suggested_name,
                                           HGLOBAL hdata) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!hdata)
    return base::FilePath();

  base::FilePath temp_path =
      CreateTemporaryFileWithSuggestedName(suggested_name);

  if (!temp_path.empty()) {
    base::win::ScopedHGlobal<char*> data(hdata);
    // Don't write to the temp file for empty content--leave it at 0-bytes.
    if (!(data.size() == 1 && data.data()[0] == '\0')) {
      if (!base::WriteFile(temp_path,
                           std::string_view(data.data(), data.size()))) {
        base::DeleteFile(temp_path);
        return base::FilePath();
      }
    }
  }

  ::GlobalFree(hdata);

  return temp_path;
}

std::vector<
    std::pair</*temp path*/ base::FilePath, /*display name*/ base::FilePath>>
WriteAllFileContentsToTempFiles(
    const std::vector<base::FilePath>& display_names,
    const std::vector<HGLOBAL>& memory_backed_contents) {
  DCHECK_EQ(display_names.size(), memory_backed_contents.size());

  std::vector<std::pair<base::FilePath, base::FilePath>> filepaths_and_names;
  for (size_t i = 0; i < display_names.size(); i++) {
    base::FilePath temp_path = WriteFileContentsToTempFile(
        display_names[i], memory_backed_contents[i]);

    filepaths_and_names.push_back({temp_path, display_names[i]});
  }

  return filepaths_and_names;
}

// Caller's responsibility to call GlobalFree on returned HGLOBAL when done with
// the data. This method must be performed on main thread as it is using the
// IDataObject marshalled there.
HGLOBAL CopyFileContentsToHGlobal(IDataObject* data_object, LONG index) {
  DCHECK(data_object);
  HGLOBAL hdata = nullptr;

  if (!HasData(data_object, ClipboardFormatType::FileContentAtIndexType(index)))
    return hdata;

  STGMEDIUM content;
  if (!GetData(data_object, ClipboardFormatType::FileContentAtIndexType(index),
               &content))
    return hdata;

  HRESULT hr = S_OK;

  if (content.tymed == TYMED_ISTORAGE) {
    // For example, messages dragged out of Outlook.exe.

    Microsoft::WRL::ComPtr<ILockBytes> lock_bytes;
    hr = ::CreateILockBytesOnHGlobal(nullptr, /* fDeleteOnRelease*/ FALSE,
                                     &lock_bytes);

    Microsoft::WRL::ComPtr<IStorage> storage;
    if (SUCCEEDED(hr)) {
      hr = ::StgCreateDocfileOnILockBytes(
          lock_bytes.Get(), STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE,
          0, &storage);
    }

    if (SUCCEEDED(hr))
      hr = content.pstg->CopyTo(0, nullptr, nullptr, storage.Get());

    if (SUCCEEDED(hr))
      hr = storage->Commit(STGC_OVERWRITE);

    if (SUCCEEDED(hr))
      hr = ::GetHGlobalFromILockBytes(lock_bytes.Get(), &hdata);

    if (FAILED(hr))
      hdata = nullptr;
  } else if (content.tymed == TYMED_ISTREAM) {
    // For example, attachments dragged out of messages in Outlook.exe.

    Microsoft::WRL::ComPtr<IStream> stream;
    hr =
        ::CreateStreamOnHGlobal(nullptr, /* fDeleteOnRelease */ FALSE, &stream);
    if (SUCCEEDED(hr)) {
      // A properly implemented IDataObject::GetData moves the stream pointer to
      // the end. Need to seek to the beginning before copying the data then
      // seek back to the original position.
      const LARGE_INTEGER zero_displacement = {};
      ULARGE_INTEGER original_position = {};
      // Obtain the original stream pointer position. If the stream doesn't
      // support seek, will still attempt to copy the data unless the failure is
      // due to access being denied (enterprise protected data e.g.).
      HRESULT hr_seek = content.pstm->Seek(zero_displacement, STREAM_SEEK_CUR,
                                           &original_position);
      if (hr_seek != E_ACCESSDENIED) {
        if (SUCCEEDED(hr_seek)) {
          // Seek to the beginning.
          hr_seek =
              content.pstm->Seek(zero_displacement, STREAM_SEEK_SET, nullptr);
        }

        // Copy all data to the file stream.
        ULARGE_INTEGER max_bytes;
        max_bytes.QuadPart = std::numeric_limits<uint64_t>::max();
        hr = content.pstm->CopyTo(stream.Get(), max_bytes, nullptr, nullptr);

        if (SUCCEEDED(hr_seek)) {
          // Restore the stream pointer to its original position.
          LARGE_INTEGER original_offset;
          original_offset.QuadPart = original_position.QuadPart;
          content.pstm->Seek(original_offset, STREAM_SEEK_SET, nullptr);
        }
      } else {
        // Access was denied.
        hr = hr_seek;
      }

      if (SUCCEEDED(hr))
        hr = ::GetHGlobalFromStream(stream.Get(), &hdata);

      if (FAILED(hr))
        hdata = nullptr;
    }
  } else if (content.tymed == TYMED_HGLOBAL) {
    // For example, anchor (internet shortcut) dragged out of Spartan Edge.
    // Copy the data as it will be written to a file on a worker thread and we
    // need to call ReleaseStgMedium to free the memory allocated by the drag
    // source.
    base::win::ScopedHGlobal<char*> data_source(content.hGlobal);
    hdata = ::GlobalAlloc(GHND, data_source.size());
    if (hdata) {
      base::win::ScopedHGlobal<char*> data_destination(hdata);
      memcpy(data_destination.data(), data_source.data(), data_source.size());
    }
  }

  // Safe to release the medium now since all the data has been copied.
  ReleaseStgMedium(&content);

  return hdata;
}

std::wstring ConvertString(const char* string) {
  return base::UTF8ToWide(string);
}

std::wstring ConvertString(const wchar_t* string) {
  return string;
}

template <typename FileGroupDescriptorType>
struct FileGroupDescriptorData;

template <>
struct FileGroupDescriptorData<FILEGROUPDESCRIPTORW> {
  static bool get(IDataObject* data_object, STGMEDIUM* medium) {
    return GetData(data_object, ClipboardFormatType::FileDescriptorType(),
                   medium);
  }
};

template <>
struct FileGroupDescriptorData<FILEGROUPDESCRIPTORA> {
  static bool get(IDataObject* data_object, STGMEDIUM* medium) {
    return GetData(data_object, ClipboardFormatType::FileDescriptorAType(),
                   medium);
  }
};

// Retrieves display names of virtual files, making sure they are unique.
// Use template parameter of FILEGROUPDESCRIPTORW for retrieving Unicode data
// and FILEGROUPDESCRIPTORA for ascii.
template <typename FileGroupDescriptorType>
std::optional<std::vector<base::FilePath>> GetVirtualFilenames(
    IDataObject* data_object) {
  STGMEDIUM medium;

  if (!FileGroupDescriptorData<FileGroupDescriptorType>::get(data_object,
                                                             &medium)) {
    return std::nullopt;
  }

  std::vector<base::FilePath> filenames;

  {
    base::win::ScopedHGlobal<FileGroupDescriptorType*> fgd(medium.hGlobal);
    if (!fgd.data()) {
      return std::nullopt;
    }

    unsigned int num_files = fgd->cItems;
    // We expect there to be at least one file in here.
    DCHECK_GE(num_files, 1u);

    // Value to be incremented to ensure a unique display name, as it is
    // possible that the filenames found in the file group descriptor are not
    // unique (e.g. multiple emails with the same subject line are dragged out
    // of Outlook.exe).
    unsigned int uniquifier = 1;

    for (size_t i = 0; i < num_files; i++) {
      // Folder entries not currently supported--skip this item.
      if ((fgd->fgd[i].dwFlags & FD_ATTRIBUTES) &&
          (fgd->fgd[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        DLOG(WARNING) << "GetVirtualFilenames: display name '"
                      << ConvertString(fgd->fgd[i].cFileName)
                      << "' refers to a directory (not supported).";
        continue;
      }
      base::FilePath display_name = GetUniqueVirtualFilename(
          ConvertString(fgd->fgd[i].cFileName), filenames, &uniquifier);

      filenames.push_back(display_name);
    }
  }

  ReleaseStgMedium(&medium);
  return filenames;
}

template <typename FileGroupDescriptorType>
bool GetFileNameFromFirstDescriptor(IDataObject* data_object,
                                    std::wstring* filename) {
  STGMEDIUM medium;

  if (!FileGroupDescriptorData<FileGroupDescriptorType>::get(data_object,
                                                             &medium))
    return false;

  {
    base::win::ScopedHGlobal<FileGroupDescriptorType*> fgd(medium.hGlobal);
    // We expect there to be at least one file in here.
    DCHECK_GE(fgd->cItems, 1u);
    filename->assign(ConvertString(fgd->fgd[0].cFileName));
  }
  ReleaseStgMedium(&medium);
  return true;
}

}  // namespace

namespace clipboard_util {

bool HasUrl(IDataObject* data_object, bool convert_filenames) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::MozUrlType()) ||
         HasData(data_object, ClipboardFormatType::UrlType()) ||
         HasData(data_object, ClipboardFormatType::UrlAType()) ||
         (convert_filenames && HasFilenames(data_object));
}

bool HasFilenames(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::CFHDropType()) ||
         HasData(data_object, ClipboardFormatType::FilenameType()) ||
         HasData(data_object, ClipboardFormatType::FilenameAType());
}

bool HasVirtualFilenames(IDataObject* data_object) {
  DCHECK(data_object);
  // Favor real files on the file system over virtual files.
  return !HasFilenames(data_object) &&
         HasData(data_object, ClipboardFormatType::FileContentAtIndexType(0)) &&
         (HasData(data_object, ClipboardFormatType::FileDescriptorType()) ||
          HasData(data_object, ClipboardFormatType::FileDescriptorAType()));
}

bool HasFileContents(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::FileContentZeroType()) &&
         (HasData(data_object, ClipboardFormatType::FileDescriptorType()) ||
          HasData(data_object, ClipboardFormatType::FileDescriptorAType()));
}

bool HasHtml(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::HtmlType()) ||
         HasData(data_object, ClipboardFormatType::TextHtmlType());
}

bool HasPlainText(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::PlainTextType()) ||
         HasData(data_object, ClipboardFormatType::PlainTextAType());
}

bool GetUrl(IDataObject* data_object,
            GURL* url,
            std::u16string* title,
            bool convert_filenames) {
  DCHECK(data_object && url && title);
  if (!HasUrl(data_object, convert_filenames))
    return false;

  // Try to extract a URL from |data_object| in a variety of formats.
  STGMEDIUM store;
  if (GetUrlFromHDrop(data_object, url, title))
    return true;

  if (GetData(data_object, ClipboardFormatType::MozUrlType(), &store) ||
      GetData(data_object, ClipboardFormatType::UrlType(), &store)) {
    {
      // Mozilla URL format or Unicode URL
      base::win::ScopedHGlobal<wchar_t*> data(store.hGlobal);
      SplitUrlAndTitle(base::WideToUTF16(data.data()), url, title);
    }
    ReleaseStgMedium(&store);
    return url->is_valid();
  }

  if (GetData(data_object, ClipboardFormatType::UrlAType(), &store)) {
    {
      // URL using ASCII
      base::win::ScopedHGlobal<char*> data(store.hGlobal);
      SplitUrlAndTitle(base::UTF8ToUTF16(data.data()), url, title);
    }
    ReleaseStgMedium(&store);
    return url->is_valid();
  }

  if (convert_filenames) {
    std::vector<std::wstring> filenames;
    if (!GetFilenames(data_object, &filenames))
      return false;
    DCHECK_GT(filenames.size(), 0U);
    *url = net::FilePathToFileURL(base::FilePath(filenames[0]));
    return url->is_valid();
  }

  return false;
}

bool GetFilenames(IDataObject* data_object,
                  std::vector<std::wstring>* filenames) {
  DCHECK(data_object && filenames);
  if (!HasFilenames(data_object))
    return false;

  STGMEDIUM medium;
  if (GetData(data_object, ClipboardFormatType::CFHDropType(), &medium)) {
    {
      base::win::ScopedHGlobal<HDROP> hdrop(medium.hGlobal);
      if (!hdrop.data()) {
        return false;
      }

      const int kMaxFilenameLen = 4096;
      const unsigned num_files = DragQueryFileW(hdrop.data(), 0xffffffff, 0, 0);
      for (unsigned int i = 0; i < num_files; ++i) {
        wchar_t filename[kMaxFilenameLen];
        if (!DragQueryFileW(hdrop.data(), i, filename, kMaxFilenameLen)) {
          continue;
        }
        filenames->push_back(filename);
      }
    }
    ReleaseStgMedium(&medium);
    return !filenames->empty();
  }

  if (GetData(data_object, ClipboardFormatType::FilenameType(), &medium)) {
    {
      // filename using Unicode
      base::win::ScopedHGlobal<wchar_t*> data(medium.hGlobal);
      if (data.data() && data.data()[0]) {
        filenames->push_back(data.data());
      }
    }
    ReleaseStgMedium(&medium);
    return true;
  }

  if (GetData(data_object, ClipboardFormatType::FilenameAType(), &medium)) {
    {
      // filename using ASCII
      base::win::ScopedHGlobal<char*> data(medium.hGlobal);
      if (data.data() && data.data()[0]) {
        filenames->push_back(base::SysNativeMBToWide(data.data()));
      }
    }
    ReleaseStgMedium(&medium);
    return true;
  }

  return false;
}

STGMEDIUM CreateStorageForFileNames(const std::vector<FileInfo>& filenames) {
  // CF_HDROP clipboard format consists of DROPFILES structure, a series of file
  // names including the terminating null character and the additional null
  // character at the tail to terminate the array.
  // For example,
  //| DROPFILES | FILENAME 1 | NULL | ... | FILENAME n | NULL | NULL |
  // For more details, please refer to
  // https://docs.microsoft.com/en-us/windows/desktop/shell/clipboard#cf_hdrop

  if (filenames.empty())
    return kNullStorageMedium;

  const size_t kDropFilesHeaderSizeInBytes = sizeof(DROPFILES);
  size_t total_bytes = kDropFilesHeaderSizeInBytes;
  for (const auto& filename : filenames) {
    // Allocate memory of the filename's length including the null
    // character.
    total_bytes += (filename.path.value().length() + 1) * sizeof(wchar_t);
  }
  // |data| needs to be terminated by an additional null character.
  total_bytes += sizeof(wchar_t);

  // GHND combines GMEM_MOVEABLE and GMEM_ZEROINIT, and GMEM_ZEROINIT
  // initializes memory contents to zero.
  HANDLE hdata = GlobalAlloc(GHND, total_bytes);

  base::win::ScopedHGlobal<DROPFILES*> locked_mem(hdata);
  DROPFILES* drop_files = locked_mem.data();
  drop_files->pFiles = sizeof(DROPFILES);
  drop_files->fWide = TRUE;

  wchar_t* data = reinterpret_cast<wchar_t*>(
      reinterpret_cast<BYTE*>(drop_files) + kDropFilesHeaderSizeInBytes);

  size_t next_filename_offset = 0;
  for (const auto& filename : filenames) {
    wcscpy(data + next_filename_offset, filename.path.value().c_str());
    // Skip the terminating null character of the filename.
    next_filename_offset += filename.path.value().length() + 1;
  }

  STGMEDIUM storage = {
      .tymed = TYMED_HGLOBAL, .hGlobal = hdata, .pUnkForRelease = nullptr};
  return storage;
}

std::optional<std::vector<base::FilePath>> GetVirtualFilenames(
    IDataObject* data_object) {
  DCHECK(data_object);
  if (!HasVirtualFilenames(data_object))
    return std::nullopt;

  // Nothing prevents the drag source app from using the CFSTR_FILEDESCRIPTORA
  // ANSI format (e.g., it could be that it doesn't support Unicode). So need to
  // check for both the ANSI and Unicode file group descriptors.

  // Unicode.
  std::optional<std::vector<base::FilePath>> filenames =
      ui::GetVirtualFilenames<FILEGROUPDESCRIPTORW>(data_object);
  if (filenames) {
    return filenames;
  }

  // ASCII.
  return ui::GetVirtualFilenames<FILEGROUPDESCRIPTORA>(data_object);
}

void GetVirtualFilesAsTempFiles(
    IDataObject* data_object,
    base::OnceCallback<
        void(const std::vector<std::pair</*temp path*/ base::FilePath,
                                         /*display name*/ base::FilePath>>&)>
        callback) {
  // Retrieve the display names of the virtual files.
  std::optional<std::vector<base::FilePath>> display_names =
      GetVirtualFilenames(data_object);
  if (!display_names) {
    std::move(callback).Run({});
    return;
  }

  // Write the file contents to global memory.
  std::vector<HGLOBAL> memory_backed_contents;
  for (size_t i = 0; i < display_names.value().size(); i++) {
    HGLOBAL hdata = CopyFileContentsToHGlobal(data_object, i);
    memory_backed_contents.push_back(hdata);
  }

  // Queue a task to actually write the temp files on a worker thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteAllFileContentsToTempFiles, display_names.value(),
                     memory_backed_contents),
      std::move(callback));  // callback on the UI thread
}

bool GetPlainText(IDataObject* data_object, std::u16string* plain_text) {
  DCHECK(data_object && plain_text);
  if (!HasPlainText(data_object))
    return false;

  STGMEDIUM store;
  if (GetData(data_object, ClipboardFormatType::PlainTextType(), &store)) {
    {
      // Unicode text
      base::win::ScopedHGlobal<wchar_t*> data(store.hGlobal);
      plain_text->assign(base::as_u16cstr(data.data()));
    }
    ReleaseStgMedium(&store);
    return true;
  }

  if (GetData(data_object, ClipboardFormatType::PlainTextAType(), &store)) {
    {
      // ASCII text
      base::win::ScopedHGlobal<char*> data(store.hGlobal);
      plain_text->assign(base::UTF8ToUTF16(data.data()));
    }
    ReleaseStgMedium(&store);
    return true;
  }

  // If a file is dropped on the window, it does not provide either of the
  // plain text formats, so here we try to forcibly get a url.
  GURL url;
  std::u16string title;
  if (GetUrl(data_object, &url, &title, false)) {
    *plain_text = base::UTF8ToUTF16(url.spec());
    return true;
  }
  return false;
}

bool GetHtml(IDataObject* data_object,
             std::u16string* html,
             std::string* base_url) {
  DCHECK(data_object && html && base_url);

  STGMEDIUM store;
  if (HasData(data_object, ClipboardFormatType::HtmlType()) &&
      GetData(data_object, ClipboardFormatType::HtmlType(), &store)) {
    {
      // MS CF html
      base::win::ScopedHGlobal<char*> data(store.hGlobal);

      std::string html_utf8;
      CFHtmlToHtml(std::string_view(data.data(), data.size()), &html_utf8,
                   base_url);
      html->assign(base::UTF8ToUTF16(html_utf8));
    }
    ReleaseStgMedium(&store);
    return true;
  }

  if (!HasData(data_object, ClipboardFormatType::TextHtmlType()))
    return false;

  if (!GetData(data_object, ClipboardFormatType::TextHtmlType(), &store))
    return false;

  {
    // text/html
    base::win::ScopedHGlobal<wchar_t*> data(store.hGlobal);
    html->assign(base::as_u16cstr(data.data()));
  }
  ReleaseStgMedium(&store);
  return true;
}

bool GetFileContents(IDataObject* data_object,
                     std::wstring* filename,
                     std::string* file_contents) {
  DCHECK(data_object && filename && file_contents);
  if (!HasFileContents(data_object))
    return false;

  STGMEDIUM content;
  // The call to GetData can be very slow depending on what is in
  // |data_object|.
  if (GetData(data_object, ClipboardFormatType::FileContentZeroType(),
              &content)) {
    if (TYMED_HGLOBAL == content.tymed) {
      base::win::ScopedHGlobal<char*> data(content.hGlobal);
      file_contents->assign(data.data(), data.size());
    }
    ReleaseStgMedium(&content);
  }

  // Nothing prevents the drag source app from using the CFSTR_FILEDESCRIPTORA
  // ANSI format (e.g., it could be that it doesn't support Unicode). So need to
  // check for both the ANSI and Unicode file group descriptors.
  if (GetFileNameFromFirstDescriptor<FILEGROUPDESCRIPTORW>(data_object,
                                                           filename)) {
    // file group descriptor using Unicode.
    return true;
  }

  if (GetFileNameFromFirstDescriptor<FILEGROUPDESCRIPTORA>(data_object,
                                                           filename)) {
    // file group descriptor using ASCII.
    return true;
  }

  return false;
}

bool GetDataTransferCustomData(
    IDataObject* data_object,
    std::unordered_map<std::u16string, std::u16string>* custom_data) {
  DCHECK(data_object && custom_data);

  if (!HasData(data_object, ClipboardFormatType::DataTransferCustomType())) {
    return false;
  }

  STGMEDIUM store;
  if (GetData(data_object, ClipboardFormatType::DataTransferCustomType(),
              &store)) {
    {
      base::win::ScopedHGlobal<const uint8_t*> data(store.hGlobal);
      if (std::optional<std::unordered_map<std::u16string, std::u16string>>
              maybe_custom_data = ReadCustomDataIntoMap(data);
          maybe_custom_data) {
        *custom_data = std::move(*maybe_custom_data);
        return true;
      }
    }
    ReleaseStgMedium(&store);
  }
  return false;
}

// HtmlToCFHtml and CFHtmlToHtml are based on similar methods in
// WebCore/platform/win/ClipboardUtilitiesWin.cpp.
/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Helper method for converting from text/html to MS CF_HTML.
// Documentation for the CF_HTML format is available at
// http://msdn.microsoft.com/en-us/library/aa767917(VS.85).aspx
std::string HtmlToCFHtml(std::string_view html, std::string_view base_url) {
  if (html.empty()) {
    return std::string();
  }

#define MAX_DIGITS 10
#define MAKE_NUMBER_FORMAT_1(digits) MAKE_NUMBER_FORMAT_2(digits)
#define MAKE_NUMBER_FORMAT_2(digits) "%0" #digits "zu"
#define NUMBER_FORMAT MAKE_NUMBER_FORMAT_1(MAX_DIGITS)

  static constexpr char kHeader[] =
      "Version:0.9\r\n"
      "StartHTML:" NUMBER_FORMAT
      "\r\n"
      "EndHTML:" NUMBER_FORMAT
      "\r\n"
      "StartFragment:" NUMBER_FORMAT
      "\r\n"
      "EndFragment:" NUMBER_FORMAT "\r\n";
  static const char kSourceUrlPrefix[] = "SourceURL:";
  static const char kStartMarkup[] = "<html>\r\n<body>\r\n";
  static const char kEndMarkup[] = "\r\n</body>\r\n</html>";
  static const char kStartFragment[] = "<!--StartFragment-->";
  static const char kEndFragment[] = "<!--EndFragment-->";

  // Windows apps expect HTML in the clipboard to be in the text format CF_HTML
  // so that they can figure out the length of the HTML document and extract
  // fragments of the content out if needed. `content_type` describes the
  // sanitization of the markup that will be converted to CF_HTML.

  // Given the following unsanitized HTML string
  // <html>
  //   <head> <style>p {color:blue}</style> </head>
  //   <body>
  //     <p>Hello World</p>
  //     <script> alert("Hello World!"); </script>
  //   </body>
  // </html>

  // Windows apps may extract the content from the headers to know where the
  // HTML or fragment starts. If we wrap the content by simply "sticking" the
  // headers (like we do with sanitized HTML), then it may result in double
  // tags.

  // Sticking the headers using the previous unsanitized HTML string (shortened
  // for brevity):
  // Version:0.9
  // StartHTML:0000000132
  // EndHTML:0000000637
  // ...
  // <html>
  // <body>
  //   <!--StartFragment-->
  //   <html>
  //     <head> <style>p {color:blue}</style> </head>
  //     <body> <p>...</p> <script>...</script> </body>
  //   </html>
  //   <!--EndFragment-->
  // </body>
  // </html>

  // Wrapping the unsanitized HTML string (shortened for brevity):
  // Version:0.9
  // StartHTML:0000000132
  // EndHTML:0000000274
  // ...
  // <!--StartFragment-->
  //   <html>
  //     <head> <style>p {color:blue}</style> </head>
  //     <body> <p>...</p> <script>...</script> </body>
  //   </html>
  // <!--EndFragment-->

  // The only way to write unsanitized HTML is by using the Async Clipboard API
  // write pipeline.

  // We don't want to regress the behavior of current DataTransfer APIs and
  // getData calls for apps that rely on markup with duplicate tags (e.g. Excel
  // Online expects this type of markup). As a result, if the HTML is sanitized,
  // we only "stick" the CF_HTML headers to the HTML string.
  std::string markup = kStartMarkup;
  base::StrAppend(&markup, {kStartFragment, html, kEndFragment});
  markup += kEndMarkup;

  // Calculate the offsets required for the HTML headers. This is used by Apps
  // on Windows to figure out the length of the HTML document and fragments.
  // Additionally, Apps can process specific parts of the HTML document. e.g.,
  // if they choose to process fragments of the HTML document, then they can use
  // the start and end fragments offsets to extract the content out.
  size_t headers_offset =
      strlen(kHeader) - strlen(NUMBER_FORMAT) * 4 + MAX_DIGITS * 4;
  if (!base_url.empty()) {
    headers_offset +=
        strlen(kSourceUrlPrefix) + base_url.length() + 2;  // Add 2 for \r\n.
  }

  size_t start_html_offset = headers_offset;
  size_t start_fragment_offset = headers_offset + strlen(kStartFragment);
  start_fragment_offset += strlen(kStartMarkup);
  size_t end_fragment_offset = start_fragment_offset + html.length();
  size_t end_html_offset = end_fragment_offset + strlen(kEndFragment);
  end_html_offset += strlen(kEndMarkup);

  std::string result =
      base::StringPrintf(kHeader, start_html_offset, end_html_offset,
                         start_fragment_offset, end_fragment_offset);
  if (!base_url.empty()) {
    base::StrAppend(&result, {kSourceUrlPrefix, base_url, "\r\n"});
  }
  result += markup;

#undef MAX_DIGITS
#undef MAKE_NUMBER_FORMAT_1
#undef MAKE_NUMBER_FORMAT_2
#undef NUMBER_FORMAT

  return result;
}

// Helper method for converting from MS CF_HTML to text/html.
void CFHtmlToHtml(std::string_view cf_html,
                  std::string* html,
                  std::string* base_url) {
  size_t fragment_start = std::string::npos;
  size_t fragment_end = std::string::npos;

  CFHtmlExtractMetadata(cf_html, base_url, nullptr, &fragment_start,
                        &fragment_end);

  if (html &&
      fragment_start != std::string::npos &&
      fragment_end != std::string::npos) {
    *html = cf_html.substr(fragment_start, fragment_end - fragment_start);
    base::TrimWhitespaceASCII(*html, base::TRIM_ALL, html);
  }
}

void CFHtmlExtractMetadata(std::string_view cf_html,
                           std::string* base_url,
                           size_t* html_start,
                           size_t* fragment_start,
                           size_t* fragment_end) {
  // Obtain base_url if present.
  if (base_url) {
    static constexpr char kSrcUrlStr[] = "SourceURL:";
    size_t line_start = cf_html.find(kSrcUrlStr);
    if (line_start != std::string::npos) {
      size_t src_end = cf_html.find("\n", line_start);
      size_t src_start = line_start + strlen(kSrcUrlStr);
      if (src_end != std::string::npos && src_start != std::string::npos) {
        *base_url = cf_html.substr(src_start, src_end - src_start);
        base::TrimWhitespaceASCII(*base_url, base::TRIM_ALL, base_url);
      }
    }
  }

  // Find the markup between "<!--StartFragment-->" and "<!--EndFragment-->".
  // If the comments cannot be found, like copying from OpenOffice Writer,
  // we simply fall back to using StartFragment/EndFragment bytecount values
  // to determine the fragment indexes.
  std::string cf_html_lower = base::ToLowerASCII(cf_html);
  size_t markup_start = cf_html_lower.find("<html", 0);
  if (html_start) {
    *html_start = markup_start;
  }
  size_t tag_start = cf_html.find("<!--StartFragment", markup_start);
  if (tag_start == std::string::npos) {
    static constexpr char kStartFragmentStr[] = "StartFragment:";
    size_t start_fragment_start = cf_html.find(kStartFragmentStr);
    if (start_fragment_start != std::string::npos) {
      *fragment_start = static_cast<size_t>(atoi(
          cf_html.data() + start_fragment_start + strlen(kStartFragmentStr)));
    }

    static constexpr char kEndFragmentStr[] = "EndFragment:";
    size_t end_fragment_start = cf_html.find(kEndFragmentStr);
    if (end_fragment_start != std::string::npos) {
      *fragment_end = static_cast<size_t>(
          atoi(cf_html.data() + end_fragment_start + strlen(kEndFragmentStr)));
    }
  } else {
    *fragment_start = cf_html.find('>', tag_start) + 1;
    size_t tag_end = cf_html.rfind("<!--EndFragment", std::string::npos);
    *fragment_end = cf_html.rfind('<', tag_end);
  }
}

}  // namespace clipboard_util

}  // namespace ui
