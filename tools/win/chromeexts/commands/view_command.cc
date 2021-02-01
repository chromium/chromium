// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/commands/view_command.h"

#include <dbgeng.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"

namespace tools {
namespace win {
namespace chromeexts {

namespace {

using Microsoft::WRL::ComPtr;

class VirtualMemoryBlock {
 public:
  VirtualMemoryBlock(IDebugClient* debug_client,
                     const std::string symbol,
                     uint64_t address) {
    unsigned long type_size;
    if (FAILED(debug_client->QueryInterface(IID_PPV_ARGS(&symbols_))) ||
        FAILED(
            symbols_->GetSymbolTypeId(symbol.c_str(), &type_id_, &module_)) ||
        FAILED(symbols_->GetTypeSize(module_, type_id_, &type_size))) {
      return;
    }

    ComPtr<IDebugDataSpaces> data;
    if (FAILED(symbols_.As(&data))) {
      return;
    }

    storage_.resize(type_size);
    data->ReadVirtual(address, storage_.data(), type_size, nullptr);
  }

  ~VirtualMemoryBlock() = default;

  template <typename T>
  const T& As() const {
    return *reinterpret_cast<const T*>(storage_.data());
  }

  template <typename T>
  T GetFieldValue(std::string field_name) const {
    unsigned long field_type_id;
    unsigned long field_offset;
    unsigned long field_size;
    if (FAILED(symbols_->GetFieldTypeAndOffset(
            module_, type_id_, field_name.c_str(), &field_type_id,
            &field_offset)) ||
        FAILED(symbols_->GetTypeSize(module_, field_type_id, &field_size))) {
      return T();
    }

    return *reinterpret_cast<const T*>(storage_.data() + field_offset);
  }

  VirtualMemoryBlock GetFieldMemoryBlock(std::string field_name) const {
    unsigned long field_type_id;
    unsigned long field_offset;
    unsigned long field_size;
    if (FAILED(symbols_->GetFieldTypeAndOffset(
            module_, type_id_, field_name.c_str(), &field_type_id,
            &field_offset)) ||
        FAILED(symbols_->GetTypeSize(module_, field_type_id, &field_size))) {
      return VirtualMemoryBlock();
    }
    VirtualMemoryBlock field;
    field.symbols_ = symbols_;
    auto start = storage_.cbegin() + field_offset;
    auto end = start + field_size;
    field.storage_ = std::vector<char>(start, end);
    field.module_ = module_;
    field.type_id_ = field_type_id;
    return field;
  }

 private:
  VirtualMemoryBlock() = default;

  ComPtr<IDebugSymbols3> symbols_;
  std::vector<char> storage_;
  uint64_t module_ = 0;
  unsigned long type_id_ = 0;
};

template <typename T>
std::vector<T> ReadVirtualVector(IDebugDataSpaces* data,
                                 const std::vector<T>& vector) {
  size_t size = vector.size();
  std::vector<T> values(size);
  values.reserve(vector.capacity());
  data->ReadVirtual(reinterpret_cast<uint64_t>(vector.data()), values.data(),
                    sizeof(T) * size, nullptr);
  return values;
}

}  // namespace

ViewCommand::ViewCommand() = default;

ViewCommand::~ViewCommand() = default;

HRESULT ViewCommand::Execute() {
  auto remaining_arguments = command_line().GetArgs();
  if (remaining_arguments.size() > 1) {
    Printf("Unexpected number of arguments %d\n", remaining_arguments.size());
  }

  DEBUG_VALUE address_debug_value{};
  if (FAILED(GetDebugClientAs<IDebugControl>()->Evaluate(
          base::WideToASCII(remaining_arguments[0]).c_str(), DEBUG_VALUE_INT64,
          &address_debug_value, nullptr))) {
    Printf("Unevaluatable Expression %ws", remaining_arguments[0].c_str());
  }

  uint64_t address = reinterpret_cast<uint64_t>(address_debug_value.I64);

  VirtualMemoryBlock view_block(GetDebugClientAs<IDebugClient>().Get(),
                                "views!views::View", address);

  if (command_line().HasSwitch("children")) {
    auto children_block = view_block.GetFieldMemoryBlock("children_");
    auto& children = children_block.As<std::vector<intptr_t>>();

    Printf("Child Count: %d\n", children.size());
    std::vector<intptr_t> children_ptrs =
        ReadVirtualVector(GetDebugClientAs<IDebugDataSpaces>().Get(), children);

    for (auto val : children_ptrs) {
      Printf("%x ", val);
    }
    Printf("\n");
  } else {
    VirtualMemoryBlock bounds_block = view_block.GetFieldMemoryBlock("bounds_");
    VirtualMemoryBlock origin_block =
        bounds_block.GetFieldMemoryBlock("origin_");
    VirtualMemoryBlock size_block = bounds_block.GetFieldMemoryBlock("size_");
    Printf("Bounds: %d,%d (%dx%d)\n", origin_block.GetFieldValue<int>("x_"),
           origin_block.GetFieldValue<int>("y_"),
           size_block.GetFieldValue<int>("width_"),
           size_block.GetFieldValue<int>("height_"));
    Printf("Parent: 0x%08x\n", view_block.GetFieldValue<intptr_t>("parent_"));
  }

  return S_OK;
}

}  // namespace chromeexts
}  // namespace win
}  // namespace tools
