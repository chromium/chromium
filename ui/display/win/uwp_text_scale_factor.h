// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_UWP_TEXT_SCALE_FACTOR_H_
#define UI_DISPLAY_WIN_UWP_TEXT_SCALE_FACTOR_H_

#include "base/observer_list.h"
#include "ui/display/display_export.h"

namespace display {
namespace win {

// Provides access to UWP TextScaleFactor, an accessibility feature.
//
// TODO(dfried): In the future we may need to expand this class to capture
// other UWP-only metrics and functionality. At that point, we probably want to
// rename the class and/or put it in base/win.
class DISPLAY_EXPORT UwpTextScaleFactor {
 public:
  // Observer for UWP screen metrics and accessibility events.
  class DISPLAY_EXPORT Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that the text scale has changed. Will be called on
    // the main thread.
    virtual void OnUwpTextScaleFactorChanged() = 0;

    // Notifies the observer that this object is about to go out of scope and
    // that it should release any references to this object. The base
    // implementation removes this object as a listener, so remember to call
    // it when you're done with derived class cleanup.
    virtual void OnUwpTextScaleFactorCleanup(UwpTextScaleFactor* source);
  };

  // Retrieves the one global instance. Lazily-created unless you want to mock
  // it out with SetImplementationForTesting().
  static UwpTextScaleFactor* Instance();

  virtual ~UwpTextScaleFactor();

  // Retrieves the Windows Text Zoom scale factor. Guaranteed to be >= 1.
  virtual float GetTextScaleFactor() const;

  // Registers and observer that will be notified of any changes to UWP screen
  // or accessibility metrics.
  void AddObserver(Observer* observer);

  // Removes an observer that has been added. Observers should remove
  // themselves either during OnUwpScreenMetricsCleanup() or at destruction
  // (whichever is first).
  void RemoveObserver(Observer* observer);

  // Override creation of the default UwpTextScaleFactor implementation, using
  // a mock or other stub implementation of your choice. The caller is
  // responsible for cleaning up this object.
  static void SetImplementationForTesting(UwpTextScaleFactor* mock_impl);

 protected:
  UwpTextScaleFactor();
  void NotifyUwpTextScaleFactorChanged();

  // The observers that need to be notified when a display is modified, added
  // or removed.
  base::ObserverList<Observer> observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UwpTextScaleFactor);
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_UWP_TEXT_SCALE_FACTOR_H_
