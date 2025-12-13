// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform.h"

#include "base/check_op.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_mode_observer.h"

#if BUILDFLAG(IS_WIN)
#include <oleacc.h>

#include <uiautomation.h>
#endif  // BUILDFLAG(IS_WIN)

namespace ui {

namespace {

AXPlatform* g_instance = nullptr;

}  // namespace

// static
AXPlatform& AXPlatform::GetInstance() {
  CHECK_NE(g_instance, nullptr)
      << "AXPlatform::GetInstance() called before AXPlatform was initialized "
         "or destroyed. If you are in a browser test, you may need cleanup in "
         "TearDownOnMainThread().";
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
  return delegate_->GetAccessibilityMode();
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

void AXPlatform::DisableActiveUiaProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (uia_provider_enablement_ == UiaProviderEnablement::kDisabled) {
    // Already disabled.
    return;
  }

  uia_provider_enablement_ = UiaProviderEnablement::kDisabled;

  // We must call this *after* we disabled the UIA provider to ensure that we
  // don't respond with the same provider to a re-entrant WM_GETOBJECT call. See
  // https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiadisconnectallproviders
  // for more info.
  HRESULT hr = ::UiaDisconnectAllProviders();
  DCHECK(SUCCEEDED(hr));
}

bool AXPlatform::IsUiaProviderEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return uia_provider_enablement_ == UiaProviderEnablement::kVariations
             ? base::FeatureList::IsEnabled(features::kUiaProvider)
             : (uia_provider_enablement_ == UiaProviderEnablement::kEnabled);
}

void AXPlatform::SetUiaClientServiced(bool uia_client_serviced) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  has_serviced_uia_clients_ = uia_client_serviced;
}

bool AXPlatform::HasServicedUiaClients() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return has_serviced_uia_clients_;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
void AXPlatform::OnScreenReaderHoneyPotQueried() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We used to trust this as a signal that a screen reader is running, but it's
  // been abused. Now only enable accessibility if we detect that the name is
  // also used. Do not do the same for location and role, as the Windows Text
  // Services Framework (MSTSF) has been known to check the role of each new
  // window; see https://crbug.com/416429182.
  if (screen_reader_honeypot_queried_) {
    return;
  }
  screen_reader_honeypot_queried_ = true;
  if (is_name_used_) {
    OnPropertiesUsedInWebContent();
  }
}
#endif  // BUILDFLAG(IS_WIN)

void AXPlatform::OnMinimalPropertiesUsed(bool is_name_used) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnMinimalPropertiesUsed();
#if BUILDFLAG(IS_WIN)
  // See OnScreenReaderHoneyPotQueried, above.
  if (!is_name_used || is_name_used_) {
    return;
  }
  is_name_used_ = true;
  if (screen_reader_honeypot_queried_) {
    OnPropertiesUsedInWebContent();
    return;
  }
#endif
}

void AXPlatform::OnPropertiesUsedInBrowserUI() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnPropertiesUsedInBrowserUI();
}

void AXPlatform::OnPropertiesUsedInWebContent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnPropertiesUsedInWebContent();
}

void AXPlatform::OnInlineTextBoxesUsedInWebContent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnInlineTextBoxesUsedInWebContent();
}

void AXPlatform::OnExtendedPropertiesUsedInWebContent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnExtendedPropertiesUsedInWebContent();
}

void AXPlatform::OnHTMLAttributesUsed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnHTMLAttributesUsed();
}

void AXPlatform::OnActionFromAssistiveTech() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnActionFromAssistiveTech();
}

void AXPlatform::DetachFromThreadForTesting() {
  DETACH_FROM_THREAD(thread_checker_);
}

#if BUILDFLAG(IS_WIN)
void AXPlatform::RetrieveProductStringsIfNeeded() const {
  if (!product_strings_) {
    product_strings_ = delegate_->GetProductStrings();
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace ui
