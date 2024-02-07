// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_IDLE_POLLING_SERVICE_H_
#define UI_BASE_IDLE_IDLE_POLLING_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ui {

class IdleTimeProvider;

// Polls the system to determine whether the user is idle or the screen is
// locked and notifies observers.
class COMPONENT_EXPORT(UI_BASE_IDLE) IdlePollingService {
 public:
  static IdlePollingService* GetInstance();

  static constexpr base::TimeDelta kPollInterval = base::Seconds(15);

  struct State {
    bool locked;
    base::TimeDelta idle_time;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIdleStateChange(const State& state) = 0;
  };

  IdlePollingService(const IdlePollingService&) = delete;
  IdlePollingService& operator=(const IdlePollingService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const State& GetIdleState();

  void SetProviderForTest(std::unique_ptr<IdleTimeProvider> provider);
  void SetPollIntervalForTest(base::TimeDelta interval);
  bool IsPollingForTest();
  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  friend class base::NoDestructor<IdlePollingService>;

  IdlePollingService();
  ~IdlePollingService();

  void PollIdleState();

  base::TimeDelta poll_interval_;
  base::RepeatingTimer timer_;
  std::unique_ptr<IdleTimeProvider> provider_;
  State last_state_;
  base::ObserverList<Observer> observers_;
};

}  // namespace ui

#endif  // UI_BASE_IDLE_IDLE_POLLING_SERVICE_H_
