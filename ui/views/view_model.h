// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_MODEL_H_
#define UI_VIEWS_VIEW_MODEL_H_

#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class ViewModelUtils;

// Internal implementation of the templated ViewModelT class. Provides
// non-templated "unsafe" methods for ViewModelT to wrap around. Any methods
// that allow insertion of or access to a View* should be protected, and have a
// public method in the ViewModelT subclass that provides type-safe access to
// the correct View subclass.
class VIEWS_EXPORT ViewModelBase {
 public:
  struct Entry {
    Entry() = default;

    raw_ptr<View, DanglingUntriaged> view = nullptr;
    gfx::Rect ideal_bounds;
  };
  using Entries = std::vector<Entry>;

  ViewModelBase(const ViewModelBase&) = delete;
  ViewModelBase& operator=(const ViewModelBase&) = delete;

  ~ViewModelBase();

  const Entries& entries() const { return entries_; }

  // Removes the view at the specified index. This does not actually remove the
  // view from the view hierarchy.
  void Remove(size_t index);

  // Moves the view at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the view at |index| is removed.
  void Move(size_t index, size_t target_index);

  // Variant of Move() that leaves the bounds as is. That is, after invoking
  // this the bounds of the view at |target_index| (and all other indices) are
  // exactly the same as the bounds of the view at |target_index| before
  // invoking this.
  void MoveViewOnly(size_t index, size_t target_index);

  // Returns the number of views.
  size_t view_size() const { return entries_.size(); }

  // Removes and deletes all the views.
  void Clear();

  void set_ideal_bounds(size_t index, const gfx::Rect& bounds) {
    check_index(index);
    entries_[index].ideal_bounds = bounds;
  }

  const gfx::Rect& ideal_bounds(size_t index) const {
    check_index(index);
    return entries_[index].ideal_bounds;
  }

  // Returns the index of the specified view, or nullopt if the view isn't in
  // the model.
  std::optional<size_t> GetIndexOfView(const View* view) const;

 protected:
  ViewModelBase();

  // Returns the view at the specified index. Note: Most users should use
  // view_at() in the subclass, to get a view of the correct type. (Do not call
  // ViewAtBase then static_cast to the desired type.)
  View* ViewAtBase(size_t index) const {
    check_index(index);
    return entries_[index].view;
  }

  // Adds |view| to this model. This does not add |view| to a view hierarchy,
  // only to this model.
  void AddUnsafe(View* view, size_t index);

 private:
  // For access to ViewAtBase().
  friend class ViewModelUtils;

  void check_index(size_t index) const { DCHECK_LT(index, entries_.size()); }

  Entries entries_;
};

// ViewModelT is used to track an 'interesting' set of a views. Often times
// during animations views are removed after a delay, which makes for tricky
// coordinate conversion as you have to account for the possibility of the
// indices from the model not lining up with those you expect. This class lets
// you define the 'interesting' views and operate on those views.
template <class T>
class ViewModelT : public ViewModelBase {
 public:
  ViewModelT() = default;

  ViewModelT(const ViewModelT&) = delete;
  ViewModelT& operator=(const ViewModelT&) = delete;

  // Adds |view| to this model. This does not add |view| to a view hierarchy,
  // only to this model.
  void Add(T* view, size_t index) { AddUnsafe(view, index); }

  // Returns the view at the specified index.
  T* view_at(size_t index) const { return static_cast<T*>(ViewAtBase(index)); }
};

// ViewModel is a collection of views with no specfic type. If all views have
// the same type, the use of ViewModelT is preferred so that the views can be
// retrieved without potentially unsafe downcasts.
using ViewModel = ViewModelT<View>;

}  // namespace views

#endif  // UI_VIEWS_VIEW_MODEL_H_
