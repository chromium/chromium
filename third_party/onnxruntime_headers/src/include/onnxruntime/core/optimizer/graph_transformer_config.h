// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>
#include <vector>
#include <type_traits>
namespace onnxruntime {

struct GraphTransformerConfiguration {
  struct PropagateCastOpsConfiguration {
    // Propagate FP16 Cast operations up and FP32 operations down
    /*
     * Cast propagation strategy.
     * One strategy is to insert casts around all the nodes with the allowed opcodes
     * and reduce, by removing redundant-casts and back-to-back-casts etc., and
     * the other is to propagate casts using flood-fill approach, expanding float16 regions in the graph
     * traversing the graph up/down.
     */
    enum class Strategy {
      None = 0,
      InsertAndReduce = 1,
      FloodFill = 2, /* Propagate FP16 Cast operations up and FP32 operations down */
    };
    using Strategy_t = std::underlying_type<Strategy>::type;
    friend constexpr Strategy operator|(const Strategy s1, const Strategy s2) {
      return static_cast<Strategy>(static_cast<Strategy_t>(s1) | static_cast<Strategy_t>(s2));
    }

    friend Strategy& operator|=(Strategy& s1, Strategy s2) {
      s1 = s1 | s2;
      return s1;
    }

    friend constexpr Strategy operator&(const Strategy s1, const Strategy s2) {
      return static_cast<Strategy>(static_cast<Strategy_t>(s1) & static_cast<Strategy_t>(s2));
    }

    friend constexpr Strategy& operator&=(Strategy& s1, Strategy s2) {
      s1 = s1 & s2;
      return s1;
    }

    friend constexpr bool operator==(Strategy s1, Strategy s2) {
      return static_cast<Strategy_t>(s1) == static_cast<Strategy_t>(s2);
    }

    friend constexpr bool operator!=(Strategy s1, Strategy s2) {
      return (s1 == s2) == false;
    }

    int level{1}; /* -1 => no cast propagation,
                       0 => use user specified list of opcodes to allow moving cast operations,
                       1 => use ORT predefined list of level 1 opcodes in addition to the user specified allow opcodes
                       2 => use ORT predefined list of level 2 opcodes in addition to the user specified allow opcodes
                    */
    Strategy strategy = Strategy::FloodFill;
    // List of allowed opcodes to consider as safe to execute in float16, while moving cast operations
    std::vector<std::string> allow;
  };

  PropagateCastOpsConfiguration propagate_cast_ops_config;
};

// The following declarations are required to refer to these operators in pybind11.
constexpr GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy operator|(GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy,
                                                                                           GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy);
constexpr GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy operator&(GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy,
                                                                                           GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy);
constexpr bool operator==(GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy, GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy);
constexpr bool operator!=(GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy, GraphTransformerConfiguration::PropagateCastOpsConfiguration::Strategy);

}  // namespace onnxruntime
