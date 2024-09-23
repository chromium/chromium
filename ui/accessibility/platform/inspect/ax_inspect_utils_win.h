// Copyright 2012 The Chromium Authors
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

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"
#include "ui/gfx/win/hwnd_util.h"

namespace ui {
struct AXTreeSelector;

COMPONENT_EXPORT(AX_PLATFORM)
std::wstring IAccessibleRoleToString(int32_t ia_role);
COMPONENT_EXPORT(AX_PLATFORM)
std::wstring IAccessible2RoleToString(int32_t ia_role);
COMPONENT_EXPORT(AX_PLATFORM)
std::wstring IAccessibleStateToString(int32_t ia_state);
COMPONENT_EXPORT(AX_PLATFORM)
void IAccessibleStateToStringVector(int32_t ia_state,
                                    std::vector<std::wstring>* result);
COMPONENT_EXPORT(AX_PLATFORM)
std::wstring IAccessible2StateToString(int32_t ia2_state);
COMPONENT_EXPORT(AX_PLATFORM)
void IAccessible2StateToStringVector(int32_t ia_state,
                                     std::vector<std::wstring>* result);

// Handles both IAccessible/MSAA events and IAccessible2 events.
COMPONENT_EXPORT(AX_PLATFORM)
std::wstring AccessibilityEventToString(int32_t event_id);

COMPONENT_EXPORT(AX_PLATFORM)
std::wstring UiaIdentifierToString(int32_t identifier);
COMPONENT_EXPORT(AX_PLATFORM)
std::wstring UiaOrientationToString(int32_t identifier);
COMPONENT_EXPORT(AX_PLATFORM)
std::wstring UiaLiveSettingToString(int32_t identifier);

COMPONENT_EXPORT(AX_PLATFORM) std::string BstrToUTF8(BSTR bstr);
COMPONENT_EXPORT(AX_PLATFORM) std::string UiaIdentifierToStringUTF8(int32_t id);

COMPONENT_EXPORT(AX_PLATFORM) HWND GetHwndForProcess(base::ProcessId pid);

// Returns HWND of window matching a given tree selector.
COMPONENT_EXPORT(AX_PLATFORM)
HWND GetHWNDBySelector(const AXTreeSelector& selector);

COMPONENT_EXPORT(AX_PLATFORM)
std::u16string RoleVariantToU16String(const base::win::ScopedVariant& role);
COMPONENT_EXPORT(AX_PLATFORM)
std::string RoleVariantToString(const base::win::ScopedVariant& role);

COMPONENT_EXPORT(AX_PLATFORM)
std::optional<std::string> GetIAccessible2Attribute(
    Microsoft::WRL::ComPtr<IAccessible2> element,
    std::string attribute);
COMPONENT_EXPORT(AX_PLATFORM)
std::string GetDOMId(Microsoft::WRL::ComPtr<IAccessible> element);
COMPONENT_EXPORT(AX_PLATFORM)
std::vector<Microsoft::WRL::ComPtr<IAccessible>> IAccessibleChildrenOf(
    Microsoft::WRL::ComPtr<IAccessible> parent);

// Returns IA2 Interfaces
template <typename ServiceType>
HRESULT IA2QueryInterface(IUnknown* accessible, ServiceType** out_accessible) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(__uuidof(ServiceType), out_accessible);
}

// Represent MSAA child, either as IAccessible object or as VARIANT.
class COMPONENT_EXPORT(AX_PLATFORM) MSAAChild final {
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
class COMPONENT_EXPORT(AX_PLATFORM) MSAAChildren final {
 public:
  MSAAChildren(IAccessible* parent);
  MSAAChildren(const Microsoft::WRL::ComPtr<IAccessible>& parent);
  ~MSAAChildren();

  const MSAAChild& ChildAt(LONG index) const { return children_[index]; }
  IAccessible* Parent() const { return parent_.Get(); }

  class COMPONENT_EXPORT(AX_PLATFORM) Iterator final {
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
