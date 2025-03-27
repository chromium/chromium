// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform.h"

#include "base/check_op.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_mode_observer.h"

namespace ui {

namespace {

AXPlatform* g_instance = nullptr;

}  // namespace

// static
AXPlatform& AXPlatform::GetInstance() {
  CHECK_NE(g_instance, nullptr);
  DCHECK_CALLED_ON_VALID_THREAD(g_instance->thread_checker_);
  return *g_instance;
}

AXPlatform::AXPlatform(Delegate& delegate) : delegate_(delegate) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AXPlatform::~AXPlatform() {
  DCHECK_EQ(g_instance, this);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_instance = nullptr;
}

AXMode AXPlatform::GetMode() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return delegate_->GetProcessMode();
}

void AXPlatform::AddModeObserver(AXModeObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void AXPlatform::RemoveModeObserver(AXModeObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

void AXPlatform::NotifyModeAdded(AXMode mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.Notify(&AXModeObserver::OnAXModeAdded, mode);
}

void AXPlatform::NotifyAssistiveTechChanged(AssistiveTech assistive_tech) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_assistive_tech_ == assistive_tech) {
    return;
  }
  active_assistive_tech_ = assistive_tech;
  observers_.Notify(&AXModeObserver::OnAssistiveTechChanged, assistive_tech);
}

bool AXPlatform::IsScreenReaderActive() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return IsScreenReader(active_assistive_tech_);
}

void AXPlatform::NotifyAccessibilityApiUsage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnAccessibilityApiUsage();
}

bool AXPlatform::IsCaretBrowsingEnabled() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return caret_browsing_enabled_;
}

void AXPlatform::SetCaretBrowsingState(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  caret_browsing_enabled_ = enabled;
}

#if BUILDFLAG(IS_WIN)
const std::string& AXPlatform::GetProductName() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RetrieveProductStringsIfNeeded();
  return product_strings_->product_name;
}

const std::string& AXPlatform::GetProductVersion() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RetrieveProductStringsIfNeeded();
  return product_strings_->product_version;
}

const std::string& AXPlatform::GetToolkitVersion() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RetrieveProductStringsIfNeeded();
  return product_strings_->toolkit_version;
}

void AXPlatform::SetUiaProviderEnabled(bool is_enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(uia_provider_enablement_, UiaProviderEnablement::kVariations);
  uia_provider_enablement_ = is_enabled ? UiaProviderEnablement::kEnabled
                                        : UiaProviderEnablement::kDisabled;
}

bool AXPlatform::IsUiaProviderEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return uia_provider_enablement_ == UiaProviderEnablement::kVariations
             ? base::FeatureList::IsEnabled(features::kUiaProvider)
             : (uia_provider_enablement_ == UiaProviderEnablement::kEnabled);
}

void AXPlatform::OnUiaProviderRequested(bool uia_provider_enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnUiaProviderRequested(uia_provider_enabled);
}
#endif  // BUILDFLAG(IS_WIN)

void AXPlatform::DetachFromThreadForTesting() {
  DETACH_FROM_THREAD(thread_checker_);
}

void AXPlatform::SetMode(AXMode new_mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->SetProcessMode(new_mode);
}

#if BUILDFLAG(IS_WIN)
void AXPlatform::RetrieveProductStringsIfNeeded() const {
  if (!product_strings_) {
    product_strings_ = delegate_->GetProductStrings();
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace ui
