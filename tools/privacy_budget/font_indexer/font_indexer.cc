// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/privacy_budget/font_indexer/font_indexer.h"

#include <iostream>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/public/browser/font_list_async.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace privacy_budget {

const std::pair<blink::FontSelectionValue, std::string> kFontWeights[] = {
    {blink::FontSelectionValue(400), ""},
    {blink::FontSelectionValue(700), "bold"},
    {blink::FontSelectionValue(100), "100w"},
    {blink::FontSelectionValue(200), "200w"},
    {blink::FontSelectionValue(300), "300w"},
    {blink::FontSelectionValue(500), "500w"},
    {blink::FontSelectionValue(600), "600w"},
    {blink::FontSelectionValue(800), "800w"},
    {blink::FontSelectionValue(900), "900w"},
    {blink::FontSelectionValue(950), "950w"},
    {blink::FontSelectionValue(1), "1w"},        // min
    {blink::FontSelectionValue(1000), "1000w"},  // max
};
const std::pair<blink::FontSelectionValue, std::string> kFontWidths[] = {
    {blink::FontSelectionValue(100.0f), ""},
    {blink::FontSelectionValue(75), "condensed"},
    {blink::FontSelectionValue(125), "expanded"},
    {blink::FontSelectionValue(62.5f), "extra-condensed"},
    {blink::FontSelectionValue(87.5f), "semi-condensed"},
    {blink::FontSelectionValue(112.5f), "semi-expanded"},
    {blink::FontSelectionValue(150), "extra-expanded"},
    {blink::FontSelectionValue(50), "ultra-condensed"},  // min
    {blink::FontSelectionValue(200), "ultra-expanded"},  // max
};
const std::pair<blink::FontSelectionValue, std::string> kFontSlopes[] = {
    {blink::FontSelectionValue(), ""},
    {blink::FontSelectionValue(20), "italic"},
    {blink::FontSelectionValue(14), "oblique"},
    {blink::FontSelectionValue(1), "1deg"},  // Chosen to search 1 upwards
    {blink::FontSelectionValue(-1), "-1deg"},
    {blink::FontSelectionValue(21), "21deg"},  // Chosen to search 21 upwards
    {blink::FontSelectionValue(-21), "-21deg"},
    {blink::FontSelectionValue(90), "90deg"},    // max
    {blink::FontSelectionValue(-90), "-90deg"},  // min
};
const std::pair<blink::FontSelectionValue, std::string>
    kAdditionalFontSlopes[] = {
        {blink::FontSelectionValue(5), "5deg"},
        {blink::FontSelectionValue(-5), "-5deg"},
        {blink::FontSelectionValue(10), "10deg"},
        {blink::FontSelectionValue(-10), "-10deg"},
        {blink::FontSelectionValue(19), "19deg"},
        {blink::FontSelectionValue(-19), "-19deg"},
        {blink::FontSelectionValue(30), "30deg"},
        {blink::FontSelectionValue(-30), "-30deg"},
        {blink::FontSelectionValue(35), "35deg"},
        {blink::FontSelectionValue(-35), "-35deg"},
        {blink::FontSelectionValue(40), "40deg"},
        {blink::FontSelectionValue(-40), "-40deg"},
        {blink::FontSelectionValue(45), "45deg"},
        {blink::FontSelectionValue(-45), "-45deg"},
        {blink::FontSelectionValue(50), "50deg"},
        {blink::FontSelectionValue(-50), "-50deg"},
        {blink::FontSelectionValue(60), "60deg"},
        {blink::FontSelectionValue(-60), "-60deg"},
        {blink::FontSelectionValue(70), "70deg"},
        {blink::FontSelectionValue(-70), "-70deg"},
        {blink::FontSelectionValue(80), "80deg"},
        {blink::FontSelectionValue(-80), "-80deg"},
};

const char kOutputHeader[] =
    "Family name\tPostScript name\tweight\twidth\tslope\ttypeface "
    "digest\tdefault family name lookup digest\tdefault PostScript name lookup "
    "digest\tPostScript name string digest";
const char kOutputSeparator[] = "\t";

FontIndexer::FontIndexer() : font_cache_(&blink::FontCache::Get()) {}
FontIndexer::~FontIndexer() = default;

void FontIndexer::PrintAllFonts() {
  // Use of base::Unretained is safe as we wait synchronously for the callback.
  content::GetFontListAsync(
      base::BindOnce(&FontIndexer::FontListHasLoaded, base::Unretained(this)));
  WaitForFontListToLoad();
}

void FontIndexer::FontListHasLoaded(base::Value::List list) {
  std::cout << kOutputHeader << std::endl;

  for (const auto& i : list) {
    DCHECK(i.is_list());
    const base::Value::List& font = i.GetList();

    std::string non_localized_name = font[0].GetString();
    PrintAllFontsWithName(WTF::AtomicString(non_localized_name.c_str()));
  }

  has_font_list_loaded_ = true;
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

bool FontIndexer::DoesFontHaveDigest(WTF::AtomicString name,
                                     blink::FontDescription font_description,
                                     int64_t digest) {
  const blink::SimpleFontData* font_data =
      font_cache_->GetFontData(font_description, name);
  DCHECK(font_data);
  return blink::FontGlobalContext::Get()
             .GetOrComputeTypefaceDigest(font_data->PlatformData())
             .ToUkmMetricValue() == digest;
}

bool FontIndexer::DoFontsWithNameHaveVaryingWeights(
    WTF::AtomicString name,
    int64_t default_font_digest) {
  blink::FontDescription font_description;

  font_description.SetWeight(blink::FontSelectionValue(900));  // max for mac
  if (!DoesFontHaveDigest(name, font_description, default_font_digest))
    return true;

  font_description.SetWeight(blink::FontSelectionValue(100));  // min for mac
  return (!DoesFontHaveDigest(name, font_description, default_font_digest));
}

bool FontIndexer::DoFontsWithNameHaveVaryingWidths(
    WTF::AtomicString name,
    int64_t default_font_digest) {
  blink::FontDescription font_description;

  font_description.SetStretch(blink::FontSelectionValue(50));  // min
  if (!DoesFontHaveDigest(name, font_description, default_font_digest))
    return true;

  font_description.SetStretch(blink::FontSelectionValue(200));  // max
  return (!DoesFontHaveDigest(name, font_description, default_font_digest));
}

bool FontIndexer::DoFontsWithNameHaveVaryingSlopes(
    WTF::AtomicString name,
    int64_t default_font_digest) {
  blink::FontDescription font_description;

  font_description.SetStyle(blink::FontSelectionValue(90));  // max
  if (!DoesFontHaveDigest(name, font_description, default_font_digest))
    return true;

  font_description.SetStyle(blink::FontSelectionValue(-90));  // min
  return (!DoesFontHaveDigest(name, font_description, default_font_digest));
}

void FontIndexer::PrintAllFontsWithName(WTF::AtomicString name) {
  WTF::HashSet<int64_t> set_of_digests;

  // First, we load the font with default selection settings to verify any font
  // exists and for later comparison.
  int64_t default_font_digest;
  {
    const blink::SimpleFontData* font_data =
        font_cache_->GetFontData(blink::FontDescription(), name);
    default_font_digest =
        font_data ? blink::FontGlobalContext::Get()
                        .GetOrComputeTypefaceDigest(font_data->PlatformData())
                        .ToUkmMetricValue()
                  : 0;
  }
  if (!default_font_digest) {
    LOG(ERROR) << "No default font loaded for " << name;
    return;
  }

  bool should_vary_weights = true;
  bool should_vary_widths = true;
  bool should_vary_slopes = true;
  if (smart_skipping_) {
    // With smart skipping on, we only test different values along an axis if we
    // think the font varies along that axis.
    should_vary_weights =
        DoFontsWithNameHaveVaryingWeights(name, default_font_digest);
    should_vary_widths =
        DoFontsWithNameHaveVaryingWidths(name, default_font_digest);
    should_vary_slopes =
        DoFontsWithNameHaveVaryingSlopes(name, default_font_digest);
  }

  std::vector<std::pair<blink::FontSelectionValue, std::string>> weights;
  std::vector<std::pair<blink::FontSelectionValue, std::string>> widths;
  std::vector<std::pair<blink::FontSelectionValue, std::string>> slopes;
  if (should_vary_weights) {
    weights.insert(weights.begin(), std::begin(kFontWeights),
                   std::end(kFontWeights));
  } else {
    weights.push_back(kFontWeights[0]);
  }
  if (should_vary_widths) {
    widths.insert(widths.begin(), std::begin(kFontWidths),
                  std::end(kFontWidths));
  } else {
    widths.push_back(kFontWidths[0]);
  }
  if (should_vary_slopes) {
    slopes.insert(slopes.begin(), std::begin(kFontSlopes),
                  std::end(kFontSlopes));
    if (more_slope_checks_) {
      slopes.insert(slopes.end(), std::begin(kAdditionalFontSlopes),
                    std::end(kAdditionalFontSlopes));
    }
  } else {
    slopes.push_back(kFontSlopes[0]);
  }

  blink::FontDescription font_description;
  for (auto weight_pair : weights) {
    font_description.SetWeight(weight_pair.first);
    for (auto width_pair : widths) {
      font_description.SetStretch(width_pair.first);
      for (auto slope_pair : slopes) {
        font_description.SetStyle(slope_pair.first);

        if (const blink::SimpleFontData* font_data =
                font_cache_->GetFontData(font_description, name)) {
          uint64_t typeface_digest =
              blink::FontGlobalContext::Get()
                  .GetOrComputeTypefaceDigest(font_data->PlatformData())
                  .ToUkmMetricValue();
          if (set_of_digests.insert(typeface_digest).is_new_entry) {
            WTF::String postscript_name =
                font_data->PlatformData().GetPostScriptName();

            // Matches behavior in FontMatchingMetrics for lookups using the
            // family name and the PostScript name, respectively, with default
            // FontSelectionRequests.
            uint64_t default_family_name_lookup_digest;
            {
              blink::IdentifiableTokenBuilder builder;
              builder.AddValue(
                  blink::FontDescription().GetFontSelectionRequest().GetHash());
              builder.AddToken(
                  blink::IdentifiabilityBenignCaseFoldingStringToken(name));
              default_family_name_lookup_digest =
                  builder.GetToken().ToUkmMetricValue();
            }
            uint64_t default_postscript_name_lookup_digest;
            {
              blink::IdentifiableTokenBuilder builder;
              builder.AddValue(
                  blink::FontDescription().GetFontSelectionRequest().GetHash());
              builder.AddToken(
                  blink::IdentifiabilityBenignCaseFoldingStringToken(
                      postscript_name));
              default_postscript_name_lookup_digest =
                  builder.GetToken().ToUkmMetricValue();
            }
            uint64_t postscript_name_string_digest =
                blink::IdentifiabilityBenignStringToken(postscript_name)
                    .ToUkmMetricValue();

            std::cout << name.Ascii() << kOutputSeparator
                      << postscript_name.Ascii() << kOutputSeparator
                      << weight_pair.second << kOutputSeparator
                      << width_pair.second << kOutputSeparator
                      << slope_pair.second << kOutputSeparator
                      << typeface_digest << kOutputSeparator
                      << default_family_name_lookup_digest << kOutputSeparator
                      << default_postscript_name_lookup_digest
                      << kOutputSeparator << postscript_name_string_digest
                      << std::endl;
          }
        }
      }
    }
  }
}

void FontIndexer::WaitForFontListToLoad() {
  if (has_font_list_loaded_)
    return;
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace privacy_budget
