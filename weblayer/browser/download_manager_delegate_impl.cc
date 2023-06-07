// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_manager_delegate_impl.h"

#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "net/base/filename_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/browser/persistent_download.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

namespace {

void GenerateFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& suggested_filename,
    const std::string& mime_type,
    const base::FilePath& suggested_directory,
    base::OnceCallback<void(const base::FilePath&)> callback) {
  base::FilePath generated_name =
      net::GenerateFileName(url, content_disposition, std::string(),
                            suggested_filename, mime_type, "download");

  if (!base::PathExists(suggested_directory))
    base::CreateDirectory(suggested_directory);

  base::FilePath suggested_path(suggested_directory.Append(generated_name));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), suggested_path));
}

}  // namespace

const char kDownloadNextIDPref[] = "weblayer_download_next_id";

DownloadManagerDelegateImpl::DownloadManagerDelegateImpl(
    content::DownloadManager* download_manager)
    : download_manager_(download_manager) {
  download_manager_->AddObserver(this);

  // WebLayer doesn't use a history DB as the in-progress database maintained by
  // the download component is enough. However the download code still depends
  // this notification. TODO(jam): update download code to handle this.
  download_manager_->PostInitialization(
      content::DownloadManager::DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB);
}

DownloadManagerDelegateImpl::~DownloadManagerDelegateImpl() {
  download_manager_->RemoveObserver(this);
  // Match the AddObserver calls added in OnDownloadCreated to avoid UaF.
  download::SimpleDownloadManager::DownloadVector downloads;
  download_manager_->GetAllDownloads(&downloads);
  for (auto* download : downloads)
    download->RemoveObserver(this);
}

void DownloadManagerDelegateImpl::GetNextId(
    content::DownloadIdCallback callback) {
  // Need to return a unique id, even across crashes, to avoid notification
  // intents with different data (e.g. notification GUID) getting dup'd. This is
  // also persisted in the on-disk download database to support resumption.
  auto* local_state = BrowserProcess::GetInstance()->GetLocalState();
  std::move(callback).Run(local_state->GetInteger(kDownloadNextIDPref));
}

bool DownloadManagerDelegateImpl::DetermineDownloadTarget(
    download::DownloadItem* item,
    content::DownloadTargetCallback* callback) {
  if (!item->GetForcedFilePath().empty()) {
    std::move(*callback).Run(
        item->GetForcedFilePath(),
        download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        download::DownloadItem::InsecureDownloadStatus::UNKNOWN,
        item->GetForcedFilePath(), base::FilePath(),
        std::string() /*mime_type*/, download::DOWNLOAD_INTERRUPT_REASON_NONE);
    return true;
  }

  auto filename_determined_callback = base::BindOnce(
      &DownloadManagerDelegateImpl::OnDownloadPathGenerated,
      weak_ptr_factory_.GetWeakPtr(), item->GetId(), std::move(*callback));

  auto* browser_context = content::DownloadItemUtils::GetBrowserContext(item);
  base::FilePath default_download_path;
  GetSaveDir(browser_context, nullptr, &default_download_path);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          GenerateFilename, item->GetURL(), item->GetContentDisposition(),
          item->GetSuggestedFilename(), item->GetMimeType(),
          default_download_path, std::move(filename_determined_callback)));
  return true;
}

bool DownloadManagerDelegateImpl::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    content::WebContents* web_contents) {
  // Don't intercept transient downloads (such as Background Fetches).
  if (is_transient)
    return false;

  // If there's no DownloadDelegate, the download is simply dropped.
  auto* delegate = GetDelegate(web_contents);
  if (!delegate)
    return true;

  return delegate->InterceptDownload(url, user_agent, content_disposition,
                                     mime_type, content_length);
}

void DownloadManagerDelegateImpl::GetSaveDir(
    content::BrowserContext* browser_context,
    base::FilePath* website_save_dir,
    base::FilePath* download_save_dir) {
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(browser_context);
  auto* profile = browser_context_impl->profile_impl();
  if (!profile->download_directory().empty())
    *download_save_dir = profile->download_directory();
}

void DownloadManagerDelegateImpl::CheckDownloadAllowed(
    const content::WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    absl::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    bool content_initiated,
    content::CheckDownloadAllowedCallback check_download_allowed_cb) {
  auto* web_contents = web_contents_getter.Run();
  // If there's no DownloadDelegate, the download is simply dropped.
  auto* delegate = GetDelegate(web_contents);
  auto* tab = TabImpl::FromWebContents(web_contents);
  if (!delegate || !tab) {
    std::move(check_download_allowed_cb).Run(false);
    return;
  }

  delegate->AllowDownload(tab, url, request_method, request_initiator,
                          std::move(check_download_allowed_cb));
}

void DownloadManagerDelegateImpl::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  auto* local_state = BrowserProcess::GetInstance()->GetLocalState();
  int next_id = local_state->GetInteger(kDownloadNextIDPref);
  if (item->GetId() >= static_cast<uint32_t>(next_id)) {
    next_id = item->GetId();
    // Reset the counter when it gets close to max value of unsigned 32 bit
    // integer since that's what the download system persists.
    if (++next_id == (std::numeric_limits<uint32_t>::max() / 2) - 1)
      next_id = 0;
    local_state->SetInteger(kDownloadNextIDPref, next_id);
  }

  if (item->IsTransient())
    return;

  // As per the documentation in DownloadItem, transient items should not
  // be shown in the UI. (Note that they may be surface by other means,
  // such as through the BackgroundFetch system.)
  item->AddObserver(this);
  // Create a PersistentDownload which will be owned by |item|.
  PersistentDownload::Create(item);

  if (item->GetLastReason() == download::DOWNLOAD_INTERRUPT_REASON_CRASH &&
      item->CanResume() &&
      // Don't automatically resume downloads which were previously paused.
      !item->IsPaused()) {
    PersistentDownload::Get(item)->Resume();
  }

  auto* delegate = GetDelegate(item);
  if (delegate)
    delegate->DownloadStarted(PersistentDownload::Get(item));
}

void DownloadManagerDelegateImpl::OnDownloadDropped(
    content::DownloadManager* manager) {
  if (download_dropped_callback_)
    download_dropped_callback_.Run();
}

void DownloadManagerDelegateImpl::OnManagerInitialized() {
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(download_manager_->GetBrowserContext());
  auto* profile = browser_context_impl->profile_impl();
  profile->DownloadsInitialized();
}

void DownloadManagerDelegateImpl::OnDownloadUpdated(
    download::DownloadItem* item) {
  // If this is the first navigation in a tab it should be closed. Wait until
  // the target path is determined or the download is canceled to check.
  if (!item->GetTargetFilePath().empty() ||
      item->GetState() == download::DownloadItem::CANCELLED) {
    content::WebContents* web_contents =
        content::DownloadItemUtils::GetWebContents(item);
    if (web_contents && web_contents->GetController().IsInitialNavigation())
      web_contents->Close();
  }

  auto* delegate = GetDelegate(item);
  if (item->GetState() == download::DownloadItem::COMPLETE ||
      item->GetState() == download::DownloadItem::CANCELLED ||
      item->GetState() == download::DownloadItem::INTERRUPTED) {
    // Stop observing now to ensure we only send one complete/fail notification.
    item->RemoveObserver(this);

    if (item->GetState() == download::DownloadItem::COMPLETE)
      delegate->DownloadCompleted(PersistentDownload::Get(item));
    else
      delegate->DownloadFailed(PersistentDownload::Get(item));

    // Needs to happen asynchronously to avoid nested observer calls.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadManagerDelegateImpl::RemoveItem,
                       weak_ptr_factory_.GetWeakPtr(), item->GetGuid()));
    return;
  }

  if (delegate)
    delegate->DownloadProgressChanged(PersistentDownload::Get(item));
}

void DownloadManagerDelegateImpl::OnDownloadPathGenerated(
    uint32_t download_id,
    content::DownloadTargetCallback callback,
    const base::FilePath& suggested_path) {
  std::move(callback).Run(
      suggested_path, download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      download::DownloadItem::InsecureDownloadStatus::UNKNOWN,
      suggested_path.AddExtension(FILE_PATH_LITERAL(".crdownload")),
      base::FilePath(), std::string() /*mime_type*/,
      download::DOWNLOAD_INTERRUPT_REASON_NONE);
}

void DownloadManagerDelegateImpl::RemoveItem(const std::string& guid) {
  auto* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    item->Remove();
}

DownloadDelegate* DownloadManagerDelegateImpl::GetDelegate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return GetDelegate(web_contents->GetBrowserContext());
}

DownloadDelegate* DownloadManagerDelegateImpl::GetDelegate(
    content::BrowserContext* browser_context) {
  auto* profile = ProfileImpl::FromBrowserContext(browser_context);
  if (!profile)
    return nullptr;

  return profile->download_delegate();
}

DownloadDelegate* DownloadManagerDelegateImpl::GetDelegate(
    download::DownloadItem* item) {
  auto* browser_context = content::DownloadItemUtils::GetBrowserContext(item);
  return GetDelegate(browser_context);
}

}  // namespace weblayer
