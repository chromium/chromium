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

#include "dex_file_exception_helpers.h"

#include "code_item_accessors-inl.h"
#include "dex_file_structs.h"

namespace art {

CatchHandlerIterator::CatchHandlerIterator(const CodeItemDataAccessor& accessor, uint32_t address) {
  handler_.address_ = -1;
  int32_t offset = -1;

  // Short-circuit the overwhelmingly common cases.
  switch (accessor.TriesSize()) {
    case 0:
      break;
    case 1: {
      const dex::TryItem* tries = accessor.TryItems().begin();
      uint32_t start = tries->start_addr_;
      if (address >= start) {
        uint32_t end = start + tries->insn_count_;
        if (address < end) {
          offset = tries->handler_off_;
        }
      }
      break;
    }
    default: {
      const dex::TryItem* try_item = accessor.FindTryItem(address);
      offset = try_item != nullptr ? try_item->handler_off_ : -1;
      break;
    }
  }
  Init(accessor, offset);
}

CatchHandlerIterator::CatchHandlerIterator(const CodeItemDataAccessor& accessor,
                                           const dex::TryItem& try_item) {
  handler_.address_ = -1;
  Init(accessor, try_item.handler_off_);
}

void CatchHandlerIterator::Init(const CodeItemDataAccessor& accessor, int32_t offset) {
  if (offset >= 0) {
    Init(accessor.GetCatchHandlerData(offset));
  } else {
    // Not found, initialize as empty
    current_data_ = nullptr;
    remaining_count_ = -1;
    catch_all_ = false;
    DCHECK(!HasNext());
  }
}

void CatchHandlerIterator::Init(const uint8_t* handler_data) {
  current_data_ = handler_data;
  remaining_count_ = DecodeSignedLeb128(&current_data_);

  // If remaining_count_ is non-positive, then it is the negative of
  // the number of catch types, and the catches are followed by a
  // catch-all handler.
  if (remaining_count_ <= 0) {
    catch_all_ = true;
    remaining_count_ = -remaining_count_;
  } else {
    catch_all_ = false;
  }
  Next();
}

void CatchHandlerIterator::Next() {
  if (remaining_count_ > 0) {
    handler_.type_idx_ = dex::TypeIndex(DecodeUnsignedLeb128(&current_data_));
    handler_.address_  = DecodeUnsignedLeb128(&current_data_);
    remaining_count_--;
    return;
  }

  if (catch_all_) {
    handler_.type_idx_ = dex::TypeIndex(DexFile::kDexNoIndex16);
    handler_.address_  = DecodeUnsignedLeb128(&current_data_);
    catch_all_ = false;
    return;
  }

  // no more handler
  remaining_count_ = -1;
}

}  // namespace art
