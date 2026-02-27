// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_HEADLESS_HEADLESS_SCREEN_MANAGER_H_
#define UI_DISPLAY_HEADLESS_HEADLESS_SCREEN_MANAGER_H_

#include <stdint.h>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ui/display/display_export.h"

namespace display {
class Display;

// Provides headless screen management functionality.
class DISPLAY_EXPORT HeadlessScreenManager {
 public:
  HeadlessScreenManager(const HeadlessScreenManager&) = delete;
  HeadlessScreenManager& operator=(const HeadlessScreenManager&) = delete;

  virtual ~HeadlessScreenManager() = default;

  // Actual implementation is delegated to headless screen objects.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual int64_t AddDisplay(const Display& display) = 0;
    virtual void RemoveDisplay(int64_t display_id) = 0;
    virtual void SetPrimaryDisplay(int64_t display_id) = 0;
  };

  // Returns HeadlessScreenManager singleton object; creates one if it does
  // not exist.
  static HeadlessScreenManager* Get();

  // Returns new headless display id.
  static int64_t GetNewDisplayId();

  // Sets the delegate. Will crash if delegate is already set.
  void SetDelegate(Delegate* delegate,
                   const base::Location& location = FROM_HERE);

  // Adds a new display. Returns the newly added display unique id.
  int64_t AddDisplay(const Display& display);

  // Removes the specified display.
  void RemoveDisplay(int64_t display_id);

  // Sets primary display for the given display list. Will crash if specified
  // display does not exist.
  void SetPrimaryDisplay(int64_t display_id);

 private:
  friend class base::NoDestructor<HeadlessScreenManager>;

  HeadlessScreenManager() = default;

  raw_ptr<Delegate> delegate_;
  base::Location location_;
};

}  // namespace display

#endif  // UI_DISPLAY_HEADLESS_HEADLESS_SCREEN_MANAGER_H_
