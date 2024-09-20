// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle_polling_service.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_time_provider.h"

namespace ui {

namespace {

// Default provider implementation. Everything is delegated to
// ui::CalculateIdleTime and ui::CheckIdleStateIsLocked.
class DefaultIdleProvider : public IdleTimeProvider {
 public:
  DefaultIdleProvider() = default;
  ~DefaultIdleProvider() override = default;

  base::TimeDelta CalculateIdleTime() override {
    return base::Seconds(ui::CalculateIdleTime());
  }

  bool CheckIdleStateIsLocked() override {
    return ui::CheckIdleStateIsLocked();
  }
};

}  // namespace

// static
IdlePollingService* IdlePollingService::GetInstance() {
  static base::NoDestructor<IdlePollingService> instance;
  return instance.get();
}

const IdlePollingService::State& IdlePollingService::GetIdleState() {
  // |last_state_| won't be valid if we aren't polling.
  DCHECK(timer_.IsRunning());
  return last_state_;
}

void IdlePollingService::AddObserver(Observer* observer) {
  // Fetch the current state and start polling if this was the first observer.
  if (observers_.empty()) {
    DCHECK(!timer_.IsRunning());
    PollIdleState();
    timer_.Start(FROM_HERE, poll_interval_,
                 base::BindRepeating(&IdlePollingService::PollIdleState,
                                     base::Unretained(this)));
  }

  observers_.AddObserver(observer);
}

void IdlePollingService::RemoveObserver(Observer* observer) {
  DCHECK(timer_.IsRunning());
  observers_.RemoveObserver(observer);

  // Stop polling when there are no observers to save resources.
  if (observers_.empty()) {
    timer_.Stop();
  }
}

void IdlePollingService::SetProviderForTest(
    std::unique_ptr<IdleTimeProvider> provider) {
  provider_ = std::move(provider);
  if (!provider_) {
    provider_ = std::make_unique<DefaultIdleProvider>();
  }
}

void IdlePollingService::SetPollIntervalForTest(base::TimeDelta poll_interval) {
  DCHECK(!timer_.IsRunning());
  poll_interval_ = poll_interval;
}

bool IdlePollingService::IsPollingForTest() {
  return timer_.IsRunning();
}

void IdlePollingService::SetTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  timer_.SetTaskRunner(std::move(task_runner));
}

IdlePollingService::IdlePollingService()
    : poll_interval_(kPollInterval),
      provider_(std::make_unique<DefaultIdleProvider>()) {
  DCHECK(!timer_.IsRunning());
}

IdlePollingService::~IdlePollingService() = default;

void IdlePollingService::PollIdleState() {
  last_state_.idle_time = provider_->CalculateIdleTime();
  last_state_.locked = provider_->CheckIdleStateIsLocked();

  // TODO(crbug.com/41445751): Only notify observers on change.
  observers_.Notify(&Observer::OnIdleStateChange, last_state_);
}

}  // namespace ui
