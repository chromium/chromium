// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_WIN_H_

#include <stdint.h>
#include <wtypes.h>

#include <string>
#include <vector>

#include <oleacc.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/win/hwnd_util.h"

namespace ui {
struct AXTreeSelector;

AX_EXPORT std::wstring IAccessibleRoleToString(int32_t ia_role);
AX_EXPORT std::wstring IAccessible2RoleToString(int32_t ia_role);
AX_EXPORT std::wstring IAccessibleStateToString(int32_t ia_state);
AX_EXPORT void IAccessibleStateToStringVector(
    int32_t ia_state,
    std::vector<std::wstring>* result);
AX_EXPORT std::wstring IAccessible2StateToString(int32_t ia2_state);
AX_EXPORT void IAccessible2StateToStringVector(
    int32_t ia_state,
    std::vector<std::wstring>* result);

// Handles both IAccessible/MSAA events and IAccessible2 events.
AX_EXPORT std::wstring AccessibilityEventToString(int32_t event_id);

AX_EXPORT std::wstring UiaIdentifierToString(int32_t identifier);
AX_EXPORT std::wstring UiaOrientationToString(int32_t identifier);
AX_EXPORT std::wstring UiaLiveSettingToString(int32_t identifier);

AX_EXPORT std::string BstrToUTF8(BSTR bstr);
AX_EXPORT std::string UiaIdentifierToStringUTF8(int32_t id);

AX_EXPORT HWND GetHwndForProcess(base::ProcessId pid);

// Returns HWND of window matching a given tree selector.
AX_EXPORT HWND GetHWNDBySelector(const ui::AXTreeSelector& selector);

// Represent MSAA child, either as IAccessible object or as VARIANT.
class AX_EXPORT MSAAChild final {
 public:
  MSAAChild();
  MSAAChild(IAccessible* parent, VARIANT&& child);
  MSAAChild(MSAAChild&&);
  ~MSAAChild();

  MSAAChild& operator=(MSAAChild&& rhs) = default;

  IAccessible* AsIAccessible() const { return child_.Get(); }
  const base::win::ScopedVariant& AsVariant() const { return child_variant_; }

  IAccessible* Parent() const { return parent_.Get(); }

 private:
  Microsoft::WRL::ComPtr<IAccessible> parent_;
  Microsoft::WRL::ComPtr<IAccessible> child_;
  base::win::ScopedVariant child_variant_;
};

// Represents MSAA children of an IAccessible object.
class AX_EXPORT MSAAChildren final {
 public:
  MSAAChildren(IAccessible* parent);
  MSAAChildren(const Microsoft::WRL::ComPtr<IAccessible>& parent);
  ~MSAAChildren();

  const MSAAChild& ChildAt(LONG index) const { return children_[index]; }
  IAccessible* Parent() const { return parent_.Get(); }

  class AX_EXPORT Iterator final {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = MSAAChild;
    using difference_type = std::ptrdiff_t;
    using pointer = MSAAChild*;
    using reference = MSAAChild&;

    Iterator(MSAAChildren*);
    Iterator(MSAAChildren*, LONG);
    Iterator(const Iterator&);
    ~Iterator();

    Iterator& operator++() {
      ++index_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp(*this);
      operator++();
      return tmp;
    }
    bool operator==(const Iterator& rhs) const {
      return children_ == rhs.children_ && index_ == rhs.index_;
    }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    const MSAAChild& operator*() { return children_->ChildAt(index_); }

   private:
    LONG index_{0};
    raw_ptr<MSAAChildren> children_{nullptr};
  };

  Iterator begin() { return {this}; }
  Iterator end() { return {this, count_}; }

 private:
  LONG count_{-1};
  Microsoft::WRL::ComPtr<IAccessible> parent_{nullptr};
  std::vector<MSAAChild> children_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_WIN_H_
