// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_PERMISSION_MANAGER_H_
#define WOLVIC_WOLVIC_PERMISSION_MANAGER_H_

#include "base/containers/unique_ptr_adapters.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"

namespace wolvic {

// Holds callbacks for in-progress permission requests.
struct InProgressRequest {
  explicit InProgressRequest(
      base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
          callback);

  ~InProgressRequest();

  base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
      callback;
};

class WolvicPermissionManager : public content::PermissionControllerDelegate {
 public:
  explicit WolvicPermissionManager(content::BrowserContext* browser_context);
  ~WolvicPermissionManager() override;

  // PermissionControllerDelegate overrides:
  void RequestPermissions(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
          callback) override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<content::PermissionStatus>&)>
          callback) override;
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
  SubscriptionId SubscribePermissionStatusChange(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

  void OnPermissionResult(InProgressRequest* in_progress_request,
                          const std::vector<content::PermissionStatus>& result);

 private:
  raw_ptr<content::BrowserContext> browser_context_;

  std::vector<std::unique_ptr<InProgressRequest>> in_progress_requests_;
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_PERMISSION_MANAGER_H_
