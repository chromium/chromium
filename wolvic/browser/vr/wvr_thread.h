// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_THREAD_H_
#define WOLVIC_BROWSER_VR_WVR_THREAD_H_

#include <memory>

#include "base/android/java_handler_thread.h"
#include "base/functional/callback.h"
#include "wolvic/browser/vr/wvr_manager.h"

namespace wolvic {

class WvrThread : public base::android::JavaHandlerThread {
 public:
  WvrThread(base::OnceCallback<void()> initialized_callback);

  WvrThread(const WvrThread&) = delete;
  WvrThread& operator=(const WvrThread&) = delete;

  ~WvrThread() override;
  WvrManager* GetWvrManager() { return wvr_manager_.get(); }
  WvrGraphicsDelegate* GetWvrGraphics() { return wvr_graphics_.get(); }

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  base::OnceCallback<void()> initialized_callback_;

  // Created on GL thread.
  std::unique_ptr<WvrGraphicsDelegate> wvr_graphics_;
  std::unique_ptr<WvrManager> wvr_manager_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_THREAD_H_
