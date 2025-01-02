// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/common/inlined_containers.h"
#include "core/framework/session_options.h"
#include "core/framework/tensor.h"
#include "core/optimizer/graph_transformer.h"
#include "core/platform/threadpool.h"

#if !defined(ORT_MINIMAL_BUILD)
#include "core/optimizer/rule_based_graph_transformer.h"
#include "core/optimizer/rewrite_rule.h"
#endif

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
#include "core/optimizer/selectors_actions/selector_action_transformer_apply_contexts.h"
#endif

namespace onnxruntime {
class IExecutionProvider;

namespace optimizer_utils {

#if !defined(ORT_MINIMAL_BUILD)

/** Generates all predefined rules for this level.
   If rules_to_enable is not empty, it returns the intersection of predefined rules and rules_to_enable.
   TODO: This is visible for testing at the moment, but we should rather make it private. */
InlinedVector<std::unique_ptr<RewriteRule>> GenerateRewriteRules(
    TransformerLevel level,
    const InlinedHashSet<std::string>& rules_to_disable = {});

/** Given a TransformerLevel, this method generates a name for the rule-based graph transformer of that level. */
std::string GenerateRuleBasedTransformerName(TransformerLevel level);

/** Generates all rule-based transformers for this level. */
std::unique_ptr<RuleBasedGraphTransformer> GenerateRuleBasedGraphTransformer(
    TransformerLevel level,
    const InlinedHashSet<std::string>& rules_to_disable,
    const InlinedHashSet<std::string_view>& compatible_execution_providers);

/** Generates all predefined (both rule-based and non-rule-based) transformers for this level.
    Any transformers or rewrite rules named in rules_and_transformers_to_disable will be excluded. */
InlinedVector<std::unique_ptr<GraphTransformer>> GenerateTransformers(
    TransformerLevel level,
    const SessionOptions& session_options,
    const IExecutionProvider& execution_provider /*required by constant folding*/,
    const logging::Logger& logger,
    const InlinedHashSet<std::string>& rules_and_transformers_to_disable = {},
    concurrency::ThreadPool* intra_op_thread_pool = nullptr,
    std::unordered_map<std::string, std::unique_ptr<Tensor>>* p_buffered_tensors = nullptr);

#endif  // !defined(ORT_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

/** Generates all predefined transformers which can be used to provide runtime optimizations for this level
    in a minimal build.
    Any transformers or rewrite rules named in rules_and_transformers_to_disable will be excluded.

    This is a distinct function from GenerateTransformers() because:
    - An ORT format model used in a minimal build will have been pre-optimized to at least level 1 when created, so
      level 1 transformers are not included.
    - In a minimal build we have limited optimization/Graph capabilities
      - Graph::Resolve is not available so the transformer must keep the Graph in a valid state
      - Limited graph_utils capabilities are included
    - Only a small subset of transformers support storing/replaying runtime optimizations with an ORT format model
      - this capability is provided by the SelectionActionTransformer infrastructure
      - the logic to determine the set of nodes a transformer should modify is captured during creation of the ORT
        format model
      - this information is saved in the ORT format model
      - only the logic to modify the set of nodes is included in the minimal build
    - The QDQFinalCleanupTransformer and NhwcTransformer transformers are also supported in a minimal build
*/
InlinedVector<std::unique_ptr<GraphTransformer>> GenerateTransformersForMinimalBuild(
    TransformerLevel level,
    const SessionOptions& session_options,
    const SatApplyContextVariant& apply_context,
    const IExecutionProvider& cpu_execution_provider,
    const logging::Logger& logger,
    const InlinedHashSet<std::string>& rules_and_transformers_to_disable = {},
    concurrency::ThreadPool* intra_op_thread_pool = nullptr,
    std::unordered_map<std::string, std::unique_ptr<Tensor>>* p_buffered_tensors = nullptr);

#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

}  // namespace optimizer_utils
}  // namespace onnxruntime
