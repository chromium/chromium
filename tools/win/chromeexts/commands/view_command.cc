// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/commands/view_command.h"

#include <windows.h>

#include <dbgeng.h>
#include <wrl/client.h>

#include <ostream>
#include <streambuf>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "ui/views/debug/debugger_utils.h"

namespace tools {
namespace win {
namespace chromeexts {

namespace {

using Microsoft::WRL::ComPtr;

class DebugOutputBuffer : public std::basic_streambuf<char> {
 public:
  DebugOutputBuffer(IDebugControl* debug_control)
      : debug_control_(debug_control) {}
  DebugOutputBuffer(const DebugOutputBuffer&) = delete;
  DebugOutputBuffer& operator=(const DebugOutputBuffer&) = delete;
  ~DebugOutputBuffer() override = default;

  std::streamsize xsputn(const char* s, std::streamsize count) override {
    std::string str(s, count);
    debug_control_->Output(DEBUG_OUTPUT_NORMAL, str.c_str());
    return count;
  }

 private:
  ComPtr<IDebugControl> debug_control_;
};

class VirtualMemoryBlock {
 public:
  VirtualMemoryBlock(IDebugClient* debug_client,
                     const std::string symbol,
                     uint64_t address)
      : address_(address) {
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
    data->ReadVirtual(address_, storage_.data(), type_size, nullptr);
  }

  ~VirtualMemoryBlock() = default;

  uint64_t address() const { return address_; }

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

  template <typename T>
  T GetValueFromOffset(size_t offset) const {
    return *reinterpret_cast<const T*>(storage_.data() + offset);
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

  uint64_t address_;
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

class VirtualViewDebugWrapper : public views::debug::ViewDebugWrapper {
 public:
  VirtualViewDebugWrapper(VirtualMemoryBlock view_block,
                          IDebugClient* debug_client)
      : view_block_(view_block), debug_client_(debug_client) {}
  VirtualViewDebugWrapper(const VirtualViewDebugWrapper&) = delete;
  VirtualViewDebugWrapper& operator=(const VirtualViewDebugWrapper&) = delete;
  ~VirtualViewDebugWrapper() override = default;

  std::string GetViewClassName() override {
    unsigned long vtable = view_block_.GetValueFromOffset<unsigned long>(0);

    ComPtr<IDebugSymbols3> symbols;
    debug_client_.As(&symbols);
    // TODO: Handle cross-DLL references.
    char buffer[255];
    buffer[0] = '\0';
    symbols->GetNameByOffset(vtable, buffer, ARRAYSIZE(buffer), nullptr,
                             nullptr);
    return buffer;
  }

  std::optional<intptr_t> GetAddress() override {
    return view_block_.address();
  }

  int GetID() override { return view_block_.GetFieldValue<int>("id_"); }
  BoundsTuple GetBounds() override {
    VirtualMemoryBlock bounds_block =
        view_block_.GetFieldMemoryBlock("bounds_");
    VirtualMemoryBlock origin_block =
        bounds_block.GetFieldMemoryBlock("origin_");
    VirtualMemoryBlock size_block = bounds_block.GetFieldMemoryBlock("size_");
    return BoundsTuple(origin_block.GetFieldValue<int>("x_"),
                       origin_block.GetFieldValue<int>("y_"),
                       size_block.GetFieldValue<int>("height_"),
                       size_block.GetFieldValue<int>("width_"));
  }
  bool GetVisible() override {
    return view_block_.GetFieldValue<bool>("visible_");
  }
  bool GetNeedsLayout() override {
    return view_block_.GetFieldValue<bool>("needs_layout_");
  }
  bool GetEnabled() override {
    return view_block_.GetFieldValue<bool>("enabled_");
  }
  std::vector<ViewDebugWrapper*> GetChildren() override {
    if (children_.empty()) {
      auto children_block = view_block_.GetFieldMemoryBlock("children_");
      auto& children = children_block.As<std::vector<intptr_t>>();
      children_.reserve(children.size());

      ComPtr<IDebugDataSpaces> debug_data_spaces;
      debug_client_.As(&debug_data_spaces);
      std::vector<intptr_t> virtual_children_ptrs =
          ReadVirtualVector(debug_data_spaces.Get(), children);
      for (intptr_t virtual_child_ptr : virtual_children_ptrs) {
        VirtualMemoryBlock child_memory_block(
            debug_client_.Get(), "views!views::View", virtual_child_ptr);
        children_.push_back(std::make_unique<VirtualViewDebugWrapper>(
            child_memory_block, debug_client_.Get()));
      }
    }

    std::vector<ViewDebugWrapper*> child_ptrs;
    child_ptrs.reserve(children_.size());
    for (auto& child : children_) {
      child_ptrs.push_back(child.get());
    }
    return child_ptrs;
  }

 private:
  VirtualMemoryBlock view_block_;
  ComPtr<IDebugClient> debug_client_;
  std::vector<std::unique_ptr<VirtualViewDebugWrapper>> children_;
};

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

    if (command_line().HasSwitch("r")) {
      DebugOutputBuffer buffer(GetDebugClientAs<IDebugControl>().Get());
      std::ostream out(&buffer);
      VirtualViewDebugWrapper root(view_block,
                                   GetDebugClientAs<IDebugClient>().Get());
      PrintViewHierarchy(&out, &root);
    } else {
      for (auto val : children_ptrs) {
        Printf("%x ", val);
      }
      Printf("\n");
    }
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
