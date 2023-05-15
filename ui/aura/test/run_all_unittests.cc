// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_suite.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_surface_test_support.h"

class AuraTestSuite;

namespace {

AuraTestSuite* g_test_suite = nullptr;

}  // namespace

class AuraTestSuite : public base::TestSuite {
 public:
  AuraTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  AuraTestSuite(const AuraTestSuite&) = delete;
  AuraTestSuite& operator=(const AuraTestSuite&) = delete;

  void DestroyEnv() { env_.reset(); }
  void CreateEnv() { env_ = aura::Env::CreateInstance(); }

 protected:
  void Initialize() override {
    DCHECK(!g_test_suite);
    g_test_suite = this;
    base::TestSuite::Initialize();
    gl::GLSurfaceTestSupport::InitializeOneOff();
    env_ = aura::Env::CreateInstance();

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  void Shutdown() override {
    env_.reset();
    base::TestSuite::Shutdown();
    g_test_suite = nullptr;
  }

 private:
  std::unique_ptr<aura::Env> env_;
};

namespace aura {
namespace test {

EnvReinstaller::EnvReinstaller() {
  g_test_suite->DestroyEnv();
}

EnvReinstaller::~EnvReinstaller() {
  g_test_suite->CreateEnv();
}

}  // namespace test
}  // namespace aura

int main(int argc, char** argv) {
  AuraTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
