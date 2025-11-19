// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "ui/base/accelerators/command.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {

class Accelerator;

// Platform-neutral implementation of a class that keeps track of observers and
// monitors keystrokes. It relays messages to the appropriate observer when a
// global accelerator has been struck by the user.
class GlobalAcceleratorListener {
 public:
  class Observer {
   public:
    // Called when your global accelerator is struck.
    virtual void OnKeyPressed(const ui::Accelerator& accelerator) = 0;
    // Called when a command should be executed
    // directly.
    virtual void ExecuteCommand(const std::string& accelerator_group_id,
                                const std::string& command_id) = 0;

   protected:
    virtual ~Observer() = default;
  };

  GlobalAcceleratorListener(const GlobalAcceleratorListener&) = delete;
  GlobalAcceleratorListener& operator=(const GlobalAcceleratorListener&) =
      delete;

  virtual ~GlobalAcceleratorListener();

  // The instance may be nullptr.
  static GlobalAcceleratorListener* GetInstance();

  // Register an observer for when a certain `accelerator` is struck. Returns
  // true if register successfully, or false if the specified `accelerator`
  // has been registered by another caller or other native applications.
  //
  // Note that we do not support recognizing that an accelerator has been
  // registered by another application on all platforms. This is a per-platform
  // consideration.
  bool RegisterAccelerator(const ui::Accelerator& accelerator,
                           Observer* observer);

  // Unregister and stop listening for the given `accelerator`.
  void UnregisterAccelerator(const ui::Accelerator& accelerator,
                             Observer* observer);

  // Unregister and stop listening for all accelerators of the given `observer`.
  // Returns a vector of the accelerators that were unregistered.
  void UnregisterAccelerators(Observer* observer);

  // Suspend/Resume global shortcut handling. Note that when suspending,
  // RegisterAccelerator/UnregisterAccelerator/UnregisterAccelerators are not
  // allowed to be called until shortcut handling has been resumed.
  void SetShortcutHandlingSuspended(bool suspended);

  // Returns whether shortcut handling is currently suspended.
  bool IsShortcutHandlingSuspended() const;

  // Called when a group of commands are registered.
  virtual void OnCommandsChanged(const std::string& accelerator_group_id,
                                 const std::string& profile_id,
                                 const CommandMap& commands,
                                 gfx::AcceleratedWidget widget,
                                 Observer* observer) {}

  virtual bool IsRegistrationHandledExternally() const;

 protected:
  GlobalAcceleratorListener();

  // Called by platform specific implementations of this class whenever a key
  // is struck. Only called for keys that have an observer registered.
  void NotifyKeyPressed(const ui::Accelerator& accelerator);

 private:
  // The following methods are implemented by platform-specific implementations
  // of this class.
  //
  // Start/StopListening are called when transitioning between zero and nonzero
  // registered accelerators. StartListening will be called after
  // StartListeningForAccelerator and StopListening will be called after
  // StopListeningForAccelerator.
  //
  // For StartListeningForAccelerator, implementations return false if
  // registration did not complete successfully.
  virtual void StartListening() = 0;
  virtual void StopListening() = 0;

  // Begin listening to an accelerator that has already been registered by
  // calling `RegisterAccelerator`.
  virtual bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) = 0;
  // Stop listening to an accelerator that has already been registered by
  // calling `RegisterAccelerator`.
  virtual void StopListeningForAccelerator(
      const ui::Accelerator& accelerator) = 0;

  // The map of accelerators that have been successfully registered as global
  // accelerators and their observer.
  typedef std::map<ui::Accelerator, raw_ptr<Observer, CtnExperimental>>
      AcceleratorMap;
  AcceleratorMap accelerator_map_;

  // Keeps track of whether shortcut handling is currently suspended.
  bool shortcut_handling_suspended_ = false;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_H_
