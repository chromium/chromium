// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/metadata/metadata_cache.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ui/base/metadata/metadata_types.h"

namespace ui {
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
  DCHECK(!base::Contains(class_data_cache_, class_data->GetUniqueName(),
                         &ClassMetaData::GetUniqueName));
  class_data_cache_.push_back(class_data.release());
}

std::vector<raw_ptr<ClassMetaData, VectorExperimental>>&
MetaDataCache::GetCachedTypes() {
  return class_data_cache_;
}

void RegisterClassInfo(std::unique_ptr<ClassMetaData> meta_data) {
  MetaDataCache* cache = MetaDataCache::GetInstance();
  cache->AddClassMetaData(std::move(meta_data));
}

}  // namespace metadata
}  // namespace ui
