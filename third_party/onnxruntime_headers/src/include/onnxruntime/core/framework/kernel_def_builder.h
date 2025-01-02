// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <limits.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/common/common.h"
#include "core/framework/allocator.h"
#include "core/framework/data_types.h"
#include "core/graph/basic_types.h"

namespace onnxruntime {
class KernelDefBuilder;

typedef std::map<size_t, OrtMemType> MemTypeMap;

class KernelDef {
 private:
  // note that input/output might be on CPU implicitly when the node is from CPU execution provider
  constexpr static inline bool MemTypeOnCpuExplicitly(OrtMemType mem_type) {
    return mem_type == OrtMemTypeCPUInput || mem_type == OrtMemTypeCPUOutput;
  }

 public:
  explicit KernelDef() = default;

  const std::string& OpName() const {
    return op_name_;
  }

  const std::string& Domain() const {
    return op_domain_;
  }

  void SinceVersion(/*out*/ int* start, /*out*/ int* end) const {
    *start = op_since_version_start_;
    *end = op_since_version_end_;
  }

  const std::pair<int, int> SinceVersion() const {
    return std::pair<int, int>(op_since_version_start_, op_since_version_end_);
  }

  onnxruntime::ProviderType Provider() const {
    return provider_type_;
  }

  // type constraints with types supported in this build
  const std::unordered_map<std::string, std::vector<MLDataType>>& TypeConstraints() const {
    return type_constraints_;
  }

  const std::vector<std::pair<int, int>>& MayInplace() const {
    return inplace_map_;
  }

  const std::vector<std::pair<int, int>>& Alias() const {
    return alias_map_;
  }

  const std::optional<std::pair<int, int>>& VariadicAlias() const {
    return variadic_alias_offsets_;
  }

  OrtMemType InputMemoryType(size_t input_index) const {
    auto it = input_memory_type_args_.find(input_index);
    if (it == input_memory_type_args_.end())
      return default_inputs_mem_type_;
    return it->second;
  }

  bool IsInputOnCpu(size_t input_index) const { return MemTypeOnCpuExplicitly(InputMemoryType(input_index)); }

  bool IsOutputOnCpu(size_t output_index) const { return MemTypeOnCpuExplicitly(OutputMemoryType(output_index)); }

  bool AllocateInputsContiguously() const { return allocate_inputs_contiguously_; }

  bool HasExternalOutputs() const { return external_outputs_; }

#ifdef ENABLE_STRIDED_TENSORS
  const std::vector<int>& MayStridedInput() const { return may_strided_inputs_; }
  const std::vector<std::pair<int, int>>& MayStridedOutput() const { return may_strided_output_map_; }
#endif

  OrtMemType OutputMemoryType(size_t output_index) const {
    auto it = output_memory_type_args_.find(output_index);
    if (it == output_memory_type_args_.end())
      return default_outputs_mem_type_;
    return it->second;
  }

  int ExecQueueId() const {
    return exec_queue_id_;
  }

  bool IsConflict(const KernelDef& other) const;

 private:
  friend class KernelDefBuilder;

  // The operator name supported by <*this> kernel..
  std::string op_name_;

  // The operator since_version range supported by <*this> kernel.
  // A kernel could support an operator definition between <op_since_version_start>
  // and <op_since_version_end> (inclusive).
  int op_since_version_start_ = 1;
  int op_since_version_end_ = INT_MAX;

  // The operator domain supported by <*this> kernel.
  // Default to 'onnxruntime::kOnnxDomain'.
  // Please note the behavior of std::string("") and std::string() are different
  std::string op_domain_;

  // The type of the execution provider.
  std::string provider_type_;

  // The data types that are supported in this build (enabled) for inputs/outputs.
  // Key is input/output/type constraint name defined in op schema, Value is supported types.
  std::unordered_map<std::string, std::vector<MLDataType>> type_constraints_;

  // An element <i, j> means that output j reuses the memory of input i.
  std::vector<std::pair<int, int>> inplace_map_;

  // An element <i, j> means that output j is an alias of input i.
  std::vector<std::pair<int, int>> alias_map_;

  // This variable stores <input_offset, output_offset> for the variadic alias mapping
  // output 'i + output_offset' is an alias of input 'i + input_offset' for all i >= 0
  std::optional<std::pair<int, int>> variadic_alias_offsets_;

  // Require input tensors to be allocated contiguously.
  bool allocate_inputs_contiguously_ = false;

  // Whether the outputs are from external.
  bool external_outputs_ = false;

#ifdef ENABLE_STRIDED_TENSORS
  // An element i means i-th input can be strided tensor.
  std::vector<int> may_strided_inputs_;

  // An element <i, j> means j-th output can be a strided tensor, which share the data from i-th input.
  std::vector<std::pair<int, int>> may_strided_output_map_;
#endif

  // The memory types of inputs/outputs of this kernel
  MemTypeMap input_memory_type_args_;
  MemTypeMap output_memory_type_args_;

  // execution command queue id, 0 for default queue in execution provider
  int exec_queue_id_ = 0;
  // Default memory type for all inputs
  OrtMemType default_inputs_mem_type_{OrtMemTypeDefault};
  // Default memory type for all outputs
  OrtMemType default_outputs_mem_type_{OrtMemTypeDefault};
};

class KernelDefBuilder {
 public:
  static std::unique_ptr<KernelDefBuilder> Create() { return std::make_unique<KernelDefBuilder>(); }

  explicit KernelDefBuilder()
      : kernel_def_(std::make_unique<KernelDef>()) {}

  KernelDefBuilder& SetName(const std::string& op_name);
  KernelDefBuilder& SetName(const char* op_name);

  KernelDefBuilder& SetDomain(const std::string& domain);
  KernelDefBuilder& SetDomain(const char* domain);

  /**
     This kernel supports operator definition since <since_version> (to latest).
  */
  KernelDefBuilder& SinceVersion(int since_version) {
    kernel_def_->op_since_version_start_ = since_version;
    return *this;
  }

  /**
     The start and end version should be set accordingly per version range for
     each domain registered in OpSchemaRegistry::DomainToVersionRange in
     \onnxruntime\onnxruntime\core\graph\op.h as below.
     Key: domain. Value: <lowest version, highest version> pair.
     std::unordered_map<std::string, std::pair<int, int>> map_;
  */
  KernelDefBuilder& SinceVersion(int since_version_start, int since_version_end) {
    kernel_def_->op_since_version_start_ = since_version_start;
    kernel_def_->op_since_version_end_ = since_version_end;
    return *this;
  }

  /**
     The execution provider type of the kernel.
  */
  KernelDefBuilder& Provider(ProviderType provider_type);
  KernelDefBuilder& Provider(const char* provider_type);

  /**
     Specify the set of types that this kernel supports. A further restriction
     of the set of types specified in the op schema.

     @param arg_name The arg name can be either op formal parameter name, say "X", or type
                     argument name specified in op schema, say "T".
     @param types The types that are supported in this build.
  */
  KernelDefBuilder& TypeConstraint(const std::string& arg_name, std::vector<MLDataType> types);
  KernelDefBuilder& TypeConstraint(const char* arg_name, std::vector<MLDataType> types);

  /**
     Like TypeConstraint but supports just a single type.
  */
  KernelDefBuilder& TypeConstraint(const std::string& arg_name, MLDataType type);
  KernelDefBuilder& TypeConstraint(const char* arg_name, MLDataType type);

  /**
     Inplace mapping from inputs to outputs allowed.
     It means that uplayer runtime could do memory in-place optimization
     as it will not impact the correctness of this kernel.
  */
  KernelDefBuilder& MayInplace(const std::vector<std::pair<int, int>>& inplaces);
  KernelDefBuilder& MayInplace(int input_index, int output_index);

  /**
     Alias mapping from inputs to outputs. Different from Inplace that the
     content of the tensor is not changed. This is to take care of operators
     such as Identity and Reshape.
  */
  KernelDefBuilder& Alias(const std::vector<std::pair<int, int>>& aliases);
  KernelDefBuilder& Alias(int input_index, int output_index);

  /**
     Apply variadic number of alias mapping from inputs to outputs.
     This is effectively applying Alias(i + input_offset, i + output_offset) for i >= 0
  */
  KernelDefBuilder& VariadicAlias(int input_offset, int output_offset);

  /**
     Specify that this kernel requires input tensors to be allocated
     contiguously. This allows kernels to execute as a single large
     computation, rather than numerous smaller computations.
  */
  KernelDefBuilder& AllocateInputsContiguously() {
    kernel_def_->allocate_inputs_contiguously_ = true;
    return *this;
  }

  /**
     Specify that this kernel's output buffers are passed from external,
     i.e. not created or managed by ORT's memory allocator.

     The OrtValue set as external outputs, must be safe to release as long as the OrtValue's reference
     count reaches zero in ORT's allocation/deallocation plan. We usually create such an OrtValue
     following flows: torch tensors --> to dlpack tensors (destructor will release a view of original torch tensor,
         instead of releasing original torch tensor) --> to OrtValue.

     When the OrtValue is not needed in the graph, then it will be released after calling the attached
     destructor. The destructor will release the view of the original torch tensor, instead of releasing the original
     torch tensor. This is to make sure the original torch tensor can still be okay to use externally,
     even after OrtValue is released in the graph. (Recalled this OrtValue is also not reused by ORT).
  */
  KernelDefBuilder& ExternalOutputs() {
    kernel_def_->external_outputs_ = true;
    return *this;
  }

#ifdef ENABLE_STRIDED_TENSORS
  /**
     Specify that the input_index-th input can be strided tensor.
   */
  KernelDefBuilder& MayStridedInput(int input_index);

  /**
     Specify that the output_index-th output can be strided tensor, and share the data
     from input_index-th input.
   */
  KernelDefBuilder& MayStridedOutput(int input_index, int output_index);
#endif

  /**
     Specify that this kernel requires an input arg
     in certain memory type (instead of the default, device memory).
  */
  KernelDefBuilder& InputMemoryType(OrtMemType type, int input_index) {
    kernel_def_->input_memory_type_args_.insert(std::make_pair(input_index, type));
    return *this;
  }

  /**
     Specify that this kernel requires input arguments
     in certain memory type (instead of the default, device memory).
  */
  KernelDefBuilder& InputMemoryType(OrtMemType type, const std::vector<int>& input_indexes) {
    for (auto input_index : input_indexes) {
      kernel_def_->input_memory_type_args_.insert(std::make_pair(input_index, type));
    }
    return *this;
  }

  /**
     Specify that this kernel provides an output arg
     in certain memory type (instead of the default, device memory).
  */
  KernelDefBuilder& OutputMemoryType(OrtMemType type, int output_index) {
    kernel_def_->output_memory_type_args_.insert(std::make_pair(output_index, type));
    return *this;
  }

  /**
     Specify that this kernel provides an output arguments
     in certain memory type (instead of the default, device memory).
  */
  KernelDefBuilder& OutputMemoryType(OrtMemType type, const std::vector<int>& output_indexes) {
    for (auto output_index : output_indexes) {
      kernel_def_->output_memory_type_args_.insert(std::make_pair(output_index, type));
    }
    return *this;
  }

  /**
     Specify that this kernel runs on which execution queue in the provider
  */
  KernelDefBuilder& ExecQueueId(int queue_id) {
    kernel_def_->exec_queue_id_ = queue_id;
    return *this;
  }

  /**
  Specify the default inputs memory type, if not specified, it is DefaultMemory
  */
  KernelDefBuilder& SetDefaultInputsMemoryType(OrtMemType mem_type) {
    kernel_def_->default_inputs_mem_type_ = mem_type;
    return *this;
  }

  /**
  Specify the default outputs memory type, if not specified, it is DefaultMemory
  */
  KernelDefBuilder& SetDefaultOutputMemoryType(OrtMemType mem_type) {
    kernel_def_->default_outputs_mem_type_ = mem_type;
    return *this;
  }

  /**
     Return the kernel definition, passing ownership of the KernelDef to the caller
  */
  std::unique_ptr<KernelDef> Build() {
    return std::move(kernel_def_);
  }

 private:
  // we own the KernelDef until Build() is called.
  std::unique_ptr<KernelDef> kernel_def_;
};

}  // namespace onnxruntime
