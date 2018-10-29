// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_WINDOW_PORT_MUS_TEST_HELPER_H_
#define UI_AURA_TEST_MUS_WINDOW_PORT_MUS_TEST_HELPER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace cc {
class LayerTreeFrameSink;
}

namespace viz {
class ParentLocalSurfaceIdAllocator;
}

namespace aura {

class Window;
class WindowPortMus;

class WindowPortMusTestHelper {
 public:
  explicit WindowPortMusTestHelper(Window* window);
  ~WindowPortMusTestHelper();

  void SimulateEmbedding();

  base::WeakPtr<cc::LayerTreeFrameSink> GetFrameSink();

  viz::ParentLocalSurfaceIdAllocator* GetParentLocalSurfaceIdAllocator();

 private:
  static uint32_t next_client_id_;

  WindowPortMus* window_port_mus_;

  DISALLOW_COPY_AND_ASSIGN(WindowPortMusTestHelper);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_WINDOW_PORT_MUS_TEST_HELPER_H_
