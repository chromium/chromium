// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/lzma_sdk/google/seven_zip_reader.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/memory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

extern "C" {
#include "third_party/lzma_sdk/src/C/7z.h"
#include "third_party/lzma_sdk/src/C/7zAlloc.h"
#include "third_party/lzma_sdk/src/C/7zCrc.h"
}

#if BUILDFLAG(IS_WIN)
#include <ntstatus.h>
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

namespace seven_zip {

namespace internal {

namespace {

// Copies `length` bytes from `source` to `destination`. Returns false in case
// of I/O error or premature EOF.
bool CopyStream(ILookInStreamPtr source,
                base::File& destination,
                size_t length) {
  Byte buffer_array[4096];
  base::span<Byte> buffer(buffer_array);
  while (length > 0) {
    size_t to_read = std::min(length, buffer.size());
    size_t bytes_read = to_read;
    if (ILookInStream_Read(source, buffer.data(), &bytes_read) != SZ_OK ||
        bytes_read == 0) {
      return false;
    }
    if (!destination.WriteAtCurrentPosAndCheck(buffer.first(bytes_read))) {
      return false;
    }
    length -= bytes_read;
  }

  return true;
}

}  // namespace

enum : uint32_t { kNoFolder = static_cast<uint32_t>(-1) };

// An implementation of lzma_sdk's `ISeekInStream` that reads data from a
// `base::File`.
class FileSeekInStream : public ISeekInStream {
 public:
  explicit FileSeekInStream(base::File file)
      : ISeekInStream{&FileSeekInStream::DoRead, &FileSeekInStream::DoSeek},
        file_(std::move(file)) {}

 private:
  static SRes DoRead(const ISeekInStream* p, void* buf, size_t* size);
  static SRes DoSeek(const ISeekInStream* p, Int64* pos, ESzSeek origin);

  base::File file_;
};

// The LZMA SDK's SzArEx_Open and SzAr_DecodeFolder use an ILookInStream to
// fetch data. This interface isn't documented in the SDK, so here is some
// documentation inferred from the implementation of CLookToRead2 as of 26.02:
//
// `ILookInStream` is a buffering stream reader that provides a read-only view
// into its buffer in addition to the facilities of `ISeekInStream` (which
// provides random access to a stream via Seek and Read). Its methods are:
//
// - SRes Look(const void** buf, size_t *size):
//   Requests a read-only view on the instance's internal buffer to access
//   `size` bytes of data at the current buffer position. Returns SZ_OK and sets
//   `size` to the actual number of bytes available via `buf` (zero in case of
//   EOF) on success. Does not advance the buffer's position. May replenish the
//   instance's buffer if it has been drained. Sets `size` to zero on error.
//
// - SRes Skip(size_t offset):
//   Advances the current position in the internal buffer by `offset` bytes,
//   which must be no more than a size reported by a previous call to Look.
//
// - SRes Read(void* buf, size_t *size):
//   Reads up to `size` bytes from the input stream into `buf`. Returns SZ_OK on
//   success, in which case `size` is populated with the actual number of bytes
//   read (which is zero in case of EOF). Sets `size` to zero on error.
//
// - SRes Seek(int64_t* pos, ESzSeek origin):
//   Moves the file pointer to the position pointed to by `pos` relative to
//   `origin`. Returns SZ_OK on success, in which case `pos` is set to the new
//   file pointer. Implementations that use a buffer may invalidate the buffer.

// An ILookInStream backed by a `base::File` that uses a 16KB buffer.
class FileLookInStream {
 public:
  explicit FileLookInStream(base::File input_file)
      : stream_(std::move(input_file)) {
    LookToRead2_CreateVTable(&look_to_read_, /*lookahead=*/0);
    LookToRead2_INIT(&look_to_read_);

    if (!look_stream_buffer_) {
      // Failed to allocate the buffer, so report ERROR_MEM for all reads.
      stream_.Read = [](const ISeekInStream*, void*, size_t*) -> SRes {
        return SZ_ERROR_MEM;
      };
    }
  }

  FileLookInStream(const FileLookInStream&) = delete;
  FileLookInStream& operator=(const FileLookInStream&) = delete;

  ILookInStreamPtr AsILookInStream() { return &look_to_read_.vt; }

 private:
  static std::unique_ptr<uint8_t, base::UncheckedFreeDeleter> AllocateBuffer() {
    void* buffer = nullptr;
    if (!base::UncheckedMalloc(kBufferSize, &buffer)) {
      return {};
    }
    return std::unique_ptr<uint8_t, base::UncheckedFreeDeleter>(
        reinterpret_cast<uint8_t*>(buffer));
  }

  static constexpr size_t kBufferSize = 1 << 14;
  FileSeekInStream stream_;
  std::unique_ptr<uint8_t, base::UncheckedFreeDeleter> look_stream_buffer_ =
      AllocateBuffer();
  CLookToRead2 look_to_read_{.realStream = &stream_,
                             .buf = look_stream_buffer_.get(),
                             .bufSize = look_stream_buffer_ ? kBufferSize : 0};
};

class SevenZipReaderImpl {
 public:
  SevenZipReaderImpl(
      base::File archive_file,
      base::OnceCallback<base::File()> temp_file_request_callback);
  ~SevenZipReaderImpl();

  SevenZipReaderImpl(const SevenZipReaderImpl&) = delete;
  SevenZipReaderImpl& operator=(const SevenZipReaderImpl&) = delete;

  Result Open();
  size_t num_entries() const { return db_.NumFiles; }
  base::span<uint8_t> mapped_span() {
    return temp_file_mapped_ ? temp_file_mapped_->mutable_bytes()
                             : base::span<uint8_t>();
  }
  EntryInfo GetEntryInfo(size_t entry_index) const;
  bool IsDirectory(size_t entry_index) const;
  Result ExtractFile(size_t entry_index, base::span<uint8_t> output);

 private:
  static Result SResToResult(SRes res);

  // The SDK may allocate large buffers while extracting. Rather than crash,
  // allow such allocations to fail. `SzArEx_Open` and `SzAr_DecodeFolder` will
  // fail with `SZ_ERROR_MEM` if an allocation fails.
  static void* AllocTemp(ISzAllocPtr p, size_t size);
  static void FreeTemp(ISzAllocPtr p, void* address);

  // Return whether the seven zip archive has encrypted headers. This
  // requires creating a modified version of the archive in a temporary
  // file. Returns false in case of error.
  static bool AreHeadersEncrypted(ILookInStreamPtr archive_stream,
                                  base::File temp_file);

  ILookInStreamPtr in_stream() { return in_stream_.AsILookInStream(); }

  Result ExtractIntoTempFile(size_t folder_index);

  bool IsFolderEncrypted(size_t folder_index) const;

  const ISzAlloc alloc_{.Alloc = &SzAlloc, .Free = &SzFree};
  const ISzAlloc alloc_temp_{.Alloc = &AllocTemp, .Free = &FreeTemp};
  base::OnceCallback<base::File()> temp_file_request_callback_;
  base::File temp_file_;
  FileLookInStream in_stream_;
  CSzArEx db_{};

  // The index of the folder that has been decoded into the temp file via
  // `temp_file_mapped`, or `kNoFolder` if no folder has been extracted.
  size_t temp_folder_index_ = kNoFolder;
  absl::optional<base::MemoryMappedFile> temp_file_mapped_;
};

// FileSeekInStream ------------------------------------------------------------

SRes FileSeekInStream::DoRead(const ISeekInStream* p, void* buf, size_t* size) {
  // ISeekInStream is just a v-table of function pointers, which we shouldn't
  // change. But this function is called expecting that the file would be read,
  // so we cast away the const to do this.
  auto* stream =
      const_cast<FileSeekInStream*>(static_cast<const FileSeekInStream*>(p));
  std::optional<size_t> res = stream->file_.ReadAtCurrentPos(
      base::span(static_cast<uint8_t*>(buf), *size));
  if (!res.has_value()) {
    return SZ_ERROR_READ;
  }
  *size = *res;
  return SZ_OK;
}

SRes FileSeekInStream::DoSeek(const ISeekInStream* p,
                              Int64* pos,
                              ESzSeek origin) {
  // ISeekInStream is just a v-table of function pointers, which we shouldn't
  // change. But this function is called expecting that the file would be
  // seeked, so we cast away the const to do this.
  auto* stream =
      const_cast<FileSeekInStream*>(static_cast<const FileSeekInStream*>(p));

  base::File::Whence whence;
  switch (origin) {
    case SZ_SEEK_SET:
      whence = base::File::FROM_BEGIN;
      break;
    case SZ_SEEK_CUR:
      whence = base::File::FROM_CURRENT;
      break;
    case SZ_SEEK_END:
      whence = base::File::FROM_END;
      break;
  }
  int64_t res = stream->file_.Seek(whence, *pos);
  if (res < 0)
    return SZ_ERROR_READ;
  *pos = res;
  return SZ_OK;
}

// SevenZipReaderImpl ----------------------------------------------------------

SevenZipReaderImpl::SevenZipReaderImpl(
    base::File archive_file,
    base::OnceCallback<base::File()> temp_file_request_callback)
    : temp_file_request_callback_(std::move(temp_file_request_callback)),
      in_stream_(std::move(archive_file)) {
  EnsureLzmaSdkInitialized();
  SzArEx_Init(&db_);
}

SevenZipReaderImpl::~SevenZipReaderImpl() {
  // SzArEx_Open may have already called SzArEx_Free in case of error. It is
  // safe to call it twice, as it zeros all fields and `free` is a no-op when
  // called on a null ptr.
  SzArEx_Free(&db_, &alloc_);
}

Result SevenZipReaderImpl::Open() {
  SRes sz_res = SzArEx_Open(&db_, in_stream(), &alloc_, &alloc_temp_);
  if (sz_res == SZ_OK) {
    return Result::kSuccess;
  }

  Result result = SResToResult(sz_res);
  if (result == Result::kUnsupported) {
    base::File temp_file = temp_file_request_callback_
                               ? std::move(temp_file_request_callback_).Run()
                               : std::move(temp_file_);
    if (temp_file.IsValid() && SevenZipReaderImpl::AreHeadersEncrypted(
                                   in_stream(), std::move(temp_file))) {
      result = Result::kEncryptedHeaders;
    }
  }
  return result;
}

EntryInfo SevenZipReaderImpl::GetEntryInfo(size_t entry_index) const {
  EntryInfo entry{};

  size_t file_name_length = SzArEx_GetFileNameUtf16(&db_, entry_index, nullptr);
  std::vector<UInt16> file_name(file_name_length);
  file_name_length =
      SzArEx_GetFileNameUtf16(&db_, entry_index, file_name.data());
  DCHECK_EQ(file_name_length, file_name.size());

  if (file_name_length >= 1) {
    // |file_name| has a string terminator.
    entry.file_path = base::FilePath::FromUTF16Unsafe(
        std::u16string(file_name.begin(), --file_name.end()));
  }

  uint32_t folder_index = db_.FileToFolder[entry_index];
  if (folder_index != kNoFolder) {
    uint64_t file_offset = db_.UnpackPositions[entry_index];
    // |UnpackPositions| has NumFiles + 1 entries, with an extra entry
    // for the sentinel.
    entry.file_size =
        static_cast<size_t>(db_.UnpackPositions[entry_index + 1] - file_offset);
  }

  if (SzBitWithVals_Check(&db_.MTime, entry_index)) {
    // 7z archives store times as number of 100-nanosecond intervals since the
    // Windows epoch, just like a FILETIME on Windows. For cross-platform
    // support, we convert this to microseconds here rather than using the
    // Windows-only helper function base::Time::FromFileTime.
    const CNtfsFileTime& timestamp = db_.MTime.Vals[entry_index];
    int64_t intervals = (static_cast<int64_t>(timestamp.High) << 32) |
                        static_cast<int64_t>(timestamp.Low);
    entry.last_modified_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(intervals / 10));
  }

  entry.is_encrypted = IsFolderEncrypted(folder_index);

  return entry;
}

bool SevenZipReaderImpl::IsDirectory(size_t entry_index) const {
  return SzArEx_IsDir(&db_, entry_index);
}

Result SevenZipReaderImpl::ExtractFile(size_t entry_index,
                                       base::span<uint8_t> output) {
  const size_t folder_index = db_.FileToFolder[entry_index];
  if (folder_index == kNoFolder)
    return Result::kSuccess;

  const uint64_t file_offset = db_.UnpackPositions[entry_index];
  // |UnpackPositions| has NumFiles + 1 entries, with an extra entry
  // for the sentinel.
  const size_t file_size =
      static_cast<size_t>(db_.UnpackPositions[entry_index + 1] - file_offset);
  const uint64_t folder_offset =
      db_.UnpackPositions[db_.FolderToFile[folder_index]];

  if (folder_offset > file_offset)
    return Result::kMalformedArchive;

  // The offset to the start of the file's data in the unpacked folder (in the
  // 7z-internal sense).
  const size_t file_offset_in_folder =
      static_cast<size_t>(file_offset - folder_offset);
  const uint64_t folder_unpack_size =
      SzAr_GetFolderUnpackSize(&db_.db, folder_index);
  if (file_offset_in_folder + file_size > folder_unpack_size)
    return Result::kMalformedArchive;

  // A buffer is used iff the folder doesn't match exactly the target file.
  // Otherwise, the target is written directly to |output|.
  // In practice, all folders are single file.
  if (folder_unpack_size != file_size) {
    Result result = ExtractIntoTempFile(folder_index);
    if (result != Result::kSuccess)
      return result;

    // Copy the range of extracted folder corresponding to `entry_index` into
    // `output`.
    output.copy_from_nonoverlapping(temp_file_mapped_->bytes().subspan(
        file_offset_in_folder, output.size()));
  } else {
    // Extract directly into `output`.
    SRes sz_res =
        SzAr_DecodeFolder(&db_.db, folder_index, in_stream(), db_.dataPos,
                          output.data(), output.size(), &alloc_temp_);
    if (sz_res != SZ_OK)
      return SResToResult(sz_res);
  }

  if (SzBitWithVals_Check(&db_.CRCs, entry_index) &&
      CrcCalc(output.data(), output.size()) != db_.CRCs.Vals[entry_index]) {
    return Result::kBadCrc;
  }

  return Result::kSuccess;
}

// static
Result SevenZipReaderImpl::SResToResult(SRes res) {
  switch (res) {
    case SZ_OK:
      return Result::kSuccess;
    case SZ_ERROR_DATA:
      return Result::kMalformedArchive;
    case SZ_ERROR_MEM:
      return Result::kFailedToAllocate;
    case SZ_ERROR_CRC:
      return Result::kBadCrc;
    case SZ_ERROR_READ:
    case SZ_ERROR_WRITE:
      return Result::kIoError;
    case SZ_ERROR_UNSUPPORTED:
      return Result::kUnsupported;
    case SZ_ERROR_PARAM:
    case SZ_ERROR_INPUT_EOF:
    case SZ_ERROR_OUTPUT_EOF:
    case SZ_ERROR_PROGRESS:
    case SZ_ERROR_FAIL:
    case SZ_ERROR_THREAD:
    case SZ_ERROR_ARCHIVE:
    case SZ_ERROR_NO_ARCHIVE:
    default:
      return Result::kMalformedArchive;
  }
}

// static
void* SevenZipReaderImpl::AllocTemp(ISzAllocPtr p, size_t size) {
  void* result = nullptr;
  if (!base::UncheckedMalloc(size, &result)) {
    result = nullptr;
  }
  return result;
}

// static
void SevenZipReaderImpl::FreeTemp(ISzAllocPtr p, void* address) {
  base::UncheckedFree(address);
}

// static
bool SevenZipReaderImpl::AreHeadersEncrypted(ILookInStreamPtr archive_stream,
                                             base::File temp_file) {
  // See
  // https://github.com/jljusten/LZMA-SDK/blob/master/DOC/7zFormat.txt
  // for context on 7z archive file structure.

  // 7z archives have two sets of metadata, a signature header at the
  // beginning of the file which points to a "header" at the end of the
  // file. The header can be "packed" by by placing a tag
  // `k7zIdEncodedHeader` at the beginning of header and then a
  // StreamsInfo describing how to get the real header content. For
  // archives with metadata encryption, the StreamsInfo will include
  // encryption.

  // This function modifies the StreamsInfo to make it MainStreamsInfo
  // for a regular archive. We can then check the resulting folder info
  // for encryption coders. This involves adjusting tags to move the
  // StreamsInfo from [1] to [2], and updating the signature header
  // accordingly. [1]
  // https://github.com/jljusten/LZMA-SDK/blob/b7f8583f8d78dbbbdab0c9e6b62ab0437c8b404d/DOC/7zFormat.txt#L464
  // [2]
  // https://github.com/jljusten/LZMA-SDK/blob/b7f8583f8d78dbbbdab0c9e6b62ab0437c8b404d/DOC/7zFormat.txt#L450

  // Constants from 7zArcIn.c
  enum : Byte {
    k7zIdEnd = 0,
    k7zIdHeader = 1,
    k7zIdMainStreamsInfo = 4,
    k7zIdEncodedHeader = 23,
  };

  auto signature_header = base::HeapArray<Byte>::WithSize(k7zStartHeaderSize);
  size_t bytes_read = signature_header.size();
  if (LookInStream_SeekTo(archive_stream, 0) != SZ_OK ||
      ILookInStream_Read(archive_stream, signature_header.data(),
                         &bytes_read) != SZ_OK ||
      bytes_read != k7zStartHeaderSize) {
    return false;
  }

  base::HeapArray<Byte> header;
  size_t header_offset;
  {
    base::BufferIterator<Byte> iterator(signature_header);
    iterator.Seek(12);  // Seek to header information
    std::optional<uint64_t> header_offset_or = iterator.CopyObject<uint64_t>();
    std::optional<uint64_t> header_size_or = iterator.CopyObject<uint64_t>();
    if (!header_offset_or.has_value() || !header_size_or.has_value()) {
      return false;
    }

    int64_t archive_size = 0;
    if (ILookInStream_Seek(archive_stream, &archive_size, SZ_SEEK_END) !=
            SZ_OK ||
        archive_size < 0 || header_size_or > archive_size) {
      return false;
    }

    header = base::HeapArray<Byte>::WithSize(*header_size_or);
    header_offset = *header_offset_or;
    bytes_read = header.size();
    if (LookInStream_SeekTo(archive_stream,
                            k7zStartHeaderSize + header_offset) != SZ_OK ||
        ILookInStream_Read(archive_stream, header.data(), &bytes_read) !=
            SZ_OK ||
        bytes_read != header.size()) {
      return false;
    }
  }

  if (header.size() == 0 || header[0] != k7zIdEncodedHeader) {
    return false;
  }

  auto modified_header = base::HeapArray<Byte>::WithSize(header.size() + 2);
  {
    base::SpanWriter<Byte> iterator(modified_header);
    iterator.WriteU8LittleEndian(k7zIdHeader);
    iterator.WriteU8LittleEndian(k7zIdMainStreamsInfo);
    iterator.Write(header.subspan(1));
    iterator.WriteU8LittleEndian(k7zIdEnd);
  }

  auto modified_signature_header =
      base::HeapArray<Byte>::WithSize(k7zStartHeaderSize);
  modified_signature_header.copy_from(signature_header);
  {
    // The NextHeaderSize is an unaligned 64-bit number, so use
    // SpanWriter instead of BufferIterator.
    base::SpanWriter<Byte> iterator(modified_signature_header);
    iterator.Skip(20u);  // Fast-forward to
                         // SignatureHeader.NextHeaderSize.
    iterator.WriteU64LittleEndian(modified_header.size());
    iterator.WriteU32LittleEndian(
        CrcCalc(modified_header.data(), modified_header.size()));
  }
  {
    base::SpanWriter<Byte> iterator(modified_signature_header);
    iterator.Skip(8u);  // Fast-forward to SignatureHeaderCRC
    base::span<Byte> checksum_data = modified_signature_header.subspan(12);
    iterator.WriteU32LittleEndian(
        CrcCalc(checksum_data.data(), checksum_data.size()));
  }

  // Write the modified archive to the temp file.
  if (temp_file.Seek(base::File::FROM_BEGIN, 0) < 0 ||
      !temp_file.SetLength(0) ||
      !temp_file.WriteAtCurrentPosAndCheck(modified_signature_header) ||
      LookInStream_SeekTo(archive_stream, k7zStartHeaderSize) != SZ_OK ||
      !CopyStream(archive_stream, temp_file, header_offset) ||
      !temp_file.WriteAtCurrentPosAndCheck(modified_header) ||
      temp_file.Seek(base::File::FROM_BEGIN, 0) < 0) {
    return false;
  }

  SevenZipReaderImpl modified_archive_reader(std::move(temp_file),
                                             base::NullCallback());
  if (modified_archive_reader.Open() != Result::kSuccess) {
    return false;
  }
  return modified_archive_reader.IsFolderEncrypted(0);
}

Result SevenZipReaderImpl::ExtractIntoTempFile(size_t folder_index) {
  DCHECK_NE(folder_index, kNoFolder);
  if (!temp_file_.IsValid()) {
    temp_file_ = std::move(temp_file_request_callback_).Run();
  }
  DCHECK(temp_file_.IsValid());

  // Skip extraction if `folder_index` has already been extracted into
  // `temp_file_mapped_`.
  if (temp_folder_index_ == folder_index)
    return Result::kSuccess;

  // Forget about a mapping for a previous folder, if there was one.
  temp_file_mapped_.reset();
  temp_folder_index_ = kNoFolder;

  uint64_t folder_unpack_size = SzAr_GetFolderUnpackSize(&db_.db, folder_index);
  temp_file_mapped_.emplace();
  if (!temp_file_mapped_->Initialize(
          temp_file_.Duplicate(), {0, static_cast<size_t>(folder_unpack_size)},
          base::MemoryMappedFile::READ_WRITE_EXTEND)) {
    temp_file_mapped_.reset();
    return Result::kMemoryMappingFailed;
  }

  const base::span<uint8_t> temp_file_span = temp_file_mapped_->mutable_bytes();
  SRes sz_res = SzAr_DecodeFolder(&db_.db, folder_index, in_stream(),
                                  db_.dataPos, temp_file_span.data(),
                                  temp_file_span.size(), &alloc_temp_);

  if (sz_res != SZ_OK) {
    temp_file_mapped_.reset();
    return SResToResult(sz_res);
  }

  temp_folder_index_ = folder_index;
  return Result::kSuccess;
}

bool SevenZipReaderImpl::IsFolderEncrypted(size_t folder_index) const {
  if (folder_index == kNoFolder) {
    return false;
  }

  if (folder_index >= db_.db.NumFolders) {
    return false;
  }

  CSzData span;
  span.Data = db_.db.CodersData + db_.db.FoCodersOffsets[folder_index];
  span.Size = db_.db.FoCodersOffsets[folder_index + 1] -
              db_.db.FoCodersOffsets[folder_index];

  CSzFolder folder;
  SzGetNextFolderItem(&folder, &span);

  for (size_t i = 0; i < folder.NumCoders && i < SZ_NUM_CODERS_IN_FOLDER_MAX;
       ++i) {
    // LZMA SDK's DOC/Methods.txt specifies that MethodIDs starting with 06
    // are "Crypto" of some sort.
    if ((folder.Coders[i].MethodID & 0xff000000) == 0x06000000) {
      return true;
    }
  }

  return false;
}

#if BUILDFLAG(IS_WIN)

// define NTSTATUS to avoid including winternl.h
using NTSTATUS = LONG;

// Returns EXCEPTION_EXECUTE_HANDLER and populates `status` with the underlying
// NTSTATUS code for paging errors encountered while accessing file-backed
// mapped memory. Otherwise, return EXCEPTION_CONTINUE_SEARCH.
DWORD FilterPageError(const base::span<uint8_t>& mapped_file,
                      const base::span<uint8_t>& output,
                      DWORD exception_code,
                      const EXCEPTION_POINTERS* info,
                      int32_t* status) {
  if (exception_code != EXCEPTION_IN_PAGE_ERROR)
    return EXCEPTION_CONTINUE_SEARCH;

  const EXCEPTION_RECORD* exception_record = info->ExceptionRecord;
  const uint8_t* address = reinterpret_cast<const uint8_t*>(
      exception_record->ExceptionInformation[1]);
  if ((mapped_file.data() <= address &&
       address < mapped_file.data() + mapped_file.size()) ||
      (output.data() <= address && address < output.data() + output.size())) {
    // Cast NTSTATUS to int32_t to avoid including winternl.h
    *status = exception_record->ExceptionInformation[2];

    return EXCEPTION_EXECUTE_HANDLER;
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace internal

// SevenZipReader --------------------------------------------------------------

// static
std::unique_ptr<SevenZipReader> SevenZipReader::Create(
    base::File seven_zip_file,
    Delegate& delegate) {
  DCHECK(seven_zip_file.IsValid());
  // Unretained is safe here because the delegate is required to outlive
  // the returned `SevenZipReader`, and the `SevenZipReaderImpl` only
  // ever runs the callback synchronously.
  auto impl = std::make_unique<internal::SevenZipReaderImpl>(
      std::move(seven_zip_file), base::BindOnce(&Delegate::OnTempFileRequest,
                                                base::Unretained(&delegate)));
  Result open_result = impl->Open();
  if (open_result != Result::kSuccess) {
    delegate.OnOpenError(open_result);
    return nullptr;
  }

  return base::WrapUnique(new SevenZipReader(std::move(impl), delegate));
}

SevenZipReader::~SevenZipReader() = default;

void SevenZipReader::Extract() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (entry_index_ < impl_->num_entries()) {
    bool should_continue = ExtractEntry();
    entry_index_++;
    if (!should_continue)
      break;
  }
}

SevenZipReader::SevenZipReader(
    std::unique_ptr<internal::SevenZipReaderImpl> impl,
    Delegate& delegate)
    : impl_(std::move(impl)), delegate_(delegate) {}

bool SevenZipReader::ExtractEntry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EntryInfo entry = impl_->GetEntryInfo(entry_index_);
  if (entry.file_path.empty()) {
    return delegate_.EntryDone(Result::kNoFilename, entry);
  }

  if (impl_->IsDirectory(entry_index_)) {
    return delegate_.OnDirectory(entry);
  }

  base::span<uint8_t> output;
  if (!delegate_.OnEntry(entry, output))
    return false;
  CHECK_EQ(output.size(), entry.file_size);

  Result extract_result = Result::kUnknownError;
#if BUILDFLAG(IS_WIN)
  int32_t ntstatus = 0;
  __try {
    extract_result = impl_->ExtractFile(entry_index_, output);
  } __except (internal::FilterPageError(impl_->mapped_span(), output,
                                        GetExceptionCode(),
                                        GetExceptionInformation(), &ntstatus)) {
    LOG(ERROR) << "EXCEPTION_IN_PAGE_ERROR while accessing mapped memory; "
                  "NTSTATUS = "
               << ntstatus;
    // Return kIoError for all known errors except DISK_FULL.
    switch (ntstatus) {
      case STATUS_DEVICE_DATA_ERROR:
      case STATUS_DEVICE_HARDWARE_ERROR:
      case STATUS_DEVICE_NOT_CONNECTED:
      case STATUS_INVALID_DEVICE_REQUEST:
      case STATUS_INVALID_LEVEL:
      case STATUS_IO_DEVICE_ERROR:
      case STATUS_IO_TIMEOUT:
      case STATUS_NO_SUCH_DEVICE:
        extract_result = Result::kIoError;
        break;
      case STATUS_DISK_FULL:
        extract_result = Result::kDiskFull;
        break;
      default:
        // This error indicates an unexpected error. Spikes in this are
        // worth investigation.
        extract_result = Result::kUnknownError;
        break;
    }
  }
#else
  extract_result = impl_->ExtractFile(entry_index_, output);
#endif  // BUILDFLAG(IS_WIN)

  return delegate_.EntryDone(extract_result, entry);
}

void EnsureLzmaSdkInitialized() {
  [[maybe_unused]] static const bool initialized = []() {
    CrcGenerateTable();
    return true;
  }();
}

}  // namespace seven_zip
