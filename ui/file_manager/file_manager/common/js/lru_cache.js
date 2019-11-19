// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A linked-list node which holds data for cache entry such as key, value, size.
 * @template T
 */
class LRUCacheNode {
  /**
   * @param {string} key
   * @param {T} value
   * @param {number} size
   */
  constructor(key, value, size) {
    /** @type {string} */
    this.key = key;

    /** @type {T} */
    this.value = value;

    /** @type {number} */
    this.size = size;

    /** @type {LRUCacheNode} */
    this.next = null;

    /** @type {LRUCacheNode} */
    this.prev = null;
  }
}

/**
 * Container of the list of cache nodes.
 */
class LRUCacheList {
  constructor() {
    /** @private {!LRUCacheNode} */
    this.sentinelTail_ = new LRUCacheNode('sentinel', null, 0);

    /** @private {LRUCacheNode} */
    this.head_ = this.sentinelTail_;
  }

  /**
   * Removes a node from this list.
   * @param {!LRUCacheNode} node
   */
  remove(node) {
    if (node.prev) {
      node.prev.next = node.next;
    }
    if (node.next) {
      node.next.prev = node.prev;
    }
    if (node === this.head_) {
      this.head_ = node.next;
    }
    node.prev = null;
    node.next = null;
  }

  /**
   * Adds a node at the head of this list.
   * @param {!LRUCacheNode} node
   */
  prepend(node) {
    node.prev = null;
    node.next = this.head_;
    node.next.prev = node;
    this.head_ = node;
  }

  /**
   * Returns the last node of the list, or null if the list has no nodes.
   * @return {LRUCacheNode}
   */
  lastNode() {
    return this.sentinelTail_.prev;
  }
}

/**
 * Cache management class implementing LRU algorithm.
 * @template T
 */
class LRUCache {
  /**
   * @param {number} maxSize Maximum total size of items this cache can hold.
   *     When items are put without specifying their sizes, their sizes are
   *     treated as 1 and the |maxSize| can be interpreted as the maximum number
   *     of items. If items are put with specifying their sizes in bytes, the
   *     |maxSize| can be interpreted as the maximum number of bytes.
   */
  constructor(maxSize) {
    /** @private {number} */
    this.totalSize_ = 0;

    /** @private {number} */
    this.maxSize_ = maxSize;

    /** @private {!LRUCacheList} */
    this.list_ = new LRUCacheList();

    /** @private {!Object<!LRUCacheNode>} */
    this.nodes_ = {};
  }

  /**
   * Returns a cached item corresponding to the given key. The referenced item
   * will be the most recently used item and won't be evicted soon.
   * @param {string} key
   * @return {T}
   */
  get(key) {
    const node = this.nodes_[key];
    if (!node) {
      return null;
    }

    this.moveNodeToHead_(node);
    return node.value;
  }

  /**
   * Returns a cached item corresponding to the given key without making the
   * referenced item the most recently used item.
   * @param {string} key
   * @return {T}
   */
  peek(key) {
    const node = this.nodes_[key];
    if (!node) {
      return null;
    }

    return node.value;
  }

  /**
   * Returns true if the cache contains the key.
   * @param {string} key
   * @return {boolean}
   */
  hasKey(key) {
    return key in this.nodes_;
  }

  /**
   * Saves an item in this cache. The saved item will be the most recently used
   * item and won't be evicted soon. If an item with the same key already exists
   * in the cache, the existing item's value and size will be updated and the
   * item will become the most recently used item.
   * @param {string} key Key to find the cached item later.
   * @param {T} value Value of the item to be cached.
   * @param {number=} opt_size Size of the put item. If not specified, the size
   *     is regarded as 1. If the size is larger than the |maxSize_|, put
   *     operation will be ignored keeping cache state unchanged.
   */
  put(key, value, opt_size) {
    const size = opt_size ? opt_size : 1;
    if (size > this.maxSize_) {
      return;
    }

    let node = this.nodes_[key];

    while (this.totalSize_ + size - (node ? node.size : 0) > this.maxSize_) {
      this.evictLastNode_();
      // The referenced node may be evicted, so it needs to be updated.
      node = this.nodes_[key];
    }

    if (node) {
      this.updateNode_(node, value, size);
      this.moveNodeToHead_(node);
    } else {
      node = new LRUCacheNode(key, value, size);
      this.prependNode_(node);
    }
  }

  /**
   * Removes an item from the cache.
   * @param {string} key
   */
  remove(key) {
    const node = this.nodes_[key];
    if (node) {
      this.removeNode_(node);
    }
  }

  /**
   * Returns the cache size.
   * @return {number}
   */
  size() {
    return this.totalSize_;
  }

  /**
   * Updates max size of the cache.
   * @param {number} value New max size.
   */
  setMaxSize(value) {
    this.maxSize_ = value;
    while (this.totalSize_ > this.maxSize_) {
      this.evictLastNode_();
    }
  }

  /**
   * Returns the max size of the cache.
   * @return {number}
   */
  getMaxSize() {
    return this.maxSize_;
  }

  /**
   * Evicts the oldest cache node.
   * @private
   */
  evictLastNode_() {
    const lastNode = this.list_.lastNode();
    if (!lastNode) {
      throw new Error('No more nodes to evict.');
    }

    this.removeNode_(lastNode);
  }

  /**
   * Removes given node from this cache store completely.
   * @param {!LRUCacheNode} node
   * @private
   */
  removeNode_(node) {
    this.list_.remove(node);
    this.totalSize_ -= node.size;
    console.assert(this.totalSize_ >= 0);
    console.assert(!!this.nodes_[node.key]);
    delete this.nodes_[node.key];
  }

  /**
   * Prepends given node to the head of list.
   * @param {!LRUCacheNode} node
   * @private
   */
  prependNode_(node) {
    this.list_.prepend(node);
    this.totalSize_ += node.size;
    console.assert(this.totalSize_ <= this.maxSize_);
    console.assert(!this.nodes_[node.key]);
    this.nodes_[node.key] = node;
  }

  /**
   * Updates the given nodes size and value.
   * @param {!LRUCacheNode} node
   * @param {T} value
   * @param {number} size
   * @private
   */
  updateNode_(node, value, size) {
    this.totalSize_ += size - node.size;
    console.assert(this.totalSize_ >= 0 && this.totalSize_ <= this.maxSize_);
    node.value = value;
    node.size = size;
  }

  /**
   * Moves the given node to the head of the linked list.
   * @param {!LRUCacheNode} node
   * @private
   */
  moveNodeToHead_(node) {
    this.list_.remove(node);
    this.list_.prepend(node);
  }
}
