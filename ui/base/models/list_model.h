// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_LIST_MODEL_H_
#define UI_BASE_MODELS_LIST_MODEL_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/observer_list.h"
#include "ui/base/models/list_model_observer.h"

namespace ui {

// A list model that manages a list of ItemType pointers. Items added to the
// model are owned by the model. An item can be taken out of the model by
// RemoveAt.
template <class ItemType>
class ListModel {
 public:
  using ItemList = std::vector<std::unique_ptr<ItemType>>;

  ListModel() {}

  ListModel(const ListModel&) = delete;
  ListModel& operator=(const ListModel&) = delete;

  ~ListModel() {}

  // Adds |item| at the |index| into |items_|. Returns a raw pointer.
  ItemType* AddAt(size_t index, std::unique_ptr<ItemType> item) {
    DCHECK_LE(index, item_count());
    ItemType* item_ptr = item.get();
    items_.insert(items_.begin() + index, std::move(item));
    NotifyItemsAdded(index, 1);
    return item_ptr;
  }

  // Convenience function to append an item to the model.
  ItemType* Add(std::unique_ptr<ItemType> item) {
    return AddAt(item_count(), std::move(item));
  }

  // Removes the item at |index| from |items_| without deleting it.
  // Returns a scoped pointer containing the removed item.
  std::unique_ptr<ItemType> RemoveAt(size_t index) {
    DCHECK_LT(index, item_count());
    std::unique_ptr<ItemType> item = std::move(items_[index]);
    items_.erase(items_.begin() + index);
    NotifyItemsRemoved(index, 1);
    return item;
  }

  // Removes all items from the model without deleting them.
  // Returns a vector containing the removed items.
  ItemList RemoveAll() {
    ItemList result;
    result.swap(items_);
    NotifyItemsRemoved(0, result.size());
    return result;
  }

  // Removes the item at |index| from |items_| and deletes it.
  void DeleteAt(size_t index) {
    std::unique_ptr<ItemType> item = RemoveAt(index);
    // |item| will be deleted on destruction.
  }

  // Removes and deletes all items from the model.
  void DeleteAll() {
    ItemList to_be_deleted;
    to_be_deleted.swap(items_);
    NotifyItemsRemoved(0, to_be_deleted.size());
  }

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(size_t index, size_t target_index) {
    DCHECK_LT(index, item_count());
    DCHECK_LT(target_index, item_count());

    if (index == target_index)
      return;

    std::unique_ptr<ItemType> item = std::move(items_[index]);
    items_.erase(items_.begin() + index);
    items_.insert(items_.begin() + target_index, std::move(item));
    NotifyItemMoved(index, target_index);
  }

  void AddObserver(ListModelObserver* observer) const {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(ListModelObserver* observer) const {
    observers_.RemoveObserver(observer);
  }

  void NotifyItemsAdded(size_t start, size_t count) {
    observers_.Notify(&ListModelObserver::ListItemsAdded, start, count);
  }

  void NotifyItemsRemoved(size_t start, size_t count) {
    observers_.Notify(&ListModelObserver::ListItemsRemoved, start, count);
  }

  void NotifyItemMoved(size_t index, size_t target_index) {
    observers_.Notify(&ListModelObserver::ListItemMoved, index, target_index);
  }

  void NotifyItemsChanged(size_t start, size_t count) {
    observers_.Notify(&ListModelObserver::ListItemsChanged, start, count);
  }

  size_t item_count() const { return items_.size(); }

  const ItemType* GetItemAt(size_t index) const {
    DCHECK_LT(index, item_count());
    return items_[index].get();
  }
  ItemType* GetItemAt(size_t index) {
    return const_cast<ItemType*>(
        const_cast<const ListModel<ItemType>*>(this)->GetItemAt(index));
  }

  // Iteration interface.
  typename ItemList::iterator begin() { return items_.begin(); }
  typename ItemList::const_iterator begin() const { return items_.begin(); }
  typename ItemList::iterator end() { return items_.end(); }
  typename ItemList::const_iterator end() const { return items_.end(); }

 private:
  ItemList items_;

  // Mutable to allow adding/removing `ListModelObserver`'s through a const
  // ListModel in order to preserve underlying data const-ness.
  mutable base::ObserverList<ListModelObserver>::Unchecked observers_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_LIST_MODEL_H_
