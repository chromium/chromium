// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/metadata_cache.h"

#include <algorithm>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "ui/views/metadata/metadata_types.h"

namespace views {
namespace metadata {

MetaDataCache::MetaDataCache() = default;
MetaDataCache::~MetaDataCache() = default;

// static
MetaDataCache* MetaDataCache::GetInstance() {
  static base::NoDestructor<MetaDataCache> instance;
  return instance.get();
}

void MetaDataCache::AddClassMetaData(
    std::unique_ptr<ClassMetaData> class_data) {
#if DCHECK_IS_ON()
  const std::vector<ClassMetaData*>::const_reverse_iterator existing_data =
      std::find_if(class_data_cache_.rbegin(), class_data_cache_.rend(),
                   [&class_data](ClassMetaData* comp_data) {
                     return comp_data->type_name() == class_data->type_name();
                   });

  DCHECK(existing_data == class_data_cache_.rend());
#endif

  class_data_cache_.push_back(class_data.release());
}

std::vector<ClassMetaData*>& MetaDataCache::GetCachedTypes() {
  return class_data_cache_;
}

void RegisterClassInfo(std::unique_ptr<ClassMetaData> meta_data) {
  MetaDataCache* cache = MetaDataCache::GetInstance();
  cache->AddClassMetaData(std::move(meta_data));
}

}  // namespace metadata
}  // namespace views
