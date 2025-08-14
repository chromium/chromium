// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_SEARCH_ENGINES_DATA_BUILT_IN_MARKETING_SNIPPETS_H_
#define THIRD_PARTY_SEARCH_ENGINES_DATA_BUILT_IN_MARKETING_SNIPPETS_H_

#include <string>

#include "build/build_config.h"

namespace search_engines_data {

#if !BUILDFLAG(IS_ANDROID)
// Returns the engine marketing snippet string resource id or -1 if the snippet
// was not found.
// The function definition is generated in `resources_to_move_out/built_in_marketing_snippets.cc`.
// `engine_keyword` is the search engine keyword.
int GetMarketingSnippetResourceId(const std::u16string& engine_keyword);
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace search_engines_data

#endif  // THIRD_PARTY_SEARCH_ENGINES_DATA_BUILT_IN_MARKETING_SNIPPETS_H_
