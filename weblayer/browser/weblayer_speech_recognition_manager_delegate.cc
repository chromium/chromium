// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_speech_recognition_manager_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

using content::BrowserThread;

namespace weblayer {

WebLayerSpeechRecognitionManagerDelegate::
    WebLayerSpeechRecognitionManagerDelegate() = default;

WebLayerSpeechRecognitionManagerDelegate::
    ~WebLayerSpeechRecognitionManagerDelegate() = default;

void WebLayerSpeechRecognitionManagerDelegate::OnRecognitionStart(
    int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result) {}

void WebLayerSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id,
    const blink::mojom::SpeechRecognitionError& error) {}

void WebLayerSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id,
    float volume,
    float noise_volume) {}

void WebLayerSpeechRecognitionManagerDelegate::OnRecognitionEnd(
    int session_id) {}

void WebLayerSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const content::SpeechRecognitionSessionContext& context =
      content::SpeechRecognitionManager::GetInstance()->GetSessionContext(
          session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the profile.
  DCHECK_NE(context.render_process_id, 0);

  int render_process_id = context.render_process_id;
  int render_frame_id = context.render_frame_id;
  if (context.embedder_render_process_id) {
    // If this is a request originated from a guest, we need to re-route the
    // permission check through the embedder (app).
    render_process_id = context.embedder_render_process_id;
    render_frame_id = context.embedder_render_frame_id;
  }

  // Check that the render frame type is appropriate, and whether or not we
  // need to request permission from the user.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CheckRenderFrameType, std::move(callback),
                                render_process_id, render_frame_id));
}

content::SpeechRecognitionEventListener*
WebLayerSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

bool WebLayerSpeechRecognitionManagerDelegate::FilterProfanities(
    int render_process_id) {
  // TODO(timvolodine): to confirm how this setting should be used in weblayer.
  // https://crbug.com/1068679.
  return false;
}

// static.
void WebLayerSpeechRecognitionManagerDelegate::CheckRenderFrameType(
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Regular tab contents.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true /* check_permission */,
                     true /* allowed */));
}

}  // namespace weblayer
