// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/framework/execution_provider.h"
#include "core/framework/kernel_def_builder.h"
#include "core/framework/ort_value.h"
#include "core/framework/op_node_proto_helper.h"
#include "core/graph/graph_viewer.h"
#include <gsl/gsl>

namespace onnxruntime {

class DataTransferManager;
class FuncManager;
class OrtValueNameIdxMap;
struct AllocPlanPerValue;

// A very light-weight class, which works as an aggregated
// view of all data needed for constructing a Kernel instance.
// NOTE: it does not own/hold any objects.
class OpKernelInfo : public OpNodeProtoHelper<ProtoHelperNodeContext> {
 public:
  explicit OpKernelInfo(const onnxruntime::Node& node,
                        const KernelDef& kernel_def,
                        const IExecutionProvider& execution_provider,
                        const std::unordered_map<int, OrtValue>& constant_initialized_tensors,
                        const OrtValueNameIdxMap& mlvalue_name_idx_map,
                        const DataTransferManager& data_transfer_mgr,
                        const AllocatorMap& allocators,
                        const ConfigOptions& config_options);

  OpKernelInfo(const OpKernelInfo& other);

  const OrtDevice GetDevice(OrtMemType mem_type) const;

  AllocatorPtr GetAllocator(OrtMemType mem_type) const;

  const KernelDef& GetKernelDef() const;

  const IExecutionProvider* GetExecutionProvider() const noexcept;

  const DataTransferManager& GetDataTransferManager() const noexcept;

  const onnxruntime::Node& node() const noexcept;

  bool TryGetConstantInput(int input_index, const Tensor** constant_input_value) const;

  bool TryGetConstantInput(int input_index, const OrtValue** constant_input_value) const;

  const AllocatorMap& GetAllocators() const { return allocators_; }

  const ConfigOptions& GetConfigOptions() const { return config_options_; }

 private:
  ORT_DISALLOW_MOVE(OpKernelInfo);
  ORT_DISALLOW_ASSIGNMENT(OpKernelInfo);

  const onnxruntime::Node& node_;
  const KernelDef& kernel_def_;
  // For non cpu/cuda case, this pointer should be set so that function kernel
  // will delegate kernel compute call to <execution_provider> compute call.
  gsl::not_null<const ::onnxruntime::IExecutionProvider*> execution_provider_;
  const std::unordered_map<int, OrtValue>& constant_initialized_tensors_;
  const OrtValueNameIdxMap& ort_value_name_idx_map_;
  const DataTransferManager& data_transfer_mgr_;
  ProtoHelperNodeContext proto_helper_context_;
  const AllocatorMap& allocators_;
  const ConfigOptions& config_options_;
};

}  // namespace onnxruntime
