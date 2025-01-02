// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/common/common.h"
#include "core/graph/graph_viewer.h"

namespace onnxruntime {

/**
@class RewriteRule

The base class for a rewrite rule. A rewrite rule represents a semantics-preserving transformation of a
computation graph. It can be used to represent, for example, the elimination of operators that serve as
no-ops (e.g., dropout during inference), as well as inlining of "function" definitions or the dual operation
of replacing a complex expression by an equivalent function-call). Unlike the more general GraphTransformer,
a rewrite rule is a more local transformation that is triggered on a particular node of the graph.

Each rule has a set of conditions and a body. The conditions have to be satisfied for the body of the rule
to be triggered. Therefore, when creating a new rewrite rule, two main functions have to be implemented:
- SatisfyCondition defines the condition checks. It is advisable to add the more selective checks first,
  because those will lead to discarding fast rules that cannot be applied on a node.
- Apply is the actual body of the rule that will be executed if SatisfyCondition returns true for a particular
  node. Note that additional, more complex checks can be included in the Apply if putting them in the
  SatisfyCondition would lead to duplicate work (e.g., when we make a check on a Node attribute but we need
  that attribute to execute the rule too).
In general, simple fast checks are a better fit for SatisfyCondition, whereas more complex ones can be added
in the Apply.

In order to avoid evaluating the SatisfyCondition for each rule and each node of the graph, each rewrite rule
should specify the target op types for which a rule will be evaluated, by overriding the TargetOpTypes() function.
If the op type of a node is not included in the target op types of a rule, that rule would not be considered at all.
If the list of op types is left empty, that rule will be triggered for every op type.
*/
class RewriteRule {
 public:
  /**
  @class RewriteRuleEffect

  Class used to indicate the effect of rule application on a graph's node.
  */
  enum class RewriteRuleEffect : uint8_t {
    kNone,                // The rewrite rule has not modified the graph.
    kUpdatedCurrentNode,  // The rewrite rule updated (but did not remove) the node on which it was triggered.
    kRemovedCurrentNode,  // The rewrite rule removed the node on which it was triggered.
    kModifiedRestOfGraph  // The rewrite rule modified nodes other than the one it was triggered on.
  };

  RewriteRule(const std::string& name) : name_(name) {}

  virtual ~RewriteRule() = default;

  /** Gets the name of this rewrite rule. */
  const std::string& Name() const noexcept {
    return name_;
  }

  /** Returns the node op types for which this rule will be triggered. If the op type of a node is not included in the
      target op types of a rule, that rule would not be considered at all. Returning an empty list indicates that we
      will attempt to trigger the rule for every op type. */
  virtual std::vector<std::string> TargetOpTypes() const noexcept = 0;

  /** Checks if the condition of the rule is satisfied, and if so applies the body of the rule.
      @param[in] graph The Graph.
      @param[in] node The Node to apply the rewrite to.
      @param[out] rule_effect Enum to indicate if and how the graph was modified as a result of the rule application.
      @returns Status indicating success or providing error information */
  common::Status CheckConditionAndApply(Graph& graph, Node& node, RewriteRuleEffect& rule_effect, const logging::Logger& logger) const {
    return SatisfyCondition(graph, node, logger) ? Apply(graph, node, rule_effect, logger) : Status::OK();
  }

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(RewriteRule);

  const std::string name_;

  /** Checks if the Node of the given Graph satisfies the conditions of this rule. The body of the rule will be
      evaluated if this condition function returns true. This can include a more complex pattern matching (conditions
      on the ascending or descending nodes of the node for which this rule was triggered) or some other properties
      of the nodes. */
  virtual bool SatisfyCondition(const Graph& graph, const Node& node, const logging::Logger& logger) const = 0;

  /** This is the actual body of the rule that performs the graph transformation. The transformation happens in-place.
      The return-value of node may be different from the input-value due to rewriting.
      The value of "rule_effect" indicates whether and how the graph was modified by the rule. */
  virtual common::Status Apply(Graph& graph, Node& node, RewriteRuleEffect& rule_effect, const logging::Logger& logger) const = 0;
};
}  // namespace onnxruntime
