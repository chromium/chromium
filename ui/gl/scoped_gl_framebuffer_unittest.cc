// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_gl_framebuffer.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

using ::testing::_;
using ::testing::Exactly;

class ScopedGLFramebufferTest : public testing::Test {
 public:
  void SetUp() override {
    SetGLGetProcAddressProc(MockGLInterface::GetGLProcAddress);
    display_ = GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::StrictMock<MockGLInterface>>();
    MockGLInterface::SetGLInterface(gl_.get());

    context_ = new GLContextStub;
    surface_ = new GLSurfaceStub;
    context_->MakeCurrent(surface_.get());
  }

  void TearDown() override {
    context_ = nullptr;
    surface_ = nullptr;
    MockGLInterface::SetGLInterface(nullptr);
    GLSurfaceTestSupport::ShutdownGL(display_);
    gl_.reset();
  }

 protected:
  std::unique_ptr<::testing::StrictMock<MockGLInterface>> gl_;
  scoped_refptr<GLContextStub> context_;
  scoped_refptr<GLSurfaceStub> surface_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(ScopedGLFramebufferTest, HelperDeletesFramebuffer) {
  GLuint framebuffer_id = 42;

  // Expect that glDeleteFramebuffersEXT is called with the framebuffer ID
  // when the ScopedGLFramebuffer goes out of scope.
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, testing::Pointee(framebuffer_id)))
      .Times(Exactly(1));

  {
    ScopedGLFramebuffer framebuffer(framebuffer_id);
    EXPECT_EQ(framebuffer_id, framebuffer.get());
  }
}

TEST_F(ScopedGLFramebufferTest, HelperDoesNotDeleteInvalidFramebuffer) {
  // Expect that glDeleteFramebuffersEXT is NOT called for 0.
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(_, _)).Times(Exactly(0));

  {
    ScopedGLFramebuffer framebuffer(0);
    EXPECT_EQ(0u, framebuffer.get());
  }
}

TEST_F(ScopedGLFramebufferTest, CreateScopedGLFramebufferHelper) {
  GLuint framebuffer_id = 42;

  // Expect that glGenFramebuffersEXT is called and returns the ID.
  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, testing::NotNull()))
      .WillOnce(testing::SetArgPointee<1>(framebuffer_id));

  // Expect that glDeleteFramebuffersEXT is called when it goes out of scope.
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, testing::Pointee(framebuffer_id)))
      .Times(Exactly(1));

  {
    auto framebuffer = CreateScopedGLFramebuffer(g_current_gl_context);
    EXPECT_EQ(framebuffer_id, framebuffer.get());
  }
}

}  // namespace gl
