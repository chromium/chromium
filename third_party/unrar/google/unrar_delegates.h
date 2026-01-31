#ifndef THIRD_PARTY_UNRAR_GOOGLE_UNRAR_DELEGATES_H_
#define THIRD_PARTY_UNRAR_GOOGLE_UNRAR_DELEGATES_H_

#include "base/containers/span.h"

namespace third_party_unrar {

// Delegate interface to provide file access to the RarReader.
class RarReaderDelegate {
 public:
  virtual ~RarReaderDelegate() = default;
  // Reads the next chunk of data. Returns the number of bytes read and -1 on error.
  virtual int64_t Read(base::span<uint8_t> data) = 0;
  // Seeks to the specified offset. Returns true on success.
  virtual bool Seek(int64_t offset) = 0;
  // Gets the current offset. Returns -1 on error.
  virtual int64_t Tell() = 0;
  // Gets the size of the file. Returns -1 on error.
  virtual int64_t GetLength() = 0;
};

// Delegate interface for writing extracted data.
class RarWriterDelegate {
 public:
  virtual ~RarWriterDelegate() = default;

  // Writes the given data to the output. Returns true on success.
  virtual bool Write(base::span<const uint8_t> data) = 0;

  virtual void Close() = 0;
};

}  // namespace third_party_unrar

#endif  // THIRD_PARTY_UNRAR_GOOGLE_UNRAR_DELEGATES_H_
