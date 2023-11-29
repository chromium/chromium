// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_NATIVE_DISPLAY_DELEGATE_H_
#define UI_DISPLAY_TYPES_NATIVE_DISPLAY_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_types_export.h"
#include "ui/display/types/fake_display_controller.h"

namespace display {
class DisplaySnapshot;
class GammaCurve;
class NativeDisplayObserver;

struct DisplayConfigurationParams;

using GetDisplaysCallback =
    base::OnceCallback<void(const std::vector<DisplaySnapshot*>&)>;
using ConfigureCallback = base::OnceCallback<void(bool)>;
using SetHdcpKeyPropCallback = base::OnceCallback<void(bool)>;
using GetHDCPStateCallback =
    base::OnceCallback<void(bool, HDCPState, ContentProtectionMethod)>;
using SetHDCPStateCallback = base::OnceCallback<void(bool)>;
using DisplayControlCallback = base::OnceCallback<void(bool)>;
using SetPrivacyScreenCallback = base::OnceCallback<void(bool)>;

// Interface for classes that perform display configuration actions on behalf
// of DisplayConfigurator.
// Implementations may perform calls asynchronously. In the case of functions
// taking callbacks, the callbacks may be called asynchronously when the results
// are available. The implementations must provide a strong guarantee that the
// callbacks are always called.
class DISPLAY_TYPES_EXPORT NativeDisplayDelegate {
 public:
  virtual ~NativeDisplayDelegate();

  virtual void Initialize() = 0;

  // Take control of the display from any other controlling process.
  virtual void TakeDisplayControl(DisplayControlCallback callback) = 0;

  // Let others control the display.
  virtual void RelinquishDisplayControl(DisplayControlCallback callback) = 0;

  // Queries for a list of fresh displays and returns them via |callback|.
  // Note the query operation may be expensive and take over 60 milliseconds.
  virtual void GetDisplays(GetDisplaysCallback callback) = 0;

  // Configures the displays represented by |config_requests| to use |mode| and
  // positions the display to |origin| in the framebuffer. The callback will
  // return the status of the operation. Adjusts the behavior of the commit
  // according to |modeset_flag| (see display::ModesetFlag).
  virtual void Configure(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      ConfigureCallback callback,
      uint32_t modeset_flag) = 0;

  // Sets the HDCP Key Property.
  virtual void SetHdcpKeyProp(int64_t display_id,
                              const std::string& key,
                              SetHdcpKeyPropCallback callback) = 0;

  // Gets HDCP state of output.
  virtual void GetHDCPState(const DisplaySnapshot& output,
                            GetHDCPStateCallback callback) = 0;

  // Sets HDCP state of output.
  virtual void SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state,
                            ContentProtectionMethod protection_method,
                            SetHDCPStateCallback callback) = 0;

  // Sets the given 3x3 |color_matrix| on the display with |display_id|.
  // This doesn't affect gamma or degamma. It returns true the color matrix was
  // sent to the GPU process successfully.
  virtual bool SetColorMatrix(int64_t display_id,
                              const std::vector<float>& color_matrix) = 0;

  // Sets the given |gamma_lut| and |degamma_lut| on the display with
  // |display_id|. Returns true if the given tables were sent to the GPU process
  // successfully.
  virtual bool SetGammaCorrection(int64_t display_id,
                                  const GammaCurve& degamma,
                                  const GammaCurve& gamma) = 0;

  // Sets the privacy screen state on the display with |display_id|.
  virtual void SetPrivacyScreen(int64_t display_id,
                                bool enabled,
                                SetPrivacyScreenCallback callback) = 0;

  virtual void AddObserver(NativeDisplayObserver* observer) = 0;

  virtual void RemoveObserver(NativeDisplayObserver* observer) = 0;

  // Returns a fake display controller that can modify the fake display state.
  // Will return null if not needed, most likely because the delegate is
  // intended for use on device and doesn't need to fake the display state.
  virtual FakeDisplayController* GetFakeDisplayController() = 0;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_NATIVE_DISPLAY_DELEGATE_H_
