// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_HOST_BEGIN_FRAME_OBSERVER_H_
#define UI_COMPOSITOR_HOST_BEGIN_FRAME_OBSERVER_H_

#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/begin_frame_observer.mojom.h"
#include "ui/compositor/compositor_export.h"

namespace base {
class TimeTicks;
class TimeDelta;
}  // namespace base

namespace ui {

// A BeginFrameObserver implementation that forwards begin frame messages to
// registered sub-SimpleBeginFrameObservers.
//
// In case the host thread is slow, frame message scan get queued up.
// Provides coalescing of begin frame messages in this case, by gathering
// pending messages into a single message call.
class COMPOSITOR_EXPORT HostBeginFrameObserver
    : public viz::mojom::BeginFrameObserver {
 public:
  class SimpleBeginFrameObserver : public base::CheckedObserver {
   public:
    ~SimpleBeginFrameObserver() override = default;
    virtual void OnBeginFrame(base::TimeTicks frame_begin_time,
                              base::TimeDelta frame_interval) = 0;
    virtual void OnBeginFrameSourceShuttingDown() = 0;
  };

  using SimpleBeginFrameObserverList =
      base::ObserverList<ui::HostBeginFrameObserver::SimpleBeginFrameObserver,
                         true>;

  HostBeginFrameObserver(
      SimpleBeginFrameObserverList& observers,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~HostBeginFrameObserver() override;

  void OnStandaloneBeginFrame(const viz::BeginFrameArgs& args) override;
  mojo::PendingRemote<viz::mojom::BeginFrameObserver> GetBoundRemote();

 private:
  void CoalescedBeginFrame();
  void CallObservers(const viz::BeginFrameArgs& args);

  const raw_ref<SimpleBeginFrameObserverList> simple_begin_frame_observers_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool pending_coalesce_callback_ = false;
  viz::BeginFrameArgs begin_frame_args_;

  mojo::Receiver<viz::mojom::BeginFrameObserver> receiver_{this};
  base::WeakPtrFactory<HostBeginFrameObserver> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_HOST_BEGIN_FRAME_OBSERVER_H_
