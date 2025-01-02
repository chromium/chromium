// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "boost/mp11.hpp"

#include "core/common/common.h"
#include "core/framework/to_tensor_proto_element_type.h"
#ifndef SHARED_PROVIDER
#include "core/common/type_list.h"
#include "core/framework/data_types.h"
#include "core/graph/onnx_protobuf.h"
#endif

namespace onnxruntime {
namespace utils {

// The following primitives are strongly recommended for switching on tensor input datatypes for
// kernel implementations.
//
//  1) If you need to handle all of the primitive tensor contained datatypes, the best choice would be macros
//     DispatchOnTensorType or DispatchOnTensorTypeWithReturn. Use inline wrappers so your function can be invoked as function<T>().
//  2) if you have a few types, use Tensor.IsDataType<T>()/IsDataTypeString() or use utils::IsPrimitiveDataType<T>()
//     if you have a standalone MLDatatType with a sequence of if/else statements.
//  3) For something in between, we suggest to use CallDispatcher pattern.
//
// Invoking DataTypeImpl::GetType<T>() for switching on input types is discouraged and should be avoided.
// Every primitive type carries with it an integer constant that can be used for quick switching on types.

#if !defined(DISABLE_FLOAT8_TYPES)

#define DispatchOnTensorType(tensor_type, function, ...)          \
  switch (tensor_type->AsPrimitiveDataType()->GetDataType()) {    \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT:              \
      function<float>(__VA_ARGS__);                               \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_BOOL:               \
      function<bool>(__VA_ARGS__);                                \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_DOUBLE:             \
      function<double>(__VA_ARGS__);                              \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_STRING:             \
      function<std::string>(__VA_ARGS__);                         \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT8:               \
      function<int8_t>(__VA_ARGS__);                              \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT8:              \
      function<uint8_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT16:              \
      function<int16_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT16:             \
      function<uint16_t>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT32:              \
      function<int32_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT32:             \
      function<uint32_t>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT64:              \
      function<int64_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT64:             \
      function<uint64_t>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT16:            \
      function<MLFloat16>(__VA_ARGS__);                           \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_BFLOAT16:           \
      function<BFloat16>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E4M3FN:       \
      function<Float8E4M3FN>(__VA_ARGS__);                        \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E4M3FNUZ:     \
      function<Float8E4M3FNUZ>(__VA_ARGS__);                      \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E5M2:         \
      function<Float8E5M2>(__VA_ARGS__);                          \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E5M2FNUZ:     \
      function<Float8E5M2FNUZ>(__VA_ARGS__);                      \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT4:               \
      function<Int4x2>(__VA_ARGS__);                              \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT4:              \
      function<UInt4x2>(__VA_ARGS__);                             \
      break;                                                      \
    default:                                                      \
      ORT_ENFORCE(false, "Unknown tensor type of ", tensor_type); \
  }

#define DispatchOnTensorTypeWithReturn(tensor_type, retval, function, ...) \
  switch (tensor_type->AsPrimitiveDataType()->GetDataType()) {             \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT:                       \
      retval = function<float>(__VA_ARGS__);                               \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_BOOL:                        \
      retval = function<bool>(__VA_ARGS__);                                \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_DOUBLE:                      \
      retval = function<double>(__VA_ARGS__);                              \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_STRING:                      \
      retval = function<std::string>(__VA_ARGS__);                         \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT8:                        \
      retval = function<int8_t>(__VA_ARGS__);                              \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT8:                       \
      retval = function<uint8_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT16:                      \
      retval = function<uint16_t>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT16:                       \
      retval = function<int16_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT32:                       \
      retval = function<int32_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT32:                      \
      retval = function<uint32_t>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT64:                       \
      retval = function<int64_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT64:                      \
      retval = function<uint64_t>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT16:                     \
      retval = function<MLFloat16>(__VA_ARGS__);                           \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_BFLOAT16:                    \
      retval = function<BFloat16>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E4M3FN:                \
      retval = function<Float8E4M3FN>(__VA_ARGS__);                        \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E4M3FNUZ:              \
      retval = function<Float8E4M3FNUZ>(__VA_ARGS__);                      \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E5M2:                  \
      retval = function<Float8E5M2>(__VA_ARGS__);                          \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E5M2FNUZ:              \
      retval = function<Float8E5M2FNUZ>(__VA_ARGS__);                      \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT4:                        \
      retval = function<Int4x2>(__VA_ARGS__);                              \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT4:                       \
      retval = function<UInt4x2>(__VA_ARGS__);                             \
      break;                                                               \
    default:                                                               \
      ORT_ENFORCE(false, "Unknown tensor type of ", tensor_type);          \
  }

#else

#define DispatchOnTensorType(tensor_type, function, ...)          \
  switch (tensor_type->AsPrimitiveDataType()->GetDataType()) {    \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT:              \
      function<float>(__VA_ARGS__);                               \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_BOOL:               \
      function<bool>(__VA_ARGS__);                                \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_DOUBLE:             \
      function<double>(__VA_ARGS__);                              \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_STRING:             \
      function<std::string>(__VA_ARGS__);                         \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT8:               \
      function<int8_t>(__VA_ARGS__);                              \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT8:              \
      function<uint8_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT16:              \
      function<int16_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT16:             \
      function<uint16_t>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT32:              \
      function<int32_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT32:             \
      function<uint32_t>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT64:              \
      function<int64_t>(__VA_ARGS__);                             \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT64:             \
      function<uint64_t>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT16:            \
      function<MLFloat16>(__VA_ARGS__);                           \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_BFLOAT16:           \
      function<BFloat16>(__VA_ARGS__);                            \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_INT4:               \
      function<Int4x2>(__VA_ARGS__);                              \
      break;                                                      \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT4:              \
      function<UInt4x2>(__VA_ARGS__);                             \
      break;                                                      \
    default:                                                      \
      ORT_ENFORCE(false, "Unknown tensor type of ", tensor_type); \
  }

#define DispatchOnTensorTypeWithReturn(tensor_type, retval, function, ...) \
  switch (tensor_type->AsPrimitiveDataType()->GetDataType()) {             \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT:                       \
      retval = function<float>(__VA_ARGS__);                               \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_BOOL:                        \
      retval = function<bool>(__VA_ARGS__);                                \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_DOUBLE:                      \
      retval = function<double>(__VA_ARGS__);                              \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_STRING:                      \
      retval = function<std::string>(__VA_ARGS__);                         \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT8:                        \
      retval = function<int8_t>(__VA_ARGS__);                              \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT8:                       \
      retval = function<uint8_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT16:                      \
      retval = function<uint16_t>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT16:                       \
      retval = function<int16_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT32:                       \
      retval = function<int32_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT32:                      \
      retval = function<uint32_t>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT64:                       \
      retval = function<int64_t>(__VA_ARGS__);                             \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT64:                      \
      retval = function<uint64_t>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_FLOAT16:                     \
      retval = function<MLFloat16>(__VA_ARGS__);                           \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_BFLOAT16:                    \
      retval = function<BFloat16>(__VA_ARGS__);                            \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_INT4:                        \
      retval = function<Int4x2>(__VA_ARGS__);                              \
      break;                                                               \
    case ONNX_NAMESPACE::TensorProto_DataType_UINT4:                       \
      retval = function<UInt4x2>(__VA_ARGS__);                             \
      break;                                                               \
    default:                                                               \
      ORT_ENFORCE(false, "Unknown tensor type of ", tensor_type);          \
  }

#endif

////////////////////////////////////////////////////////////////////////////////
/// Use the following primitives if you have a few types to switch on so you
//  can write a short sequence of if/else statements.

// This is a frequently used check so we make a separate utility function.
inline bool IsDataTypeString(MLDataType dt_type) {
  auto prim_type = dt_type->AsPrimitiveDataType();
  return (prim_type != nullptr && prim_type->GetDataType() == ONNX_NAMESPACE::TensorProto_DataType_STRING);
}

// Test if MLDataType is a concrete type of PrimitiveDataTypeBase
// and it is T
template <class T>
inline bool IsPrimitiveDataType(MLDataType dt_type) {
  auto prim_type = dt_type->AsPrimitiveDataType();
  return (prim_type != nullptr && prim_type->GetDataType() == ToTensorProtoElementType<T>());
}

// Use after AsPrimitiveDataType() is successful
// Check if PrimitiveDataTypeBase is of type T
template <class T>
inline bool IsPrimitiveDataType(const PrimitiveDataTypeBase* prim_type) {
  assert(prim_type != nullptr);
  return prim_type->GetDataType() == ToTensorProtoElementType<T>();
}

// This implementation contains a workaround for GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47226
// GCC until very recently does not support template parameter pack expansion within lambda context.
namespace mltype_dispatcher_internal {

// T - type handled by this helper
class CallableDispatchableHelper {
  int32_t dt_type_;  // Type currently dispatched
  size_t called_;

 public:
  explicit CallableDispatchableHelper(int32_t dt_type) noexcept : dt_type_(dt_type), called_(0) {}

  // Must return integer to be in a expandable context
  template <class T, class Fn, class... Args>
  int Invoke(Fn&& fn, Args&&... args) {
    if (utils::ToTensorProtoElementType<T>() == dt_type_) {
      std::forward<Fn>(fn)(std::forward<Args>(args)...);
      ++called_;
    }
    return 0;
  }

  void CheckCalledOnce() const {
    ORT_ENFORCE(called_ == 1, "Unsupported data type: ", dt_type_);
  }
};

// Default policy is to throw an exception.
// Other policies may set the second result argument accordingly.
template <class Ret>
struct UnsupportedTypeDefaultPolicy {
  void operator()(int32_t dt_type, Ret& /*result*/) const {
    ORT_THROW("Unsupported data type: ", dt_type);
  }
};

// Helper with the result type
template <class Ret, class UnsupportedPolicy>
class CallableDispatchableRetHelper {
  int32_t dt_type_;  // Type currently dispatched
  size_t called_;
  Ret result_;

 public:
  explicit CallableDispatchableRetHelper(int32_t dt_type) noexcept : dt_type_(dt_type), called_(0), result_() {}

  Ret Get() {
    // No type was invoked
    if (called_ == 0) {
      UnsupportedPolicy()(dt_type_, result_);
    }
    return result_;
  }

  // Must return integer to be in a expandable context
  template <class T, class Fn, class... Args>
  int Invoke(Fn&& fn, Args&&... args) {
    if (utils::ToTensorProtoElementType<T>() == dt_type_) {
      result_ = std::forward<Fn>(fn)(std::forward<Args>(args)...);
      ++called_;
    }
    return 0;
  }
};

template <typename T>
using TensorProtoElementTypeConstant =
    std::integral_constant<ONNX_NAMESPACE::TensorProto_DataType, ToTensorProtoElementType<T>()>;

using UndefinedTensorProtoElementTypeConstant =
    std::integral_constant<ONNX_NAMESPACE::TensorProto_DataType, ONNX_NAMESPACE::TensorProto_DataType_UNDEFINED>;

}  // namespace mltype_dispatcher_internal

/**
 * This class helps to efficiently dispatch calls to implementation function
 * objects with a tensor element type template argument.
 *
 * The constructor accepts a value corresponding to a tensor element type.
 * For example, it can be obtained from:
 *   input_tensor->GetElementType()
 *
 * The Invoke member functions will instantiate and invoke the provided
 * function object template, Fn. Fn must be default constructible. Fn must also
 * have a tensor element type template argument. This type template argument
 * will be the type that corresponds to the value given in the constructor.
 * These functions accept and forward arbitrary function arguments. They ensure
 * that Fn is called once with the type specified in the constructor.
 *
 * @tparam Types The types supported by the implementation. This should be a
 *         set of ONNX tensor element types that are supported by ORT.
 */
template <typename... Types>
class MLTypeCallDispatcher {
  using SupportedTypeList = TypeList<Types...>;
  using SupportedTensorProtoElementTypeList =
      boost::mp11::mp_transform<
          mltype_dispatcher_internal::TensorProtoElementTypeConstant, SupportedTypeList>;

  static_assert(
      boost::mp11::mp_and<
          boost::mp11::mp_is_set<SupportedTensorProtoElementTypeList>,
          boost::mp11::mp_not<
              boost::mp11::mp_set_contains<
                  SupportedTensorProtoElementTypeList,
                  mltype_dispatcher_internal::UndefinedTensorProtoElementTypeConstant>>>::value,
      "Types must map to a unique set of ONNX tensor element data types supported by ORT.");

  int32_t dt_type_;

 public:
  /**
   * Constructor.
   * @param dt_type The value corresponding to the tensor element type to be
   *        dispatched to. This can be obtained from
   *        input_tensor->GetElementType() or
   *        utils::ToTensorProtoElementType<T>().
   */
  explicit MLTypeCallDispatcher(int32_t dt_type) noexcept : dt_type_(dt_type) {}

  /**
   * Invokes Fn<T> with the specified arguments.
   *
   * @tparam Fn The function object template.
   * @tparam Args The argument types.
   */
  template <template <typename...> class Fn, typename... Args>
  void Invoke(Args&&... args) const {
    InvokeWithLeadingTemplateArgs<Fn, TypeList<>>(std::forward<Args>(args)...);
  }

  /**
   * Invokes Fn<..., T> with leading template arguments and the specified
   * arguments.
   *
   * @tparam Fn The function object template.
   * @tparam LeadingTemplateArgTypeList A type list of the leading template
   *         arguments.
   * @tparam Args The argument types.
   */
  template <template <typename...> class Fn, typename LeadingTemplateArgTypeList, typename... Args>
  void InvokeWithLeadingTemplateArgs(Args&&... args) const {
    static_assert(
        boost::mp11::mp_is_list<LeadingTemplateArgTypeList>::value,
        "LeadingTemplateArgTypeList must be a type list (e.g., onnxruntime::TypeList<T1, T2, ...>).");

    mltype_dispatcher_internal::CallableDispatchableHelper helper(dt_type_);

    // given LeadingTemplateArgTypeList is a type list L<U1, U2, ...>,
    //   call helper.Invoke() with Fn<U1, U2, ..., T> for each T in Types
    static_cast<void>(std::array<int, sizeof...(Types)>{
        helper.template Invoke<Types>(
            boost::mp11::mp_apply<Fn, boost::mp11::mp_push_back<LeadingTemplateArgTypeList, Types>>(),
            std::forward<Args>(args)...)...});

    // avoid "unused parameter" warning for the case where Types is empty
    static_cast<void>(std::array<int, sizeof...(Args)>{(ORT_UNUSED_PARAMETER(args), 0)...});

    helper.CheckCalledOnce();
  }

  /**
   * Invokes Fn<T> with the specified arguments and returns the result.
   *
   * @tparam Ret The return type. Fn should return a type convertible to Ret.
   * @tparam Fn The function object template.
   * @tparam Args The argument types.
   */
  template <class Ret, template <typename...> class Fn, typename... Args>
  Ret InvokeRet(Args&&... args) const {
    return InvokeRetWithUnsupportedPolicy<
        Ret, Fn, mltype_dispatcher_internal::UnsupportedTypeDefaultPolicy<Ret>>(
        std::forward<Args>(args)...);
  }

  /**
   * Invokes Fn<T> with the specified arguments and returns the result.
   *
   * @tparam Ret The return type. Fn should return a type convertible to Ret.
   * @tparam Fn The function object template.
   * @tparam UnsupportedPolicy The policy used to handle unsupported types.
   *         See mltype_dispatcher_internal::UnsupportedTypeDefaultPolicy
   *         for an example.
   * @tparam Args The argument types.
   */
  template <class Ret, template <typename...> class Fn, class UnsupportedPolicy, typename... Args>
  Ret InvokeRetWithUnsupportedPolicy(Args&&... args) const {
    return InvokeRetWithUnsupportedPolicyAndLeadingTemplateArgs<
        Ret, Fn, UnsupportedPolicy, TypeList<>>(
        std::forward<Args>(args)...);
  }

  /**
   * Invokes Fn<..., T> with leading template arguments and the specified
   * arguments and returns the result.
   *
   * @tparam Ret The return type. Fn should return a type convertible to Ret.
   * @tparam Fn The function object template.
   * @tparam LeadingTemplateArgTypeList A type list of the leading template
   *         arguments.
   * @tparam Args The argument types.
   */
  template <class Ret, template <typename...> class Fn, typename LeadingTemplateArgTypeList, typename... Args>
  Ret InvokeRetWithLeadingTemplateArgs(Args&&... args) const {
    return InvokeRetWithUnsupportedPolicyAndLeadingTemplateArgs<
        Ret, Fn, mltype_dispatcher_internal::UnsupportedTypeDefaultPolicy<Ret>, LeadingTemplateArgTypeList>(
        std::forward<Args>(args)...);
  }

  /**
   * Invokes Fn<..., T> with leading template arguments and the specified
   * arguments and returns the result.
   *
   * @tparam Ret The return type. Fn should return a type convertible to Ret.
   * @tparam Fn The function object template.
   * @tparam UnsupportedPolicy The policy used to handle unsupported types.
   *         See mltype_dispatcher_internal::UnsupportedTypeDefaultPolicy
   *         for an example.
   * @tparam LeadingTemplateArgTypeList A type list of the leading template
   *         arguments.
   * @tparam Args The argument types.
   */
  template <class Ret,
            template <typename...> class Fn,
            class UnsupportedPolicy,
            typename LeadingTemplateArgTypeList,
            typename... Args>
  Ret InvokeRetWithUnsupportedPolicyAndLeadingTemplateArgs(Args&&... args) const {
    mltype_dispatcher_internal::CallableDispatchableRetHelper<Ret, UnsupportedPolicy> helper(dt_type_);

    // given LeadingTemplateArgTypeList is a type list L<U1, U2, ...>,
    //   call helper.Invoke() with Fn<U1, U2, ..., T> for each T in Types
    static_cast<void>(std::array<int, sizeof...(Types)>{
        helper.template Invoke<Types>(
            boost::mp11::mp_apply<Fn, boost::mp11::mp_push_back<LeadingTemplateArgTypeList, Types>>(),
            std::forward<Args>(args)...)...});

    // avoid "unused parameter" warning for the case where Types is empty
    static_cast<void>(std::array<int, sizeof...(Args)>{(ORT_UNUSED_PARAMETER(args), 0)...});

    return helper.Get();
  }
};

// the type MLTypeCallDispatcher<T...> given a type list L<T...>
template <typename L>
using MLTypeCallDispatcherFromTypeList = boost::mp11::mp_apply<MLTypeCallDispatcher, L>;

namespace data_types_internal {

enum class ContainerType : uint16_t {
  kUndefined = 0,
  kTensor = 1,
  kMap = 2,
  kSequence = 3,
  kOpaque = 4,
  kOptional = 5
};

class TypeNode {
  // type_ is a TypeProto value case enum
  // that may be a kTypeTensor, kTypeMap, kTypeSequence
  // prim_type_ is a TypeProto_DataType enum that has meaning
  // - for Tensor then prim_type_ is the contained type
  // - for Map prim_type is the key type. Next entry describes map value
  // - For sequence prim_type_ is not used and has no meaning. Next entry
  //   describes the value for the sequence
  // Tensor is always the last entry as it describes a contained primitive type.
  ContainerType type_;
  uint16_t prim_type_;

 public:
  TypeNode(ContainerType type, int32_t prim_type) noexcept {
    type_ = type;
    prim_type_ = static_cast<uint16_t>(prim_type);
  }

  bool IsType(ContainerType type) const noexcept {
    return type_ == type;
  }

  bool IsPrimType(int32_t prim_type) const noexcept {
    return prim_type_ == static_cast<uint16_t>(prim_type);
  }
};

}  // namespace data_types_internal

////////////////////////////////////////////////////////////////////
/// Provides generic interface to test whether MLDataType is a Sequence,
/// Map or an Opaque type including arbitrary recursive definitions
/// without querying DataTypeImpl::GetType<T> for all known complex types

// T is a sequence contained element type
// If returns true then we know that the runtime
// representation is std::vector<T>
// T itself can be a runtime representation of another
// sequence, map, opaque type or a tensor
//
// That is it can be std::vector or a std::map
// If T is a primitive type sequence is tested whether it contains
// tensors of that type
//
// If T is an opaque type, then it is only tested to be opaque but not exactly
// a specific opaque type. To Test for a specific Opaque type use IsOpaqueType() below
//
// This class examines the supplied MLDataType and records
// its information in a vector so any subsequent checks for Sequences and Maps
// are quick.
class ContainerChecker {
  using Cont = std::vector<data_types_internal::TypeNode>;
  Cont types_;

  // Default IsContainerOfType is for Opaque type
  template <class T>
  struct IsContainerOfType {
    static bool check(const Cont& c, size_t index) {
      if (index >= c.size()) {
        return false;
      }
      return c[index].IsType(data_types_internal::ContainerType::kOpaque);
    }
  };

  // Handles the case where sequence element is also a sequence
  template <class T>
  struct IsContainerOfType<std::vector<T>> {
    static bool check(const Cont& c, size_t index) {
      if (index >= c.size()) {
        return false;
      }
      if (c[index].IsType(data_types_internal::ContainerType::kSequence)) {
        ORT_ENFORCE(++index < c.size(), "Sequence is missing type entry for its element");
        constexpr int32_t prim_type = ToTensorProtoElementType<T>();
        // Check if this is a primitive type and it matches
        if constexpr (prim_type != ONNX_NAMESPACE::TensorProto_DataType_UNDEFINED) {
          return c[index].IsType(data_types_internal::ContainerType::kTensor) &&
                 c[index].IsPrimType(prim_type);
        } else {
          // T is not primitive, check next entry for non-primitive proto
          return IsContainerOfType<T>::check(c, index);
        }
      }
      return false;
    }
  };

  template <class K, class V>
  struct IsContainerOfType<std::map<K, V>> {
    static bool check(const Cont& c, size_t index) {
      static_assert(ToTensorProtoElementType<K>() != ONNX_NAMESPACE::TensorProto_DataType_UNDEFINED,
                    "Map Key can not be a non-primitive type");
      if (index >= c.size()) {
        return false;
      }
      if (!c[index].IsType(data_types_internal::ContainerType::kMap)) {
        return false;
      }
      constexpr int32_t key_type = ToTensorProtoElementType<K>();
      if (!c[index].IsPrimType(key_type)) {
        return false;
      }
      ORT_ENFORCE(++index < c.size(), "Map is missing type entry for its value");
      constexpr int32_t val_type = ToTensorProtoElementType<V>();
      if constexpr (val_type != ONNX_NAMESPACE::TensorProto_DataType_UNDEFINED) {
        return c[index].IsType(data_types_internal::ContainerType::kTensor) &&
               c[index].IsPrimType(val_type);
      } else
        return IsContainerOfType<V>::check(c, index);
    }
  };

 public:
  explicit ContainerChecker(MLDataType);
  ~ContainerChecker() = default;

  bool IsMap() const noexcept {
    assert(!types_.empty());
    return types_[0].IsType(data_types_internal::ContainerType::kMap);
  }

  bool IsSequence() const noexcept {
    assert(!types_.empty());
    return types_[0].IsType(data_types_internal::ContainerType::kSequence);
  }

  template <class T>
  bool IsSequenceOf() const {
    assert(!types_.empty());
    return IsContainerOfType<std::vector<T>>::check(types_, 0);
  }

  template <class K, class V>
  bool IsMapOf() const {
    assert(!types_.empty());
    return IsContainerOfType<std::map<K, V>>::check(types_, 0);
  }
};

bool IsOpaqueType(MLDataType ml_type, const char* domain, const char* name);

}  // namespace utils
}  // namespace onnxruntime
