// Copyright 2022 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_THREAD_NAME_LIST_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_THREAD_NAME_LIST_WRITER_H_

#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <vector>

#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_string_writer.h"
#include "minidump/minidump_writable.h"

namespace crashpad {

//! \brief The writer for a MINIDUMP_THREAD_NAME object in a minidump file.
//!
//! Because MINIDUMP_THREAD_NAME objects only appear as elements of
//! MINIDUMP_THREAD_NAME_LIST objects, this class does not write any data on its
//! own. It makes its MINIDUMP_THREAD_NAME data available to its
//! MinidumpThreadNameListWriter parent, which writes it as part of a
//! MINIDUMP_THREAD_NAME_LIST.
class MinidumpThreadNameWriter final : public internal::MinidumpWritable {
 public:
  MinidumpThreadNameWriter();

  MinidumpThreadNameWriter(const MinidumpThreadNameWriter&) = delete;
  MinidumpThreadNameWriter& operator=(const MinidumpThreadNameWriter&) = delete;

  ~MinidumpThreadNameWriter() override;

  //! \brief Returns a MINIDUMP_THREAD_NAME referencing this object’s data.
  //!
  //! This method is expected to be called by a MinidumpThreadNameListWriter in
  //! order to obtain a MINIDUMP_THREAD_NAME to include in its list.
  //!
  //! \note Valid in #kStateWritable.
  const MINIDUMP_THREAD_NAME* MinidumpThreadName() const;

  //! \brief Sets MINIDUMP_THREAD_NAME::ThreadId.
  void SetThreadId(uint32_t thread_id) { thread_name_.ThreadId = thread_id; }

  //! \brief Sets MINIDUMP_THREAD_NAME::RvaOfThreadName.
  void SetThreadName(const std::string& thread_name);

 private:
  // MinidumpWritable:
  size_t SizeOfObject() override;
  std::vector<MinidumpWritable*> Children() override;
  bool WillWriteAtOffsetImpl(FileOffset offset) override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  MINIDUMP_THREAD_NAME thread_name_;
  std::unique_ptr<internal::MinidumpUTF16StringWriter> name_;
};

//! \brief The writer for a MINIDUMP_THREAD_NAME_LIST stream in a minidump file,
//!     containing a list of MINIDUMP_THREAD_NAME objects.
class MinidumpThreadNameListWriter final
    : public internal::MinidumpStreamWriter {
 public:
  MinidumpThreadNameListWriter();

  MinidumpThreadNameListWriter(const MinidumpThreadNameListWriter&) = delete;
  MinidumpThreadNameListWriter& operator=(const MinidumpThreadNameListWriter&) =
      delete;

  ~MinidumpThreadNameListWriter() override;

  //! \brief Adds a MinidumpThreadNameWriter to the MINIDUMP_THREAD_LIST.
  //!
  //! This object takes ownership of \a thread_name and becomes its parent in
  //! the overall tree of internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void AddThreadName(std::unique_ptr<MinidumpThreadNameWriter> thread_name);

 private:
  // MinidumpWritable:
  bool Freeze() override;
  size_t SizeOfObject() override;
  std::vector<MinidumpWritable*> Children() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override;

  std::vector<std::unique_ptr<MinidumpThreadNameWriter>> thread_names_;
  MINIDUMP_THREAD_NAME_LIST thread_name_list_;
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_THREAD_NAME_LIST_WRITER_H_
