// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>
#include <string>

#ifndef SHARED_PROVIDER
#include "core/graph/onnx_protobuf.h"
#endif

#include "core/framework/float8.h"
#include "core/framework/float16.h"
#include "core/framework/int4.h"

namespace onnxruntime {
namespace utils {
/** Gets the TensorProto_DataType corresponding to the template type `T`. */
template <typename T>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType() {
  return ONNX_NAMESPACE::TensorProto_DataType_UNDEFINED;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<float>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<uint8_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT8;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<int8_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT8;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<uint16_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT16;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<int16_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT16;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<int32_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT32;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<int64_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT64;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<std::string>() {
  return ONNX_NAMESPACE::TensorProto_DataType_STRING;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<bool>() {
  return ONNX_NAMESPACE::TensorProto_DataType_BOOL;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<MLFloat16>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT16;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<double>() {
  return ONNX_NAMESPACE::TensorProto_DataType_DOUBLE;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<uint32_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT32;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<uint64_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT64;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<BFloat16>() {
  return ONNX_NAMESPACE::TensorProto_DataType_BFLOAT16;
}

#if !defined(DISABLE_FLOAT8_TYPES)

template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<Float8E4M3FN>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E4M3FN;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<Float8E4M3FNUZ>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E4M3FNUZ;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<Float8E5M2>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E5M2;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<Float8E5M2FNUZ>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT8E5M2FNUZ;
}

#endif
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<Int4x2>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT4;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorProtoElementType<UInt4x2>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT4;
}

}  // namespace utils
}  // namespace onnxruntime
