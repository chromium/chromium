// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBRTC_MEDIA_STREAM_MANAGER_H_
#define WEBLAYER_BROWSER_WEBRTC_MEDIA_STREAM_MANAGER_H_

#include <map>
#include <set>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {
class WebContents;
}

namespace weblayer {

// On Android, this class tracks active media streams and updates the Java
// object of the same name as streams come and go. The class is created and
// destroyed by the Java object.
//
// When a site requests a new stream, this class passes off the request to
// MediaStreamDevicesController, which handles Android permissions as well as
// per-site permissions. If that succeeds, the request is passed off to the
// embedder by way of MediaCaptureCallback, at which point the response is
// returned in |OnClientReadyToStream|.
class MediaStreamManager {
 public:
  // It's expected that |j_web_contents| outlasts |this|.
  MediaStreamManager(
      const base::android::JavaParamRef<jobject>& j_object,
      const base::android::JavaParamRef<jobject>& j_web_contents);
  MediaStreamManager(const MediaStreamManager&) = delete;
  MediaStreamManager& operator=(const MediaStreamManager&) = delete;
  ~MediaStreamManager();

  static MediaStreamManager* FromWebContents(content::WebContents* contents);

  // Requests media access permission for the tab, if necessary, and runs
  // |callback| as appropriate. This will create a StreamUi.
  void RequestMediaAccessPermission(const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback);

  // The embedder has responded to the stream request.
  void OnClientReadyToStream(JNIEnv* env, int request_id, bool allowed);

  // The embedder has requested all streams be stopped.
  void StopStreaming(JNIEnv* env);

 private:
  class StreamUi;

  void OnMediaAccessPermissionResult(
      content::MediaResponseCallback callback,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      bool blocked_by_permissions_policy,
      ContentSetting audio_setting,
      ContentSetting video_setting);

  void RegisterStream(StreamUi* stream);
  void UnregisterStream(StreamUi* stream);
  void Update();

  std::set<StreamUi*> active_streams_;

  // Represents a user-approved request for which we're waiting on embedder
  // approval.
  struct RequestPendingClientApproval {
    RequestPendingClientApproval();
    RequestPendingClientApproval(
        content::MediaResponseCallback callback,
        const blink::mojom::StreamDevicesSet& stream_devices_set,
        blink::mojom::MediaStreamRequestResult result);
    ~RequestPendingClientApproval();

    RequestPendingClientApproval& operator=(
        RequestPendingClientApproval&& other);

    content::MediaResponseCallback callback;
    blink::mojom::StreamDevicesSetPtr stream_devices_set_;
    blink::mojom::MediaStreamRequestResult result;
  };
  std::map<int, RequestPendingClientApproval> requests_pending_client_approval_;
  int next_request_id_ = 0;

  base::android::ScopedJavaGlobalRef<jobject> j_object_;

  base::WeakPtrFactory<MediaStreamManager> weak_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBRTC_MEDIA_STREAM_MANAGER_H_
