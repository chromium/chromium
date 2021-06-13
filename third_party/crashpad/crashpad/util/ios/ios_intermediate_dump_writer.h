// Copyright 2021 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATE_DUMP_WRITER_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATE_DUMP_WRITER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "util/ios/ios_intermediate_dump_format.h"

namespace crashpad {
namespace internal {

//! \brief Wrapper class for writing intermediate dump file.
//!
//! Due to the limitations of in-process handling, an intermediate dump file is
//! written during exceptions. The data is streamed to a file using only
//! in-process safe methods.
//!
//! The file format is similar to binary JSON, supporting keyed properties, maps
//! and arrays.
//!  - Property [key:int, length:int, value:intarray]
//!  - StartMap [key:int], followed by repeating Properties until EndMap
//!  - StartArray [key:int], followed by repeating Maps until EndArray
//!  - EndMap, EndArray, EndDocument
//!
//!  Similar to JSON, maps can contain other maps, arrays and properties.
//!
//! Note: All methods are `RUNS-DURING-CRASH`.
class IOSIntermediateDumpWriter final {
 public:
  IOSIntermediateDumpWriter() = default;

  //! \brief Command instructions for the intermediate dump reader.
  enum class CommandType : uint8_t {
    //! \brief Indicates a new map, followed by associated key.
    kMapStart = 0x01,

    //! \brief Indicates map is complete.
    kMapEnd = 0x02,

    //! \brief Indicates a new array, followed by associated key.
    kArrayStart = 0x03,

    //! \brief Indicates array is complete.
    kArrayEnd = 0x04,

    //! \brief Indicates a new property, followed by a key, length and value.
    kProperty = 0x05,

    //! \brief Indicates the start of the root map.
    kRootMapStart = 0x06,

    //! \brief Indicates the end of the root map, and that there is nothing left
    //!     to parse.
    kRootMapEnd = 0x07,
  };

  //! \brief Open and lock an intermediate dump file. This is the only method
  //!     in the writer class that is generally run outside of a crash.
  //!
  //! \param[in] path The path to the intermediate dump.
  //!
  //! \return On success, returns `true`, otherwise returns `false`.
  bool Open(const base::FilePath& path);

  //! \brief Completes writing the intermediate dump file and releases the
  //!     file handle.
  //!
  //! \return On success, returns `true`, otherwise returns `false`.
  bool Close();

  //! \brief A scoped wrapper for calls to RootMapStart and RootMapEnd.
  class ScopedRootMap {
   public:
    explicit ScopedRootMap(IOSIntermediateDumpWriter* writer)
        : writer_(writer) {
      writer->RootMapStart();
    }
    ~ScopedRootMap() { writer_->RootMapEnd(); }

   private:
    IOSIntermediateDumpWriter* writer_;
    DISALLOW_COPY_AND_ASSIGN(ScopedRootMap);
  };

  //! \brief A scoped wrapper for calls to MapStart and MapEnd.
  class ScopedMap {
   public:
    explicit ScopedMap(IOSIntermediateDumpWriter* writer,
                       IntermediateDumpKey key)
        : writer_(writer) {
      writer->MapStart(key);
    }
    ~ScopedMap() { writer_->MapEnd(); }

   private:
    IOSIntermediateDumpWriter* writer_;
    DISALLOW_COPY_AND_ASSIGN(ScopedMap);
  };

  //! \brief A scoped wrapper for calls to ArrayMapStart and MapEnd.
  class ScopedArrayMap {
   public:
    explicit ScopedArrayMap(IOSIntermediateDumpWriter* writer)
        : writer_(writer) {
      writer->ArrayMapStart();
    }
    ~ScopedArrayMap() { writer_->MapEnd(); }

   private:
    IOSIntermediateDumpWriter* writer_;
    DISALLOW_COPY_AND_ASSIGN(ScopedArrayMap);
  };

  //! \brief A scoped wrapper for calls to ArrayStart and ArrayEnd.
  class ScopedArray {
   public:
    explicit ScopedArray(IOSIntermediateDumpWriter* writer,
                         IntermediateDumpKey key)
        : writer_(writer) {
      writer->ArrayStart(key);
    }
    ~ScopedArray() { writer_->ArrayEnd(); }

   private:
    IOSIntermediateDumpWriter* writer_;
    DISALLOW_COPY_AND_ASSIGN(ScopedArray);
  };

  //! \return The `true` if able to AddPropertyInternal the \a key \a value
  //!     \a count tuple.
  template <typename T>
  bool AddProperty(IntermediateDumpKey key, const T* value, size_t count = 1) {
    return AddPropertyInternal(
        key, reinterpret_cast<const char*>(value), count * sizeof(T));
  }

  //! \return The `true` if able to AddPropertyInternal the \a key \a value
  //!     \a count tuple.
  bool AddPropertyBytes(IntermediateDumpKey key,
                        const void* value,
                        size_t value_length) {
    return AddPropertyInternal(
        key, reinterpret_cast<const char*>(value), value_length);
  }

 private:
  //! \return Returns `true` if able to write a kProperty command  with the
  //!     \a key \a value \a count tuple.
  bool AddPropertyInternal(IntermediateDumpKey key,
                           const char* value,
                           size_t value_length);

  //! \return Returns `true` if able to write a kArrayStart command  with the
  //!     \a key.
  bool ArrayStart(IntermediateDumpKey key);

  //! \return Returns `true` if able to write a kMapStart command with the
  //!     \a key.
  bool MapStart(IntermediateDumpKey key);

  //! \return Returns `true` if able to write a kMapStart command.
  bool ArrayMapStart();

  //! \return Returns `true` if able to write a kArrayEnd command.
  bool ArrayEnd();

  //! \return Returns `true` if able to write a kMapEnd command.
  bool MapEnd();

  //! \return Returns `true` if able to write a kRootMapStart command.
  bool RootMapStart();

  //! \return Returns `true` if able to write a kRootMapEnd command.
  bool RootMapEnd();

  int fd_;

  DISALLOW_COPY_AND_ASSIGN(IOSIntermediateDumpWriter);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATE_DUMP_WRITER_H_
