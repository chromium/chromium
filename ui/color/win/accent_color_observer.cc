// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/win/accent_color_observer.h"

#include <windows.ui.viewmanagement.h>
#include <wrl/client.h>

#include <utility>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/color_utils.h"

namespace {

std::optional<SkColor> TryGetAccentColorFromUISettings() {
  Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUISettings>
      ui_settings;
  base::win::ScopedHString class_id = base::win::ScopedHString::Create(
      RuntimeClass_Windows_UI_ViewManagement_UISettings);
  HRESULT hr = base::win::RoActivateInstance(class_id.get(), &ui_settings);
  if (!SUCCEEDED(hr) || !ui_settings) {
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUISettings3>
      ui_settings3;
  hr = ui_settings.As(&ui_settings3);
  if (!SUCCEEDED(hr) || !ui_settings3) {
    return std::nullopt;
  }

  ABI::Windows::UI::Color accent_color;
  hr = ui_settings3->GetColorValue(
      ABI::Windows::UI::ViewManagement::UIColorType_Accent, &accent_color);
  if (!SUCCEEDED(hr)) {
    return std::nullopt;
  }

  return SkColorSetARGB(0xFF, accent_color.R, accent_color.G, accent_color.B);
}

}  // namespace

namespace ui {

// static
AccentColorObserver* AccentColorObserver::Get() {
  static base::NoDestructor<AccentColorObserver> observer;
  return observer.get();
}

AccentColorObserver::AccentColorObserver() {
  dwm_key_ = std::make_unique<base::win::RegKey>(
      HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", KEY_READ);
  if (dwm_key_->Valid()) {
    OnDwmKeyUpdated();
  } else {
    dwm_key_.reset();
  }
}

AccentColorObserver::~AccentColorObserver() = default;

base::CallbackListSubscription AccentColorObserver::Subscribe(
    base::RepeatingClosure callback) {
  return callbacks_.Add(std::move(callback));
}

bool AccentColorObserver::ShouldUseAccentColorForWindowFrame() const {
  if (should_use_accent_color_for_window_frame_for_testing_.has_value()) {
    return should_use_accent_color_for_window_frame_for_testing_.value();
  }

  if (!dwm_key_) {
    return false;
  }

  DWORD color_prevalence = 0;
  return dwm_key_->ReadValueDW(L"ColorPrevalence", &color_prevalence) ==
             ERROR_SUCCESS &&
         color_prevalence == 1;
}

void AccentColorObserver::SetShouldUseAccentColorForWindowFrameForTesting(
    bool use_accent_color) {
  should_use_accent_color_for_window_frame_for_testing_ = use_accent_color;
  callbacks_.Notify();
}

void AccentColorObserver::SetAccentColorForTesting(
    std::optional<SkColor> accent_color) {
  accent_color_ = accent_color;
  if (!accent_color_.has_value()) {
    // Maintain invariant that a null accent color implies null other colors.
    accent_color_inactive_.reset();
    accent_border_color_.reset();
  }
  callbacks_.Notify();
}

void AccentColorObserver::OnDwmKeyUpdated() {
  UpdateAccentColors();
  callbacks_.Notify();

  // Watch for future changes. If there is no task runner, this is a test or
  // tool context and watching is unnecessary.
  if (!base::SequencedTaskRunner::HasCurrentDefault() ||
      !dwm_key_->StartWatching(base::BindOnce(
          &AccentColorObserver::OnDwmKeyUpdated, base::Unretained(this)))) {
    dwm_key_.reset();
  }
}

void AccentColorObserver::UpdateAccentColors() {
  accent_color_.reset();
  accent_color_inactive_.reset();
  accent_border_color_.reset();

  // Windows will set unsupported accent color values in the registry, while
  // coercing the value to another color. Use the UISettings API to ensure we
  // are getting the coerced color to match.
  accent_color_ = TryGetAccentColorFromUISettings();

  // Fall back to reading from registry if UISettings API failed.
  if (!accent_color_.has_value()) {
    if (DWORD accent_color = 0;
        dwm_key_->ReadValueDW(L"AccentColor", &accent_color) == ERROR_SUCCESS) {
      accent_color_ = skia::COLORREFToSkColor(accent_color);
    } else {
      // When there is no accent color, ignore inactive/border colors.
      return;
    }
  }

  if (DWORD accent_color_inactive = 0;
      dwm_key_->ReadValueDW(L"AccentColorInactive", &accent_color_inactive) ==
      ERROR_SUCCESS) {
    accent_color_inactive_ = skia::COLORREFToSkColor(accent_color_inactive);
  }

  if (DWORD colorization_color, colorization_color_balance;
      dwm_key_->ReadValueDW(L"ColorizationColor", &colorization_color) ==
          ERROR_SUCCESS &&
      dwm_key_->ReadValueDW(L"ColorizationColorBalance",
                            &colorization_color_balance) == ERROR_SUCCESS) {
    // The accent border color is a linear blend between the colorization
    // color and the neutral #d9d9d9. colorization_color_balance is the
    // percentage of the colorization color in that blend.
    //
    // On Windows version 1611 colorization_color_balance can be 0xfffffff3 if
    // the accent color is taken from the background and either the background
    // is a solid color or was just changed to a slideshow. It's unclear what
    // that value's supposed to mean, so change it to 80 to match Edge's
    // behavior.
    if (colorization_color_balance > 100) {
      colorization_color_balance = 80;
    }

    // colorization_color's high byte is not an alpha value, so replace it
    // with 0xff to make an opaque ARGB color.
    SkColor input_color = SkColorSetA(colorization_color, 0xff);

    accent_border_color_ =
        color_utils::AlphaBlend(input_color, SkColorSetRGB(0xd9, 0xd9, 0xd9),
                                colorization_color_balance / 100.0f);
  }
}

}  // namespace ui
