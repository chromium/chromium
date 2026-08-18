// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_SPECIFICATION_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_SPECIFICATION_H_

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/views_export.h"

namespace views {

// Defines which sub-regions of a view require input event activation
// protection (e.g. occlusion protection).
//
// This specification is installed on a view (e.g. a Dialog) using `Install()`
// to specify the protected bounds. The bounds are provided by a callback and
// are resolved dynamically in screen coordinates, clipped to the boundaries of
// the view that holds the specification.
//
// Installing this specification does not automatically enable input protection.
// For details on how these bounds are used and how to enable input protection,
// see ui/views/input_protection/README.md.
class VIEWS_EXPORT InputProtectionSpecification final {
 public:
  // Callback type for views to provide their local bounds that need input
  // protection. The callback receives the view on which the specification is
  // installed, and the returned bounds must be in the local coordinate space
  // of that view.
  template <std::derived_from<View> V>
  using GetBoundsCallback =
      base::RepeatingCallback<std::vector<gfx::Rect>(const V*)>;

  explicit InputProtectionSpecification(GetBoundsCallback<View> callback);
  ~InputProtectionSpecification();
  InputProtectionSpecification(const InputProtectionSpecification&) = delete;
  InputProtectionSpecification& operator=(const InputProtectionSpecification&) =
      delete;

  // Sets the `InputProtectionSpecification` on `view` with the given
  // `callback`.
  template <typename T>
  static void Install(T& view, GetBoundsCallback<T> callback);

  // Returns the bounds that need input protection, in screen coordinates, by
  // resolving the local bounds from the callback, clipping them to the `view`
  // boundaries, and converting the remaining valid bounds to screen
  // coordinates.
  std::vector<gfx::Rect> GetProtectedBoundsInScreen(const View& view) const;

 private:
  // Callback that returns the protected regions in the local coordinate space
  // of the view on which the specification is installed.
  GetBoundsCallback<View> callback_;
};

// A property key to store the `InputProtectionSpecification`.
VIEWS_EXPORT extern const ui::ClassProperty<
    InputProtectionSpecification*>* const kInputProtectionKey;

template <typename T>
void InputProtectionSpecification::Install(T& view,
                                           GetBoundsCallback<T> callback) {
  auto cb = base::BindRepeating(
      [](GetBoundsCallback<T> cb, const View* view) {
        const T* t = AsViewClass<T>(view);
        CHECK(t);
        return cb.Run(t);
      },
      std::move(callback));

  view.SetProperty(
      kInputProtectionKey,
      std::make_unique<InputProtectionSpecification>(std::move(cb)));
}

}  // namespace views

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT,
                                        views::InputProtectionSpecification*)

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_SPECIFICATION_H_
