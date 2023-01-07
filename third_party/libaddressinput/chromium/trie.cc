// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/trie.h"

#include <stddef.h>

#include <queue>
#include <string>

#include "base/check.h"

// Separating template definitions and declarations requires defining all
// possible template parameters to avoid linking errors.
namespace i18n {
namespace addressinput {
class RegionData;
}
}

namespace autofill {

template <typename T>
Trie<T>::Trie() {}

template <typename T>
Trie<T>::~Trie() {}

template <typename T>
void Trie<T>::AddDataForKey(const std::vector<uint8_t>& key,
                            const T& data_item) {
  Trie<T>* current_node = this;
  for (std::vector<uint8_t>::size_type i = 0; i < key.size(); ++i) {
    if (!key[i])
      break;
    current_node = &current_node->sub_nodes_[key[i]];
  }
  current_node->data_list_.insert(data_item);
}

template <typename T>
void Trie<T>::FindDataForKeyPrefix(const std::vector<uint8_t>& key_prefix,
                                   std::set<T>* results) const {
  DCHECK(results);

  // Find the sub-trie for the key prefix.
  const Trie<T>* current_node = this;
  for (std::vector<uint8_t>::size_type i = 0; i < key_prefix.size(); ++i) {
    if (!key_prefix[i])
      break;

    typename std::map<uint8_t, Trie<T> >::const_iterator sub_node_it =
        current_node->sub_nodes_.find(key_prefix[i]);
    if (sub_node_it == current_node->sub_nodes_.end())
      return;

    current_node = &sub_node_it->second;
  }

  // Collect data from all sub-tries.
  std::queue<const Trie<T>*> node_queue;
  node_queue.push(current_node);
  while (!node_queue.empty()) {
    const Trie<T>* queue_front = node_queue.front();
    node_queue.pop();

    results->insert(queue_front->data_list_.begin(),
                    queue_front->data_list_.end());

    for (typename std::map<uint8_t, Trie<T> >::const_iterator sub_node_it =
             queue_front->sub_nodes_.begin();
         sub_node_it != queue_front->sub_nodes_.end();
         ++sub_node_it) {
      node_queue.push(&sub_node_it->second);
    }
  }
}

template class Trie<const ::i18n::addressinput::RegionData*>;
template class Trie<std::string>;

}  // namespace autofill
