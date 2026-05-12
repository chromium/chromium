// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_EXTERNAL_BEGIN_FRAME_ADAPTER_H_
#define UI_COMPOSITOR_EXTERNAL_BEGIN_FRAME_ADAPTER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/platform_window/extensions/begin_frame_source_extension.h"

namespace ui {

// Bridges a BeginFrameSourceExtension on PlatformWindow to viz over the
// compositor.
class COMPOSITOR_EXPORT ExternalBeginFrameAdapter
    : public BeginFrameSourceExtension::Delegate,
      public ExternalBeginFrameControllerClientFactory,
      public viz::mojom::ExternalBeginFrameControllerClient {
 public:
  ExternalBeginFrameAdapter(Compositor* compositor,
                            BeginFrameSourceExtension* source);

  ExternalBeginFrameAdapter(const ExternalBeginFrameAdapter&) = delete;
  ExternalBeginFrameAdapter& operator=(const ExternalBeginFrameAdapter&) =
      delete;

  ~ExternalBeginFrameAdapter() override;

  // BeginFrameSourceExtension::Delegate implementation.
  void OnBeginFrame(
      base::TimeTicks frame_time,
      base::TimeTicks deadline,
      base::TimeDelta interval,
      base::OnceCallback<void(bool has_damage)> ack_callback) override;

  // ExternalBeginFrameControllerClientFactory implementation.
  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
  CreateExternalBeginFrameControllerClient() override;

  // viz::mojom::ExternalBeginFrameControllerClient implementation.
  void SetNeedsBeginFrame(bool needs_begin_frames) override;
  void SetPreferredInterval(base::TimeDelta interval) override;
  void NeedsBeginFrameWithId(int64_t display_id,
                             bool needs_begin_frames) override;

 private:
  void OnBeginFrameAck(base::OnceCallback<void(bool)> ack_callback,
                       const viz::BeginFrameAck& ack);

  static uint64_t AllocateSourceId();

  const raw_ptr<Compositor> compositor_;
  const raw_ptr<BeginFrameSourceExtension> source_;

  // Unique source_id for the BeginFrameArgs we generate. Must be non-zero to
  // avoid colliding with the default original_source_id_ (0) in
  // ExternalBeginFrameSourceMojo when newly instantiated.
  const uint64_t source_id_ = AllocateSourceId();

  viz::BeginFrameSource::BeginFrameArgsGenerator args_generator_;

  mojo::AssociatedReceiverSet<viz::mojom::ExternalBeginFrameControllerClient>
      receivers_;

  base::WeakPtrFactory<ExternalBeginFrameAdapter> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_EXTERNAL_BEGIN_FRAME_ADAPTER_H_
