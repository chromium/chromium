// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_
#define WEBLAYER_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class BrowsingDataRemoverDelegate : public content::BrowsingDataRemoverDelegate,
                                    public KeyedService {
 public:
  // This is an extension of content::BrowsingDataRemover::RemoveDataMask which
  // includes all datatypes therefrom and adds additional WebLayer-specific
  // ones.
  enum DataType : uint64_t {
    // Embedder can start adding datatypes after the last platform datatype.
    DATA_TYPE_EMBEDDER_BEGIN =
        content::BrowsingDataRemover::DATA_TYPE_CONTENT_END << 1,

    // WebLayer-specific datatypes.
    DATA_TYPE_ISOLATED_ORIGINS = DATA_TYPE_EMBEDDER_BEGIN,
    DATA_TYPE_FAVICONS = DATA_TYPE_EMBEDDER_BEGIN << 1,
    DATA_TYPE_SITE_SETTINGS = DATA_TYPE_EMBEDDER_BEGIN << 2,
    DATA_TYPE_AD_INTERVENTIONS = DATA_TYPE_EMBEDDER_BEGIN << 4,
  };

  explicit BrowsingDataRemoverDelegate(
      content::BrowserContext* browser_context);
  ~BrowsingDataRemoverDelegate() override;

  BrowsingDataRemoverDelegate(const BrowsingDataRemoverDelegate&) = delete;
  BrowsingDataRemoverDelegate& operator=(const BrowsingDataRemoverDelegate&) =
      delete;

  // content::BrowsingDataRemoverDelegate:
  EmbedderOriginTypeMatcher GetOriginTypeMatcher() override;
  bool MayRemoveDownloadHistory() override;
  std::vector<std::string> GetDomainsForDeferredCookieDeletion(
      uint64_t remove_mask) override;
  void RemoveEmbedderData(const base::Time& delete_begin,
                          const base::Time& delete_end,
                          uint64_t remove_mask,
                          content::BrowsingDataFilterBuilder* filter_builder,
                          uint64_t origin_type_mask,
                          base::OnceCallback<void(uint64_t)> callback) override;

 private:
  base::OnceClosure CreateTaskCompletionClosure();

  void OnTaskComplete();

  void RunCallbackIfDone();

  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  int pending_tasks_ = 0;

  // Completion callback to call when all data are deleted.
  base::OnceCallback<void(uint64_t)> callback_;

  base::WeakPtrFactory<BrowsingDataRemoverDelegate> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_
