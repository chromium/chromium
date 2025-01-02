// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "onnx/onnx_pb.h"

#include "core/graph/basic_types.h"
#include "core/common/status.h"
#include "core/common/logging/logging.h"

namespace onnxruntime {

// Node argument definition, for both input and output,
// including arg name, arg type (contains both type and shape).
//
// Design Question: in my opinion, shape should not be part of type.
// We may align the protobuf design with our operator registry interface,
// which has type specified for each operator, but no shape. Well, shape
// should be inferred with a separate shape inference function given
// input shapes, or input tensor data sometimes.
// With shape as part of type (current protobuf design),
// 1) we'll have to split the "TypeProto" into type and shape in this internal
// representation interface so that it could be easily used when doing type
// inference and matching with operator registry.
// 2) SetType should be always called before SetShape, otherwise, SetShape()
// will fail. Because shape is located in a TypeProto.
// Thoughts?
//

/**
@class NodeArg
Class representing a data type that is input or output for a Node, including the shape if it is a Tensor.
*/
class NodeArg {
 public:
  /**
  Construct a new NodeArg.
  @param name The name to use.
  @param p_arg_type Optional TypeProto specifying type and shape information.
  */
  NodeArg(const std::string& name,
          const ONNX_NAMESPACE::TypeProto* p_arg_type);

  NodeArg(NodeArg&&) = default;
  NodeArg& operator=(NodeArg&& other) = default;

  /** Gets the name. */
  const std::string& Name() const noexcept;

  /** Gets the data type. */
  const std::string* Type() const noexcept;

  /** Gets the TypeProto
  @returns TypeProto if type is set. nullptr otherwise. */
  const ONNX_NAMESPACE::TypeProto* TypeAsProto() const noexcept;

  /** Gets the shape if NodeArg is for a Tensor.
  @returns TensorShapeProto if shape is set. nullptr if there's no shape specified. */
  const ONNX_NAMESPACE::TensorShapeProto* Shape() const;

  /** Return an indicator.
  @returns true if NodeArg is a normal tensor with a non-empty shape or a scalar with an empty shape. Otherwise, returns false. */
  bool HasTensorOrScalarShape() const;

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  /** Sets the shape.
  @remarks Shape can only be set if the TypeProto was provided to the ctor, or #SetType has been called,
  as the shape information is stored as part of TypeProto. */
  void SetShape(const ONNX_NAMESPACE::TensorShapeProto& shape);

  /** Clears shape info.
  @remarks If there is a mismatch during shape inferencing that can't be resolved the shape info may be removed. */
  void ClearShape();

#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD)

  /** Override current type from input_type if override_types is set to true, return failure status otherwise.
  @param input_tensor_elem_type Tensor element type parsed input_type
  @param current_tensor_elem_type Tensor element type parsed from existing type
  @param override_types If true, resolve the two inputs or two outputs type when different
  @returns Success unless there is existing type or shape info that can't be successfully updated. */
  common::Status OverrideTypesHelper(const ONNX_NAMESPACE::TypeProto& input_type,
                                     int32_t input_tensor_elem_type,
                                     int32_t current_tensor_elem_type,
                                     bool override_types);

  /** Validate and merge type [and shape] info from input_type.
  @param strict If true, the shape update will fail if there are incompatible values.
                If false, will be lenient and merge only shape info that can be validly processed.
  @param override_types If true, resolve the two inputs or two outputs type when different
  @returns Success unless there is existing type or shape info that can't be successfully updated. */
  common::Status UpdateTypeAndShape(const ONNX_NAMESPACE::TypeProto& input_type, bool strict, bool override_types, const logging::Logger& logger);

  /** Validate and merge type [and shape] info from node_arg.
  @param strict If true, the shape update will fail if there are incompatible values.
                If false, will be lenient and merge only shape info that can be validly processed.
  @param override_types If true, resolve the two inputs or two outputs type when different
  @returns Success unless there is existing type or shape info that can't be successfully updated. */
  common::Status UpdateTypeAndShape(const NodeArg& node_arg, bool strict, bool override_types, const logging::Logger& logger);

#endif  // !defined(ORT_MINIMAL_BUILD)

  /** Gets this NodeArg as a NodeArgInfo, AKA ValueInfoProto. */
  const NodeArgInfo& ToProto() const noexcept { return node_arg_info_; }

  /** Gets a flag indicating whether this NodeArg exists or not.
  Optional inputs are allowed in ONNX and an empty #Name represents a non-existent input argument. */
  bool Exists() const noexcept;

  friend class Graph;

  NodeArg(NodeArgInfo&& node_arg_info);

 private:
  ORT_DISALLOW_COPY_AND_ASSIGNMENT(NodeArg);
  void SetType(const std::string* p_type);
#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  void SetType(const ONNX_NAMESPACE::TypeProto& type_proto);
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  // Node arg PType.
  const std::string* type_;

  // Node arg name, type and shape.
  NodeArgInfo node_arg_info_;

  // Flag indicates whether <*this> node arg exists or not.
  bool exists_;
};
}  // namespace onnxruntime
