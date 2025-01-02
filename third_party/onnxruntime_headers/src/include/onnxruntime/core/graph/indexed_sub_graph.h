// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/graph/basic_types.h"
#include "core/graph/onnx_protobuf.h"

namespace onnxruntime {

class OpKernel;
class OpKernelInfo;

/**
@class IndexedSubGraph

Class containing information about a subgraph of Nodes from a Graph.
It contains a NodeIndex array of the Nodes covered by the subgraph,
and the meta definition needed for representing this subgraph as a FunctionProto,
which could be serialized/saved to a model file.
*/
struct IndexedSubGraph {
  struct MetaDef {
    std::string name;    ///< Name of customized SubGraph/FunctionProto
    std::string domain;  ///< Domain of customized SubGraph/FunctionProto
    int since_version;   ///< Since version of customized SubGraph/FunctionProto.

    ONNX_NAMESPACE::OperatorStatus status;  ///< Status of customized SubGraph/FunctionProto.

    std::vector<std::string> inputs;                 ///< Inputs of customized SubGraph/FunctionProto.
    std::vector<std::string> outputs;                ///< Outputs of customized SubGraph/FunctionProto.
    std::vector<std::string> constant_initializers;  ///< Constant initializers of customized SubGraph/FunctionProto.
    NodeAttributes attributes;                       ///< Attributes of customized SubGraph/FunctionProto.

    std::string doc_string;  ///< Doc string of customized SubGraph/FunctionProto.
#if !defined(ORT_MINIMAL_BUILD)
    /** Type and shape inference function that can optionally be defined for the fused node */
    std::function<void(ONNX_NAMESPACE::InferenceContext&)> type_and_shape_inference_function;
#endif
  };

  /** Nodes covered by this subgraph. The NodeIndex values are from the parent Graph.*/
  std::vector<onnxruntime::NodeIndex> nodes;

  enum class SourceOfSchema : uint8_t {
    CREATE,           /// create new schema from info in IndexedSubGraph instance.
                      /// schema instance will not be re-usable.
    REUSE_OR_CREATE,  /// re-use existing dynamically created schema with matching domain+name.
                      /// create re-usable schema if one is not found.
    EXISTING,         /// use existing statically registered schema.
                      /// e.g. domain+name matches ONNX or contrib op domain+op_type+opset.
  };
  // Either using an existing schema or generating reusable one when fusing nodes using the MetaDef.
  // MetaDef.domain + MetaDef.name => the domain.op_type that a schema must exist for with a valid since_version.
  SourceOfSchema schema_source{SourceOfSchema::CREATE};

  /** Set the meta definition needed to represent this subgraph as a FunctionProto
  It's needed IF AND ONLY IF there are multiple indexes contained in #nodes. */
  void SetMetaDef(std::unique_ptr<MetaDef>&& meta_def) {
    meta_def_ = std::move(meta_def);
  }

  /** Gets the meta definition needed to represent this subgraph as a FunctionProto.
  @returns MetaDef instance if it has been set. nullptr if not. */
  const MetaDef* GetMetaDef() const {
    return meta_def_.get();
  }

 private:
  // subgraph meta definition.
  std::unique_ptr<MetaDef> meta_def_;
};

}  // namespace onnxruntime
