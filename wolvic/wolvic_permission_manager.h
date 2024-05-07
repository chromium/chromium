// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_PERMISSION_MANAGER_H_
#define WOLVIC_WOLVIC_PERMISSION_MANAGER_H_

#include "absl/types/optional.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"

namespace wolvic {

using PermissionRequestCallback =
    base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>;

// Holds callbacks for in-progress permission requests.
struct InProgressRequest {
  explicit InProgressRequest(
      const content::PermissionRequestDescription& description,
      absl::optional<PermissionRequestCallback> callback = absl::nullopt,
      absl::optional<content::MediaStreamRequest> media_request = absl::nullopt,
      absl::optional<content::MediaResponseCallback> media_callback =
          absl::nullopt);

  ~InProgressRequest();

  content::PermissionRequestDescription description;
  std::vector<PermissionRequestCallback> callbacks;
  absl::optional<std::vector<content::PermissionStatus>> content_results;
  absl::optional<std::vector<content::PermissionStatus>> android_results;

  absl::optional<content::MediaStreamRequest> media_request;
  absl::optional<content::MediaResponseCallback> media_callback;
};

class WolvicPermissionManager : public content::PermissionControllerDelegate {
 public:
  ~WolvicPermissionManager() override;

  static WolvicPermissionManager* GetInstance(bool off_the_record);

  // PermissionControllerDelegate overrides:
  void RequestPermissions(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      PermissionRequestCallback callback) override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      PermissionRequestCallback callback) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& overridden_origin) override;
  SubscriptionId SubscribeToPermissionStatusChange(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribeFromPermissionStatusChange(
      SubscriptionId subscription_id) override;

  // Wolvic specific methods.
  void RequestMediaAccessPermission(content::WebContents* web_contents,
                                    const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback);
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type);

  // Callbacks from Java.
  void OnContentPermissionResult(
      JNIEnv* env,
      InProgressRequest* in_progress_request,
      const std::vector<content::PermissionStatus>& result);
  void OnAndroidPermissionResult(
      InProgressRequest* in_progress_request,
      const std::vector<content::PermissionStatus>& result);
  // Intermediate callback which is called when content and android permissions
  // have been granted but media permission is yet to be requested.
  void OnMediaContentPermissionResult(
      InProgressRequest* in_progress_request,
      const std::vector<content::PermissionStatus>& result);
  void OnMediaPermissionResult(InProgressRequest* in_progress_request,
                               bool granted,
                               const absl::optional<std::string>& video_id,
                               const absl::optional<std::string>& audio_id);

 private:
  explicit WolvicPermissionManager(bool off_the_record);
  void CompleteRequest(InProgressRequest* in_progress_request);
  void RequestContentPermissions(JNIEnv* env,
                                 InProgressRequest* in_progress_request);
  void RequestAndroidPermissions(JNIEnv* env,
                                 InProgressRequest* in_progress_request);

  bool off_the_record_;

  InProgressRequest* FindInProgressRequest(
      const content::PermissionRequestDescription& description);

  std::vector<std::unique_ptr<InProgressRequest>> in_progress_requests_;
  // We cache allowed media permissions for each origin to avoid asking the user
  // whenever |CheckMediaAccessPermission()| is called.
  base::flat_map<GURL, base::flat_set<blink::mojom::MediaStreamType>>
      allowed_media_permissions_cache_;

  base::WeakPtrFactory<WolvicPermissionManager> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_PERMISSION_MANAGER_H_
