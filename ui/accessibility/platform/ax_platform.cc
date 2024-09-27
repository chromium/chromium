// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform.h"

#include "base/check_op.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_mode_observer.h"

namespace ui {

namespace {

AXPlatform* g_instance = nullptr;

}  // namespace

// static
AXPlatform& AXPlatform::GetInstance() {
  CHECK_NE(g_instance, nullptr);
  return *g_instance;
}

AXPlatform::AXPlatform(Delegate& delegate) : delegate_(delegate) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AXPlatform::~AXPlatform() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void AXPlatform::AddModeObserver(AXModeObserver* observer) {
  observers_.AddObserver(observer);
}

void AXPlatform::RemoveModeObserver(AXModeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AXPlatform::NotifyModeAdded(AXMode mode) {
  observers_.Notify(&AXModeObserver::OnAXModeAdded, mode);
}

bool AXPlatform::IsCaretBrowsingEnabled() {
  return caret_browsing_enabled_;
}

void AXPlatform::SetCaretBrowsingState(bool enabled) {
  caret_browsing_enabled_ = enabled;
}

#if BUILDFLAG(IS_WIN)
const std::string& AXPlatform::GetProductName() const {
  RetrieveProductStringsIfNeeded();
  return product_strings_->product_name;
}

const std::string& AXPlatform::GetProductVersion() const {
  RetrieveProductStringsIfNeeded();
  return product_strings_->product_version;
}

const std::string& AXPlatform::GetToolkitVersion() const {
  RetrieveProductStringsIfNeeded();
  return product_strings_->toolkit_version;
}

void AXPlatform::SetUiaProviderEnabled(bool is_enabled) {
  CHECK_EQ(uia_provider_enablement_, UiaProviderEnablement::kVariations);
  uia_provider_enablement_ = is_enabled ? UiaProviderEnablement::kEnabled
                                        : UiaProviderEnablement::kDisabled;
}

bool AXPlatform::IsUiaProviderEnabled() const {
  return uia_provider_enablement_ == UiaProviderEnablement::kVariations
             ? base::FeatureList::IsEnabled(features::kUiaProvider)
             : (uia_provider_enablement_ == UiaProviderEnablement::kEnabled);
}

void AXPlatform::OnUiaProviderRequested(bool uia_provider_enabled) {
  delegate_->OnUiaProviderRequested(uia_provider_enabled);
}

void AXPlatform::RetrieveProductStringsIfNeeded() const {
  if (!product_strings_) {
    product_strings_ = delegate_->GetProductStrings();
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace ui
