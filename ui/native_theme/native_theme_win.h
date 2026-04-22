// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
#define UI_NATIVE_THEME_NATIVE_THEME_WIN_H_

#include <EventToken.h>
#include <wrl/client.h>

#include <optional>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/native_theme/native_theme.h"

namespace ABI::Windows::Media::ClosedCaptioning {
struct IClosedCaptionPropertiesStatics2;
}

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeWin : public NativeTheme {
 public:
  NativeThemeWin(const NativeThemeWin&) = delete;
  NativeThemeWin& operator=(const NativeThemeWin&) = delete;

  // Closes cached theme handles so we can unload the DLL or update our UI
  // for a theme change.
  static void CloseHandles();

  // Sets the test override for the closed-caption properties statics
  // interface. Pass nullptr to clear.
  static void SetClosedCaptionPropertiesStaticsForTesting(
      Microsoft::WRL::ComPtr<ABI::Windows::Media::ClosedCaptioning::
                                 IClosedCaptionPropertiesStatics2> statics);

  // NativeTheme:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra_params) const override;

 protected:
  NativeThemeWin();
  ~NativeThemeWin() override;

  // NativeTheme:
  void PaintImpl(cc::PaintCanvas* canvas,
                 const ColorProvider* color_provider,
                 Part part,
                 State state,
                 const gfx::Rect& rect,
                 const ExtraParams& extra_params,
                 bool forced_colors,
                 bool dark_mode,
                 PreferredContrast contrast,
                 std::optional<SkColor> accent_color) const override;
  void OnToolkitSettingsChanged(bool force_notify) override;

 private:
  friend class base::NoDestructor<NativeThemeWin>;

  // Registers a WinRT listener for closed-captioning property changes.
  // Called from the constructor; stores the interface and token for cleanup.
  void RegisterClosedCaptionPropertiesChangedListener();

  // The IClosedCaptionPropertiesStatics2 interface used to register/unregister
  // the PropertiesChanged event. Stored for cleanup in the destructor.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Media::ClosedCaptioning::IClosedCaptionPropertiesStatics2>
      caption_statics2_;

  // Token returned by add_PropertiesChanged, used to unregister the event.
  EventRegistrationToken caption_properties_changed_token_{};
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_WIN_H_
