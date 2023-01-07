// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_PRIVACY_BUDGET_CLOCK_SKEW_CLOCK_SKEW_TOOL_H_
#define TOOLS_PRIVACY_BUDGET_CLOCK_SKEW_CLOCK_SKEW_TOOL_H_

#include <memory>

#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace clock_skew {

class ClockSkewTool {
 public:
  ClockSkewTool();
  ~ClockSkewTool();

  network_time::NetworkTimeTracker* tracker() const { return tracker_.get(); }

 private:
  base::SingleThreadTaskExecutor executor_{base::MessagePumpType::IO};
  // TODO(https://crbug.com/1258624) Replace `ScopedFeatureList` and
  // `TestingPrefServiceSimple` since they're meant to be used in tests.
  base::test::ScopedFeatureList features_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<network::NetworkService> network_service_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<network_time::NetworkTimeTracker> tracker_;
};

}  // namespace clock_skew

#endif  // TOOLS_PRIVACY_BUDGET_CLOCK_SKEW_CLOCK_SKEW_TOOL_H_
