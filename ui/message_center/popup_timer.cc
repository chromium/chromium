// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/popup_timer.h"
#include "base/bind.h"

namespace message_center {

PopupTimer::PopupTimer(const std::string& id,
                       base::TimeDelta timeout,
                       base::WeakPtr<Delegate> delegate)
    : id_(id),
      timeout_(timeout),
      timer_delegate_(delegate),
      timer_(new base::OneShotTimer) {
  DCHECK(timer_delegate_);
}

PopupTimer::~PopupTimer() {
  if (timer_->IsRunning())
    timer_->Stop();
}

void PopupTimer::Start() {
  if (timer_->IsRunning())
    return;

  base::TimeDelta timeout_to_close =
      timeout_ <= passed_ ? base::TimeDelta() : timeout_ - passed_;
  start_time_ = base::Time::Now();

  timer_->Start(FROM_HERE, timeout_to_close,
                base::BindOnce(&Delegate::TimerFinished, timer_delegate_, id_));
}

void PopupTimer::Pause() {
  if (!timer_->IsRunning())
    return;

  timer_->Stop();
  passed_ += base::Time::Now() - start_time_;
}

}  // namespace message_center
