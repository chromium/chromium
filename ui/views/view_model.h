// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_MODEL_H_
#define UI_VIEWS_VIEW_MODEL_H_

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
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
  ~ViewModelBase();

  // Removes the view at the specified index. This does not actually remove the
  // view from the view hierarchy.
  void Remove(int index);

  // Moves the view at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the view at |index| is removed.
  void Move(int index, int target_index);

  // Variant of Move() that leaves the bounds as is. That is, after invoking
  // this the bounds of the view at |target_index| (and all other indices) are
  // exactly the same as the bounds of the view at |target_index| before
  // invoking this.
  void MoveViewOnly(int index, int target_index);

  // Returns the number of views.
  int view_size() const { return static_cast<int>(entries_.size()); }

  // Removes and deletes all the views.
  void Clear();

  void set_ideal_bounds(int index, const gfx::Rect& bounds) {
    check_index(index);
    entries_[index].ideal_bounds = bounds;
  }

  const gfx::Rect& ideal_bounds(int index) const {
    check_index(index);
    return entries_[index].ideal_bounds;
  }

  // Returns the index of the specified view, or -1 if the view isn't in the
  // model.
  int GetIndexOfView(const View* view) const;

 protected:
  ViewModelBase();

  // Returns the view at the specified index. Note: Most users should use
  // view_at() in the subclass, to get a view of the correct type. (Do not call
  // ViewAtBase then static_cast to the desired type.)
  View* ViewAtBase(int index) const {
    check_index(index);
    return entries_[index].view;
  }

  // Adds |view| to this model. This does not add |view| to a view hierarchy,
  // only to this model.
  void AddUnsafe(View* view, int index);

 private:
  // For access to ViewAtBase().
  friend class ViewModelUtils;

  struct Entry {
    Entry() = default;

    View* view = nullptr;
    gfx::Rect ideal_bounds;
  };
  using Entries = std::vector<Entry>;

#if !defined(NDEBUG)
  void check_index(int index) const {
    DCHECK_LT(index, static_cast<int>(entries_.size()));
    DCHECK_GE(index, 0);
  }
#else
  void check_index(int index) const {}
#endif

  Entries entries_;

  DISALLOW_COPY_AND_ASSIGN(ViewModelBase);
};

// ViewModelT is used to track an 'interesting' set of a views. Often times
// during animations views are removed after a delay, which makes for tricky
// coordinate conversion as you have to account for the possibility of the
// indices from the model not lining up with those you expect. This class lets
// you define the 'interesting' views and operate on those views.
template <class T>
class ViewModelT : public ViewModelBase {
 public:
  ViewModelT<T>() = default;

  // Adds |view| to this model. This does not add |view| to a view hierarchy,
  // only to this model.
  void Add(T* view, int index) { AddUnsafe(view, index); }

  // Returns the view at the specified index.
  T* view_at(int index) const { return static_cast<T*>(ViewAtBase(index)); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewModelT<T>);
};

// ViewModel is a collection of views with no specfic type. If all views have
// the same type, the use of ViewModelT is preferred so that the views can be
// retrieved without potentially unsafe downcasts.
using ViewModel = ViewModelT<View>;

}  // namespace views

#endif  // UI_VIEWS_VIEW_MODEL_H_
