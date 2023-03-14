// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_CONTEXT_FACTORIES_H_
#define UI_COMPOSITOR_TEST_TEST_CONTEXT_FACTORIES_H_

#include <memory>

#include "ui/compositor/test/in_process_context_factory.h"

namespace gl {
class DisableNullDrawGLBindings;
}

namespace viz {
class HostFrameSinkManager;
class ServerSharedBitmapManager;
class FrameSinkManagerImpl;
}  // namespace viz

namespace ui {

// Set up the compositor ContextFactory for a test environment. Unit tests that
// do not have a full content environment need to call this before initializing
// the Compositor. Some tests expect pixel output, and they should pass true for
// |enable_pixel_output|. Most unit tests should pass false.
class TestContextFactories {
 public:
  // The default for |output_to_window| will create an OutputSurface that does
  // not display anything. Set to true if you want to see results on the screen.
  explicit TestContextFactories(bool enable_pixel_output,
                                bool output_to_window = false);
  ~TestContextFactories();

  TestContextFactories(const TestContextFactories&) = delete;
  TestContextFactories& operator=(const TestContextFactories&) = delete;

  ui::InProcessContextFactory* GetContextFactory() const;

 private:
  std::unique_ptr<gl::DisableNullDrawGLBindings> disable_null_draw_;
  std::unique_ptr<viz::ServerSharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  std::unique_ptr<ui::InProcessContextFactory> implicit_factory_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_CONTEXT_FACTORIES_H_
