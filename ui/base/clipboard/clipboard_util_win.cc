// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_win.h"

#include <shellapi.h>
#include <wininet.h>  // For INTERNET_MAX_URL_LENGTH.
#include <wrl/client.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_hglobal.h"
#include "base/win/shlwapi.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "url/gurl.h"

namespace ui {

namespace {

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
                     base::string16* title) {
  DCHECK(data_object && url && title);

  bool success = false;
  STGMEDIUM medium;
  if (!GetData(data_object, ClipboardFormatType::GetCFHDropType(), &medium))
    return false;

  {
    base::win::ScopedHGlobal<HDROP> hdrop(medium.hGlobal);

    if (!hdrop.get())
      return false;

    wchar_t filename[MAX_PATH];
    if (DragQueryFileW(hdrop.get(), 0, filename, base::size(filename))) {
      wchar_t url_buffer[INTERNET_MAX_URL_LENGTH];
      if (0 == _wcsicmp(PathFindExtensionW(filename), L".url") &&
          GetPrivateProfileStringW(L"InternetShortcut", L"url", 0, url_buffer,
                                   base::size(url_buffer), filename)) {
        *url = GURL(url_buffer);
        PathRemoveExtension(filename);
        title->assign(PathFindFileName(filename));
        success = url->is_valid();
      }
    }
  }

  ReleaseStgMedium(&medium);
  return success;
}

void SplitUrlAndTitle(const base::string16& str,
                      GURL* url,
                      base::string16* title) {
  DCHECK(url && title);
  size_t newline_pos = str.find('\n');
  if (newline_pos != base::string16::npos) {
    *url = GURL(base::string16(str, 0, newline_pos));
    title->assign(str, newline_pos + 1, base::string16::npos);
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
  return std::find_if(std::begin(existing_filenames),
                      std::end(existing_filenames),
                      [&candidate_path](const base::FilePath& elem) {
                        return base::FilePath::CompareEqualIgnoreCase(
                            elem.value(), candidate_path.value());
                      }) != std::end(existing_filenames);
}

// Returns a unique display name for a virtual file, as it is possible that the
// filenames found in the file group descriptor are not unique (e.g. multiple
// emails with the same subject line are dragged out of Outlook.exe).
// |uniquifier| is incremented on encountering a non-unique file name.
base::FilePath GetUniqueVirtualFilename(
    const base::string16& candidate_name,
    const std::vector<base::FilePath>& existing_filenames,
    unsigned int* uniquifier) {
  // Remove any possible filepath components/separators that drag source may
  // have included in virtual file name.
  base::FilePath unique_name = base::FilePath(candidate_name).BaseName();

  // To mitigate against running up against MAX_PATH limitations (temp files
  // failing to be created), truncate the display name.
  const size_t kTruncatedDisplayNameLength = 128;
  const base::string16 extension = unique_name.Extension();
  unique_name = unique_name.RemoveExtension();
  base::string16 truncated = unique_name.value();
  if (truncated.length() > kTruncatedDisplayNameLength) {
    truncated.erase(kTruncatedDisplayNameLength);
    unique_name = base::FilePath(truncated);
  }
  unique_name = unique_name.AddExtension(extension);

  // Replace any file name illegal characters.
  unique_name = net::GenerateFileName(GURL(), std::string(), std::string(),
                                      base::UTF16ToUTF8(unique_name.value()),
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
    if (!(data.Size() == 1 && data.get()[0] == '\0')) {
      if (base::WriteFile(temp_path, data.get(), data.Size()) < 0) {
        base::DeleteFile(temp_path, false);
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

  if (!HasData(data_object,
               ClipboardFormatType::GetFileContentAtIndexType(index)))
    return hdata;

  STGMEDIUM content;
  if (!GetData(data_object,
               ClipboardFormatType::GetFileContentAtIndexType(index), &content))
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
    hdata = ::GlobalAlloc(GHND, data_source.Size());
    if (hdata) {
      base::win::ScopedHGlobal<char*> data_destination(hdata);
      memcpy(data_destination.get(), data_source.get(), data_source.Size());
    }
  }

  // Safe to release the medium now since all the data has been copied.
  ReleaseStgMedium(&content);

  return hdata;
}

base::string16 ConvertString(const char* string) {
  return base::UTF8ToWide(string);
}

base::string16 ConvertString(const wchar_t* string) {
  return string;
}

template <typename FileGroupDescriptorType>
struct FileGroupDescriptorData;

template <>
struct FileGroupDescriptorData<FILEGROUPDESCRIPTORW> {
  static bool get(IDataObject* data_object, STGMEDIUM* medium) {
    return GetData(data_object, ClipboardFormatType::GetFileDescriptorWType(),
                   medium);
  }
};

template <>
struct FileGroupDescriptorData<FILEGROUPDESCRIPTORA> {
  static bool get(IDataObject* data_object, STGMEDIUM* medium) {
    return GetData(data_object, ClipboardFormatType::GetFileDescriptorType(),
                   medium);
  }
};

// Retrieves display names of virtual files, making sure they are unique.
// Use template parameter of FILEGROUPDESCRIPTORW for retrieving Unicode data
// and FILEGROUPDESCRIPTORA for ascii.
template <typename FileGroupDescriptorType>
bool GetVirtualFilenames(IDataObject* data_object,
                         std::vector<base::FilePath>* filenames) {
  STGMEDIUM medium;

  if (!FileGroupDescriptorData<FileGroupDescriptorType>::get(data_object,
                                                             &medium))
    return false;

  {
    base::win::ScopedHGlobal<FileGroupDescriptorType*> fgd(medium.hGlobal);
    if (!fgd.get())
      return false;

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
          ConvertString(fgd->fgd[i].cFileName), *filenames, &uniquifier);

      filenames->push_back(display_name);
    }
  }

  ReleaseStgMedium(&medium);
  return !filenames->empty();
}

template <typename FileGroupDescriptorType>
bool GetFileNameFromFirstDescriptor(IDataObject* data_object,
                                    base::string16* filename) {
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

bool ClipboardUtil::HasUrl(IDataObject* data_object, bool convert_filenames) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::GetMozUrlType()) ||
         HasData(data_object, ClipboardFormatType::GetUrlWType()) ||
         HasData(data_object, ClipboardFormatType::GetUrlType()) ||
         (convert_filenames && HasFilenames(data_object));
}

bool ClipboardUtil::HasFilenames(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::GetCFHDropType()) ||
         HasData(data_object, ClipboardFormatType::GetFilenameWType()) ||
         HasData(data_object, ClipboardFormatType::GetFilenameType());
}

bool ClipboardUtil::HasVirtualFilenames(IDataObject* data_object) {
  DCHECK(data_object);
  // Favor real files on the file system over virtual files.
  return !HasFilenames(data_object) &&
         HasData(data_object,
                 ClipboardFormatType::GetFileContentAtIndexType(0)) &&
         (HasData(data_object, ClipboardFormatType::GetFileDescriptorWType()) ||
          HasData(data_object, ClipboardFormatType::GetFileDescriptorType()));
}

bool ClipboardUtil::HasFileContents(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::GetFileContentZeroType()) &&
         (HasData(data_object, ClipboardFormatType::GetFileDescriptorWType()) ||
          HasData(data_object, ClipboardFormatType::GetFileDescriptorType()));
}

bool ClipboardUtil::HasHtml(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::GetHtmlType()) ||
         HasData(data_object, ClipboardFormatType::GetTextHtmlType());
}

bool ClipboardUtil::HasPlainText(IDataObject* data_object) {
  DCHECK(data_object);
  return HasData(data_object, ClipboardFormatType::GetPlainTextWType()) ||
         HasData(data_object, ClipboardFormatType::GetPlainTextType());
}

bool ClipboardUtil::GetUrl(IDataObject* data_object,
                           GURL* url,
                           base::string16* title,
                           bool convert_filenames) {
  DCHECK(data_object && url && title);
  if (!HasUrl(data_object, convert_filenames))
    return false;

  // Try to extract a URL from |data_object| in a variety of formats.
  STGMEDIUM store;
  if (GetUrlFromHDrop(data_object, url, title))
    return true;

  if (GetData(data_object, ClipboardFormatType::GetMozUrlType(), &store) ||
      GetData(data_object, ClipboardFormatType::GetUrlWType(), &store)) {
    {
      // Mozilla URL format or Unicode URL
      base::win::ScopedHGlobal<wchar_t*> data(store.hGlobal);
      SplitUrlAndTitle(data.get(), url, title);
    }
    ReleaseStgMedium(&store);
    return url->is_valid();
  }

  if (GetData(data_object, ClipboardFormatType::GetUrlType(), &store)) {
    {
      // URL using ASCII
      base::win::ScopedHGlobal<char*> data(store.hGlobal);
      SplitUrlAndTitle(base::UTF8ToWide(data.get()), url, title);
    }
    ReleaseStgMedium(&store);
    return url->is_valid();
  }

  if (convert_filenames) {
    std::vector<base::string16> filenames;
    if (!GetFilenames(data_object, &filenames))
      return false;
    DCHECK_GT(filenames.size(), 0U);
    *url = net::FilePathToFileURL(base::FilePath(filenames[0]));
    return url->is_valid();
  }

  return false;
}

bool ClipboardUtil::GetFilenames(IDataObject* data_object,
                                 std::vector<base::string16>* filenames) {
  DCHECK(data_object && filenames);
  if (!HasFilenames(data_object))
    return false;

  STGMEDIUM medium;
  if (GetData(data_object, ClipboardFormatType::GetCFHDropType(), &medium)) {
    {
      base::win::ScopedHGlobal<HDROP> hdrop(medium.hGlobal);
      if (!hdrop.get())
        return false;

      const int kMaxFilenameLen = 4096;
      const unsigned num_files = DragQueryFileW(hdrop.get(), 0xffffffff, 0, 0);
      for (unsigned int i = 0; i < num_files; ++i) {
        wchar_t filename[kMaxFilenameLen];
        if (!DragQueryFileW(hdrop.get(), i, filename, kMaxFilenameLen))
          continue;
        filenames->push_back(filename);
      }
    }
    ReleaseStgMedium(&medium);
    return !filenames->empty();
  }

  if (GetData(data_object, ClipboardFormatType::GetFilenameWType(), &medium)) {
    {
      // filename using Unicode
      base::win::ScopedHGlobal<wchar_t*> data(medium.hGlobal);
      if (data.get() && data.get()[0])
        filenames->push_back(data.get());
    }
    ReleaseStgMedium(&medium);
    return true;
  }

  if (GetData(data_object, ClipboardFormatType::GetFilenameType(), &medium)) {
    {
      // filename using ASCII
      base::win::ScopedHGlobal<char*> data(medium.hGlobal);
      if (data.get() && data.get()[0])
        filenames->push_back(base::SysNativeMBToWide(data.get()));
    }
    ReleaseStgMedium(&medium);
    return true;
  }

  return false;
}

bool ClipboardUtil::GetVirtualFilenames(
    IDataObject* data_object,
    std::vector<base::FilePath>* filenames) {
  DCHECK(data_object && filenames);
  if (!HasVirtualFilenames(data_object))
    return false;

  // Nothing prevents the drag source app from using the CFSTR_FILEDESCRIPTORA
  // ANSI format (e.g., it could be that it doesn't support Unicode). So need to
  // check for both the ANSI and Unicode file group descriptors.
  if (ui::GetVirtualFilenames<FILEGROUPDESCRIPTORW>(data_object, filenames)) {
    // file group descriptor using Unicode.
    return true;
  }

  if (ui::GetVirtualFilenames<FILEGROUPDESCRIPTORA>(data_object, filenames)) {
    // file group descriptor using ascii.
    return true;
  }

  return false;
}

bool ClipboardUtil::GetVirtualFilesAsTempFiles(
    IDataObject* data_object,
    base::OnceCallback<
        void(const std::vector<std::pair</*temp path*/ base::FilePath,
                                         /*display name*/ base::FilePath>>&)>
        callback) {
  // Retrieve the display names of the virtual files.
  std::vector<base::FilePath> display_names;
  if (!GetVirtualFilenames(data_object, &display_names))
    return false;

  // Write the file contents to global memory.
  std::vector<HGLOBAL> memory_backed_contents;
  for (size_t i = 0; i < display_names.size(); i++) {
    HGLOBAL hdata = CopyFileContentsToHGlobal(data_object, i);
    memory_backed_contents.push_back(hdata);
  }

  // Queue a task to actually write the temp files on a worker thread.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteAllFileContentsToTempFiles, display_names,
                     memory_backed_contents),
      std::move(callback));  // callback on the UI thread

  return true;
}

bool ClipboardUtil::GetPlainText(IDataObject* data_object,
                                 base::string16* plain_text) {
  DCHECK(data_object && plain_text);
  if (!HasPlainText(data_object))
    return false;

  STGMEDIUM store;
  if (GetData(data_object, ClipboardFormatType::GetPlainTextWType(), &store)) {
    {
      // Unicode text
      base::win::ScopedHGlobal<wchar_t*> data(store.hGlobal);
      plain_text->assign(data.get());
    }
    ReleaseStgMedium(&store);
    return true;
  }

  if (GetData(data_object, ClipboardFormatType::GetPlainTextType(), &store)) {
    {
      // ASCII text
      base::win::ScopedHGlobal<char*> data(store.hGlobal);
      plain_text->assign(base::UTF8ToWide(data.get()));
    }
    ReleaseStgMedium(&store);
    return true;
  }

  // If a file is dropped on the window, it does not provide either of the
  // plain text formats, so here we try to forcibly get a url.
  GURL url;
  base::string16 title;
  if (GetUrl(data_object, &url, &title, false)) {
    *plain_text = base::UTF8ToUTF16(url.spec());
    return true;
  }
  return false;
}

bool ClipboardUtil::GetHtml(IDataObject* data_object,
                            base::string16* html, std::string* base_url) {
  DCHECK(data_object && html && base_url);

  STGMEDIUM store;
  if (HasData(data_object, ClipboardFormatType::GetHtmlType()) &&
      GetData(data_object, ClipboardFormatType::GetHtmlType(), &store)) {
    {
      // MS CF html
      base::win::ScopedHGlobal<char*> data(store.hGlobal);

      std::string html_utf8;
      CFHtmlToHtml(std::string(data.get(), data.Size()), &html_utf8, base_url);
      html->assign(base::UTF8ToWide(html_utf8));
    }
    ReleaseStgMedium(&store);
    return true;
  }

  if (!HasData(data_object, ClipboardFormatType::GetTextHtmlType()))
    return false;

  if (!GetData(data_object, ClipboardFormatType::GetTextHtmlType(), &store))
    return false;

  {
    // text/html
    base::win::ScopedHGlobal<wchar_t*> data(store.hGlobal);
    html->assign(data.get());
  }
  ReleaseStgMedium(&store);
  return true;
}

bool ClipboardUtil::GetFileContents(IDataObject* data_object,
                                    base::string16* filename,
                                    std::string* file_contents) {
  DCHECK(data_object && filename && file_contents);
  if (!HasFileContents(data_object))
    return false;

  STGMEDIUM content;
  // The call to GetData can be very slow depending on what is in
  // |data_object|.
  if (GetData(data_object, ClipboardFormatType::GetFileContentZeroType(),
              &content)) {
    if (TYMED_HGLOBAL == content.tymed) {
      base::win::ScopedHGlobal<char*> data(content.hGlobal);
      file_contents->assign(data.get(), data.Size());
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

bool ClipboardUtil::GetWebCustomData(
    IDataObject* data_object,
    std::unordered_map<base::string16, base::string16>* custom_data) {
  DCHECK(data_object && custom_data);

  if (!HasData(data_object, ClipboardFormatType::GetWebCustomDataType()))
    return false;

  STGMEDIUM store;
  if (GetData(data_object, ClipboardFormatType::GetWebCustomDataType(),
              &store)) {
    {
      base::win::ScopedHGlobal<char*> data(store.hGlobal);
      ReadCustomDataIntoMap(data.get(), data.Size(), custom_data);
    }
    ReleaseStgMedium(&store);
    return true;
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
std::string ClipboardUtil::HtmlToCFHtml(const std::string& html,
                                        const std::string& base_url) {
  if (html.empty())
    return std::string();

  #define MAX_DIGITS 10
  #define MAKE_NUMBER_FORMAT_1(digits) MAKE_NUMBER_FORMAT_2(digits)
  #define MAKE_NUMBER_FORMAT_2(digits) "%0" #digits "u"
  #define NUMBER_FORMAT MAKE_NUMBER_FORMAT_1(MAX_DIGITS)

  static const char* header = "Version:0.9\r\n"
      "StartHTML:" NUMBER_FORMAT "\r\n"
      "EndHTML:" NUMBER_FORMAT "\r\n"
      "StartFragment:" NUMBER_FORMAT "\r\n"
      "EndFragment:" NUMBER_FORMAT "\r\n";
  static const char* source_url_prefix = "SourceURL:";

  static const char* start_markup =
      "<html>\r\n<body>\r\n<!--StartFragment-->";
  static const char* end_markup =
      "<!--EndFragment-->\r\n</body>\r\n</html>";

  // Calculate offsets
  size_t start_html_offset = strlen(header) - strlen(NUMBER_FORMAT) * 4 +
      MAX_DIGITS * 4;
  if (!base_url.empty()) {
    start_html_offset += strlen(source_url_prefix) +
        base_url.length() + 2;  // Add 2 for \r\n.
  }
  size_t start_fragment_offset = start_html_offset + strlen(start_markup);
  size_t end_fragment_offset = start_fragment_offset + html.length();
  size_t end_html_offset = end_fragment_offset + strlen(end_markup);

  std::string result = base::StringPrintf(header,
                                          start_html_offset,
                                          end_html_offset,
                                          start_fragment_offset,
                                          end_fragment_offset);
  if (!base_url.empty()) {
    result += source_url_prefix;
    result += base_url;
    result += "\r\n";
  }
  result += start_markup;
  result += html;
  result += end_markup;

  #undef MAX_DIGITS
  #undef MAKE_NUMBER_FORMAT_1
  #undef MAKE_NUMBER_FORMAT_2
  #undef NUMBER_FORMAT

  return result;
}

// Helper method for converting from MS CF_HTML to text/html.
void ClipboardUtil::CFHtmlToHtml(const std::string& cf_html,
                                 std::string* html,
                                 std::string* base_url) {
  size_t fragment_start = std::string::npos;
  size_t fragment_end = std::string::npos;

  ClipboardUtil::CFHtmlExtractMetadata(cf_html, base_url, nullptr,
                                       &fragment_start, &fragment_end);

  if (html &&
      fragment_start != std::string::npos &&
      fragment_end != std::string::npos) {
    *html = cf_html.substr(fragment_start, fragment_end - fragment_start);
    base::TrimWhitespaceASCII(*html, base::TRIM_ALL, html);
  }
}

void ClipboardUtil::CFHtmlExtractMetadata(const std::string& cf_html,
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
          cf_html.c_str() + start_fragment_start + strlen(kStartFragmentStr)));
    }

    static constexpr char kEndFragmentStr[] = "EndFragment:";
    size_t end_fragment_start = cf_html.find(kEndFragmentStr);
    if (end_fragment_start != std::string::npos) {
      *fragment_end = static_cast<size_t>(
          atoi(cf_html.c_str() + end_fragment_start + strlen(kEndFragmentStr)));
    }
  } else {
    *fragment_start = cf_html.find('>', tag_start) + 1;
    size_t tag_end = cf_html.rfind("<!--EndFragment", std::string::npos);
    *fragment_end = cf_html.rfind('<', tag_end);
  }
}

}  // namespace ui
