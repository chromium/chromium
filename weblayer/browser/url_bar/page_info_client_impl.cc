// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/url_bar/page_info_client_impl.h"

#include "content/public/browser/web_contents.h"
#include "weblayer/browser/android/resource_mapper.h"
#include "weblayer/browser/url_bar/page_info_delegate_impl.h"

namespace weblayer {

// static
PageInfoClientImpl* PageInfoClientImpl::GetInstance() {
  return new PageInfoClientImpl();
}

std::unique_ptr<PageInfoDelegate> PageInfoClientImpl::CreatePageInfoDelegate(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return std::make_unique<PageInfoDelegateImpl>(web_contents);
}

int PageInfoClientImpl::GetJavaResourceId(int native_resource_id) {
  return weblayer::MapToJavaDrawableId(native_resource_id);
}

}  // namespace weblayer
