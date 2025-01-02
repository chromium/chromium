// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <map>
#include <unordered_map>
#include <gsl/gsl>
#include "core/common/common.h"
#include "core/common/exceptions.h"
#include "core/framework/endian.h"
#include "core/framework/float8.h"
#include "core/framework/float16.h"
#include "core/framework/int4.h"
#include "core/graph/onnx_protobuf.h"
#include "core/framework/to_tensor_proto_element_type.h"

struct OrtValue;

namespace ONNX_NAMESPACE {
class TypeProto;
}  // namespace ONNX_NAMESPACE

namespace onnxruntime {
/// Predefined registered types

#if !defined(DISABLE_ML_OPS)

// maps (only used by ML ops)
using MapStringToString = std::map<std::string, std::string>;
using MapStringToInt64 = std::map<std::string, int64_t>;
using MapStringToFloat = std::map<std::string, float>;
using MapStringToDouble = std::map<std::string, double>;
using MapInt64ToString = std::map<int64_t, std::string>;
using MapInt64ToInt64 = std::map<int64_t, int64_t>;
using MapInt64ToFloat = std::map<int64_t, float>;
using MapInt64ToDouble = std::map<int64_t, double>;

// vectors/sequences
using VectorMapStringToFloat = std::vector<MapStringToFloat>;
using VectorMapInt64ToFloat = std::vector<MapInt64ToFloat>;

#endif

using VectorString = std::vector<std::string>;
using VectorInt64 = std::vector<int64_t>;

// Forward declarations
class DataTypeImpl;
class TensorTypeBase;
#if !defined(DISABLE_SPARSE_TENSORS)
class SparseTensorTypeBase;
#endif
class SequenceTensorTypeBase;
class NonTensorTypeBase;
#if !defined(DISABLE_OPTIONAL_TYPE)
class OptionalTypeBase;
#endif
class PrimitiveDataTypeBase;
class Tensor;
class TensorSeq;

// DataTypeImpl pointer as unique DataTypeImpl identifier.
using MLDataType = const DataTypeImpl*;
// be used with class MLValue
using DeleteFunc = void (*)(void*);
using CreateFunc = void* (*)();

/**
 * \brief Base class for MLDataType
 *
 */
class DataTypeImpl {
 public:
  enum class GeneralType {
    kInvalid = 0,
    kNonTensor = 1,
    kTensor = 2,
    kTensorSequence = 3,
    kSparseTensor = 4,
    kOptional = 5,
    kPrimitive = 6,
  };

  const GeneralType type_;
  const size_t size_;

 protected:
  DataTypeImpl(GeneralType type, size_t size) : type_{type}, size_{size} {}

 public:
  virtual ~DataTypeImpl() = default;

  /**
   * \brief this API will be used to check type compatibility at runtime
   *
   * \param type_proto a TypeProto instance that is constructed for a specific type
   *        will be checked against a TypeProto instance contained within a corresponding
   *        MLDataType instance.
   */
  virtual bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const = 0;

  size_t Size() const { return size_; }

  virtual DeleteFunc GetDeleteFunc() const = 0;

  /**
   * \brief Retrieves an instance of TypeProto for
   *        a given MLDataType
   * \returns optional TypeProto. Only ONNX types
              has type proto, non-ONNX types will return nullptr.
   */
  virtual const ONNX_NAMESPACE::TypeProto* GetTypeProto() const = 0;

  bool IsTensorType() const {
    return type_ == GeneralType::kTensor;
  }

  bool IsTensorSequenceType() const {
    return type_ == GeneralType::kTensorSequence;
  }

  bool IsSparseTensorType() const {
    return type_ == GeneralType::kSparseTensor;
  }

  bool IsOptionalType() const {
    return type_ == GeneralType::kOptional;
  }

  bool IsNonTensorType() const {
    return type_ == GeneralType::kNonTensor;
  }

  bool IsPrimitiveDataType() const {
    return type_ == GeneralType::kPrimitive;
  }

  // Returns this if this is of tensor-type and null otherwise
  const TensorTypeBase* AsTensorType() const;

  const SequenceTensorTypeBase* AsSequenceTensorType() const;

#if !defined(DISABLE_SPARSE_TENSORS)
  // Returns this if this is of sparse-tensor-type and null otherwise
  const SparseTensorTypeBase* AsSparseTensorType() const;
#endif

#if !defined(DISABLE_OPTIONAL_TYPE)
  const OptionalTypeBase* AsOptionalType() const;
#endif

  const NonTensorTypeBase* AsNonTensorType() const;

  // Returns this if this is one of the primitive data types (specialization of PrimitiveDataTypeBase)
  // and null otherwise
  const PrimitiveDataTypeBase* AsPrimitiveDataType() const;

  // Return the type meta that we are using in the runtime.
  template <typename T>
  static MLDataType GetType();

  // Return the types for a concrete tensor type, like Tensor_Float
  template <typename elemT>
  static MLDataType GetTensorType();

  template <typename elemT>
  static MLDataType GetSequenceTensorType();

#if !defined(DISABLE_SPARSE_TENSORS)
  // Return the MLDataType for a concrete sparse tensor type.
  template <typename elemT>
  static MLDataType GetSparseTensorType();
#endif

  template <typename T, typename elemT>
  static MLDataType GetOptionalType();

  /**
   * Convert an ONNX TypeProto to onnxruntime DataTypeImpl.
   * However, this conversion is lossy. Don't try to use 'this->GetTypeProto()' converting it back.
   * Even though GetTypeProto() will not have the original information, it will still have enough to correctly
   * map to MLDataType.
   * \param proto
   */
  static MLDataType TypeFromProto(const ONNX_NAMESPACE::TypeProto& proto);

  static const TensorTypeBase* TensorTypeFromONNXEnum(int type);
  static const SequenceTensorTypeBase* SequenceTensorTypeFromONNXEnum(int type);
#if !defined(DISABLE_SPARSE_TENSORS)
  static const SparseTensorTypeBase* SparseTensorTypeFromONNXEnum(int type);
#endif

  static const char* ToString(MLDataType type);
  static std::vector<std::string> ToString(const std::vector<MLDataType>& types);
  // Registers ONNX_NAMESPACE::DataType (internalized string) with
  // MLDataType. DataType is produced by internalizing an instance of
  // TypeProto contained within MLDataType
  static void RegisterDataType(MLDataType);
  static MLDataType GetDataType(const std::string&);

  // IR4: includes all float types, includes float16, bfloat16
  // IR9: includes float 8 types as well
  static const std::vector<MLDataType>& AllTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllTensorTypesIRv4();
  static const std::vector<MLDataType>& AllTensorTypesIRv9();

  static const std::vector<MLDataType>& AllFixedSizeTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllFixedSizeTensorTypesIRv4();
  static const std::vector<MLDataType>& AllFixedSizeTensorTypesIRv9();

  static const std::vector<MLDataType>& AllSequenceTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllSequenceTensorTypesIRv4();
  static const std::vector<MLDataType>& AllSequenceTensorTypesIRv9();

  static const std::vector<MLDataType>& AllFixedSizeSequenceTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllFixedSizeSequenceTensorTypesIRv4();
  static const std::vector<MLDataType>& AllFixedSizeSequenceTensorTypesIRv9();

  static const std::vector<MLDataType>& AllNumericTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllNumericTensorTypesIRv4();
  static const std::vector<MLDataType>& AllNumericTensorTypesIRv9();

  static const std::vector<MLDataType>& AllIEEEFloatTensorTypes();  // float16, float, double

  static const std::vector<MLDataType>& AllTensorAndSequenceTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllTensorAndSequenceTensorTypesIRv4();
  static const std::vector<MLDataType>& AllTensorAndSequenceTensorTypesIRv9();

  static const std::vector<MLDataType>& AllOptionalAndTensorAndSequenceTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllOptionalAndTensorAndSequenceTensorTypesIRv4();
  static const std::vector<MLDataType>& AllOptionalAndTensorAndSequenceTensorTypesIRv9();

  static const std::vector<MLDataType>& AllFixedSizeTensorAndSequenceTensorTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllFixedSizeTensorAndSequenceTensorTypesIRv4();
  static const std::vector<MLDataType>& AllFixedSizeTensorAndSequenceTensorTypesIRv9();

  static const std::vector<MLDataType>& AllOptionalTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllOptionalTypesIRv4();
  static const std::vector<MLDataType>& AllOptionalTypesIRv9();

  static const std::vector<MLDataType>& AllTensorAndSequenceTensorAndOptionalTypes();  // up to IR4 (no float 8), deprecated
  static const std::vector<MLDataType>& AllTensorAndSequenceTensorAndOptionalTypesIRv4();
  static const std::vector<MLDataType>& AllTensorAndSequenceTensorAndOptionalTypesIRv9();
};

std::ostream& operator<<(std::ostream& out, MLDataType data_type);

/*
 * Type registration helpers
 */
namespace data_types_internal {
/// TensorType helpers
///

/// Is a given type on the list of types?
/// Accepts a list of types and the first argument is the type
/// We are checking if it is listed among those that follow
template <typename T, typename... Types>
struct IsAnyOf;

/// Two types remaining, end of the list
template <typename T, typename Tail>
struct IsAnyOf<T, Tail> : public std::is_same<T, Tail> {
};

template <typename T, typename H, typename... Tail>
struct IsAnyOf<T, H, Tail...> {
  static constexpr bool value = (std::is_same<T, H>::value ||
                                 IsAnyOf<T, Tail...>::value);
};

/// Tells if the specified type is one of fundamental types
/// that can be contained within a tensor.
/// We do not have raw fundamental types, rather a subset
/// of fundamental types is contained within tensors.
template <typename T>
struct IsTensorContainedType : public IsAnyOf<T, float, uint8_t, int8_t, uint16_t, int16_t,
                                              int32_t, int64_t, std::string, bool, MLFloat16,
                                              double, uint32_t, uint64_t, BFloat16,
                                              Int4x2, UInt4x2
#if !defined(DISABLE_FLOAT8_TYPES)
                                              ,
                                              Float8E4M3FN, Float8E4M3FNUZ, Float8E5M2, Float8E5M2FNUZ
#endif
                                              > {
};

#if !defined(DISABLE_SPARSE_TENSORS)
/// Use "IsSparseTensorContainedType<T>::value" to test if a type T
/// is permitted as the element-type of a sparse-tensor.

template <typename T>
struct IsSparseTensorContainedType : public IsAnyOf<T, float, uint8_t, int8_t, uint16_t, int16_t,
                                                    int32_t, int64_t, std::string, bool, MLFloat16,
                                                    double, uint32_t, uint64_t, BFloat16
#if !defined(DISABLE_FLOAT8_TYPES)
                                                    ,
                                                    Float8E4M3FN, Float8E4M3FNUZ, Float8E5M2, Float8E5M2FNUZ
#endif
                                                    > {
};
#endif

#if !defined(DISABLE_OPTIONAL_TYPE)
/// Tells if the specified type is one of ORT types
/// that can be contained within an optional struct.
template <typename T>
struct IsOptionalOrtType : public IsAnyOf<T, Tensor, TensorSeq> {
};
#endif

/// This template's Get() returns a corresponding MLDataType
/// It dispatches the call to either GetTensorType<>() or
/// GetType<>()
template <typename T, bool TensorContainedType>
struct GetMLDataType;

template <typename T>
struct GetMLDataType<T, true> {
  static MLDataType Get() {
    return DataTypeImpl::GetTensorType<T>();
  }
};

template <typename T>
struct GetMLDataType<T, false> {
  static MLDataType Get() {
    return DataTypeImpl::GetType<T>();
  }
};

struct TensorTypeHelper {
  static void Set(ONNX_NAMESPACE::TensorProto_DataType element_type,
                  ONNX_NAMESPACE::TypeProto& proto) {
    proto.mutable_tensor_type()->set_elem_type(element_type);
  }
};

#if !defined(DISABLE_SPARSE_TENSORS)
struct SparseTensorTypeHelper {
  static void Set(ONNX_NAMESPACE::TensorProto_DataType element_type,
                  ONNX_NAMESPACE::TypeProto& proto) {
    proto.mutable_sparse_tensor_type()->set_elem_type(element_type);
  }
};
#endif  // !defined(DISABLE_SPARSE_TENSORS)

#if !defined(DISABLE_ML_OPS)
/// Map helpers

void CopyMutableMapValue(const ONNX_NAMESPACE::TypeProto&,
                         ONNX_NAMESPACE::TypeProto&);

struct MapTypeHelper {
  // V can be either a primitive type (in which case it is a tensor)
  // or other preregistered types
  template <typename V>
  static MLDataType GetValueType() {
    return GetMLDataType<V, IsTensorContainedType<V>::value>::Get();
  }

  static void Set(ONNX_NAMESPACE::TensorProto_DataType key_type, const ONNX_NAMESPACE::TypeProto* value_proto,
                  ONNX_NAMESPACE::TypeProto& proto) {
    ORT_ENFORCE(value_proto != nullptr, "expected a registered ONNX type");
    proto.mutable_map_type()->set_key_type(key_type);
    CopyMutableMapValue(*value_proto, proto);
  }
};
#endif

/// Sequence helpers

// Element type is a primitive type so we set it to a tensor<elemT>
void CopyMutableSeqElement(const ONNX_NAMESPACE::TypeProto&,
                           ONNX_NAMESPACE::TypeProto&);

// helper to create TypeProto with minimal binary size impact
struct SequenceTypeHelper {
  template <typename T>
  static MLDataType GetElemType() {
    return GetMLDataType<T, IsTensorContainedType<T>::value>::Get();
  }

  static void Set(const ONNX_NAMESPACE::TypeProto* elem_proto,
                  ONNX_NAMESPACE::TypeProto& proto) {
    ORT_ENFORCE(elem_proto != nullptr, "expected a registered ONNX type");
    CopyMutableSeqElement(*elem_proto, proto);
  }
};

/// Optional helpers

void CopyMutableOptionalElement(const ONNX_NAMESPACE::TypeProto&,
                                ONNX_NAMESPACE::TypeProto&);

// helper to create TypeProto with minimal binary size impact
struct OptionalTypeHelper {
  template <typename T, typename elemT>
  static MLDataType GetElemType() {
    if constexpr (std::is_same<T, Tensor>::value) {
      return DataTypeImpl::GetTensorType<elemT>();
    } else {
      static_assert(std::is_same<T, TensorSeq>::value, "Unsupported element type for optional type");
      return DataTypeImpl::GetSequenceTensorType<elemT>();
    }
  }

  static void Set(const onnx::TypeProto* elem_proto, ONNX_NAMESPACE::TypeProto& proto) {
    ORT_ENFORCE(elem_proto != nullptr, "expected a registered ONNX type");
    CopyMutableOptionalElement(*elem_proto, proto);
  }
};

/// OpaqueTypes helpers

void AssignOpaqueDomainName(const char* domain, const char* name,
                            ONNX_NAMESPACE::TypeProto& proto);

}  // namespace data_types_internal

// The suppressed warning is: "The type with a virtual function needs either public virtual or protected nonvirtual destructor."
// However, we do not allocate this type on heap.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 26436)
#endif
/// All tensors base
class TensorTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  /// We first compare type_proto pointers and then
  /// if they do not match try to account for the case
  /// where TypeProto was created ad-hoc and not queried from MLDataType
  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  DeleteFunc GetDeleteFunc() const override;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(TensorTypeBase);

 protected:
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  TensorTypeBase();
  ~TensorTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};

/**
 * \brief Tensor type. This type does not have a C++ type associated with
 * it at registration time except the element type. One of the types mentioned
 * above at IsTensorContainedType<> list is acceptable.
 *
 * \details
 *        Usage:
 *        ORT_REGISTER_TENSOR(ELEMENT_TYPE)
 *        Currently all of the Tensors irrespective of the dimensions are mapped to Tensor<type>
 *        type. IsCompatible() currently ignores shape.
 */

template <typename elemT>
class TensorType : public TensorTypeBase {
 public:
  static_assert(data_types_internal::IsTensorContainedType<elemT>::value,
                "Requires one of the tensor fundamental types");

  static MLDataType Type();

  /// Tensors only can contain basic data types
  /// that have been previously registered with ONNXRuntime
  MLDataType GetElementType() const override {
    return DataTypeImpl::GetType<elemT>();
  }

 private:
  TensorType() {
    using namespace data_types_internal;
    TensorTypeHelper::Set(utils::ToTensorProtoElementType<elemT>(), MutableTypeProto());
  }
};

#if defined(DISABLE_OPTIONAL_TYPE)

// TODO is this still needed after removing kernel def hashes?
/// Common base-class for all disabled types. We need DataTypeImpl::ToString to work in a minimal build
/// with disabled types to keep the ORT format model kernel hashes stable.
class DisabledTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto&) const override {
    // We always want to return false for the IsCompatible() for a disabled type
    // because this will ensure that no kernel supporting the disabled type will
    // be matched to a model node requiring that type and the model load will
    // result in failure.
    return false;
  }

  DeleteFunc GetDeleteFunc() const override {
    ORT_THROW("Type is disabled in this build.");
  }

  // This must work
  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(DisabledTypeBase);

 protected:
  // This must work
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  DisabledTypeBase(DataTypeImpl::GeneralType type, size_t size);
  ~DisabledTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};

#endif

#if !defined(DISABLE_SPARSE_TENSORS)
/// Common base-class for all sparse-tensors (with different element types).
class SparseTensorTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  DeleteFunc GetDeleteFunc() const override;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(SparseTensorTypeBase);

 protected:
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  SparseTensorTypeBase();
  ~SparseTensorTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};

template <typename elemT>
class SparseTensorType : public SparseTensorTypeBase {
 public:
  static_assert(data_types_internal::IsSparseTensorContainedType<elemT>::value,
                "Requires one of the sparse-tensor fundamental types");

  static MLDataType Type();

  /// Return a MLDataType representing the element-type
  MLDataType GetElementType() const override {
    return DataTypeImpl::GetType<elemT>();
  }

 private:
  SparseTensorType() {
    using namespace data_types_internal;
    SparseTensorTypeHelper::Set(utils::ToTensorProtoElementType<elemT>(), MutableTypeProto());
  }
};

#endif  // !defined(DISABLE_SPARSE_TENSORS)

/// Common base-class for all optional types.

#if !defined(DISABLE_OPTIONAL_TYPE)
class OptionalTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  DeleteFunc GetDeleteFunc() const override {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  OptionalTypeBase(const OptionalTypeBase&) = delete;
  OptionalTypeBase& operator=(const OptionalTypeBase&) = delete;

 protected:
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  OptionalTypeBase();
  ~OptionalTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};
#endif

// Derive from OptionalTypeBase if the Optional type support is enabled,
// else derive from DisabledTypeBase
template <typename T, typename elemT>
class OptionalType :
#if !defined(DISABLE_OPTIONAL_TYPE)
    public OptionalTypeBase
#else
    public DisabledTypeBase
#endif
{
 public:
  static MLDataType Type();

#if !defined(DISABLE_OPTIONAL_TYPE)
  static_assert(data_types_internal::IsOptionalOrtType<T>::value,
                "Requires one of the supported types: Tensor or TensorSeq");

  static_assert(data_types_internal::IsTensorContainedType<elemT>::value,
                "Requires one of the tensor fundamental types");

  MLDataType GetElementType() const override {
    return data_types_internal::OptionalTypeHelper::GetElemType<T, elemT>();
  }
#endif

 private:
#if !defined(DISABLE_OPTIONAL_TYPE)
  OptionalType()
#else
  OptionalType() : DisabledTypeBase{DataTypeImpl::GeneralType::kOptional, 0}
#endif
  {
    using namespace data_types_internal;
    OptionalTypeHelper::Set(OptionalTypeHelper::GetElemType<T, elemT>()->GetTypeProto(), MutableTypeProto());
  }
};  // namespace onnxruntime

/**
 * \brief Provide a specialization for your C++ Non-tensor type
 *        so your implementation FromDataTypeContainer/ToDataTypeContainer
 *        functions correctly. Otherwise you get a default implementation
 *        which may not be what you need/want.
 *
 * This class is used to create OrtValue, fetch data from OrtValue via
 * C/C++ APIs
 */
template <class T>
struct NonTensorTypeConverter {
  static void FromContainer(MLDataType /*dtype*/, const void* /*data*/, size_t /*data_size*/, OrtValue& /*output*/) {
    ORT_THROW("Not implemented");
  }
  static void ToContainer(const OrtValue& /*input*/, size_t /*data_size*/, void* /*data*/) {
    ORT_THROW("Not implemented");
  }
};

/**
 * \brief Base type for all non-tensors, maps, sequences and opaques
 */
class NonTensorTypeBase : public DataTypeImpl {
 public:
  DeleteFunc GetDeleteFunc() const override = 0;

  virtual CreateFunc GetCreateFunc() const = 0;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  // \brief Override for Non-tensor types to initialize non-tensor CPP
  // data representation from data. The caller of the interface
  // should have a shared definition of the data which is used to initialize
  // CPP data representation. This is used from C API.
  //
  // \param data - pointer to a data container structure non_tensor type specific
  // \param data_size - size of the data container structure, used for rudimentary checks
  // \param output - reference to a default constructed non-tensor type
  // \returns OrtValue
  // \throw if there is an error
  virtual void FromDataContainer(const void* data, size_t data_size, OrtValue& output) const;

  // \brief Override for Non-tensor types to fetch data from the internal CPP data representation
  // The caller of the interface should have a shared definition of the data which is used to initialize
  // CPP data representation. This is used from C API.
  //
  // \param input - OrtValue containing data
  // \param data_size - size of the structure that is being passed for receiving data, used for
  //                    validation
  // \param data - pointer to receiving data structure
  virtual void ToDataContainer(const OrtValue& input, size_t data_size, void* data) const;

  NonTensorTypeBase(const NonTensorTypeBase&) = delete;
  NonTensorTypeBase& operator=(const NonTensorTypeBase&) = delete;

 protected:
  NonTensorTypeBase(size_t size);
  ~NonTensorTypeBase() override;

  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  bool IsMapCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const;

  bool IsSequenceCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const;

  bool IsOpaqueCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const;

 private:
  struct Impl;
  Impl* impl_;
};

// This is where T is the actual CPPRuntimeType
template <typename T>
class NonTensorType : public NonTensorTypeBase {
 private:
  static void Delete(void* p) {
    delete static_cast<T*>(p);
  }

 public:
  DeleteFunc GetDeleteFunc() const override {
    return &Delete;
  }

  CreateFunc GetCreateFunc() const override {
    return []() -> void* { return new T(); };
  }

 protected:
  NonTensorType() : NonTensorTypeBase(sizeof(T)) {}
};

#if !defined(DISABLE_ML_OPS)
/**
 * \brief MapType. Use this type to register
 * mapping types.
 *
 * \param T - cpp type that you wish to register as runtime MapType
 *
 * \details Usage: ORT_REGISTER_MAP(C++Type)
 *          The type is required to have mapped_type and
 *          key_type defined
 */
template <typename CPPType>
class MapType : public NonTensorType<CPPType> {
 public:
  static_assert(data_types_internal::IsTensorContainedType<typename CPPType::key_type>::value,
                "Requires one of the tensor fundamental types as key");

  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override {
    return this->IsMapCompatible(type_proto);
  }

 private:
  MapType() {
    using namespace data_types_internal;
    MapTypeHelper::Set(utils::ToTensorProtoElementType<typename CPPType::key_type>(),
                       MapTypeHelper::GetValueType<typename CPPType::mapped_type>()->GetTypeProto(),
                       this->MutableTypeProto());
  }
};
#endif

/**
 * \brief SequenceType. Use to register sequence for non-tensor types.
 *
 *  \param T - CPP type that you wish to register as Sequence
 *             runtime type.
 *
 * \details Usage: ORT_REGISTER_SEQ(C++Type)
 *          The type is required to have value_type defined
 */
template <typename CPPType>
class SequenceType : public NonTensorType<CPPType> {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override {
    return this->IsSequenceCompatible(type_proto);
  }

 private:
  SequenceType() {
    using namespace data_types_internal;
    SequenceTypeHelper::Set(SequenceTypeHelper::GetElemType<typename CPPType::value_type>()->GetTypeProto(),
                            this->MutableTypeProto());
  }
};

/**
 * \brief SequenceTensorTypeBase serves as a base type class for
 *        Tensor sequences. Akin to TensorTypeBase.
 *        Runtime representation is always TensorSeq.
 */
class SequenceTensorTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  DeleteFunc GetDeleteFunc() const override;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  SequenceTensorTypeBase(const SequenceTensorTypeBase&) = delete;
  SequenceTensorTypeBase& operator=(const SequenceTensorTypeBase&) = delete;

 protected:
  SequenceTensorTypeBase();
  ~SequenceTensorTypeBase();

  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

 private:
  struct Impl;
  Impl* impl_;
};
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif
/**
 * \brief SequenceTensorType. Use to register sequence for non-tensor types.
 *
 *  \param CPPRuntime - We always use TensorSeq
 *
 *  \param TensorElemType - one of the primitive types
 *
 * \details Usage: ORT_REGISTER_SEQ_TENSOR_TYPE()
 *          The type is required to have value_type defined
 */
template <typename TensorElemType>
class SequenceTensorType : public SequenceTensorTypeBase {
 public:
  static_assert(data_types_internal::IsTensorContainedType<TensorElemType>::value,
                "Requires one of the tensor fundamental types");

  static MLDataType Type();

  /// Return a MLDataType representing the element-type
  MLDataType GetElementType() const override {
    return DataTypeImpl::GetType<TensorElemType>();
  }

 private:
  SequenceTensorType() {
    using namespace data_types_internal;
    SequenceTypeHelper::Set(SequenceTypeHelper::GetElemType<TensorElemType>()->GetTypeProto(),
                            MutableTypeProto());
  }
};

/**
 * \brief OpaqueType
 *
 * \tparam T - cpp runtume that implements the Opaque type
 *
 * \tparam const char D[] - domain must be extern to be unique
 *
 * \tparam const char N[] - name must be extern to be unique
 *
 * \details Only one CPP type can be associated with a particular
 *          OpaqueType registration
 *
 */
template <typename T, const char D[], const char N[]>
class OpaqueType : public NonTensorType<T> {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override {
    return this->IsOpaqueCompatible(type_proto);
  }

  void FromDataContainer(const void* data, size_t data_size, OrtValue& output) const override {
    NonTensorTypeConverter<T>::FromContainer(this, data, data_size, output);
  }

  void ToDataContainer(const OrtValue& input, size_t data_size, void* data) const override {
    NonTensorTypeConverter<T>::ToContainer(input, data_size, data);
  }

 private:
  OpaqueType() {
    data_types_internal::AssignOpaqueDomainName(D, N, this->MutableTypeProto());
  }
};

/**
 * \brief PrimitiveDataTypeBase
 *        Base class for primitive Tensor contained types
 *
 * \details This class contains an integer constant that can be
 *          used for input data type dispatching. This class also stores the number of subelements per size units.
 *          Example: For int4, the size unit is 1 byte and the number of subelements is 2.
 *
 */
class PrimitiveDataTypeBase : public DataTypeImpl {
 public:
  bool IsCompatible(const ONNX_NAMESPACE::TypeProto&) const override {
    return false;
  }

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const final {
    return nullptr;
  }

  int32_t GetDataType() const {
    return data_type_;
  }

  int32_t GetNumSubElems() const {
    return num_sub_elems_;
  }

  bool HasSubElems() const {
    return num_sub_elems_ > 1;
  }

 protected:
  PrimitiveDataTypeBase(size_t size, int32_t data_type, int32_t num_sub_elems)
      : DataTypeImpl{GeneralType::kPrimitive, size}, data_type_{data_type}, num_sub_elems_{num_sub_elems} {}

 private:
  const int32_t data_type_;
  const int32_t num_sub_elems_;  // > 1 for subbyte primitives, 1 for normal primitives.
};

/**
 * \brief PrimitiveDataType
 *        Typed specialization for primitive types.
 *        Concrete instances of this class are used by Tensor.
 *
 * \param T - primitive data type
 *
 */
template <typename T>
class PrimitiveDataType : public PrimitiveDataTypeBase {
 private:
  static void Delete(void* p) {
    delete static_cast<T*>(p);
  }

 public:
  static MLDataType Type();

  DeleteFunc GetDeleteFunc() const override {
    return &Delete;
  }

 private:
  explicit PrimitiveDataType(int32_t num_sub_elems)
      : PrimitiveDataTypeBase{sizeof(T),
                              utils::ToTensorProtoElementType<T>(), num_sub_elems} {
  }
};

inline const TensorTypeBase* DataTypeImpl::AsTensorType() const {
  return IsTensorType() ? static_cast<const TensorTypeBase*>(this) : nullptr;
}

inline const SequenceTensorTypeBase* DataTypeImpl::AsSequenceTensorType() const {
  return IsTensorSequenceType() ? static_cast<const SequenceTensorTypeBase*>(this) : nullptr;
}

#if !defined(DISABLE_SPARSE_TENSORS)
inline const SparseTensorTypeBase* DataTypeImpl::AsSparseTensorType() const {
  return IsSparseTensorType() ? static_cast<const SparseTensorTypeBase*>(this) : nullptr;
}
#endif

#if !defined(DISABLE_OPTIONAL_TYPE)
inline const OptionalTypeBase* DataTypeImpl::AsOptionalType() const {
  return IsOptionalType() ? static_cast<const OptionalTypeBase*>(this) : nullptr;
}
#endif

inline const NonTensorTypeBase* DataTypeImpl::AsNonTensorType() const {
  return IsNonTensorType() ? static_cast<const NonTensorTypeBase*>(this) : nullptr;
}

inline const PrimitiveDataTypeBase* DataTypeImpl::AsPrimitiveDataType() const {
  return IsPrimitiveDataType() ? static_cast<const PrimitiveDataTypeBase*>(this) : nullptr;
}

// Explicit specialization of base class template function
// is only possible within the enclosing namespace scope,
// thus a simple way to pre-instantiate a given template
// at a registration time does not currently work and the macro
// is needed.
#define ORT_REGISTER_TENSOR_TYPE(ELEM_TYPE)             \
  template <>                                           \
  MLDataType TensorType<ELEM_TYPE>::Type() {            \
    static TensorType<ELEM_TYPE> tensor_type;           \
    return &tensor_type;                                \
  }                                                     \
  template <>                                           \
  MLDataType DataTypeImpl::GetTensorType<ELEM_TYPE>() { \
    return TensorType<ELEM_TYPE>::Type();               \
  }

#if !defined(DISABLE_SPARSE_TENSORS)
#define ORT_REGISTER_SPARSE_TENSOR_TYPE(ELEM_TYPE)            \
  template <>                                                 \
  MLDataType SparseTensorType<ELEM_TYPE>::Type() {            \
    static SparseTensorType<ELEM_TYPE> tensor_type;           \
    return &tensor_type;                                      \
  }                                                           \
  template <>                                                 \
  MLDataType DataTypeImpl::GetSparseTensorType<ELEM_TYPE>() { \
    return SparseTensorType<ELEM_TYPE>::Type();               \
  }
#endif

#define ORT_REGISTER_OPTIONAL_TYPE(ORT_TYPE, TYPE)             \
  template <>                                                  \
  MLDataType OptionalType<ORT_TYPE, TYPE>::Type() {            \
    static OptionalType<ORT_TYPE, TYPE> optional_type;         \
    return &optional_type;                                     \
  }                                                            \
  template <>                                                  \
  MLDataType DataTypeImpl::GetOptionalType<ORT_TYPE, TYPE>() { \
    return OptionalType<ORT_TYPE, TYPE>::Type();               \
  }

#if !defined(DISABLE_ML_OPS)
#define ORT_REGISTER_MAP(TYPE)               \
  template <>                                \
  MLDataType MapType<TYPE>::Type() {         \
    static MapType<TYPE> map_type;           \
    return &map_type;                        \
  }                                          \
  template <>                                \
  MLDataType DataTypeImpl::GetType<TYPE>() { \
    return MapType<TYPE>::Type();            \
  }
#endif

#define ORT_REGISTER_SEQ(TYPE)               \
  template <>                                \
  MLDataType SequenceType<TYPE>::Type() {    \
    static SequenceType<TYPE> sequence_type; \
    return &sequence_type;                   \
  }                                          \
  template <>                                \
  MLDataType DataTypeImpl::GetType<TYPE>() { \
    return SequenceType<TYPE>::Type();       \
  }

#define ORT_REGISTER_SEQ_TENSOR_TYPE(ELEM_TYPE)                 \
  template <>                                                   \
  MLDataType SequenceTensorType<ELEM_TYPE>::Type() {            \
    static SequenceTensorType<ELEM_TYPE> sequence_tensor_type;  \
    return &sequence_tensor_type;                               \
  }                                                             \
  template <>                                                   \
  MLDataType DataTypeImpl::GetSequenceTensorType<ELEM_TYPE>() { \
    return SequenceTensorType<ELEM_TYPE>::Type();               \
  }

#define ORT_REGISTER_PRIM_TYPE(TYPE)                  \
  template <>                                         \
  MLDataType PrimitiveDataType<TYPE>::Type() {        \
    static PrimitiveDataType<TYPE> prim_data_type(1); \
    return &prim_data_type;                           \
  }                                                   \
  template <>                                         \
  MLDataType DataTypeImpl::GetType<TYPE>() {          \
    return PrimitiveDataType<TYPE>::Type();           \
  }

// Registers a subbyte primitive.
// Examples:
//   - Int4x2 stores 2 packed 4-bit elements in 1 byte: ORT_*_SUBBYTE_TYPE(Int4x2, 2)
//   - [not supported] Int3x8 could store 8 packed 3-bit elements in 3 bytes: ORT_*_SUBBYTE_TYPE(Int3x8, 8)
#define ORT_REGISTER_PRIM_SUBBYTE_TYPE(TYPE, NUM_SUB_ELEMS)       \
  template <>                                                     \
  MLDataType PrimitiveDataType<TYPE>::Type() {                    \
    static PrimitiveDataType<TYPE> prim_data_type(NUM_SUB_ELEMS); \
    return &prim_data_type;                                       \
  }                                                               \
  template <>                                                     \
  MLDataType DataTypeImpl::GetType<TYPE>() {                      \
    return PrimitiveDataType<TYPE>::Type();                       \
  }

#define ORT_REGISTER_OPAQUE_TYPE(CPPType, Domain, Name)   \
  template <>                                             \
  MLDataType OpaqueType<CPPType, Domain, Name>::Type() {  \
    static OpaqueType<CPPType, Domain, Name> opaque_type; \
    return &opaque_type;                                  \
  }                                                       \
  template <>                                             \
  MLDataType DataTypeImpl::GetType<CPPType>() {           \
    return OpaqueType<CPPType, Domain, Name>::Type();     \
  }
}  // namespace onnxruntime
