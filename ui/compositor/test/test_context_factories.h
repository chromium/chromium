// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_CONTEXT_FACTORIES_H_
#define UI_COMPOSITOR_TEST_TEST_CONTEXT_FACTORIES_H_

#include <memory>

namespace gl {
class DisableNullDrawGLBindings;
}

namespace viz {
class HostFrameSinkManager;
class ServerSharedBitmapManager;
class FrameSinkManagerImpl;
}  // namespace viz

namespace ui {

class InProcessContextFactory;
class ContextFactory;
class ContextFactoryPrivate;

// Set up the compositor ContextFactory for a test environment. Unit tests that
// do not have a full content environment need to call this before initializing
// the Compositor. Some tests expect pixel output, and they should pass true for
// |enable_pixel_output|. Most unit tests should pass false.
class TestContextFactories {
 public:
  explicit TestContextFactories(bool enable_pixel_output);
  TestContextFactories(bool enable_pixel_output, bool use_skia_renderer);
  ~TestContextFactories();

  TestContextFactories(const TestContextFactories&) = delete;
  TestContextFactories& operator=(const TestContextFactories&) = delete;

  ContextFactory* GetContextFactory() const;
  ContextFactoryPrivate* GetContextFactoryPrivate() const;

 private:
  std::unique_ptr<gl::DisableNullDrawGLBindings> disable_null_draw_;
  std::unique_ptr<viz::ServerSharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  std::unique_ptr<ui::InProcessContextFactory> implicit_factory_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_CONTEXT_FACTORIES_H_
