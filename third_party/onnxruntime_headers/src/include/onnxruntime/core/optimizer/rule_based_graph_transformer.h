// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/common/common.h"
#include "core/graph/graph_viewer.h"
#include "core/optimizer/graph_transformer.h"
#include "core/optimizer/rewrite_rule.h"

namespace onnxruntime {

/**
@class RuleBasedGraphTransformer

Rule-based graph transformer that provides an API to register rewrite rules
and an API to apply all applicable rules to a Graph.

Represents an IGraphTransformer determined by a set of rewrite rules.
The transformer will apply all the rewrite rules iteratively as determined by the underlying rewriting strategy.
Several rewriting-strategies are possible when traversing the graph and applying rewrite rules,
each with different trade offs. At the moment, we define one that performs top-down traversal of nodes.

@TODO: Is a bottom-up traversal more efficient?
@TODO: Is it worth adding the max number of passes a rule should be applied for?
@TODO: We need to define a contract about whether a rewrite rule is allowed to leave
       the graph in an inconsistent state (this will determine when and where we will be
       calling Graph::resolve().
*/
class RuleBasedGraphTransformer : public GraphTransformer {
 public:
  RuleBasedGraphTransformer(const std::string& name,
                            const InlinedHashSet<std::string_view>& compatible_execution_providers = {})
      : GraphTransformer(name, compatible_execution_providers) {}

  /** Registers a rewrite rule in this transformer. */
  Status Register(std::unique_ptr<RewriteRule> rule);

  /** Gets the list of registered rewrite rules that will be triggered on nodes with the given op type
      by this rule-based transformer.
      @returns a pointer to the vector containing all the registered rewrite rules. */
  const InlinedVector<std::reference_wrapper<const RewriteRule>>* GetRewriteRulesForOpType(const std::string& op_type) const {
    auto rules = op_type_to_rules_.find(op_type);
    return (rules != op_type_to_rules_.cend()) ? &rules->second : nullptr;
  }

  /** Gets the rewrite rules that are evaluated on all nodes irrespective of their op type.
      @returns a pointer to the vector containing all such rewrite rules or nullptr if no such rule. */
  const InlinedVector<std::reference_wrapper<const RewriteRule>>* GetAnyOpRewriteRules() const {
    return &any_op_type_rules_;
  }

  /** Returns the total number of rules that are registered in this transformer. */
  size_t RulesCount() const;

 protected:
  /** Applies the given set of rewrite rules on the Node of this Graph.
      @param[in] graph The Graph.
      @param[in] node The Node to apply the rules to.
      @param[in] rules The vector of RewriteRules that will be applied to the Node.
      @param[out] rule_effect Enum that indicates whether and how the graph was modified as a result of
      applying rules on this node.
      @returns Status indicating success or providing error information. */
  common::Status ApplyRulesOnNode(Graph& graph, Node& node,
                                  gsl::span<const std::reference_wrapper<const RewriteRule>> rules,
                                  RewriteRule::RewriteRuleEffect& rule_effect, const logging::Logger& logger) const;

 private:
  using RuleEffect = RewriteRule::RewriteRuleEffect;

  // The list of unique pointers for all rules (so that rules can be registered for several op types).
  InlinedVector<std::unique_ptr<RewriteRule>> rules_;
  // Map that associates a node's op type with the vector of rules that are registered to be triggered for that node.
  InlinedHashMap<std::string, InlinedVector<std::reference_wrapper<const RewriteRule>>> op_type_to_rules_;
  // Rules that will be evaluated regardless of the op type of the node.
  InlinedVector<std::reference_wrapper<const RewriteRule>> any_op_type_rules_;

  // Performs a single top-down traversal of the graph and applies all registered rules.
  common::Status ApplyImpl(Graph& graph, bool& modified, int graph_level, const logging::Logger& logger) const override;
};

}  // namespace onnxruntime
