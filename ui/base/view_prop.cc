// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/view_prop.h"

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"

namespace ui {

// Maints the actual view, key and data.
class ViewProp::Data : public base::RefCounted<ViewProp::Data> {
 public:
  // Returns the Data* for the view/key pair. If |create| is false and |Get|
  // has not been invoked for the view/key pair, NULL is returned.
  static void Get(gfx::AcceleratedWidget view,
                  const char* key,
                  bool create,
                  scoped_refptr<Data>* data) {
    if (!data_set_)
      data_set_ = new DataSet;
    scoped_refptr<Data> new_data(new Data(view, key));
    auto i = data_set_->find(new_data.get());
    if (i != data_set_->end()) {
      *data = *i;
      return;
    }
    if (!create)
      return;
    data_set_->insert(new_data.get());
    *data = new_data.get();
  }

  // The data.
  void set_data(void* data) { data_ = data; }
  void* data() const { return data_; }

  const char* key() const { return key_; }

 private:
  friend class base::RefCounted<Data>;

  // Used to order the Data in the map.
  class DataComparator {
   public:
    bool operator()(const Data* d1, const Data* d2) const {
      return (d1->view_ == d2->view_) ? (d1->key_ < d2->key_) :
                                        (d1->view_ < d2->view_);
    }
  };

  typedef std::set<Data*, DataComparator> DataSet;

  Data(gfx::AcceleratedWidget view, const char* key)
      : view_(view), key_(key), data_(nullptr) {}

  Data(const Data&) = delete;
  Data& operator=(const Data&) = delete;

  ~Data() {
    auto i = data_set_->find(this);
    // Also check for equality using == as |Get| creates dummy values in order
    // to look up a value.
    if (i != data_set_->end() && *i == this)
      data_set_->erase(i);
  }

  // The existing set of Data is stored here. ~Data removes from the set.
  static DataSet* data_set_;

  const gfx::AcceleratedWidget view_;
  const char* key_;
  raw_ptr<void> data_;
};

// static
ViewProp::Data::DataSet* ViewProp::Data::data_set_ = NULL;

ViewProp::ViewProp(gfx::AcceleratedWidget view, const char* key, void* data) {
  Data::Get(view, key, true, &data_);
  data_->set_data(data);
}

ViewProp::~ViewProp() {
  // This is done to provide similar semantics to SetProp. In particular it's
  // assumed that ~ViewProp should behave as though RemoveProp was invoked.
  data_->set_data(NULL);
}

// static
void* ViewProp::GetValue(gfx::AcceleratedWidget view, const char* key) {
  scoped_refptr<Data> data;
  Data::Get(view, key, false, &data);
  return data.get() ? data->data() : NULL;
}

// static
const char* ViewProp::Key() const {
  return data_->key();
}

}  // namespace ui
