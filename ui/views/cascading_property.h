// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CASCADING_PROPERTY_H_
#define UI_VIEWS_CASCADING_PROPERTY_H_

#include <memory>
#include <optional>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/class_property.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class View;

template <typename T>
class CascadingProperty {
 public:
  CascadingProperty() = default;
  CascadingProperty(const CascadingProperty&) = delete;
  CascadingProperty& operator=(const CascadingProperty&) = delete;
  virtual ~CascadingProperty() = default;

  virtual T GetValue(const View* view) const = 0;
};

template <typename T>
const CascadingProperty<T>* GetCascadingPropertyObject(
    View* view,
    const ui::ClassProperty<CascadingProperty<T>*>* property_key) {
  const CascadingProperty<T>* property = view->GetProperty(property_key);
  if (property != nullptr)
    return property;
  if (!view->parent())
    return nullptr;
  return GetCascadingPropertyObject(view->parent(), property_key);
}

template <typename T>
std::optional<T> GetCascadingProperty(
    View* view,
    const ui::ClassProperty<CascadingProperty<T>*>* property_key) {
  const CascadingProperty<T>* property =
      GetCascadingPropertyObject(view, property_key);
  return property ? std::optional<T>(property->GetValue(view)) : std::nullopt;
}

template <typename T, typename K>
void SetCascadingProperty(
    View* view,
    const ui::ClassProperty<CascadingProperty<T>*>* property_key,
    std::unique_ptr<K> property) {
  // TODO(pbos): See if there could be a way to (D)CHECK that property_key is
  // actually owned.
  view->SetProperty(property_key,
                    static_cast<CascadingProperty<T>*>(property.release()));
}

VIEWS_EXPORT void SetCascadingColorProviderColor(
    View* view,
    const ui::ClassProperty<CascadingProperty<SkColor>*>* property_key,
    ui::ColorId color_id);

VIEWS_EXPORT extern const ui::ClassProperty<CascadingProperty<SkColor>*>* const
    kCascadingBackgroundColor;

VIEWS_EXPORT SkColor GetCascadingBackgroundColor(View* view);

VIEWS_EXPORT SkColor GetCascadingAccentColor(View* view);

}  // namespace views

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                        views::CascadingProperty<SkColor>*)

#endif  // UI_VIEWS_CASCADING_PROPERTY_H_
