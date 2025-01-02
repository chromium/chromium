// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>

#include "core/common/common.h"
#include "core/common/inlined_containers.h"
#include "core/framework/data_types.h"
#include "core/graph/graph_viewer.h"
#include "core/optimizer/graph_transformer_level.h"

namespace onnxruntime {

/**
@class GraphTransformer

The interface for in-place transformation of a Graph.
*/
class GraphTransformer {
 public:
  GraphTransformer(const std::string& name,
                   const InlinedHashSet<std::string_view>& compatible_execution_providers = {}) noexcept
      : name_(name), compatible_provider_types_(compatible_execution_providers) {
  }

  virtual ~GraphTransformer() = default;

  /** Gets the name of this graph transformer. */
  const std::string& Name() const noexcept {
    return name_;
  }

  const InlinedHashSet<std::string_view>& GetCompatibleExecutionProviders() const noexcept {
    return compatible_provider_types_;
  }

  /** Apply the in-place transformation defined by this transformer to the provided Graph instance.
  @param[out] modified Set to true if the Graph was modified.
  @returns Status with success or error information.
  */
  Status Apply(Graph& graph, bool& modified, const logging::Logger& logger) const;

  virtual bool ShouldOnlyApplyOnce() const { return false; }

 protected:
  /** Helper method to call ApplyImpl on any subgraphs in the Node. */
  Status Recurse(Node& node, bool& modified, int graph_level, const logging::Logger& logger) const {
    int subgraph_level = ++graph_level;
    for (auto& entry : node.GetAttributeNameToMutableSubgraphMap()) {
      auto& subgraph = *entry.second;
      ORT_RETURN_IF_ERROR(ApplyImpl(subgraph, modified, subgraph_level, logger));
    }

    return Status::OK();
  }

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(GraphTransformer);

  // Apply the transform to the graph.
  // graph_level is 0 for the main graph, and is incremented when descending into the subgraph of a node.
  // You MUST call Recurse for all valid Nodes in the graph to ensure any subgraphs in control flow nodes
  // (Scan/If/Loop) are processed as well.
  // You should avoid calling Graph::Resolve in ApplyImpl unless you are 100% sure it's required. In most cases
  // the call to Graph::Resolve in GraphTransformer::Apply after the call to ApplyImpl (if 'modified' is true)
  // should suffice.
  virtual Status ApplyImpl(Graph& graph, bool& modified, int graph_level, const logging::Logger& logger) const = 0;

  const std::string name_;
  const InlinedHashSet<std::string_view> compatible_provider_types_;
};

/**
 * @brief Immutable object to identify a kernel registration.
 *
 * This data structure is used by the graph transformers to check whether
 * a kernel is registered with the execution provider (i.e. has an
 * implementation). If not, the transformer can not generate a node with
 * such kernel.
 */
struct OpKernelRegistryId {
  const std::string op_type_;
  const std::string domain_;
  const int version_;
  const InlinedHashMap<std::string, MLDataType> type_constraints_;

  OpKernelRegistryId(
      const std::basic_string_view<char>& op,
      const std::basic_string_view<char>& domain,
      const int version,
      const std::initializer_list<std::pair<const std::string, MLDataType>>& init_list)
      : op_type_(op), domain_(domain), version_(version), type_constraints_(init_list) {}
};

}  // namespace onnxruntime
