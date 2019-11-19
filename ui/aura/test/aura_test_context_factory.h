// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_CONTEXT_FACTORY_H_
#define UI_AURA_TEST_AURA_TEST_CONTEXT_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "ui/compositor/test/fake_context_factory.h"

namespace cc {
class TestLayerTreeFrameSinkClient;
}

namespace aura {
namespace test {

class AuraTestContextFactory : public ui::FakeContextFactory {
 public:
  AuraTestContextFactory();
  ~AuraTestContextFactory() override;

  // ui::FakeContextFactory
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;

 private:
  std::set<std::unique_ptr<cc::TestLayerTreeFrameSinkClient>>
      frame_sink_clients_;

  DISALLOW_COPY_AND_ASSIGN(AuraTestContextFactory);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_CONTEXT_FACTORY_H_<
