// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_
#define UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_

#include <memory>
#include <string>

#include "ui/views/examples/views_examples_export.h"

namespace base {
class DictValue;
}

namespace views {
class View;
}

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT JsonViewBuilder {
 public:
  // Tree Construction.
  static std::unique_ptr<views::View> BuildView(const base::DictValue& dict,
                                                std::string* error_msg);

  // Property Application.
  static bool ApplyPropertiesRecursive(views::View* view,
                                       const base::DictValue& dict,
                                       std::string* error_msg);
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_
