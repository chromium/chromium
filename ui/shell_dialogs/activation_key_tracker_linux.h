// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_ACTIVATION_KEY_TRACKER_LINUX_H_
#define UI_SHELL_DIALOGS_ACTIVATION_KEY_TRACKER_LINUX_H_

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/aura/env_observer.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// Accept responses from a file dialog are ignored until the dialog has been
// shown for at least this long. Matches the Windows and Mac file dialogs.
inline constexpr base::TimeDelta kFileDialogMinDisplayDuration =
    base::Milliseconds(500);

// Tracks whether Enter or Space is held so file dialogs can be deferred until
// the keys are released. This prevents a held key from accepting a dialog
// that appears underneath it.
class SHELL_DIALOGS_EXPORT ActivationKeyTracker : public ui::EventHandler,
                                                  public aura::EnvObserver {
 public:
  static ActivationKeyTracker* GetInstance();

  ActivationKeyTracker(const ActivationKeyTracker&) = delete;
  ActivationKeyTracker& operator=(const ActivationKeyTracker&) = delete;

  // Starts observing key events if aura::Env exists. Call this early (e.g. at
  // LinuxUi initialization) so the key press that opens a dialog is seen.
  void EnsureObserving();

  bool IsActivationKeyDown() const;

  // Runs `callback` immediately if no activation key is down, otherwise once
  // all activation keys are released.
  void WaitForKeyRelease(base::OnceClosure callback);

  void SetActivationKeyDownForTesting(bool is_down);
  void ResetForTesting();

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnCancelMode(ui::CancelModeEvent* event) override;

  // aura::EnvObserver:
  void OnWillDestroyEnv() override;

 private:
  friend class base::NoDestructor<ActivationKeyTracker>;

  ActivationKeyTracker();
  // Only ever stored in a NoDestructor, so this never runs.
  ~ActivationKeyTracker() override;

  static bool IsActivationKey(ui::KeyboardCode key_code);
  void NotifyCallbacks();

  bool observing_ = false;
  base::flat_set<ui::KeyboardCode> pressed_keys_;
  std::vector<base::OnceClosure> release_callbacks_;
  std::optional<bool> is_down_for_testing_;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_ACTIVATION_KEY_TRACKER_LINUX_H_
