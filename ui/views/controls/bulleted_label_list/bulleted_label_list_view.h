// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BULLETED_LABEL_LIST_BULLETED_LABEL_LIST_VIEW_H_
#define UI_VIEWS_CONTROLS_BULLETED_LABEL_LIST_BULLETED_LABEL_LIST_VIEW_H_

#include <vector>

#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"


namespace views {

class VIEWS_EXPORT BulletedLabelListView : public View {
  METADATA_HEADER(BulletedLabelListView, View)

 public:
  BulletedLabelListView();
  explicit BulletedLabelListView(const std::vector<std::u16string>& texts);
  BulletedLabelListView(const BulletedLabelListView&) = delete;
  BulletedLabelListView& operator=(const BulletedLabelListView&) = delete;
  ~BulletedLabelListView() override;

  void AddLabel(const std::u16string& text);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BULLETED_LABEL_LIST_BULLETED_LABEL_LIST_VIEW_H_
