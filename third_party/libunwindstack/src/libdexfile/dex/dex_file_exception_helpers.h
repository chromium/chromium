/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_EXCEPTION_HELPERS_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_EXCEPTION_HELPERS_H_

#include <android-base/logging.h>

#include "dex_file_types.h"

namespace art {

namespace dex {
struct TryItem;
}  // namespace dex

class CodeItemDataAccessor;

class CatchHandlerIterator {
 public:
  CatchHandlerIterator(const CodeItemDataAccessor& accessor, uint32_t address);

  CatchHandlerIterator(const CodeItemDataAccessor& accessor, const dex::TryItem& try_item);

  explicit CatchHandlerIterator(const uint8_t* handler_data) {
    Init(handler_data);
  }

  dex::TypeIndex GetHandlerTypeIndex() const {
    return handler_.type_idx_;
  }
  uint32_t GetHandlerAddress() const {
    return handler_.address_;
  }
  void Next();
  bool HasNext() const {
    return remaining_count_ != -1 || catch_all_;
  }
  // End of this set of catch blocks, convenience method to locate next set of catch blocks
  const uint8_t* EndDataPointer() const {
    CHECK(!HasNext());
    return current_data_;
  }

 private:
  void Init(const CodeItemDataAccessor& accessor, int32_t offset);
  void Init(const uint8_t* handler_data);

  struct CatchHandlerItem {
    dex::TypeIndex type_idx_;  // type index of the caught exception type
    uint32_t address_;  // handler address
  } handler_;
  const uint8_t* current_data_;  // the current handler in dex file.
  int32_t remaining_count_;   // number of handlers not read.
  bool catch_all_;            // is there a handler that will catch all exceptions in case
                              // that all typed handler does not match.
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_EXCEPTION_HELPERS_H_
