// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CONFIGURE_DISPLAYS_TASK_H_
#define UI_DISPLAY_MANAGER_CONFIGURE_DISPLAYS_TASK_H_

#include <stddef.h>

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace display {

class DisplayMode;
class DisplaySnapshot;
class NativeDisplayDelegate;

struct DISPLAY_MANAGER_EXPORT DisplayConfigureRequest {
  DisplayConfigureRequest(DisplaySnapshot* display,
                          const DisplayMode* mode,
                          const gfx::Point& origin);

  DisplaySnapshot* display;
  const DisplayMode* mode;
  gfx::Point origin;
};

// Applies the display configuration asynchronously.
class DISPLAY_MANAGER_EXPORT ConfigureDisplaysTask
    : public NativeDisplayObserver {
 public:
  enum Status {
    // At least one of the displays failed to apply any mode it supports.
    ERROR,

    // The requested configuration was applied.
    SUCCESS,

    // At least one of the displays failed to apply the requested
    // configuration, but it managed to fall back to another mode.
    PARTIAL_SUCCESS,
  };

  using ResponseCallback = base::OnceCallback<void(Status)>;

  ConfigureDisplaysTask(NativeDisplayDelegate* delegate,
                        const std::vector<DisplayConfigureRequest>& requests,
                        ResponseCallback callback);
  ~ConfigureDisplaysTask() override;

  // Starts the configuration task.
  void Run();

  // display::NativeDisplayObserver:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

 private:
  void OnConfigured(size_t index, bool success);

  NativeDisplayDelegate* delegate_;  // Not owned.

  std::vector<DisplayConfigureRequest> requests_;

  // When the task finishes executing it runs the callback to notify that the
  // task is done and the task status.
  ResponseCallback callback_;

  // Stores the indexes of pending requests in |requests_|.
  base::queue<size_t> pending_request_indexes_;

  // Used to keep make sure that synchronous executions do not recurse during
  // the configuration.
  bool is_configuring_;

  // Number of display configured. This is used to check whether there are
  // pending requests.
  size_t num_displays_configured_;

  Status task_status_;

  base::WeakPtrFactory<ConfigureDisplaysTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConfigureDisplaysTask);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CONFIGURE_DISPLAYS_TASK_H_
