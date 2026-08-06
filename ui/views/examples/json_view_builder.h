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

// JsonViewBuilder provides declarative runtime instantiation and property
// application for Views UI components from JSON specifications without
// requiring recompilation.
//
// The complete JSON Schema specification describing all supported component
// types, layout managers, properties, and dynamic token resolvers is documented
// in:
//   ui/views/examples/json_view_builder_schema.md
//
// Whenever new components, properties, or converters are added or modified in
// JsonViewBuilder, the json_view_builder_schema.md file MUST also be updated to
// maintain congruency.
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
