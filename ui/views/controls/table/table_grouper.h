// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_GROUPER_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_GROUPER_H_

#include "ui/views/views_export.h"

namespace views {

struct VIEWS_EXPORT GroupRange {
  size_t start;
  size_t length;
};

// TableGrouper is used by TableView to group a set of rows and treat them
// as one. Rows that fall in the same group are selected together and sorted
// together.
class VIEWS_EXPORT TableGrouper {
 public:
  virtual void GetGroupRange(size_t model_index, GroupRange* range) = 0;

 protected:
  virtual ~TableGrouper() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_GROUPER_H_
