// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <type_traits>
#include <vector>

namespace onnxruntime {

class Node;

/**
Class to filter out null entries from either a vector of unique_ptr<Node> or a vector of [const] Node* and
provide an iterator interface that returns [const] Node& for the valid entries.
*/
template <typename TNodesContainer>
class ValidNodes {
 public:
  template <typename TIterator>
  class NodeIterator;

  // optional filtering function to return a subset of nodes
  using NodeFilterFunc = std::function<bool(NodeIndex)>;

  /**
  Construct a ValidNodes instance to provide iteration over all valid nodes in the TNodesCollection
  @param[in] nodes Nodes to iterate, skipping invalid entries.
  */
  explicit ValidNodes(TNodesContainer& nodes) noexcept : nodes_(&nodes) {}

  explicit ValidNodes(TNodesContainer& nodes, NodeFilterFunc&& filter_node_fn) noexcept
      : nodes_(&nodes), filter_node_fn_{std::move(filter_node_fn)} {}

  using ConstNodeIterator = NodeIterator<typename TNodesContainer::const_iterator>;
  using MutableNodeIterator = NodeIterator<typename TNodesContainer::iterator>;
  using ConstReverseNodeIterator = NodeIterator<typename TNodesContainer::const_reverse_iterator>;

  ConstNodeIterator cbegin() const noexcept {
    return {nodes_->cbegin(), nodes_->cend(), filter_node_fn_};
  }

  ConstNodeIterator cend() const noexcept {
    return {nodes_->cend(), nodes_->cend(), filter_node_fn_};
  }

  ConstNodeIterator begin() const noexcept {
    return cbegin();
  }

  ConstNodeIterator end() const noexcept {
    return cend();
  }

  ConstReverseNodeIterator rbegin() const noexcept {
    return {nodes_->crbegin(), nodes_->crend(), filter_node_fn_};
  }

  ConstReverseNodeIterator rend() const noexcept {
    return {nodes_->crend(), nodes_->crend(), filter_node_fn_};
  }

  // we only allow mutable access if the container is non-const.
  // we need to templatize the functions for enable_if to work at this level, but mandate T2 being TNodesContainer
  template <typename T2 = TNodesContainer>
  typename std::enable_if<!std::is_const<T2>::value, MutableNodeIterator>::type begin() noexcept {
    static_assert(std::is_same<T2, TNodesContainer>::value, "Explicit specialization is not allowed");
    return MutableNodeIterator(nodes_->begin(), nodes_->end(), filter_node_fn_);
  }

  template <typename T2 = TNodesContainer>
  typename std::enable_if<!std::is_const<T2>::value, MutableNodeIterator>::type end() noexcept {
    static_assert(std::is_same<T2, TNodesContainer>::value, "Explicit specialization is not allowed");
    return MutableNodeIterator(nodes_->end(), nodes_->end(), filter_node_fn_);
  }

  bool empty() const noexcept { return nodes_->empty(); }

  /**
  @class NodeIterator
  Iterator to provide const and non-const access to valid Node instances in a Graph.
  @remarks Skips invalid nodes.
  */
  template <typename TIterator>
  class NodeIterator {
    // get the type being returned by the iterator. can't use TIterator::value_type as that is always non-const
    using IterType = typename std::remove_reference<typename std::iterator_traits<TIterator>::reference>::type;
    // and determine what we will return based on its constness
    using T = typename std::conditional<std::is_const<IterType>::value,
                                        const Node,   // return const Node if this is a const iterator
                                        Node>::type;  // else return Node

   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = typename std::iterator_traits<TIterator>::difference_type;
    using pointer = T*;
    using reference = T&;
    using const_reference = const T&;

    /** Construct a NodeInterator and move to the first valid node. */
    NodeIterator(const TIterator current, const TIterator end, const NodeFilterFunc& filter_fn) noexcept
        : current_{current}, end_{end}, apply_filter_{filter_fn != nullptr}, filter_func_{&filter_fn} {
      // skip to next valid node, stopping at end if none are found
      while (current_ < end && (*current_ == nullptr ||
                                (apply_filter_ && (*filter_func_)((*current_)->Index()) == true))) {
        ++current_;
      }
    }

    bool operator==(const NodeIterator<TIterator>& other) const noexcept {
      return (current_ == other.current_);
    }

    bool operator!=(const NodeIterator<TIterator>& other) const noexcept {
      return (current_ != other.current_);
    }

    NodeIterator<TIterator>& operator++() {
      if (current_ < end_) {
        while (++current_ != end_) {
          if (*current_ != nullptr && (!apply_filter_ || (*filter_func_)((*current_)->Index()) == false))
            break;
        }
      }
      return *this;
    }

    NodeIterator<TIterator> operator++(int) {
      NodeIterator<TIterator> tmp{*this};
      ++(*this);

      return tmp;
    }

    /** Return the current Node&. This will be const if the iterator was returned from a const GraphNodes instance. */
    reference operator*() const {
      // if iterator is valid we always have a non-nullptr node
      // if this is a nullptr we're at end_ and this shouldn't be being called
      return **current_;
    }

    pointer operator->() const {
      return current_->get();
    }

   private:
    TIterator current_;
    TIterator end_;
    bool apply_filter_;                  // store whether filter_func_ is not nullptr and contains a callable
    const NodeFilterFunc* filter_func_;  // store as pointer so iterator is copyable
  };

 private:
  gsl::not_null<TNodesContainer*> nodes_;  // always set by ctor

  // no filtering if not set. this instance owns the filter func if set.
  NodeFilterFunc filter_node_fn_;
};

/**
Class that provides iteration over all valid nodes in the Graph.
*/
class GraphNodes : public ValidNodes<std::vector<std::unique_ptr<Node>>> {
 public:
  GraphNodes(std::vector<std::unique_ptr<Node>>& nodes) : ValidNodes(nodes) {
  }
};

// Variant that only ever allows const access to nodes and optionally allows filtering of the nodes.
class ConstGraphNodes : public ValidNodes<const std::vector<std::unique_ptr<Node>>> {
 public:
  ConstGraphNodes(const std::vector<std::unique_ptr<Node>>& nodes) : ValidNodes(nodes) {
  }

  ConstGraphNodes(const std::vector<std::unique_ptr<Node>>& nodes,
                  GraphNodes::NodeFilterFunc&& filter_func)
      : ValidNodes(nodes, std::move(filter_func)) {
  }
};

}  // namespace onnxruntime
