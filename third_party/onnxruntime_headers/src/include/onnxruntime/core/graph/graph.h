// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "core/common/flatbuffers.h"

#include <gsl/gsl>

#include "core/common/common.h"
#include "core/common/path_string.h"
#include "core/common/const_pointer_container.h"
#include "core/common/inlined_containers_fwd.h"
#if !defined(ORT_MINIMAL_BUILD)
#include "core/common/inlined_containers.h"
#endif
#include "core/common/span_utils.h"
#include "core/common/status.h"
#include "core/common/logging/logging.h"
#include "core/framework/ort_value.h"
#include "core/framework/prepacked_weights_container.h"
#include "core/graph/onnx_protobuf.h"
#include "core/graph/basic_types.h"
#include "core/graph/constants.h"
#include "core/graph/function.h"
#if !defined(ORT_MINIMAL_BUILD)
#include "core/graph/function_template.h"
#endif
#include "core/graph/graph_nodes.h"
#include "core/graph/node_arg.h"
#include "core/graph/ort_format_load_options.h"

// Type from Graph API in ORT C API so can't be in a namespace
struct OrtGraph;

namespace onnxruntime {
class Graph;
struct IndexedSubGraph;
class Model;
struct ModelSavingOptions;
class OpSignature;

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
class RuntimeOptimizationRecordContainer;
#endif

namespace fbs {
struct Graph;
struct Node;
struct NodeEdge;
}  // namespace fbs

/**
@class Node
Class representing a node in the graph.
*/
class Node {
 public:
  /** Node types */
  enum class Type {
    Primitive = 0,  ///< The node refers to a primitive operator.
    Fused = 1,      ///< The node refers to a function.
  };

  explicit Node() = default;

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD) || defined(ORT_MINIMAL_BUILD_CUSTOM_OPS)
  Node(std::string_view name,
       std::string_view op_type,
       std::string_view description,
       gsl::span<NodeArg* const> input_args,
       gsl::span<NodeArg* const> output_args,
       const NodeAttributes* attributes,
       std::string_view domain) {
    Init(name, op_type, description,
         input_args,
         output_args,
         attributes, domain);
  }
#endif

  ~Node() = default;

  /**
  @class EdgeEnd
  Class representing the end of an edge. It could be an input or output edge end of a node.
  For the node's input edge end, it's the source end, as the destination end is the node itself.
  For the node's output edge end, it's the destination end, as the source end is the node itself.
  */
  class EdgeEnd {
   public:
    /**
    Construct an EdgeEnd
    @param node The source node if this is an input edge to the current node,
    or the destination node if this is an output edge from the current node.
    @param src_arg_index The node arg index of source node of the edge.
    @param dst_arg_index The node arg index of destination node of the edge.
    */
    EdgeEnd(const Node& node, int src_arg_index, int dst_arg_index) noexcept;

    /** Construct a control edge.
    @param node The node the edge joins to the current node.
    */
    explicit EdgeEnd(const Node& node) noexcept;

    /** Gets the Node that this EdgeEnd refers to. */
    const Node& GetNode() const noexcept { return *node_; }

    /** Gets the source arg index.
    @returns the source arg index of <*this> edge.*/
    int GetSrcArgIndex() const { return src_arg_index_; }

    /** Gets the destination arg index.
    @returns the destination arg index of <*this> edge.*/
    int GetDstArgIndex() const { return dst_arg_index_; }

   private:
    const Node* node_;
    const int src_arg_index_;
    const int dst_arg_index_;
  };

  /** Gets the Node's NodeIndex. */
  NodeIndex Index() const noexcept { return index_; }

  /** Gets the Node's name. */
  const std::string& Name() const noexcept { return name_; }

  /** Gets the Node's operator type. */
  const std::string& OpType() const noexcept { return op_type_; }

  /** Gets the domain of the OperatorSet that specifies the operator returned by #OpType.
   * @remarks If this is an ONNX operator the value will be kOnnxDomain not kOnnxDomainAlias
   */
  const std::string& Domain() const noexcept { return domain_; }

  /** Gets the path of the owning model if any. */
  const std::filesystem::path& ModelPath() const noexcept;

  /** Gets the Node's execution priority.
  @remarks Lower value means higher priority  */
  int Priority() const noexcept { return priority_; };

  /** Sets the execution priority of a node.
  @remarks Lower value means higher priority  */
  void SetPriority(int priority) noexcept;

  /** Gets the node description. */
  const std::string& Description() const noexcept { return description_; }

  /** Gets the Node's Node::Type. */
  Node::Type NodeType() const noexcept { return node_type_; }

  /** Gets the opset version that the Node's operator was first defined in.
  @returns Opset version. If -1 the Node's operator has not been set.
  @remarks Prefer over Op()->SinceVersion() as Op() is disabled in a minimal build
  */
  int SinceVersion() const noexcept { return since_version_; }

  /** Sets the since version (opset version that the Node's operator was first defined in.) for this node.
  @remarks Used during layout transformation for setting since version for layout transformed nodes with
  domain kMSNHWC.
  */
  void SetSinceVersion(int since_version) noexcept { since_version_ = since_version; }

#if !defined(ORT_MINIMAL_BUILD)
  /** Gets the Node's OpSchema.
  @remarks The graph containing this node must be resolved, otherwise nullptr will be returned. */
  const ONNX_NAMESPACE::OpSchema* Op() const noexcept { return op_; }

  /** Create a copy of the called op's FunctionProto if it has one. Returns true if successful. */
  bool TryGetFunctionProto(ONNX_NAMESPACE::FunctionProto& func_proto) const;

  bool CanBeInlined() const;

  /** Gets the function body if applicable otherwise nullptr. */
  const Function* GetFunctionBody() const noexcept { return func_body_.get(); }
#endif

  /**
  Helper to iterate through the container returned by #InputDefs() or #OutputDefs() and call the provided function.
  @param node_args Collection of NodeArgs returned by #InputDefs() or #OutputDefs()
  @param func Function to call for each valid NodeArg in the node_args. The function is called with the NodeArg
              and the index number in the container.
  @returns common::Status with success or error information.
  @remarks Returns immediately on error.
  */
  static common::Status ForEachWithIndex(const ConstPointerContainer<std::vector<NodeArg*>>& node_args,
                                         std::function<common::Status(const NodeArg& arg, size_t index)> func) {
    for (size_t index = 0; index < node_args.size(); ++index) {
      auto arg = node_args[index];
      if (!arg->Exists())
        continue;
      ORT_RETURN_IF_ERROR(func(*arg, index));
    }
    return common::Status::OK();
  }

  /** Gets the count of arguments for each of the Node's explicit inputs. */
  const std::vector<int>& InputArgCount() const noexcept { return definitions_.input_arg_count; }

  /** Gets the Node's input definitions.
  @remarks requires ConstPointerContainer wrapper to apply const to the NodeArg pointers so access is read-only. */
  ConstPointerContainer<std::vector<NodeArg*>> InputDefs() const noexcept {
    return ConstPointerContainer<std::vector<NodeArg*>>(definitions_.input_defs);
  }

  /** Gets the implicit inputs to this Node.
  If this Node contains a subgraph, these are the NodeArg's that are implicitly consumed by Nodes within that
  subgraph. e.g. If and Loop operators.*/
  ConstPointerContainer<std::vector<NodeArg*>> ImplicitInputDefs() const noexcept {
    return ConstPointerContainer<std::vector<NodeArg*>>(definitions_.implicit_input_defs);
  }

  /** Gets the Node's output definitions.
  @remarks requires ConstPointerContainer wrapper to apply const to the NodeArg pointers so access is read-only. */
  ConstPointerContainer<std::vector<NodeArg*>> OutputDefs() const noexcept {
    return ConstPointerContainer<std::vector<NodeArg*>>(definitions_.output_defs);
  }

#if !defined(ORT_MINIMAL_BUILD)
  /**
  Helper to iterate through the container returned by #MutableInputDefs() or #MutableOutputDefs() and call the provided function.
  @param node_args Collection of NodeArgs returned by #MutableInputDefs() or #MutableOutputDefs()
  @param func Function to call for each valid NodeArg in the node_args. The function is called with the NodeArg
              and the index number in the container.
  @returns common::Status with success or error information.
  @remarks Returns immediately on error.
  */
  static common::Status ForEachMutableWithIndex(std::vector<NodeArg*>& node_args,
                                                std::function<common::Status(NodeArg& arg, size_t index)> func) {
    for (size_t index = 0; index < node_args.size(); ++index) {
      auto arg = node_args[index];
      if (!arg->Exists())
        continue;
      ORT_RETURN_IF_ERROR(func(*arg, index));
    }
    return common::Status::OK();
  }

  /** Gets a modifiable collection of the Node's implicit input definitions. */
  std::vector<NodeArg*>& MutableImplicitInputDefs() noexcept {
    return definitions_.implicit_input_defs;
  }
#endif  // !defined(ORT_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /** Gets a modifiable count of arguments for each of the Node's explicit inputs.
  @todo This should be removed in favor of a method that updates the input args and the count.
        Currently these operations are separate which is not a good setup. */
  std::vector<int>& MutableInputArgsCount() { return definitions_.input_arg_count; }

  /** Gets a modifiable collection of the Node's input definitions. */
  std::vector<NodeArg*>& MutableInputDefs() noexcept {
    return definitions_.input_defs;
  }

  /** Gets a modifiable collection of the Node's output definitions. */
  std::vector<NodeArg*>& MutableOutputDefs() noexcept {
    return definitions_.output_defs;
  }
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  /** Struct to provide sorting between EdgeEnd instances based on NodeIndex first, and NodeArg::Name second. */
  struct EdgeEndCompare {
    bool operator()(const EdgeEnd& lhs, const EdgeEnd& rhs) const {
      if (lhs.GetNode().Index() == rhs.GetNode().Index()) {
        if (lhs.GetSrcArgIndex() == rhs.GetSrcArgIndex()) {
          return lhs.GetDstArgIndex() < rhs.GetDstArgIndex();
        }
        return lhs.GetSrcArgIndex() < rhs.GetSrcArgIndex();
      }
      return lhs.GetNode().Index() < rhs.GetNode().Index();
    }
  };

  using EdgeSet = std::set<EdgeEnd, EdgeEndCompare>;
  using EdgeConstIterator = EdgeSet::const_iterator;

  /**
  @class NodeConstIterator
  Class to provide const access to Node instances iterated via an EdgeConstIterator. */
  class NodeConstIterator {
   public:
    NodeConstIterator(EdgeConstIterator p_iter);

    bool operator==(const NodeConstIterator& p_other) const;

    bool operator!=(const NodeConstIterator& p_other) const;

    void operator++();
    void operator--();

    const Node& operator*() const;
    const Node* operator->() const;

   private:
    EdgeConstIterator m_iter;
  };

  // Functions defined to traverse a Graph as below.

  /** Gets an iterator to the beginning of the input nodes to this Node. */
  NodeConstIterator InputNodesBegin() const noexcept { return NodeConstIterator(relationships_.input_edges.cbegin()); };
  /** Gets an iterator to the end of the input nodes to this Node. */
  NodeConstIterator InputNodesEnd() const noexcept { return NodeConstIterator(relationships_.input_edges.cend()); }

  /** Gets an iterator to the beginning of the output nodes from this Node. */
  NodeConstIterator OutputNodesBegin() const noexcept {
    return NodeConstIterator(relationships_.output_edges.cbegin());
  }

  /** Gets an iterator to the end of the output nodes from this Node. */
  NodeConstIterator OutputNodesEnd() const noexcept { return NodeConstIterator(relationships_.output_edges.cend()); }

  /** Gets an iterator to the beginning of the input edges to this Node.
  @remarks There are no nullptr entries in this collection. */
  EdgeConstIterator InputEdgesBegin() const noexcept { return relationships_.input_edges.cbegin(); }

  /** Gets an iterator to the end of the input edges to this Node. */
  EdgeConstIterator InputEdgesEnd() const noexcept { return relationships_.input_edges.cend(); }

  /** Gets an iterator to the beginning of the output edges from this Node.
  @remarks There are no nullptr entries in this collection. */
  EdgeConstIterator OutputEdgesBegin() const noexcept { return relationships_.output_edges.cbegin(); }

  /** Gets an iterator to the end of the output edges from this Node. */
  EdgeConstIterator OutputEdgesEnd() const noexcept { return relationships_.output_edges.cend(); }

  /** Gets the Node's control inputs. */
  const std::set<std::string>& ControlInputs() const noexcept { return relationships_.control_inputs; }

  /** Gets the number of input edges to this Node */
  size_t GetInputEdgesCount() const noexcept { return relationships_.input_edges.size(); }

  /** Gets the number of output edges from this Node */
  size_t GetOutputEdgesCount() const noexcept { return relationships_.output_edges.size(); }

  /** Adds an AttributeProto to this Node.
  @remarks The attribute name is used as the key in the attribute map. */
  void AddAttributeProto(ONNX_NAMESPACE::AttributeProto value);

  // keep this signature in sync with ADD_ATTR_SINGLE_INTERFACE below
  /** Adds an attribute to this Node with the specified attribute name and value. */
  void AddAttribute(std::string attr_name, int64_t value);

  // keep this signature in sync with ADD_ATTR_LIST_INTERFACE below
  /** Adds an attribute to this Node with the specified attribute name and values. */
  void AddAttribute(std::string attr_name, gsl::span<const int64_t> values);

#define ADD_ATTR_SINGLE_INTERFACE(Type) \
  void AddAttribute(std::string attr_name, Type value)

#define ADD_ATTR_LIST_INTERFACE(Type) \
  void AddAttribute(std::string attr_name, gsl::span<const Type> values)

#define ADD_ATTR_INTERFACES(Type)  \
  ADD_ATTR_SINGLE_INTERFACE(Type); \
  ADD_ATTR_LIST_INTERFACE(Type)

  ADD_ATTR_INTERFACES(float);
  ADD_ATTR_INTERFACES(std::string);
  ADD_ATTR_INTERFACES(ONNX_NAMESPACE::TensorProto);
#if !defined(DISABLE_SPARSE_TENSORS)
  ADD_ATTR_INTERFACES(ONNX_NAMESPACE::SparseTensorProto);
#endif
  ADD_ATTR_INTERFACES(ONNX_NAMESPACE::TypeProto);

  ADD_ATTR_SINGLE_INTERFACE(ONNX_NAMESPACE::GraphProto);

#undef ADD_ATTR_SINGLE_INTERFACE
#undef ADD_ATTR_LIST_INTERFACE
#undef ADD_ATTR_INTERFACES

  // The below overload is made so the compiler does not attempt to resolve
  // string literals with the gsl::span overload
  template <size_t N>
  void AddAttribute(std::string attr_name, const char (&value)[N]) {
    this->AddAttribute(std::move(attr_name), std::string(value, N - 1));
  }

  /** Gets the Node's attributes. */
  const NodeAttributes& GetAttributes() const noexcept { return attributes_; }

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /** Remove the specified attribute from this Node */
  bool ClearAttribute(const std::string& attr_name);

  /** Gets the Node's mutable attributes. */
  NodeAttributes& GetMutableAttributes() noexcept { return attributes_; }

#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  /**
   * Clears removable attributes. These are no longer needed after the initialization
   * of the session. The function returns the number of removed attributes.
   */
  int PruneRemovableAttributes(gsl::span<const std::string> removable_attributes);

#if !defined(ORT_MINIMAL_BUILD)

  /** Gets the Graph instance that is instantiated from a GraphProto attribute during Graph::Resolve.
  @param attr_name Attribute name for the GraphProto attribute.
  @returns nullptr if the Graph instance has not been instantiated or attribute does not contain a GraphProto.
  */
  const Graph* GetGraphAttribute(const std::string& attr_name) const;

  /** Gets the mutable Graph instance that is instantiated from a GraphProto attribute during Graph::Resolve.
  @param attr_name Attribute name for the GraphProto attribute.
  @returns nullptr if the Graph instance has not been instantiated or attribute does not contain a GraphProto.
  */
  Graph* GetMutableGraphAttribute(const std::string& attr_name);
#endif  // !defined(ORT_MINIMAL_BUILD)

  /** Checks if the Node contains at least one subgraph (this is the case for control flow operators, such as If, Scan, Loop).
  @returns true if the Node contains a subgraph.
  */
  bool ContainsSubgraph() const {
    return !attr_to_subgraph_map_.empty();
  }

  /** Get the const subgraphs from a node.
  @remarks Creates a new vector so calling ContainsSubgraphs first is preferred. */
  std::vector<gsl::not_null<const Graph*>> GetSubgraphs() const;

  /** Gets a map of attribute name to the mutable Graph instances for all subgraphs of the Node.
  @returns Map of the attribute name that defines the subgraph to the subgraph's Graph instance.
           nullptr if the Node has no subgraphs.
  */
  const std::unordered_map<std::string, gsl::not_null<Graph*>>& GetAttributeNameToMutableSubgraphMap() {
    return attr_to_subgraph_map_;
  }

  /** Gets a map of attribute name to the mutable Graph instances for all subgraphs of the Node.
   * @returns a mutable map of mutable subgraphs.
   */
  std::unordered_map<std::string, gsl::not_null<Graph*>>& GetMutableMapOfAttributeNameToSubgraph() {
    return attr_to_subgraph_map_;
  }

  /** Gets a map of attribute name to the const Graph instances for all subgraphs of the Node.
  @returns Map of the attribute name that defines the subgraph to the subgraph's Graph instance.
           nullptr if the Node has no subgraphs.
  */
  std::unordered_map<std::string, gsl::not_null<const Graph*>> GetAttributeNameToSubgraphMap() const;

  /** Gets the execution ProviderType that this node will be executed by. */
  ProviderType GetExecutionProviderType() const noexcept { return execution_provider_type_; }

  /** Sets the execution ProviderType that this Node will be executed by. */
  void SetExecutionProviderType(ProviderType execution_provider_type) {
    execution_provider_type_ = execution_provider_type;
  }

  /** Call the provided function for all explicit inputs, implicit inputs, and outputs of this Node.
      If the NodeArg is an explicit or implicit input, is_input will be true when func is called.
      @param include_missing_optional_defs Include NodeArgs that are optional and were not provided
                                           i.e. NodeArg::Exists() == false.
      */
  void ForEachDef(std::function<void(const onnxruntime::NodeArg&, bool is_input)> func,
                  bool include_missing_optional_defs = false) const;

#if !defined(ORT_MINIMAL_BUILD)
  /** Replaces any matching definitions in the Node's explicit inputs or explicit outputs.
  @param replacements Map of current NodeArg to replacement NodeArg.
  */
  void ReplaceDefs(const std::map<const onnxruntime::NodeArg*, onnxruntime::NodeArg*>& replacements);

  /** Gets the NodeProto representation of this Node.
  @param update_subgraphs Update the GraphProto values for any subgraphs in the returned NodeProto.
                          If graph optimization has been run this is most likely required
                          to ensure the complete Graph is valid.
  */
  void ToProto(ONNX_NAMESPACE::NodeProto& proto, bool update_subgraphs = false) const;

  Status SaveToOrtFormat(flatbuffers::FlatBufferBuilder& builder,
                         flatbuffers::Offset<onnxruntime::fbs::Node>& fbs_node) const;

  flatbuffers::Offset<onnxruntime::fbs::NodeEdge>
  SaveEdgesToOrtFormat(flatbuffers::FlatBufferBuilder& builder) const;

  void SetFunctionTemplate(const FunctionTemplate& func_template);
#endif

  static Status LoadFromOrtFormat(const onnxruntime::fbs::Node& fbs_node, Graph& graph,
                                  const OrtFormatLoadOptions& load_options,
                                  const logging::Logger& logger, std::unique_ptr<Node>& node);

  Status LoadFromOrtFormat(const onnxruntime::fbs::Node& fbs_node,
                           const OrtFormatLoadOptions& load_options,
                           const logging::Logger& logger);
  Status LoadEdgesFromOrtFormat(const onnxruntime::fbs::NodeEdge& fbs_node_edgs, const Graph& graph);

  /**
  @class Definitions
  The input and output definitions for this Node.
  */
  class Definitions {
   public:
    Definitions() = default;

    /** The Node's explicit input definitions. */
    std::vector<NodeArg*> input_defs;

    /**
    The number of inputs for each argument of the operator or function which this node refers.
    @remarks For example, #input_defs has 10 elements (inputs), and #input_arg_count is {4, 6}.
    This means that 4 elements (inputs) of input_defs map to the first argument of the operator or function, and
    the other 6 map to the second argument.
    */
    std::vector<int> input_arg_count;

    /** The Node's output definitions. */
    std::vector<NodeArg*> output_defs;

    /** The Node's implicit input definitions if the Node contains one or more subgraphs
    (i.e. GraphProto attributes) and the subgraph/s implicitly consume these values.
    @remarks For example, a subgraph in an 'If' node gets all its input values via this mechanism rather than
    there being explicit inputs to the 'If' node that are passed to the subgraph.
    They are pseudo-inputs to this Node as it has an implicit dependency on them. */
    std::vector<NodeArg*> implicit_input_defs;

    ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(Definitions);

   private:
  };

  /**
  @class Relationships
  Defines the relationships between this Node and other Nodes in the Graph.
  */
  class Relationships {
   public:
    Relationships() = default;

    void Clear() noexcept {
      input_edges.clear();
      output_edges.clear();
      control_inputs.clear();
    }

    /** The edges for Nodes that provide inputs to this Node. */
    EdgeSet input_edges;

    /** The edges for Nodes that receive outputs from this Node. */
    EdgeSet output_edges;

    /** The Node names of the control inputs to this Node. */
    std::set<std::string> control_inputs;

   private:
    ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(Relationships);
  };

  // NOTE: This friendship relationship should ONLY be used for calling methods of the Node class and not accessing
  // the data members directly, so that the Node can maintain its internal invariants.
  friend class Graph;
  Node(NodeIndex index, Graph& graph) : index_(index), graph_(&graph), can_be_saved_(true) {}

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(Node);

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD) || defined(ORT_MINIMAL_BUILD_CUSTOM_OPS)
  void Init(std::string_view name,
            std::string_view op_type,
            std::string_view description,
            gsl::span<NodeArg* const> input_args,
            gsl::span<NodeArg* const> output_args,
            const NodeAttributes* attributes,
            std::string_view domain);
  void Init(std::string_view name,
            std::string_view op_type,
            std::string_view description,
            gsl::span<NodeArg* const> input_args,
            gsl::span<NodeArg* const> output_args,
            NodeAttributes&& attributes,
            std::string_view domain);
#endif

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  // internal only method to allow selected classes to directly alter the input/output definitions and arg counts
  Definitions& MutableDefinitions() noexcept;

  // internal only method to allow selected classes to directly alter the links between nodes.
  Relationships& MutableRelationships() noexcept;

  void SetNodeType(Node::Type node_type) noexcept { node_type_ = node_type; }
#endif

  // create a Graph instance for an attribute that contains a GraphProto
  void CreateSubgraph(const std::string& attr_name);

  std::vector<std::unique_ptr<Graph>>& MutableSubgraphs() noexcept { return subgraphs_; }

  // validate and update the input arg count
  common::Status UpdateInputArgCount();

  const Definitions& GetDefinitions() const noexcept { return definitions_; }
  const Relationships& GetRelationships() const noexcept { return relationships_; }

  // Node index. Default to impossible value rather than 0.
  NodeIndex index_ = std::numeric_limits<NodeIndex>::max();

  // Node name.
  std::string name_;

  // Node operator type.
  std::string op_type_;

  // OperatorSet domain of op_type_.
  std::string domain_;

#if !defined(ORT_MINIMAL_BUILD)
  // OperatorSchema that <*this> node refers to.
  const ONNX_NAMESPACE::OpSchema* op_ = nullptr;

  // Reference to the function template defined in the model.
  const FunctionTemplate* func_template_ = nullptr;

  // set/clear NodeProto that the Node was created from.
  // Set by Graph ctor when loading a model from file.
  // Cleared after first call to onnx::check_node in VerifyNodeAndOpMatch when the first Graph::Resolve runs.
  void SetOriginalNodeProto(const ONNX_NAMESPACE::NodeProto* node_proto) {
    original_node_proto_ = node_proto;
  }

  const ONNX_NAMESPACE::NodeProto* GetOriginalNodeProto() const {
    return original_node_proto_;
  }

  // NodeProto that the Node was created from. We temporarily set this as a performance optimization to avoid calling
  // Node::ToProto when running onnx::check_node in the first Graph::Resolve. At that point we know all the nodes are
  // unchanged from the original model.
  const ONNX_NAMESPACE::NodeProto* original_node_proto_ = nullptr;
#endif

  // Execution priority, lower value for higher priority
  int priority_ = 0;

  // set from op_->SinceVersion() or via deserialization when OpSchema is not available
  int since_version_ = -1;

  Node::Type node_type_ = Node::Type::Primitive;

  // The function body is owned by graph_
  std::unique_ptr<Function> func_body_ = nullptr;

  // Node doc string.
  std::string description_;

  // input/output defs and arg count
  Definitions definitions_;

  // Relationships between this node and others in the graph
  Relationships relationships_;

  // Device.
  std::string execution_provider_type_;

  // Map from attribute name to attribute.
  // This allows attribute adding and removing.
  NodeAttributes attributes_;

  // Graph that contains this Node
  Graph* graph_ = nullptr;

  // Map of attribute name to the Graph instance created from the GraphProto attribute
  std::unordered_map<std::string, gsl::not_null<Graph*>> attr_to_subgraph_map_;

  // Graph instances for subgraphs that are owned by this Node
  std::vector<std::unique_ptr<Graph>> subgraphs_;

  // Can be saved? The node cannot be saved anymore if removable attributes have been cleared.
  bool can_be_saved_;
};

/**
@class Graph
The Graph representation containing the graph inputs and outputs, the Node instances,
and the edges connecting the nodes.
*/
class Graph {  // NOLINT(clang-analyzer-optin.performance.Padding): preserve existing data member order for readability
 public:
  /** Gets the Graph name. */
  const std::string& Name() const noexcept;

  /** Gets the Graph description. */
  const std::string& Description() const noexcept;

  /** Gets the path of the owning model, if any. */
  const std::filesystem::path& ModelPath() const;

  /** Returns true if this is a subgraph or false if it is a high-level graph. */
  bool IsSubgraph() const { return parent_graph_ != nullptr; }

  /** Returns the parent graph if this is a subgraph */
  const Graph* ParentGraph() const { return parent_graph_; }

  /** Returns the mutable parent graph if this is a subgraph */
  Graph* MutableParentGraph() { return parent_graph_; }

  /** Returns the strict_shape_type_inference that was passed into the constructor. */
  bool StrictShapeTypeInference() const { return strict_shape_type_inference_; }

#if !defined(ORT_MINIMAL_BUILD)
  /** Sets the Graph name. */
  void SetName(const std::string& name);

  /** Gets the Graph description. */
  void SetDescription(const std::string& description);

  /** Replaces the initializer tensor with the same name as the given initializer tensor.
  The replacement initializer tensor must have the same type and shape as the existing initializer tensor.

  Note: This currently has linear time complexity. There is room for improvement but it would likely require changes to
  how initializer tensors are stored and tracked.
  */
  common::Status ReplaceInitializedTensor(ONNX_NAMESPACE::TensorProto new_initializer);

#if !defined(DISABLE_EXTERNAL_INITIALIZERS)
  /** This function takes externally provided data for initializers with external data
   *    and replaces graph initializers with its content.
   */
  common::Status InjectExternalInitializedTensors(const InlinedHashMap<std::string, OrtValue>& external_initializers);

  /** This function takes externally provided files in memory for initializers with external
   *    data and replaces graph initializers with its content.
   */
  common::Status InjectExternalInitializersFromFilesInMemory(
      const InlinedHashMap<PathString, std::pair<char*, size_t>>& external_initializer_files);
#endif  // !defined(DISABLE_EXTERNAL_INITIALIZERS)

#endif  // !defined(ORT_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /** Add an initializer tensor to the Graph. */
  void AddInitializedTensor(const ONNX_NAMESPACE::TensorProto& tensor_proto);
#endif

  /** Remove the initializer tensor with the provided name from the Graph. */
  void RemoveInitializedTensor(const std::string& tensor_name);

  /** Check if a given name is an initializer tensor's name in this graph. */
  bool IsInitializedTensor(const std::string& name) const;

#if !defined(DISABLE_SPARSE_TENSORS)
  /** Check if a given name is a sparse initializer's name in the model
   * we currently convert sparse_initializer field in the model into dense Tensor instances.
   * However, we sometimes want to check if this initializer was stored as sparse in the model.
   */
  bool IsSparseInitializer(const std::string& name) const;
#endif

  /** Gets an initializer tensor with the provided name.
  @param[out] value Set to the TensorProto* if the initializer is found, or nullptr if not.
  @returns True if found.
  */
  bool GetInitializedTensor(const std::string& tensor_name, const ONNX_NAMESPACE::TensorProto*& value) const;

  /** Populate `value` if an externally allocated OrtValue exists for an initializer with the given name.
   */
  bool GetOrtValueInitializer(const std::string& name, OrtValue& value) const;

  /** Gets all the initializer tensors in this Graph. */
  const InitializedTensorSet& GetAllInitializedTensors() const noexcept { return name_to_initial_tensor_; }

  /** Removes all initializer tensors from this Graph and releases the memory they were using. */
  void CleanAllInitializedTensors() noexcept;

  /** Returns true if an initializer value can be overridden by a graph input with the same name. */
  bool CanOverrideInitializer() const noexcept { return ir_version_ >= 4; }

  /** returns the initializer's TensorProto if 'name' is an initializer, is constant and
  cannot be overridden at runtime. If the initializer is not found or is not constant, a nullptr is returned.
  @param check_outer_scope If true and the graph is a subgraph,
         check ancestor graph/s for 'name' if not found in 'graph'.
  */
  const ONNX_NAMESPACE::TensorProto* GetConstantInitializer(const std::string& name, bool check_outer_scope) const;

  /** returns the initializer's TensorProto if 'name' is an initializer (both constant and overridable).
  If the initializer is not found, a nullptr is returned.
  @param check_outer_scope If true and the graph is a subgraph,
         check ancestor graph/s for 'name' if not found in 'graph'.
  @remarks check_outer_scope of true is not supported in a minimal build
  */
  const ONNX_NAMESPACE::TensorProto* GetInitializer(const std::string& name, bool check_outer_scope) const;

  /** Gets the Graph inputs excluding initializers.
  These are the required inputs to the Graph as the initializers can be optionally overridden via graph inputs.
  @remarks Contains no nullptr values. */
  const std::vector<const NodeArg*>& GetInputs() const noexcept { return graph_inputs_excluding_initializers_; }

  /** Gets the Graph inputs including initializers.
  This is the full set of inputs, in the same order as defined in the GraphProto.
  @remarks Contains no nullptr values. */
  const std::vector<const NodeArg*>& GetInputsIncludingInitializers() const noexcept {
    return graph_inputs_including_initializers_;
  }

  /** Return true if "node_arg" is a input or an initializer. Otherwise, returns false. */
  bool IsInputsIncludingInitializers(const NodeArg* node_arg) const noexcept {
    return std::find(graph_inputs_including_initializers_.begin(),
                     graph_inputs_including_initializers_.end(), node_arg) != graph_inputs_including_initializers_.end();
  }

  /** Gets the Graph inputs that are initializers
  These are overridable initializers. This is a difference between
  graph_inputs_including_initializers_ and graph_inputs_excluding_initializers_
  @remarks Contains no nullptr values. */
  const std::vector<const NodeArg*>& GetOverridableInitializers() const {
    return graph_overridable_initializers_;
  }

  /** Gets the Graph outputs.
  @remarks Contains no nullptr values.*/
  const std::vector<const NodeArg*>& GetOutputs() const noexcept { return graph_outputs_; }

  bool IsOutput(const NodeArg* node_arg) const noexcept {
    return std::find(graph_outputs_.begin(), graph_outputs_.end(), node_arg) != graph_outputs_.end();
  }

  /** Returns true if one or more of the Node outputs are Graph outputs.
  @remarks Cheaper than calling GetNodeOutputsInGraphOutputs.
  */
  bool NodeProducesGraphOutput(const Node& node) const {
    auto end_outputs = graph_outputs_.cend();
    for (auto output_def : node.OutputDefs()) {
      if (std::find(graph_outputs_.cbegin(), end_outputs, output_def) != end_outputs) {
        return true;
      }
    }
    return false;
  }

  /** Returns a vector with the indexes of the outputs of the given Node that are also Graph outputs. */
  std::vector<int> GetNodeOutputsInGraphOutputs(const Node& node) const {
    int output_idx = 0;
    std::vector<int> indexes;
    for (auto output_def : node.OutputDefs()) {
      if (std::find(GetOutputs().cbegin(), GetOutputs().cend(), output_def) != GetOutputs().cend()) {
        indexes.push_back(output_idx);
      }

      ++output_idx;
    }

    return indexes;
  }

  /** Gets the NodeArgs that represent value_info instances in the Graph.
  These are the values that are neither Graph inputs nor outputs.
  @remarks Contains no nullptr values. */
  const std::unordered_set<const NodeArg*>& GetValueInfo() const noexcept { return value_info_; }

#if !defined(ORT_MINIMAL_BUILD)
  void AddValueInfo(const NodeArg* new_value_info);
#endif

  /** Gets the Node with the specified node index.
  @returns Node instance if found. nullptr if node_index is invalid or node has been freed.
  */
  const Node* GetNode(NodeIndex node_index) const { return NodeAtIndexImpl(node_index); }

  /** Gets the mutable Node with the specified node index.
  @returns Mutable Node instance if found. nullptr if node_index is invalid or node has been freed.
  */
  Node* GetNode(NodeIndex node_index) { return NodeAtIndexImpl(node_index); }

  /** Get a GraphNodes instance that provides mutable access to all valid Nodes in the Graph. */
  GraphNodes& Nodes() noexcept { return iterable_nodes_; }

  /** Get a GraphNodes instance that provides const access to all valid Nodes in the Graph. */
  const GraphNodes& Nodes() const noexcept { return iterable_nodes_; }

  /** Get a ConstGraphNodes instance that provides access to a filtered set of valid Nodes in the Graph.
  @remarks We can't use GraphNodes as that would provide mutable access to the nodes by default, and we can't prevent
           that by returning a const instance of GraphNodes as we're creating a new instance here due to the filter
           being something we don't control (i.e. we have to return a new instance so it can't be const).
  */
  ConstGraphNodes FilteredNodes(GraphNodes::NodeFilterFunc&& filter_func) const noexcept {
    return ConstGraphNodes(nodes_, std::move(filter_func));
  }

  /** Gets the maximum NodeIndex value used in the Graph.
  WARNING: This actually returns the max index value used + 1.
  */
  int MaxNodeIndex() const noexcept { return static_cast<int>(nodes_.size()); }  // assume the casting won't overflow

  /** Gets the number of valid Nodes in the Graph.
  @remarks This may be smaller than MaxNodeIndex(), as Nodes may be removed during optimization.
  */
  int NumberOfNodes() const noexcept { return num_of_nodes_; }

  /** Gets the mutable NodeArg with the provided name.
  @returns Pointer to NodeArg if found, nullptr if not. */
  NodeArg* GetNodeArg(const std::string& name) {
    auto iter = node_args_.find(name);
    if (iter != node_args_.end()) {
      return iter->second.get();
    }
    return nullptr;
  }

  /** Gets the const NodeArg with the provided name.
  @returns Pointer to const NodeArg if found, nullptr if not. */
  const NodeArg* GetNodeArg(const std::string& name) const {
    return const_cast<Graph*>(this)->GetNodeArg(name);
  }

  // search this and up through any parent_graph_ instance for a NodeArg
  NodeArg* GetNodeArgIncludingParentGraphs(const std::string& node_arg_name);

  /** Gets a mutable NodeArg by name. Creates a new NodeArg that is owned by this Graph if not found.
  @param name The NodeArg name.
  @param[in] p_arg_type Optional TypeProto to use if the NodeArg needs to be created.
  @returns NodeArg reference.
  */
  NodeArg& GetOrCreateNodeArg(const std::string& name, const ONNX_NAMESPACE::TypeProto* p_arg_type) {
    auto insert_result = node_args_.emplace(name, nullptr);
    if (insert_result.second) {
      insert_result.first->second = std::make_unique<NodeArg>(name, p_arg_type);
    }
    return *(insert_result.first->second);
  }

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /** Generate a unique name in this Graph for a NodeArg */
  std::string GenerateNodeArgName(const std::string& base_name);

  /** Generate a unique name in this Graph for a Node */
  std::string GenerateNodeName(const std::string& base_name);
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD)
  /** Copy a Node and add it to this Graph.
  @param other Node to copy
  @returns Reference to the Node that was created and added to this Graph.
  @remarks Do not call AddNode and Remove Node concurrently as they are not thread-safe.
  */
  Node& AddNode(const Node& other);
#endif

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /** Add a Node to this Graph.
  @param name The Node name. Must be unique in this Graph.
  @param op_type The operator type. e.g. ONNX operator name.
  @param description Arbitrary description of the Node.
  @param input_args The explicit inputs to this Node.
  @param output_args The outputs from this Node.
  @param attributes Optional NodeAttributes to add.
  @param domain The domain for the op_type.
  @returns Reference to the new Node.
  @remarks Do not call AddNode and Remove Node concurrently as they are not thread-safe.
  */
  Node& AddNode(const std::string& name,
                const std::string& op_type,
                const std::string& description,
                gsl::span<NodeArg* const> input_args,
                gsl::span<NodeArg* const> output_args,
                const NodeAttributes* attributes = nullptr,
                const std::string& domain = kOnnxDomain);

  Node& AddNode(const std::string& name,
                const std::string& op_type,
                const std::string& description,
                gsl::span<NodeArg* const> input_args,
                gsl::span<NodeArg* const> output_args,
                NodeAttributes&& attributes,
                const std::string& domain = kOnnxDomain);
  Node& AddNode(const std::string& name,
                const std::string& op_type,
                const std::string& description,
                std::initializer_list<NodeArg*> input_args,
                std::initializer_list<NodeArg*> output_args,
                const NodeAttributes* attributes = nullptr,
                const std::string& domain = kOnnxDomain) {
    return AddNode(name, op_type, description,
                   AsSpan(input_args),
                   AsSpan(output_args),
                   attributes, domain);
  }

  Node& AddNode(const std::string& name,
                const std::string& op_type,
                const std::string& description,
                gsl::span<NodeArg* const> input_args,
                std::initializer_list<NodeArg*> output_args,
                const NodeAttributes* attributes = nullptr,
                const std::string& domain = kOnnxDomain) {
    return AddNode(name, op_type, description,
                   input_args,
                   AsSpan(output_args),
                   attributes, domain);
  }

  Node& AddNode(const std::string& name,
                const std::string& op_type,
                const std::string& description,
                std::initializer_list<NodeArg*> input_args,
                gsl::span<NodeArg* const> output_args,
                const NodeAttributes* attributes = nullptr,
                const std::string& domain = kOnnxDomain) {
    return AddNode(name, op_type, description,
                   AsSpan(input_args),
                   output_args,
                   attributes, domain);
  }

  /** Remove a Node from this Graph and free it.
  The output edges of this specified node MUST have been removed before removing the node.
  The input edges of this specified node is removed while removing the node. The process of
  removing a node from a graph should be,
  1. Remove out edges of this specified node.
  2. Remove this specified node.
  3. Add new input edges connected with all out nodes.
  @returns true if the node_index was valid
  @remarks Do not call AddNode and Remove Node concurrently as they are not thread-safe.
  */
  bool RemoveNode(NodeIndex node_index);

  /** Add an edge between two Nodes.
  @param src_node_index NodeIndex of source Node that is providing output to the destination Node.
  @param dst_node_index NodeIndex of destination Node that is receiving input from the source Node.
  @param src_arg_index node arg index of source node.
  @param dst_arg_index node arg index of destination node.
  */
  void AddEdge(NodeIndex src_node_index, NodeIndex dst_node_index, int src_arg_index, int dst_arg_index);

  /** Remove an edge between two Nodes.
  @param src_node_index NodeIndex of source Node to remove an output edge from.
  @param dst_node_index NodeIndex of destination Node to remove an input edge from.
  @param src_arg_index node arg index of source node.
  @param dst_arg_index node arg index of destination node.
  */
  void RemoveEdge(NodeIndex src_node_index, NodeIndex dst_node_index, int src_arg_index, int dst_arg_index);
#endif

#if !defined(ORT_MINIMAL_BUILD)
  /**
  Add a control edge between two Nodes in this Graph.
  The source Node does not produce output that is directly consumed by the destination Node, however the
  destination Node must execute after the source node. The control edge allows this ordering to occur.
  */
  bool AddControlEdge(NodeIndex src_node_index, NodeIndex dst_node_index);
#endif  // !defined(ORT_MINIMAL_BUILD)

  /** Mark the Graph as needing Resolve() to be called.
  This should be done after modifying any aspect of the Graph that changes the Nodes or relationships between them. */
  Graph& SetGraphResolveNeeded() noexcept {
    graph_resolve_needed_ = true;
    return *this;
  }

  /** Gets flag indicating whether Graph::Resolve needs to be called before using the Graph. */
  bool GraphResolveNeeded() const noexcept {
    return graph_resolve_needed_;
  }

  /** Sets flag that Graph::graph_proto_ needs to be updated to reflect changes in the Graph. */
  Graph& SetGraphProtoSyncNeeded() noexcept {
    graph_proto_sync_needed_ = true;
    return *this;
  }

  /** Gets flag indicating whether Graph::graph_proto_ needs to be synchronized with this Graph instance. */
  bool GraphProtoSyncNeeded() const noexcept {
    return graph_proto_sync_needed_;
  }

  /** Performs a reverse depth-first search (DFS) traversal from a set of nodes, via their inputs,
  up to their source node/s.
  @param from NodeIndex values for a set of Nodes to traverse from.
  @param enter Visit function that will be invoked on a node when it is visited but its parents haven't been.
  @param leave Visit function invoked on the node after its parents have all been visited.
  @param comp Comparison function to stabilize the traversal order by making Node ordering deterministic.
  */
  void ReverseDFSFrom(gsl::span<NodeIndex const> from,
                      const std::function<void(const Node*)>& enter,
                      const std::function<void(const Node*)>& leave,
                      const std::function<bool(const Node*, const Node*)>& comp = {}) const;

  /** Performs a reverse depth-first search (DFS) traversal from a set of nodes, via their inputs,
  up to their source node/s.
  @param from Set of Nodes to traverse from.
  @param enter Visit function that will be invoked on a node when it is visited but its parents haven't been.
  @param leave Visit function invoked on the node after its parents have all been visited.
  @param comp Comparison function to stabilize the traversal order by making Node ordering deterministic.
  */
  void ReverseDFSFrom(gsl::span<const Node* const> from,
                      const std::function<void(const Node*)>& enter,
                      const std::function<void(const Node*)>& leave,
                      const std::function<bool(const Node*, const Node*)>& comp = {}) const;

  /** Performs a reverse depth-first search (DFS) traversal from a set of nodes, via their inputs,
  up to their source node/s.
  @param from Set of Nodes to traverse from.
  @param enter Visit function that will be invoked on a node when it is visited but its parents haven't been.
  @param leave Visit function invoked on the node after its parents have all been visited.
  @param stop Stop traversal from node n to input node p if stop(n, p) is true.
  @param comp Comparison function to stabilize the traversal order by making Node ordering deterministic.
  */
  void ReverseDFSFrom(gsl::span<const Node* const> from,
                      const std::function<void(const Node*)>& enter,
                      const std::function<void(const Node*)>& leave,
                      const std::function<bool(const Node*, const Node*)>& comp,
                      const std::function<bool(const Node*, const Node*)>& stop) const;

#if !defined(ORT_MINIMAL_BUILD)
  /** Performs topological sort with Kahn's algorithm on the graph/s.
  @param enter Visit function that will be invoked on a node when it is visited.
  @param comp Comparison function to stabilize the traversal order by making Node ordering deterministic.
  */
  void KahnsTopologicalSort(const std::function<void(const Node*)>& enter,
                            const std::function<bool(const Node*, const Node*)>& comp) const;

#endif

#ifdef ENABLE_TRAINING
  /**
   * @brief Performs topological sort with customized Kahn's algorithm on the graph/s.
   *  This is a specialized version for training where need memory efficient topological sort.
   * @param yield_op The YieldOp used in ORTModule training.
   * @param shape_size_parents The shape size parents nodes.
   * @param node_orders The output node orders.
   */
  void MemoryEfficientTopologicalSort(const Node* yield_op,
                                      const InlinedHashMap<NodeIndex, InlinedVector<NodeIndex>>& shape_size_parents,
                                      std::vector<NodeIndex>& node_orders) const;
#endif

  /** Gets the map of operator domains to their opset versions. */
  const std::unordered_map<std::string, int>& DomainToVersionMap() const noexcept {
    return domain_to_version_;
  }

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /**
  Create a single Node that will be the result of the a fusion of multiple nodes in this Graph.
  @param sub_graph A IndexSubGraph instance with details of the nodes to fuse.
  @param fused_node_name The name for the new Node.
  @returns Node with fused subgraph.
  @remarks As a new Graph instance for the fused nodes is not created, a GraphViewer can be constructed with the
           IndexedSubGraph information to provide a view of the subgraph. The original nodes are left in place
           while this is in use.
           Call FinalizeFuseSubGraph to remove them once the fused replacement node is fully created.
  */
  Node& BeginFuseSubGraph(const IndexedSubGraph& sub_graph, const std::string& fused_node_name);

  void FinalizeFuseSubGraph(const IndexedSubGraph& sub_graph, Node& fused_node);
#endif

#if !defined(ORT_MINIMAL_BUILD)
  /** Gets the GraphProto representation of this Graph. */
  const ONNX_NAMESPACE::GraphProto& ToGraphProto();
  ONNX_NAMESPACE::GraphProto ToGraphProto() const;

  /** Gets the GraphProto representation of this Graph
  @param external_file_path File path of the binary file to use for initializers.
  @param model_file_path path of the model file.
  @param initializer_size_threshold initializers larger or equal to this threshold (in bytes) are saved
  in the external file. Initializer smaller than this threshold are included in the onnx file.
  @param align_info offset alignment info.
  @returns GraphProto serialization of the graph.
  */
  ONNX_NAMESPACE::GraphProto ToGraphProtoWithExternalInitializers(const std::filesystem::path& external_file_path,
                                                                  const std::filesystem::path& model_file_path,
                                                                  const ModelSavingOptions& model_saving_options) const;

  /** Gets the ISchemaRegistry instances being used with this Graph. */
  IOnnxRuntimeOpSchemaCollectionPtr GetSchemaRegistry() const;

  /**
  Looks up the op schema in the schema registry and sets it for the given node.
  @param node The node to update.
  @return Whether the node's op schema was set to a valid value.
  */
  bool SetOpSchemaFromRegistryForNode(Node& node);

  /**
  Create a single Function based Node that is the result of the a fusion of multiple nodes in this Graph.
  A new Graph instance will be created for the fused nodes.
  @param sub_graph A IndexSubGraph instance with details of the nodes to fuse. Ownership is transferred to the new Node
  @param fused_node_name The name for the new Node.
  @returns Function based Node with fused subgraph. The Node body will contain a Function instance.
  */
  Node& FuseSubGraph(const IndexedSubGraph& sub_graph, const std::string& fused_node_name);

  /**
    Directly insert one of the If node branches into this Graph.
    `If` node condition must be a constant. The function would
    rename the nodes of the corresponding subgraph to make sure there is no conflict.

    Explicit and implicit inputs references stay the same.

    All of the outputs of the subgraph being inlined should be renamed
    to the outputs of the If node.

    The function will process any subgraphs in each of the nodes being inlined,
    and will rename any references to the new names introduced.

    @param condition_value If condition value
    @param if_node - the node that contains the graph_to_inline. This node is going
    to be deleted and replaced by the corresponding graph (either then or else)
    @param logger
  */
  Status InlineIfSubgraph(bool condition_value, Node& if_node, const logging::Logger& logger);

  /**
  Directly insert the nodes in the function Node provided into this Graph.
  The Graph needs to be Resolve()d after this call.
  @param node Node with Node::Type of Node::Type::Fused
  @returns Status indicating success or providing an error message.
  */
  Status InlineFunction(Node& node);

  /**
  Directly insert the nodes in the function proto provided into the graph.
  The function converts Constant nodes into the initializers in the graph.
  It then creates a node in the graph for each of the function nodes.
  All of the names are expected to be specialized, and, therefore unique.
  See function_utils::Specialize().

  The Graph needs to be Resolve()d after this call.
  @param func_to_inline
  @returns Status indicating success or providing an error message.
  */

  Status InlineFunctionProto(const ONNX_NAMESPACE::FunctionProto& func_to_inline);

  /** Mark a NodeArg name as coming from the outer scope when programmatically constructing a Graph that will
  be used as a GraphProto attribute in another Node.
  e.g. when creating a Graph instance that will be used as a subgraph in a control flow operator, it is necessary to
  define placeholder NodeArgs for outer scope values. This prevents these values from becoming explicit graph inputs
  when the Graph is resolved.
  */
  void AddOuterScopeNodeArg(const std::string& name) {
    ORT_IGNORE_RETURN_VALUE(outer_scope_node_arg_names_.insert(name));
  }

  /** Explicitly set graph inputs.
  @param inputs NodeArgs that represent complete graph inputs which need to be explicitly ordered.
  @remarks Note that the input order matters for subgraphs.
  */
  void SetInputs(gsl::span<const NodeArg* const> inputs);

  void SetInputs(std::initializer_list<const NodeArg*> inputs) {
    SetInputs(AsSpan(inputs));
  }

  const Model& GetModel() const {
    return owning_model_;
  }

  const logging::Logger& GetLogger() const {
    return logger_;
  }

  /** Explicitly set graph outputs.
  @param outputs NodeArgs that represent complete graph outputs which need to be explicitly ordered.
  @remarks Note that the output order matters for subgraphs.
  */
  void SetOutputs(gsl::span<const NodeArg* const> outputs);

  void SetOutputs(std::initializer_list<const NodeArg*> outputs) {
    SetOutputs(AsSpan(outputs));
  }

#endif  // !defined(ORT_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /** Sets the type of a NodeArg, replacing existing type/shape if any */
  void SetNodeArgType(NodeArg& arg, const ONNX_NAMESPACE::TypeProto& type_proto);

  const Node* GetProducerNode(const std::string& node_arg_name) const {
    return GetProducerNodeImpl(*this, node_arg_name);
  }

  Node* GetMutableProducerNode(const std::string& node_arg_name) {
    return GetProducerNodeImpl(*this, node_arg_name);
  }

  void UpdateProducerNode(const std::string& node_arg_name, NodeIndex node_index) {
    auto iter = node_arg_to_producer_node_.find(node_arg_name);

    if (iter != node_arg_to_producer_node_.end()) {
      iter->second = node_index;
    } else {
      node_arg_to_producer_node_[node_arg_name] = node_index;
    }
  }

  std::vector<const Node*> GetConsumerNodes(const std::string& node_arg_name) const {
    return GetConsumerNodesImpl(*this, node_arg_name);
  }

  // Without removing the existing consumers, add a consumer to the give node arg name.
  void AddConsumerNode(const std::string& node_arg_name, Node* consumer) {
    node_arg_to_consumer_nodes_[node_arg_name].insert(consumer->Index());
  }

  // Remove a consumer from the set
  void RemoveConsumerNode(const std::string& node_arg_name, Node* consumer) {
    node_arg_to_consumer_nodes_[node_arg_name].erase(consumer->Index());
  }
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD)
  std::vector<Node*> GetMutableConsumerNodes(const std::string& node_arg_name) {
    return GetConsumerNodesImpl(*this, node_arg_name);
  }

  void UpdateConsumerNodes(const std::string& node_arg_name, gsl::span<Node* const> nodes) {
    // Replace nodes for the arg
    auto& nodes_for_arg = node_arg_to_consumer_nodes_[node_arg_name];
    if (!nodes_for_arg.empty()) {
      nodes_for_arg.clear();
    }

    nodes_for_arg.reserve(nodes.size());
    for (Node* node : nodes) {
      nodes_for_arg.insert(node->Index());
    }
  }

  void UpdateConsumerNodes(const std::string& node_arg_name, std::initializer_list<Node*> nodes) {
    UpdateConsumerNodes(node_arg_name, AsSpan(nodes));
  }

  /** During constant folding it may become possible to infer the shape for a node.
      To avoid running a full Resolve allow an individual node to have the shape inferencing re-run.
  */
  Status UpdateShapeInference(Node& node);

  // Options to control Graph::Resolve.
  struct ResolveOptions {
    // Whether to override existing types with inferred types.
    bool override_types = false;
    // Names of initializers to keep even if unused (optional).
    const std::unordered_set<std::string>* initializer_names_to_preserve = nullptr;
    // Whether to set that no proto sync is required after resolving.
    // Useful for resolving right after loading from a GraphProto.
    bool no_proto_sync_required = false;
  };

  /**
  Resolve this Graph to ensure it is completely valid, fully initialized, and able to be executed.
  1. Run through all validation rules.
    a. Node name and node output's names should be unique.
    b. Attribute match between node and op definition.
    c. Input/Output match between node and op definition.
    d. Graph is acyclic and sort nodes in topological order.
  2. Check & Setup inner nodes' dependency.
  3. Cleanup function definition lists.
  Note: the weights for training can't be cleaned during resolve.
  @returns common::Status with success or error information.
  */
  common::Status Resolve(const ResolveOptions& options);

  common::Status Resolve() {
    ResolveOptions default_options;
    return Resolve(default_options);
  }

  const std::unordered_set<std::string>& GetOuterScopeNodeArgNames() const noexcept {
    return outer_scope_node_arg_names_;
  }

  common::Status SaveToOrtFormat(flatbuffers::FlatBufferBuilder& builder,
                                 flatbuffers::Offset<onnxruntime::fbs::Graph>& fbs_graph) const;

#endif  // !defined(ORT_MINIMAL_BUILD)

  // This function constructs PrepackedSharedContainer in the root graph only
  // and initializes a reference to it in all (sub)graphs
  void ConstructPrepackedSharedContainerAndSetMode(bool saving_mode_on);

  const PrepackedWeightsForGraph& GetPrepacked() const noexcept {
    return *prepacked_weights_for_graph_;
  }

  PrepackedWeightsForGraph& GetPrepacked() noexcept {
    return *prepacked_weights_for_graph_;
  }

  /** Returns the Node containing the GraphProto for this Graph instance if IsSubgraph is true */
  const Node* ParentNode() const { return parent_node_; }

  /** Returns true if the name is for a value that is coming from outer scope */
  bool IsOuterScopeValue(const std::string& name) const {
    if (!parent_node_) return false;
    const auto& implicit_input_defs = parent_node_->ImplicitInputDefs();
    return std::any_of(implicit_input_defs.cbegin(), implicit_input_defs.cend(),
                       [&name](const NodeArg* implicit_input) {
                         return implicit_input->Name() == name;
                       });
  }

#if !defined(ORT_MINIMAL_BUILD)
  /** Construct a Graph instance for a subgraph that is created from a GraphProto attribute in a Node.
  Inherits some properties from the parent graph.
  @param parent_graph The Graph containing the Node that has the GraphProto attribute.
  @param parent_node The Node that has the GraphProto attribute.
  @param subgraph_proto The GraphProto from the Node attribute.
  */
  Graph(Graph& parent_graph, const Node& parent_node, ONNX_NAMESPACE::GraphProto& subgraph_proto);

  Graph(const Model& owning_model,
        IOnnxRuntimeOpSchemaCollectionPtr schema_registry,
        ONNX_NAMESPACE::GraphProto& subgraph_proto,
        const std::unordered_map<std::string, int>& domain_version_map,
        const logging::Logger& logger,
        bool strict_shape_type_inference);
#endif

  virtual ~Graph();

  static Status LoadFromOrtFormat(const onnxruntime::fbs::Graph& fbs_graph, const Model& owning_model,
                                  const std::unordered_map<std::string, int>& domain_to_version,
#if !defined(ORT_MINIMAL_BUILD)
                                  IOnnxRuntimeOpSchemaCollectionPtr schema_registry,
#endif
                                  const OrtFormatLoadOptions& load_options,
                                  const logging::Logger& logger, std::unique_ptr<Graph>& graph);

  // deserialize a subgraph
  static Status LoadFromOrtFormat(const onnxruntime::fbs::Graph& fbs_graph,
                                  Graph& parent_graph, const Node& parent_node,
                                  const OrtFormatLoadOptions& load_options,
                                  const logging::Logger& logger, std::unique_ptr<Graph>& graph);

  static Status LoadFromModelBuilderApiModel(const OrtGraph& api_graph,
                                             const Model& owning_model,
                                             const std::unordered_map<std::string, int>& domain_to_version,
                                             IOnnxRuntimeOpSchemaCollectionPtr schema_registry,
                                             bool strict_shape_type_inference,
                                             const logging::Logger& logger,
                                             std::unique_ptr<Graph>& graph);

  Status UpdateUsingModelBuilderApiModel(const OrtModel& api_model);

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  const RuntimeOptimizationRecordContainer& RuntimeOptimizations() const {
    return runtime_optimizations_;
  }

  RuntimeOptimizationRecordContainer& MutableRuntimeOptimizations() {
    return runtime_optimizations_;
  }

  // We don't run Graph::Resolve() on an ORT format model, but a compiling EP may copy initializers to its
  // compiled model during partitioning, leaving them unused in the ORT Graph. To allow the memory to be freed
  // we need to manually run the cleanup that would usually happen as part of Graph::Resolve.
  Status RemovedUnusedInitializersOrtFormat();
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  // This friendship relationship should only be used to call Graph::Graph and
  // Graph::LoadGraph All other access should be via the public API.
  friend class Model;

  Graph() = delete;

  // Create empty Graph instance to re-create from ORT format serialized data.
  Graph(const Model& owning_model,
        const std::unordered_map<std::string, int>& domain_to_version,
#if !defined(ORT_MINIMAL_BUILD)
        IOnnxRuntimeOpSchemaCollectionPtr schema_registry,
#endif
        Graph* parent_graph, const Node* parent_node,
        const logging::Logger& logger,
        bool strict_shape_type_inference);

  // Populate Graph instance from ORT format serialized data.
  Status LoadFromOrtFormat(const onnxruntime::fbs::Graph& fbs_graph,
                           const OrtFormatLoadOptions& load_options);

#if !defined(ORT_MINIMAL_BUILD)
  // Constructor: Given a <GraphProto> loaded from model file, construct
  // a <Graph> object. Used by Model to create a Graph instance.
  Graph(const Model& owning_model,
        ONNX_NAMESPACE::GraphProto* graph_proto,
        const std::unordered_map<std::string, int>& domain_to_version,
        Version ir_version,
        IOnnxRuntimeOpSchemaCollectionPtr schema_registry,
        const logging::Logger& logger,
        bool strict_shape_type_inference);

  // internal use by the Graph class only
  Graph(const Model& owning_model,
        ONNX_NAMESPACE::GraphProto* graph_proto,
        const std::unordered_map<std::string, int>& domain_to_version,
        Version ir_version,
        IOnnxRuntimeOpSchemaCollectionPtr schema_registry,
        Graph* parent_graph,
        const Node* parent_node,
        const logging::Logger& logger,
        bool strict_shape_type_inference);

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(Graph);

 private:
  void InitializeStateFromModelFileGraphProto();

  // Add node with specified <node_proto>.
  Node& AddNode(const ONNX_NAMESPACE::NodeProto& node_proto,
                const ArgNameToTypeMap& name_to_type);

  /** Helper that converts and adds constant node proto to an initializer in the graph.
   @param constant_node_proto Constant node to convert
   @param new_name use the new name for the initializer.
  */
  Status AddConstantProtoAsInitializer(const ONNX_NAMESPACE::NodeProto& constant_node_proto,
                                       std::optional<std::string_view> new_name);

  /// <summary>
  /// This function traverses the graph bottom up and externalizes
  /// constant initializers along with their pre-packed blobs from different
  /// kernels. Writes constant initializers to the external file with any pre-packed
  /// blobs (if enabled and produced for this initializer) and then modifies TensorProto
  /// entry with external data references.
  /// </summary>
  /// <param name="model_path">model file path from Model</param>
  /// <param name="external_file_path">a binary file path for relative to the model file path
  /// where the initializers data is written</param>
  /// <param name="model_external_file_path">model file folder path with external file path appended</param>
  /// <param name="model_saving_options">model saving options including alignment and pre-packs</param>
  /// <param name="output_graph_proto">The graph proto to be modified</param>
  /// <param name="external_stream">external file stream</param>
  /// <param name="external_offset">current external file offset updated with each write</param>
  /// <returns>Status instance</returns>
  Status AddExternalInitializersToGraphProtoImpl(
      const std::filesystem::path& model_path,
      const std::filesystem::path& external_file_path,
      const std::filesystem::path& model_external_file_path,
      const ModelSavingOptions& model_saving_options,
      ONNX_NAMESPACE::GraphProto& output_graph_proto,
      std::ostream& external_stream,
      int64_t& external_offset) const;

#endif

  Version IrVersion() const noexcept {
    return ir_version_;
  }

  Graph& GraphResolveNeeded(bool needed) noexcept {
    graph_resolve_needed_ = needed;
    return *this;
  }

  Graph& GraphProtoSyncNeeded(bool needed) noexcept {
    graph_proto_sync_needed_ = needed;
    return *this;
  }

  // During the Resolve of a Graph it is necessary to recursively descend into subgraphs (created from GraphProto
  // Node attributes in the Graph) if present.
  // The ResolveContext holds the collection of values for the current Graph instance, be it the main graph
  // or a subgraph, so that the various operations that are part of the Resolve can work iteratively or
  // recursively as needed.
  struct ResolveContext {
    ResolveContext(const Graph& owning_graph) : graph{owning_graph} {
    }

    std::unordered_map<std::string_view, std::pair<Node*, int>> output_args;
    std::unordered_set<std::string_view> inputs_and_initializers;
    std::unordered_map<std::string_view, NodeIndex> node_name_to_index;
    std::unordered_set<Node*> nodes_with_subgraphs;

    // check if the provided name is an input/initialize/node output of this Graph instance during Graph::Resolve.
    // Graph::node_args_ can have stale entries so we can't rely on that.
    bool IsLocalValue(const std::string& name) const;

    // check if an ancestor graph has a valid value with the provided name during Graph::Resolve.
    // Once Graph::Resolve completes Graph::IsOuterScopeValue can be used and is more efficient.
    bool IsOuterScopeValue(const std::string& name) const;

    void Clear() {
      output_args.clear();
      inputs_and_initializers.clear();
      node_name_to_index.clear();
      nodes_with_subgraphs.clear();
    }

   private:
    bool IsInputInitializerOrOutput(const std::string& name, bool check_ancestors) const;

    const Graph& graph;
    ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(ResolveContext);
  };

  // Initialize all the graph inputs, initializers and outputs
  common::Status InitInputsInitializersOutputs();

  // Initialize overridable initializers container
  void ComputeOverridableInitializers();

#if !defined(ORT_MINIMAL_BUILD)
  // Build and verify node connection (edges).
  // Verify NodeArg name/type/shape matching correctly.
  common::Status BuildConnections(std::unordered_set<std::string>& outer_scope_node_args_consumed);

  common::Status VerifyNoDuplicateName();

  // Check whether <*this> graph is acyclic while performing a topological sort.
  // Depth-first going from bottom up through the graph and checking whether there are any back edges.
  // NodesInTopologicalOrder is updated with the nodes' indexes in topological
  // order if <Status> returned is "OK", otherwise it's undefined.
  common::Status PerformTopologicalSortAndCheckIsAcyclic();

  common::Status PerformTypeAndShapeInferencing(const ResolveOptions& options);

  common::Status InferAndVerifyTypeMatch(Node& node, const ONNX_NAMESPACE::OpSchema& op, const ResolveOptions& options);

  // perform type and shape inferencing on the subgraph and Resolve to validate
  static common::Status InferAndVerifySubgraphTypes(const Node& node, Graph& subgraph,
                                                    const std::vector<const ONNX_NAMESPACE::TypeProto*>& input_types,
                                                    std::vector<const ONNX_NAMESPACE::TypeProto*>& output_types,
                                                    const Graph::ResolveOptions& options);

  // Apply type-inference and type-checking to all inputs and initializers:
  common::Status TypeCheckInputsAndInitializers();

  // Compute set of input and initializer names and checking for duplicate names
  common::Status VerifyInputAndInitializerNames();

  // Infer and set type information across <*this> graph if needed, and verify type/attribute
  // information matches between node and op.

  common::Status VerifyNodeAndOpMatch(const ResolveOptions& options);

  // Set graph inputs/outputs when resolving a graph..
  common::Status SetGraphInputsOutputs();

  // recursively accumulate and set the outer scope node args in the resolve context for all subgraphs
  // so they can be used to resolve outer scope dependencies when running BuildConnections for the subgraphs.
  common::Status SetOuterScopeNodeArgs(const std::unordered_set<std::string>& outer_scope_node_args);

  // Implementation for initializer replacement
  Status ReplaceInitializedTensorImpl(ONNX_NAMESPACE::TensorProto new_initializer, bool is_external);

  template <typename StringRange>  // range-initializer returning std::string
  std::vector<NodeArg*> CreateNodeArgs(const StringRange& names,
                                       const ArgNameToTypeMap& name_to_type_map);

  void ToGraphProtoInternal(ONNX_NAMESPACE::GraphProto& graph_proto) const;

#endif  // !defined(ORT_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  // Recursively find all subgraphs including nested subgraphs
  void FindAllSubgraphs(std::vector<Graph*>& subgraphs);

  // Iterate this Graph instance and all subgraphs, calling the provided function for each.
  common::Status ForThisAndAllSubgraphs(const std::vector<Graph*>& subgraphs, std::function<Status(Graph&)> func);

  // Clear all unused initializers and NodeArgs
  void CleanUnusedInitializersAndNodeArgs(const std::unordered_set<std::string>* initializer_names_to_preserve = nullptr);

  Status PopulateNodeArgToProducerConsumerLookupsFromNodes();

  template <typename TInstance>
  static auto GetConsumerNodesImpl(
      TInstance& instance, const std::string& node_arg_name) -> std::vector<decltype(instance.GetNode(0))> {
    std::vector<decltype(instance.GetNode(0))> results;
    auto iter = instance.node_arg_to_consumer_nodes_.find(node_arg_name);
    if (iter != instance.node_arg_to_consumer_nodes_.end()) {
      results.reserve(iter->second.size());
      for (auto node_index : iter->second) {
        results.push_back(instance.GetNode(node_index));
      }
    }
    return results;
  }

  template <typename TInstance>
  static auto GetProducerNodeImpl(
      TInstance& instance, const std::string& node_arg_name) -> decltype(instance.GetNode(0)) {
    auto iter = instance.node_arg_to_producer_node_.find(node_arg_name);
    if (iter != instance.node_arg_to_producer_node_.end()) {
      auto node_index = iter->second;
      return instance.GetNode(node_index);
    }
    return nullptr;
  }

  gsl::not_null<Node*> AllocateNode();

  // Release the node.
  // @returns false if node_index was invalid.
  bool ReleaseNode(NodeIndex node_index);

  Node& CreateFusedSubGraphNode(const IndexedSubGraph& sub_graph, const std::string& fused_node_name);
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  Node* NodeAtIndexImpl(NodeIndex node_index) const {
    // if we are trying to access a node that doesn't exist there's (most
    // likely) either a logic issue or a graph consistency/correctness issue.
    // use ORT_ENFORCE to prove that or uncover scenarios where we actually
    // expect attempts to retrieve a non-existent node.
    ORT_ENFORCE(node_index < nodes_.size(), "Validating no unexpected access using an invalid node_index. Got:",
                node_index, " Max:", nodes_.size());
    return nodes_[node_index].get();
  }

  Status LoadFromModelBuilderApiModel(const OrtGraph& api_graph, bool updating_existing_graph = false);

  const Model& owning_model_;

  // GraphProto to store name, version, initializer.
  // When serializing <*this> Graph to a GraphProto, the nodes and
  // functions in <Graph> will also be fed into <graph_proto_> so that
  // it's consistent with <*this> graph.
  // This pointer is owned by parent model.
  ONNX_NAMESPACE::GraphProto* graph_proto_;

  // GraphProto that provides storage for the ONNX proto types deserialized from a flexbuffer/flatbuffer
  ONNX_NAMESPACE::GraphProto deserialized_proto_data_;

  InitializedTensorSet name_to_initial_tensor_;

  // Initializers that are external to the Graph. e.g. created using Model Builder API from existing memory.
  // As we need to convert to TensorProto for the optimizers to work and keep the deleter information we store them
  // in the Graph instance and retrieve during session state finalization.
  std::unordered_map<std::string, OrtValue> ortvalue_initializers_;

  std::unordered_set<std::reference_wrapper<const std::string>,
                     std::hash<std::string>, std::equal_to<std::string>>
      sparse_tensor_names_;

  // Prepacked blobs container that stored pre-packed initializers
  // data that is:
  // - mem-mapped from disk
  // - shared within the session
  // - shared across sessions by transferring the ownership of loaded data entries to
  // SessionState::PrepackedWeightsContainer* if one is present.
  // This container is optional because it is present only in the root graph.
  std::optional<PrepackedKeyToBlobMap> prepacked_key_to_blobs_;

  // This container contains a reference to the root prepacked_key_to_blobs_
  // and also (in the save mode) records association between the initializer
  // names and their pre-packed blobs (via keys).
  // This is optional due to delayed construction.
  std::optional<PrepackedWeightsForGraph> prepacked_weights_for_graph_;

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  // Runtime optimization storage.
  // Note: runtime_optimizations_ == *runtime_optimizations_ptr_ and must be initialized
  std::unique_ptr<RuntimeOptimizationRecordContainer> runtime_optimizations_ptr_;
  RuntimeOptimizationRecordContainer& runtime_optimizations_;
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

#if !defined(ORT_MINIMAL_BUILD)
  IOnnxRuntimeOpSchemaCollectionPtr schema_registry_;

  // Currently to make the ORT in-memory graph work, we have to create a temporary op schema
  // for the fused kernel. I really don't like it. but for short-term solution, let's host
  // those schemas here.
  InlinedVector<std::unique_ptr<ONNX_NAMESPACE::OpSchema>> fused_schemas_containers_;
  // in some case, a fused sub-graph will happens multiple times in one model, we use a map
  // to store reusable-schema in lookup.
  InlinedHashMap<std::string, std::reference_wrapper<ONNX_NAMESPACE::OpSchema>> reusable_fused_schema_map_;

#endif  // !defined(ORT_MINIMAL_BUILD)

  // Graph nodes.
  // Element in <nodes_> may be nullptr due to graph optimization.
  std::vector<std::unique_ptr<Node>> nodes_;

  // Wrapper of Graph nodes to provide iteration services that hide nullptr entries
  GraphNodes iterable_nodes_{nodes_};

  // Number of nodes.
  // Normally this is smaller than the size of <m_nodes>, as some
  // elements in <m_nodes> may be removed when doing graph optimization,
  // or some elements may be merged, etc.
  int num_of_nodes_ = 0;

  // A flag indicates whether <*this> graph needs to be resolved.
  bool graph_resolve_needed_ = false;

  bool graph_proto_sync_needed_ = false;

  // The topological order of node index used to do node and op match verification temporarily.
  std::vector<NodeIndex> nodes_in_topological_order_;

  // Full list of graph inputs. Matches number and order of inputs in the GraphProto.
  std::vector<const NodeArg*> graph_inputs_including_initializers_;
  bool graph_inputs_manually_set_ = false;

  // Graph inputs excluding initializers.
  std::vector<const NodeArg*> graph_inputs_excluding_initializers_;

  // Overridable Initializers. The difference between graph_inputs_including_initializers_
  // and graph_inputs_excluding_initializers_
  std::vector<const NodeArg*> graph_overridable_initializers_;

  // Graph outputs.
  std::vector<const NodeArg*> graph_outputs_;
  bool graph_outputs_manually_set_ = false;

  // Graph value_info.
  std::unordered_set<const NodeArg*> value_info_;

  // All node args owned by <*this> graph. Key is node arg name.
  std::unordered_map<std::string, std::unique_ptr<NodeArg>> node_args_;

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  int name_generator_ = 0;

  // Strings which have been used as node names.
  // New node name should not conflict with this set.
  std::unordered_set<std::string> generated_node_names_;

  // Strings which have been used as node_arg names.
  // New node_arg name should not conflict this this set.
  std::unordered_set<std::string> generated_node_arg_names_;

  // node arg to its producer node
  std::unordered_map<std::string, NodeIndex> node_arg_to_producer_node_;

  // node arg to its consumer nodes
  std::unordered_map<std::string, std::unordered_set<NodeIndex>> node_arg_to_consumer_nodes_;
#endif  // !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)

  std::unordered_map<std::string, int> domain_to_version_;

  // Model IR version.
  Version ir_version_{ONNX_NAMESPACE::Version::IR_VERSION};

  ResolveContext resolve_context_{*this};

  // the parent graph if this is a subgraph.
  Graph* parent_graph_;
  // the node containing the graph if parent_graph_ is not nullptr
  const Node* parent_node_;

  // NodeArgs that come from outer scope. Used when building a graph so that
  // these don't get recorded as graph inputs in the GraphProto.
  std::unordered_set<std::string> outer_scope_node_arg_names_;

  // number of times Resolve has run.
  int num_resolves_ = 0;

  const logging::Logger& logger_;

  // If true, all inconsistencies encountered during shape and type inference
  // will be exposed to the caller as failures. If false, in some cases
  // warnings will be logged but processing will continue and no error will
  // be returned.
  const bool strict_shape_type_inference_;

  // distinguishes between graph loaded from model file and graph created from scratch
  const bool is_loaded_from_model_file_;
};

#if !defined(ORT_MINIMAL_BUILD)
// Print NodeArg as
//  name : type
// For example,
//  "110": tensor(float)
std::ostream& operator<<(std::ostream& out, const NodeArg& node_arg);
// Print Node as,
//  (operator's name, operator's type, domain, version) : (input0, input1, ...) -> (output0, output1, ...)
// For example,
//  ("Add_14", Add, "", 7) : ("110": tensor(float),"109": tensor(float),) -> ("111": tensor(float),)
std::ostream& operator<<(std::ostream& out, const Node& node);
// Print Graph as, for example,
// Inputs:
//    "Input": tensor(float)
// Nodes:
//    ("add0", Add, "", 7) : ("Input": tensor(float),"Bias": tensor(float),) -> ("add0_out": tensor(float),)
//    ("matmul", MatMul, "", 9) : ("add0_out": tensor(float),"matmul_weight": tensor(float),) -> ("matmul_out": tensor(float),)
//    ("add1", Add, "", 7) : ("matmul_out": tensor(float),"add_weight": tensor(float),) -> ("add1_out": tensor(float),)
//    ("reshape", Reshape, "", 5) : ("add1_out": tensor(float),"concat_out": tensor(int64),) -> ("Result": tensor(float),)
// Outputs:
//    "Result": tensor(float)
// Inputs' and outputs' format is described in document of NodeArg's operator<< above.
// Node format is described in Node's operator<< above.
std::ostream& operator<<(std::ostream& out, const Graph& graph);
#endif

}  // namespace onnxruntime
