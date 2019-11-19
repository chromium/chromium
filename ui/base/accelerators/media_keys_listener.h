// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MEDIA_KEYS_LISTENER_H_
#define UI_BASE_ACCELERATORS_MEDIA_KEYS_LISTENER_H_

#include <memory>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

class Accelerator;

// Create MediaKeyListener to receive accelerators on media keys.
class UI_BASE_EXPORT MediaKeysListener {
 public:
  enum class Scope {
    kGlobal,   // Listener works whenever application in focus or not.
    kFocused,  // Listener only works whan application has focus.
  };

  // Media keys accelerators receiver.
  class UI_BASE_EXPORT Delegate : public base::CheckedObserver {
   public:
    ~Delegate() override;

    // Called on media key event.
    virtual void OnMediaKeysAccelerator(const Accelerator& accelerator) = 0;
  };

  // Can return nullptr if media keys listening is not implemented.
  // Currently implemented only on mac.
  static std::unique_ptr<MediaKeysListener> Create(Delegate* delegate,
                                                   Scope scope);

  static bool IsMediaKeycode(KeyboardCode key_code);

  virtual ~MediaKeysListener();

  // Start listening for a given media key. Returns true if the listener
  // successfully started listening for the key. Some implementations may not be
  // able to register if another application is already listening to the media
  // key.
  virtual bool StartWatchingMediaKey(KeyboardCode key_code) = 0;
  // Stop listening for a given media key.
  virtual void StopWatchingMediaKey(KeyboardCode key_code) = 0;

  // Informs the listener whether or not media is currently playing. In some
  // implementations this will prevent us from calling PlayPause unnecessarily.
  // TODO(https://crbug.com/974035): Once the MediaKeysListenerManager has been
  // refactored to work with system media controls this should no longer be
  // needed and should be deleted.
  virtual void SetIsMediaPlaying(bool is_playing) = 0;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MEDIA_KEYS_LISTENER_H_
