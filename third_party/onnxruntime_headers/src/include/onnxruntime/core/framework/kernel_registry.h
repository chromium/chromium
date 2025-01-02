// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string_view>

#include "core/framework/op_kernel.h"

namespace onnxruntime {
namespace logging {
class Logger;
}

using KernelCreateMap = std::multimap<std::string, KernelCreateInfo>;
using KernelDefHashes = std::vector<std::pair<std::string, HashValue>>;

class IKernelTypeStrResolver;

/**
 * Each provider has a KernelRegistry. Often, the KernelRegistry only belongs to that specific provider.
 */
class KernelRegistry {
 public:
  KernelRegistry() = default;

  // Register a kernel with kernel definition and function to create the kernel.
  Status Register(KernelDefBuilder& kernel_def_builder, const KernelCreateFn& kernel_creator);

  Status Register(KernelCreateInfo&& create_info);

  // TODO(edgchen1) for TryFindKernel(), consider using `out` != nullptr as indicator of whether kernel was found and
  // Status as an indication of failure

  // Check if an execution provider can create kernel for a node and return the kernel if so.
  // Kernel matching uses the types from the node and the kernel_type_str_resolver.
  Status TryFindKernel(const Node& node, ProviderType exec_provider,
                       const IKernelTypeStrResolver& kernel_type_str_resolver,
                       const logging::Logger& logger,
                       const KernelCreateInfo** out) const;

  // map of type constraint name to required type
  using TypeConstraintMap = InlinedHashMap<std::string, MLDataType>;

  // Check if an execution provider can create kernel for a node and return the kernel if so.
  // Kernel matching uses the explicit type constraint name to required type map in type_constraints.
  Status TryFindKernel(const Node& node, ProviderType exec_provider,
                       const TypeConstraintMap& type_constraints,
                       const logging::Logger& logger,
                       const KernelCreateInfo** out) const;

  /**
   * @brief Find out whether a kernel is registered, without a node.
   *        This should be useful in graph optimizers, to check whether
   *        the node it is about to generate, is supported or not.
   * @param exec_provider
   * @param op_type
   * @param domain
   * @param version
   * @param type_constraints
   * @param out
   * @return
   */
  Status TryFindKernel(ProviderType exec_provider,
                       std::string_view op_type,
                       std::string_view domain,
                       int version,
                       const KernelRegistry::TypeConstraintMap& type_constraints,
                       const logging::Logger& logger,
                       const KernelCreateInfo** out) const;

  static bool HasImplementationOf(const KernelRegistry& r, const Node& node,
                                  ProviderType exec_provider,
                                  const IKernelTypeStrResolver& kernel_type_str_resolver,
                                  const logging::Logger& logger) {
    const KernelCreateInfo* info;
    Status st = r.TryFindKernel(node, exec_provider, kernel_type_str_resolver, logger, &info);
    return st.IsOK();
  }

  bool IsEmpty() const { return kernel_creator_fn_map_.empty(); }

  // This is used by the opkernel doc generator to enlist all registered operators for a given provider's opkernel
  const KernelCreateMap& GetKernelCreateMap() const {
    return kernel_creator_fn_map_;
  }

 private:
  // TryFindKernel implementation. Either kernel_type_str_resolver or type_constraints is provided.
  Status TryFindKernelImpl(const Node& node, ProviderType exec_provider,
                           const IKernelTypeStrResolver* kernel_type_str_resolver,
                           const TypeConstraintMap* type_constraints,
                           const logging::Logger& logger,
                           const KernelCreateInfo** out) const;

  // Check whether the types of inputs/outputs of the given node match the extra
  // type-constraints of the given kernel. This serves two purposes: first, to
  // select the right kernel implementation based on the types of the arguments
  // when we have multiple kernels, e.g., Clip<float> and Clip<int>; second, to
  // accommodate (and check) mapping of ONNX (specification) type to the onnxruntime
  // implementation type (e.g., if we want to implement ONNX's float16 as a regular
  // float in onnxruntime). (The second, however, requires a globally uniform mapping.)
  //
  // Note that this is not intended for type-checking the node against the ONNX
  // type specification of the corresponding op, which is done before this check.
  //
  // In typical usage kernel_type_str_resolver is provided and type information from the node is used with
  // kernel_type_str_resolver.
  //
  // There is also usage from a node dynamically created within a custom op via OrtApi CreateOp where an explicit
  // type value for each type constraint is provided in type_constraints.
  //
  // Either kernel_type_str_resolver or type_constraints is provided and not both.
  static bool VerifyKernelDef(const Node& node, const KernelDef& kernel_def,
                              const IKernelTypeStrResolver* kernel_type_str_resolver,
                              const TypeConstraintMap* type_constraints,
                              std::string& error_str);

  static std::string GetMapKey(std::string_view op_name, std::string_view domain, std::string_view provider) {
    std::string key(op_name);
    // use the kOnnxDomainAlias of 'ai.onnx' instead of kOnnxDomain's empty string
    key.append(1, ' ').append(domain.empty() ? kOnnxDomainAlias : domain).append(1, ' ').append(provider);
    return key;
  }

  static std::string GetMapKey(const KernelDef& kernel_def) {
    return GetMapKey(kernel_def.OpName(), kernel_def.Domain(), kernel_def.Provider());
  }
  // Kernel create function map from op name to kernel creation info.
  // key is opname+domain_name+provider_name
  KernelCreateMap kernel_creator_fn_map_;
};
}  // namespace onnxruntime
