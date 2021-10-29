// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"
#include "tools/mac/power/power_sampler/csv_exporter.h"
#include "tools/mac/power/power_sampler/main_display_sampler.h"
#include "tools/mac/power/power_sampler/sampler.h"
#include "tools/mac/power/power_sampler/sampling_controller.h"
#include "tools/mac/power/power_sampler/user_idle_level_sampler.h"

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::DEFAULT);
  power_sampler::SamplingController controller;

  std::unique_ptr<power_sampler::Sampler> sampler =
      power_sampler::MainDisplaySampler::Create();
  if (sampler)
    controller.AddSampler(std::move(sampler));

  sampler = power_sampler::BatterySampler::Create();
  if (sampler)
    controller.AddSampler(std::move(sampler));

  sampler = power_sampler::UserIdleLevelSampler::Create();
  if (sampler)
    controller.AddSampler(std::move(sampler));

  base::TimeTicks start_time = base::TimeTicks::Now();
  controller.AddMonitor(power_sampler::CsvExporter::Create(
      start_time, base::File(STDOUT_FILENO)));

  controller.StartSession();

  // TODO(etiennep): Take sampling interval from command line.
  base::TimeDelta sampling_interval = base::Seconds(10);

  base::RunLoop run_loop;
  // TODO(siggi): Support battery notifications to drive OnSamplingEvent().
  base::RepeatingTimer timer(
      FROM_HERE, sampling_interval,
      BindRepeating(
          [](power_sampler::SamplingController* controller,
             base::OnceClosure quit_closure) {
            if (controller->OnSamplingEvent()) {
              std::move(quit_closure).Run();
            }
          },
          base::Unretained(&controller), run_loop.QuitClosure()));
  timer.Reset();

  run_loop.Run();

  controller.EndSession();

  // TODO(etiennep): Optionally export to a file.

  return 0;
}
