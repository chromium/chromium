// Copyright 2019 The Crashpad Authors
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

#include "util/stream/test_output_stream.h"
#include "base/check.h"

namespace crashpad {
namespace test {

TestOutputStream::TestOutputStream()
    : last_written_data_(),
      all_data_(),
      write_count_(0),
      flush_count_(0),
      flush_needed_(false) {}

TestOutputStream::~TestOutputStream() {
  DCHECK(!flush_needed_);
}

bool TestOutputStream::Write(const uint8_t* data, size_t size) {
  last_written_data_.assign(data, data + size);
  all_data_.insert(all_data_.end(), data, data + size);

  flush_needed_ = true;
  write_count_++;
  return true;
}

bool TestOutputStream::Flush() {
  flush_needed_ = false;
  flush_count_++;
  return true;
}

}  // namespace test
}  // namespace crashpad
