// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate_utils_win.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

bool IsValuePatternSupported(AXPlatformNodeDelegate* delegate) {
  // https://docs.microsoft.com/en-us/windows/desktop/winauto/uiauto-implementingvalue
  // The Value control pattern is used to support controls that have an
  // intrinsic value not spanning a range and that can be represented as
  // a string.
  //
  // IValueProvider must be implemented by controls such as the color picker
  // selection control [...]
  //
  // We also support the Value pattern on platform documents mostly for
  // historical reasons - it was supported as a way to get the URL of a document
  // via IA2's get_accValue, and the UIA bridge also populates an IValueProvider
  // with the same information.

  // https://www.w3.org/TR/html-aam-1.0/
  // The HTML AAM maps "href [a; area]" to UIA Value.Value
  if (delegate->IsCellOrHeaderOfAriaGrid() &&
      !delegate->HasState(ax::mojom::State::kEditable)) {
    return false;
  }
  return delegate->GetData().IsRangeValueSupported() ||
         delegate->IsReadOnlySupported() || IsLink(delegate->GetRole()) ||
         delegate->GetRole() == ax::mojom::Role::kColorWell ||
         delegate->IsCellOrHeaderOfAriaGrid() ||
         (delegate->GetRole() == ax::mojom::Role::kButton &&
          delegate->HasStringAttribute(ax::mojom::StringAttribute::kValue)) ||
         (IsPlatformDocument(delegate->GetRole()) &&
          delegate->GetRole() != ax::mojom::Role::kPdfRoot);
}

}  // namespace ui
