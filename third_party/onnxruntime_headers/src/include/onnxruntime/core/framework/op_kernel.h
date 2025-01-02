// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "boost/mp11.hpp"

// It is safe to include the below header even if SHARED_PROVIDER macro is enabled
// as it doesn't include any pb headers.
#include "core/framework/buffer_deleter.h"
#include "core/framework/prepacked_weights_container.h"

#ifndef SHARED_PROVIDER
#include <functional>

#include "core/common/exceptions.h"
#include "core/common/logging/logging.h"
#include "core/common/status.h"
#include "core/framework/execution_provider.h"
#include "core/framework/kernel_def_builder.h"
#include "core/framework/op_kernel_info.h"
#include "core/framework/op_node_proto_helper.h"
#include "core/framework/ort_value.h"
#include "core/framework/sparse_tensor.h"
#include "core/framework/tensor.h"
#include "core/graph/constants.h"
#include "core/graph/graph_viewer.h"
#include "core/graph/onnx_protobuf.h"
#include <gsl/gsl>
namespace onnxruntime {
class OpKernelContext;
}
#endif

namespace onnxruntime {

std::unique_ptr<OpKernelInfo> CopyOpKernelInfo(const OpKernelInfo& info);

class OpKernel {
 public:
  using DoneCallback = std::function<void()>;

  explicit OpKernel(const OpKernelInfo& info) : op_kernel_info_(CopyOpKernelInfo(info)) {}
  virtual ~OpKernel() = default;

  const onnxruntime::Node& Node() const;
  const onnxruntime::KernelDef& KernelDef() const;

  [[nodiscard]] virtual Status Compute(_Inout_ OpKernelContext* context) const = 0;

  [[nodiscard]] virtual bool IsAsync() const {
    // by default all kernels are sync version.
    return false;
  }

  [[nodiscard]] virtual Status ComputeAsync(_Inout_ OpKernelContext*, DoneCallback) const {
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  // Override this function to PrePack initialized constant tensor to the format as needed.
  // For example, MatMul kernel can pack the input B if it is constant like code below.
  //   Status PrePack(const Tensor& tensor, int input_idx, /*out*/ bool& is_packed,
  //                  /*out*/ PrePackedWeights* prepacked_weight_for_caching,
  //                  AllocatorPtr alloc) override {
  //     is_packed = false;
  //     if (input_idx == 1) {
  //       is_packed = true;
  //       this.Pack(tensor, this.buffer_, alloc);
  //       if (prepacked_weight_for_caching) {
  //           // LOGIC TO CACHE `this.buffer_` SINCE THE KERNEL DOESN"T OWN THE PACKED WEIGHT
  //       }
  //     }
  //     return Status::OK();
  //   }
  // Please refer to MatMulIntegerToFloatBase for a complete example
  // @param tensor: The initialized constant tensor
  // @param input_idx: The input index of the tensor in this kernel
  // @param alloc: The kernel's PrePack() method MUST use this allocator for allocating the pre-packed
  //               weights' buffers. The alloc that the PrePack() method will receive will be either
  //               the allocator tied to the session if the kernel owns the pre-packed buffer or an
  //               allocator shared between sessions if the pre-packed buffer is to be shared across sessions
  //               (i.e.) the kernel does not own the buffer.
  // @param is_packed: Set it to true if the kernel packed the tensor or to false
  //                   The kernel is responsible for keeping the packed data and related metadata if is_packed is true,
  //                   and the original initialized constant tensor will be released and not accessible anymore in
  //                   the Compute function.
  // @param prepacked_weights: A PrePackedWeights instance will be provided to the kernel IF the pre-packed weights
  //                          are meant to be stored in a shared container.

  virtual Status
  PrePack(const Tensor& /*tensor*/, int /*input_idx*/, AllocatorPtr /*alloc*/,
          /*out*/ bool& is_packed, /*out*/ PrePackedWeights* /*prepacked_weights*/) {
    is_packed = false;
    return Status::OK();
  }

  // Override this function to return a list of attributes the session can safely remove
  // after it is initialized and saved. This option is useful to reduce memory usage
  // when the kernel does not reuse the operator attributes but copies them.
  // All attributes returned by this method will be removed by method
  // PruneRemovableAttributes of they exists.
  // @param removable_attributes set of attributes the session can safely remove.
  virtual Status GetRemovableAttributes(InlinedVector<std::string>& removable_attributes) const {
    removable_attributes.clear();
    return Status::OK();
  }

  // Override this function to use provided pre-packed weight.
  // Status UseSharedPrePackedBuffers(std::vector<BufferUniquePtr>& prepacked_buffers,
  //                                 int input_idx,
  //                                 /*out*/ bool& used_shared_buffers) {
  //     used_shared_buffers = true;
  //     this.buffer_ = std::move(prepacked_buffers[0]);
  //     return Status::OK();
  //   }
  // Please refer to MatMulIntegerToFloatBase for a complete example
  // @param prepacked_buffers: The pre-packed buffers to be used by this kernel for the provided input index
  //                           (Sometimes a single constant initializer may have multiple pre-packed buffers associated
  //                            with it and it upto the kernel developer to store it in any order of their choice in PrePack()
  //                            and must use the same order for retrieval in UseSharedPrePackedBuffers(). Though each element
  //                           of this vector is a BufferUniquePtr, the deleter of the BufferUniquePtr is NULL. So actually they
  //                           are raw pointers.
  // @param input_idx: The input index of the tensor in this kernel
  // @param used_shared_buffers: Boolean flag set by the kernel implementation indicating
  // that the provided weight has been used by the kernel.
  virtual Status UseSharedPrePackedBuffers(std::vector<BufferUniquePtr>& /*prepacked_buffers*/,
                                           int /*input_idx*/,
                                           /*out*/ bool& used_shared_buffers) {
    used_shared_buffers = false;
    return Status::OK();
  }

  const OrtDevice GetDevice(OrtMemType mem_type) const;
  const OpKernelInfo& Info() const {
    return *op_kernel_info_;
  }

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(OpKernel);
  std::unique_ptr<OpKernelInfo> op_kernel_info_;
};
class FuncManager;
using KernelCreateFn = std::function<Status(FuncManager& func_mgr, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out)>;
using KernelCreatePtrFn = std::add_pointer<Status(FuncManager& func_mgr, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out)>::type;

struct KernelCreateInfo {
  std::unique_ptr<KernelDef> kernel_def;  // Owned and stored in the global kernel registry.
  KernelCreateFn kernel_create_func;
  Status status;

  KernelCreateInfo(std::unique_ptr<KernelDef> definition,
                   KernelCreateFn create_func)
      : kernel_def(std::move(definition)),
        kernel_create_func(create_func) {
    assert(kernel_def != nullptr);
  }

  KernelCreateInfo(KernelCreateInfo&& other) noexcept
      : kernel_def(std::move(other.kernel_def)),
        kernel_create_func(std::move(other.kernel_create_func)) {}

  KernelCreateInfo() = default;
};

// Forward declarations for the non-specialized BuildKernelCreateInfo method.
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();

namespace ml {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace ml

namespace contrib {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace contrib

namespace contrib {
namespace cuda {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace cuda
}  // namespace contrib

namespace contrib {
namespace js {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace js
}  // namespace contrib

namespace contrib {
namespace rocm {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace rocm
}  // namespace contrib

namespace contrib {
namespace snpe {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace snpe
}  // namespace contrib

using BuildKernelCreateInfoFn = KernelCreateInfo (*)();

// Naming convention for operator kernel classes
#define ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name) \
  provider##_##name##_##domain##_ver##ver

#define ONNX_CPU_OPERATOR_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kOnnxDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_ML_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kMLDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_MS_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kMSDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_KERNEL_EX(name, domain, ver, provider, builder, ...)                \
  class ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name);                     \
  template <>                                                                             \
  KernelCreateInfo                                                                        \
  BuildKernelCreateInfo<ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name)>() { \
    return KernelCreateInfo(                                                              \
        builder.SetName(#name)                                                            \
            .SetDomain(domain)                                                            \
            .SinceVersion(ver)                                                            \
            .Provider(provider)                                                           \
            .Build(),                                                                     \
        static_cast<KernelCreatePtrFn>(                                                   \
            [](FuncManager&,                                                              \
               const OpKernelInfo& info,                                                  \
               std::unique_ptr<OpKernel>& out) -> Status {                                \
              out = std::make_unique<__VA_ARGS__>(info);                                  \
              return Status::OK();                                                        \
            }));                                                                          \
  }

#define ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name) \
  provider##_##name##_##domain##_ver##startver##_##endver

#define ONNX_CPU_OPERATOR_VERSIONED_KERNEL(name, startver, endver, builder, ...) \
  ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, kOnnxDomain, startver, endver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_ML_KERNEL(name, startver, endver, builder, ...) \
  ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, kMLDomain, startver, endver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, domain, startver, endver, provider, builder, ...)                                  \
  class ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name);                                       \
  template <>                                                                                                                      \
  KernelCreateInfo                                                                                                                 \
  BuildKernelCreateInfo<ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name)>() {                   \
    return KernelCreateInfo(                                                                                                       \
        builder.SetName(#name)                                                                                                     \
            .SetDomain(domain)                                                                                                     \
            .SinceVersion(startver, endver)                                                                                        \
            .Provider(provider)                                                                                                    \
            .Build(),                                                                                                              \
        static_cast<KernelCreatePtrFn>([](FuncManager&, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out) -> Status { out = std::make_unique<__VA_ARGS__>(info); return Status::OK(); })); \
  }

#define ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name) \
  provider##_##name##_##domain##_ver##ver##_##type

#define ONNX_CPU_OPERATOR_TYPED_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kOnnxDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_TYPED_ML_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kMLDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_TYPED_MS_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kMSDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_TYPED_KERNEL_EX(name, domain, ver, type, provider, builder, ...)                                             \
  class ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name);                                                  \
  template <>                                                                                                                      \
  KernelCreateInfo                                                                                                                 \
  BuildKernelCreateInfo<ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name)>() {                              \
    return KernelCreateInfo(                                                                                                       \
        builder.SetName(#name)                                                                                                     \
            .SetDomain(domain)                                                                                                     \
            .SinceVersion(ver)                                                                                                     \
            .Provider(provider)                                                                                                    \
            .Build(),                                                                                                              \
        static_cast<KernelCreatePtrFn>([](FuncManager&, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out) -> Status { out = std::make_unique<__VA_ARGS__>(info); return Status::OK(); })); \
  }

#define ONNX_OPERATOR_TWO_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type1, type2, name) \
  provider##_##name##_##domain##_ver##ver##_##type1##_##type2

#define ONNX_OPERATOR_TWO_TYPED_KERNEL_EX(name, domain, ver, type1, type2, provider, builder, ...)                                 \
  class ONNX_OPERATOR_TWO_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type1, type2, name);                                      \
  template <>                                                                                                                      \
  KernelCreateInfo                                                                                                                 \
  BuildKernelCreateInfo<ONNX_OPERATOR_TWO_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type1, type2, name)>() {                  \
    return KernelCreateInfo(                                                                                                       \
        builder.SetName(#name)                                                                                                     \
            .SetDomain(domain)                                                                                                     \
            .SinceVersion(ver)                                                                                                     \
            .Provider(provider)                                                                                                    \
            .Build(),                                                                                                              \
        static_cast<KernelCreatePtrFn>([](FuncManager&, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out) -> Status { out = std::make_unique<__VA_ARGS__>(info); return Status::OK(); })); \
  }

#define ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type, name) \
  provider##_##name##_##domain##_ver##startver##_##endver##_##type

#define ONNX_CPU_OPERATOR_VERSIONED_TYPED_KERNEL(name, startver, endver, type, builder, ...)                         \
  ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, kOnnxDomain, startver, endver, type, kCpuExecutionProvider, builder, \
                                          __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_TYPED_ML_KERNEL(name, startver, endver, type, builder, ...)                    \
  ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, kMLDomain, startver, endver, type, kCpuExecutionProvider, builder, \
                                          __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_TYPED_MS_KERNEL(name, startver, endver, type, builder, ...)                    \
  ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, kMSDomain, startver, endver, type, kCpuExecutionProvider, builder, \
                                          __VA_ARGS__)

#define ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, domain, startver, endver, type, provider, builder, ...)                      \
  class ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type, name);                           \
  template <>                                                                                                                      \
  KernelCreateInfo                                                                                                                 \
  BuildKernelCreateInfo<ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver,                        \
                                                                        type, name)>() {                                           \
    return KernelCreateInfo(                                                                                                       \
        builder.SetName(#name)                                                                                                     \
            .SetDomain(domain)                                                                                                     \
            .SinceVersion(startver, endver)                                                                                        \
            .Provider(provider)                                                                                                    \
            .Build(),                                                                                                              \
        static_cast<KernelCreatePtrFn>([](FuncManager&, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out) -> Status { out = std::make_unique<__VA_ARGS__>(info); return Status::OK(); })); \
  }

#define ONNX_OPERATOR_VERSIONED_TWO_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type1, type2, name) \
  provider##_##name##_##domain##_ver##startver##_##endver##_##type1##_##type2

#define ONNX_OPERATOR_VERSIONED_TWO_TYPED_KERNEL_EX(name, domain, startver, endver, type1, type2,                                  \
                                                    provider, builder, ...)                                                        \
  class ONNX_OPERATOR_VERSIONED_TWO_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type1, type2, name);               \
  template <>                                                                                                                      \
  KernelCreateInfo                                                                                                                 \
  BuildKernelCreateInfo<ONNX_OPERATOR_VERSIONED_TWO_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver,                    \
                                                                            type1, type2, name)>() {                               \
    return KernelCreateInfo(                                                                                                       \
        builder.SetName(#name)                                                                                                     \
            .SetDomain(domain)                                                                                                     \
            .SinceVersion(startver, endver)                                                                                        \
            .Provider(provider)                                                                                                    \
            .Build(),                                                                                                              \
        static_cast<KernelCreatePtrFn>([](FuncManager&, const OpKernelInfo& info, std::unique_ptr<OpKernel>& out) -> Status { out = std::make_unique<__VA_ARGS__>(info); return Status::OK(); })); \
  }

template <typename... Types>
struct BuildKernelDefConstraintsImpl {
  std::vector<MLDataType> operator()() const {
    return {DataTypeImpl::GetTensorType<Types>()...};
  }
};

#if !defined(DISABLE_SPARSE_TENSORS)
template <typename... Types>
struct BuildKernelDefSparseConstraintsImpl {
  std::vector<MLDataType> operator()() const {
    return {DataTypeImpl::GetSparseTensorType<Types>()...};
  }
};
#endif

// Use within macro definitions to create a custom vector of constraints.
// Example: #define REG_KERNEL(OP, VERSION, KERNEL_CLASS, Type, ...)
//  .TypeConstraint("T", BuildKernelDefConstraints<Type, __VA_ARGS_>())
template <typename... Types>
inline std::vector<MLDataType> BuildKernelDefConstraints() {
  return BuildKernelDefConstraintsImpl<Types...>{}();
}

#if !defined(DISABLE_SPARSE_TENSORS)
template <typename... Types>
inline std::vector<MLDataType> BuildKernelDefSparseConstraints() {
  return BuildKernelDefSparseConstraintsImpl<Types...>{}();
}
#endif

// version of BuildKernelDefConstraints() which takes a type list
template <typename L>
inline std::vector<MLDataType> BuildKernelDefConstraintsFromTypeList() {
  return boost::mp11::mp_apply<BuildKernelDefConstraintsImpl, L>{}();
}

#if !defined(DISABLE_SPARSE_TENSORS)
template <typename L>
inline std::vector<MLDataType> BuildKernelDefSparseConstraintsFromTypeList() {
  return boost::mp11::mp_apply<BuildKernelDefSparseConstraintsImpl, L>{}();
}
#endif

}  // namespace onnxruntime

#ifndef SHARED_PROVIDER
#include "core/framework/op_kernel_context.h"
#endif
