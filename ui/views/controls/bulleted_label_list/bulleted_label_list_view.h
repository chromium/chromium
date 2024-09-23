// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BULLETED_LABEL_LIST_BULLETED_LABEL_LIST_VIEW_H_
#define UI_VIEWS_CONTROLS_BULLETED_LABEL_LIST_BULLETED_LABEL_LIST_VIEW_H_

#include <vector>

#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace views {

// This class provides a convenient way to build a view for a bulleted list
// comprised of labels. The labels and the text style they should use are
// expected to be provided on construction.
class VIEWS_EXPORT BulletedLabelListView : public View {
  METADATA_HEADER(BulletedLabelListView, View)

 public:
  BulletedLabelListView(const std::vector<std::u16string>& texts,
                        style::TextStyle label_text_style);
  BulletedLabelListView(const BulletedLabelListView&) = delete;
  BulletedLabelListView& operator=(const BulletedLabelListView&) = delete;
  ~BulletedLabelListView() override;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BULLETED_LABEL_LIST_BULLETED_LABEL_LIST_VIEW_H_
