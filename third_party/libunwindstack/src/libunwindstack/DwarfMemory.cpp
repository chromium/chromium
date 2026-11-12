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

#include <stdint.h>

#include <string>

#include <unwindstack/DwarfMemory.h>
#include <unwindstack/Memory.h>

#include "Check.h"
#include "DwarfEncoding.h"

namespace unwindstack {

bool DwarfMemory::ReadBytes(void* dst, size_t num_bytes) {
  if (!memory_->ReadFully(cur_offset_, dst, num_bytes)) {
    return false;
  }
  cur_offset_ += num_bytes;
  return true;
}

template <typename SignedType>
bool DwarfMemory::ReadSigned(uint64_t* value) {
  SignedType signed_value;
  if (!ReadBytes(&signed_value, sizeof(SignedType))) {
    return false;
  }
  *value = static_cast<int64_t>(signed_value);
  return true;
}

bool DwarfMemory::ReadULEB128(uint64_t* value) {
  uint64_t cur_value = 0;
  uint64_t shift = 0;
  uint8_t byte;
  do {
    if (!ReadBytes(&byte, 1)) {
      return false;
    }
    cur_value += static_cast<uint64_t>(byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);
  *value = cur_value;
  return true;
}

bool DwarfMemory::ReadSLEB128(int64_t* value) {
  uint64_t cur_value = 0;
  uint64_t shift = 0;
  uint8_t byte;
  do {
    if (!ReadBytes(&byte, 1)) {
      return false;
    }
    cur_value += static_cast<uint64_t>(byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);
  if (byte & 0x40) {
    // Negative value, need to sign extend.
    cur_value |= static_cast<uint64_t>(-1) << shift;
  }
  *value = static_cast<int64_t>(cur_value);
  return true;
}

template <typename AddressType>
size_t DwarfMemory::GetEncodedSize(uint8_t encoding) {
  switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
      return sizeof(AddressType);
    case DW_EH_PE_udata1:
    case DW_EH_PE_sdata1:
      return 1;
    case DW_EH_PE_udata2:
    case DW_EH_PE_sdata2:
      return 2;
    case DW_EH_PE_udata4:
    case DW_EH_PE_sdata4:
      return 4;
    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8:
      return 8;
    case DW_EH_PE_uleb128:
    case DW_EH_PE_sleb128:
    default:
      return 0;
  }
}

bool DwarfMemory::AdjustEncodedValue(uint8_t encoding, uint64_t* value) {
  CHECK((encoding & 0x0f) == 0);

  // Handle the encoding.
  switch (encoding) {
    case DW_EH_PE_absptr:
      // Nothing to do.
      break;
    case DW_EH_PE_pcrel:
      if (pc_offset_ == INT64_MAX) {
        // Unsupported encoding.
        return false;
      }
      *value += pc_offset_;
      break;
    case DW_EH_PE_textrel:
      if (text_offset_ == static_cast<uint64_t>(-1)) {
        // Unsupported encoding.
        return false;
      }
      *value += text_offset_;
      break;
    case DW_EH_PE_datarel:
      if (data_offset_ == static_cast<uint64_t>(-1)) {
        // Unsupported encoding.
        return false;
      }
      *value += data_offset_;
      break;
    case DW_EH_PE_funcrel:
      if (func_offset_ == static_cast<uint64_t>(-1)) {
        // Unsupported encoding.
        return false;
      }
      *value += func_offset_;
      break;
    default:
      return false;
  }

  return true;
}

template <typename AddressType>
bool DwarfMemory::ReadEncodedValue(uint8_t encoding, uint64_t* value) {
  if (encoding == DW_EH_PE_omit) {
    *value = 0;
    return true;
  } else if (encoding == DW_EH_PE_aligned) {
    if (__builtin_add_overflow(cur_offset_, sizeof(AddressType) - 1, &cur_offset_)) {
      return false;
    }
    cur_offset_ &= -sizeof(AddressType);

    if (sizeof(AddressType) != sizeof(uint64_t)) {
      *value = 0;
    }
    return ReadBytes(value, sizeof(AddressType));
  }

  // Get the data.
  switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
      if (sizeof(AddressType) != sizeof(uint64_t)) {
        *value = 0;
      }
      if (!ReadBytes(value, sizeof(AddressType))) {
        return false;
      }
      break;
    case DW_EH_PE_uleb128:
      if (!ReadULEB128(value)) {
        return false;
      }
      break;
    case DW_EH_PE_sleb128:
      int64_t signed_value;
      if (!ReadSLEB128(&signed_value)) {
        return false;
      }
      *value = static_cast<uint64_t>(signed_value);
      break;
    case DW_EH_PE_udata1: {
      uint8_t value8;
      if (!ReadBytes(&value8, 1)) {
        return false;
      }
      *value = value8;
    } break;
    case DW_EH_PE_sdata1:
      if (!ReadSigned<int8_t>(value)) {
        return false;
      }
      break;
    case DW_EH_PE_udata2: {
      uint16_t value16;
      if (!ReadBytes(&value16, 2)) {
        return false;
      }
      *value = value16;
    } break;
    case DW_EH_PE_sdata2:
      if (!ReadSigned<int16_t>(value)) {
        return false;
      }
      break;
    case DW_EH_PE_udata4: {
      uint32_t value32;
      if (!ReadBytes(&value32, 4)) {
        return false;
      }
      *value = value32;
    } break;
    case DW_EH_PE_sdata4:
      if (!ReadSigned<int32_t>(value)) {
        return false;
      }
      break;
    case DW_EH_PE_udata8:
      if (!ReadBytes(value, sizeof(uint64_t))) {
        return false;
      }
      break;
    case DW_EH_PE_sdata8:
      if (!ReadSigned<int64_t>(value)) {
        return false;
      }
      break;
    default:
      return false;
  }

  return AdjustEncodedValue(encoding & 0x70, value);
}

// Instantiate all of the needed template functions.
template bool DwarfMemory::ReadSigned<int8_t>(uint64_t*);
template bool DwarfMemory::ReadSigned<int16_t>(uint64_t*);
template bool DwarfMemory::ReadSigned<int32_t>(uint64_t*);
template bool DwarfMemory::ReadSigned<int64_t>(uint64_t*);

template size_t DwarfMemory::GetEncodedSize<uint32_t>(uint8_t);
template size_t DwarfMemory::GetEncodedSize<uint64_t>(uint8_t);

template bool DwarfMemory::ReadEncodedValue<uint32_t>(uint8_t, uint64_t*);
template bool DwarfMemory::ReadEncodedValue<uint64_t>(uint8_t, uint64_t*);

}  // namespace unwindstack
