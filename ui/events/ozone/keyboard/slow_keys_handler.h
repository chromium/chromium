// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_KEYBOARD_SLOW_KEYS_HANDLER_H_
#define UI_EVENTS_OZONE_KEYBOARD_SLOW_KEYS_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ui {

// Class that handles the Slow Keys accessibility functionality.
class COMPONENT_EXPORT(EVENTS_OZONE) SlowKeysHandler {
 public:
  // This callback is called after a key is held down past the delay threshold.
  // The TimeTicks param is the new timestamp with the delay.
  using OnKeyChangeCallback = base::OnceCallback<void(base::TimeTicks)>;

  SlowKeysHandler();
  SlowKeysHandler(const SlowKeysHandler&) = delete;
  SlowKeysHandler& operator=(const SlowKeysHandler&) = delete;
  ~SlowKeysHandler();

  // Called on every key state change to update the handler's internal key
  // state. Returns true if the key event should be dispatched by the caller
  // immediately without delay. Returns false if the key event should be
  // discarded. The `callback` is called by the handler when the slow keys delay
  // is reached and the key is still held down.
  bool UpdateKeyStateAndShouldDispatch(unsigned int key,
                                       bool down,
                                       base::TimeTicks timestamp,
                                       int device_id,
                                       OnKeyChangeCallback callback);

  // Getters and setters.
  bool IsEnabled() const;
  void SetEnabled(bool enabled);
  base::TimeDelta GetDelay() const;
  void SetDelay(base::TimeDelta delay);

 private:
  struct DelayMapKey {
    int device_id;
    unsigned int key;

    auto operator<=>(const DelayMapKey& rhs) const = default;
  };

  // Clears any pending delayed keys.
  void Clear();

  // Callback when a slow key delay is reached.
  // `input_timestamp` is the timestamp from the input event.
  // `system_timestamp` is the current system timestamp.
  // `callback` is the OnKeyChangeCallback to call.
  void OnDelayReached(base::TimeTicks input_timestamp,
                      base::TimeTicks system_timestamp,
                      OnKeyChangeCallback callback);

  bool enabled_ = false;
  base::TimeDelta delay_;

  base::flat_map<DelayMapKey, std::unique_ptr<base::OneShotTimer>>
      delayed_keys_map_;
  bool processing_delayed_key_ = false;

  base::WeakPtrFactory<SlowKeysHandler> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_KEYBOARD_SLOW_KEYS_HANDLER_H_
