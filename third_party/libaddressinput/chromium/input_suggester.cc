// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/input_suggester.h"

#include <cstddef>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "third_party/libaddressinput/chromium/trie.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/callback.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/preload_supplier.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"

namespace autofill {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::BuildCallback;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::PreloadSupplier;
using ::i18n::addressinput::RegionData;
using ::i18n::addressinput::RegionDataBuilder;

using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::POSTAL_CODE;

using ::i18n::addressinput::INVALID_FORMAT;
using ::i18n::addressinput::MISMATCHING_VALUE;

namespace {

// Initial size for the buffer used in the canonicalizer.
static const size_t kInitialBufferSize = 32;

// A region and its metadata useful for constructing a suggestion.
struct Suggestion {
 public:
  // Builds a suggestion of |region_to_suggest|. Does not take ownership of
  // |region_to_suggest|, which should not be NULL.
  Suggestion(const RegionData* region_to_suggest,
             AddressField matching_address_field,
             bool region_key_matches)
      : region_to_suggest(region_to_suggest),
        matching_address_field(matching_address_field),
        region_key_matches(region_key_matches) {
    DCHECK(region_to_suggest);
  }

  ~Suggestion() {}

  // The region that should be suggested. For example, if the region is ("CA",
  // "California"), then either "CA" or "California" should be suggested.
  const RegionData* region_to_suggest;

  // The field in the address for which the suggestion should be made. For
  // example, ADMIN_AREA in US means the suggestion should be made for the field
  // labeled "State".
  AddressField matching_address_field;

  // True if the key of the region matches user input (the name may or may not
  // match). "CA" should be suggested for a ("CA", "California") region.
  //
  // False if only the name of the region matches user input (the key does not
  // match). "California" should be suggested for a ("CA", "California") region.
  bool region_key_matches;
};

// Suggestions for an address. Contains lists of suggestions for administrative
// area, locality, and dependent locality fields of an address.
class AddressSuggestions {
 public:
  AddressSuggestions() {}
  ~AddressSuggestions() {}

  // Marks all regions at |address_field| level as matching user input.
  void AllRegionsMatchForField(AddressField address_field) {
    all_regions_match_input_.insert(address_field);
  }

  // Marks given regions at |address_field| level as matching user input. The
  // |regions_match_key| parameter contains the regions that match user input by
  // their keys. The |regions_match_name| parameter contains the regions that
  // match user input by their names.
  //
  // The |address_field| parameter should be either ADMIN_AREA, LOCALITY, or
  // DEPENDENT_LOCALITY.
  bool AddRegions(AddressField address_field,
                  const std::set<const RegionData*>& regions_match_key,
                  const std::set<const RegionData*>& regions_match_name) {
    DCHECK(address_field >= ADMIN_AREA);
    DCHECK(address_field <= DEPENDENT_LOCALITY);

    AddressField parent_address_field =
        static_cast<AddressField>(address_field - 1);

    bool all_parents_match =
        parent_address_field == COUNTRY ||
        all_regions_match_input_.find(parent_address_field) !=
            all_regions_match_input_.end();

    // Cannot build |address_field| level suggestions if there are no matches in
    // |parent_address_field| level regions.
    const RegionsMatchInput* parents = NULL;
    if (address_field > ADMIN_AREA && !all_parents_match) {
      parents = &regions_match_input_[parent_address_field];
      if (parents->keys.empty() && parents->names.empty())
        return false;
    }

    RegionsMatchInput* regions = NULL;
    if (address_field < DEPENDENT_LOCALITY)
      regions = &regions_match_input_[address_field];

    std::vector<Suggestion>* suggestions = &suggestions_[address_field];
    bool added_suggestions = false;

    // Iterate over both |regions_match_key| and |regions_match_name| and build
    // Suggestion objects based on the given RegionData objects. Advance either
    // one iterator at a time (if they point to different data) or both
    // iterators at once (if they point to the same data).
    for (std::set<const RegionData*>::const_iterator
             key_it = regions_match_key.begin(),
             name_it = regions_match_name.begin();
         key_it != regions_match_key.end() ||
             name_it != regions_match_name.end();) {
      const RegionData* key_region =
          key_it != regions_match_key.end() ? *key_it : NULL;
      const RegionData* name_region =
          name_it != regions_match_name.end() ? *name_it : NULL;

      // Regions that do not have a parent that also matches input will not
      // become suggestions.
      bool key_region_has_parent =
          all_parents_match ||
          (parents && !parents->keys.empty() && key_region &&
           parents->keys.find(&key_region->parent()) != parents->keys.end());
      bool name_region_has_parent =
          all_parents_match ||
          (parents && !parents->names.empty() && name_region &&
           parents->names.find(&name_region->parent()) != parents->names.end());

      if (name_region && (!key_region || name_region < key_region)) {
        if (name_region_has_parent) {
          suggestions->push_back(Suggestion(name_region, address_field, false));
          added_suggestions = true;
          if (regions)
            regions->names.insert(name_region);
        }

        ++name_it;
      } else if (key_region && (!name_region || key_region < name_region)) {
        if (key_region_has_parent) {
          suggestions->push_back(Suggestion(key_region, address_field, true));
          added_suggestions = true;
          if (regions)
            regions->keys.insert(key_region);
        }

        ++key_it;
      } else {
        if (key_region_has_parent) {
          suggestions->push_back(Suggestion(key_region, address_field, true));
          added_suggestions = true;
          if (regions) {
            regions->keys.insert(key_region);
            regions->names.insert(name_region);
          }
        }

        ++key_it;
        ++name_it;
      }
    }

    return added_suggestions;
  }

  // Swaps the suggestions for the smallest sub-region into |suggestions|.
  // |this| is not usable after this call due to using the swap() operation.
  //
  // The |suggestions| parameter should not be NULL.
  void SwapSmallestSubRegionSuggestions(std::vector<Suggestion>* suggestions) {
    DCHECK(suggestions);
    for (int i = DEPENDENT_LOCALITY; i >= ADMIN_AREA; --i) {
      std::vector<Suggestion>* result =
          &suggestions_[static_cast<AddressField>(i)];
      if (!result->empty()) {
        suggestions->swap(*result);
        return;
      }
    }
  }

 private:
  // The sets of non-owned regions used for looking up regions that match user
  // input by keys and names.
  struct RegionsMatchInput {
    std::set<const RegionData*> keys;
    std::set<const RegionData*> names;
  };

  // The regions that match user input at ADMIN_AREA and LOCALITY levels.
  std::map<AddressField, RegionsMatchInput> regions_match_input_;

  // The set of fields for which all regions match user input. Used to avoid
  // storing a long list in |regions_match_input_| and later looking it up
  // there.
  std::set<AddressField> all_regions_match_input_;

  // Suggestions at ADMIN_AREA, LOCALITY, and DEPENDENT_LOCALITY levels.
  std::map<AddressField, std::vector<Suggestion> > suggestions_;

  DISALLOW_COPY_AND_ASSIGN(AddressSuggestions);
};

}  // namespace

InputSuggester::StringCanonicalizer::StringCanonicalizer()
    : buffer_(kInitialBufferSize, 0) {
  UErrorCode error_code = U_ZERO_ERROR;
  collator_.reset(
      icu::Collator::createInstance(icu::Locale::getRoot(), error_code));
  if (!collator_ || !U_SUCCESS(error_code)) {
    // On some systems, the default locale is invalid to the eyes of the ICU
    // library. This could be due to a device-specific issue (has been seen in
    // the wild on Android and iOS devices). In the failure case, |collator_|
    // will be null. See http://crbug.com/558625.

    // Attempt to load the English locale.
    error_code = U_ZERO_ERROR;
    collator_.reset(
        icu::Collator::createInstance(icu::Locale::getEnglish(), error_code));
    if (!collator_) {
      LOG(ERROR) << "Failed to initialize the ICU Collator with the English "
                 << "locale.";
    }
  }

  DCHECK(U_SUCCESS(error_code));
  if (collator_ && U_SUCCESS(error_code))
    collator_->setStrength(icu::Collator::PRIMARY);
}

InputSuggester::StringCanonicalizer::~StringCanonicalizer() {}

const std::vector<uint8_t>& InputSuggester::StringCanonicalizer::Canonicalize(
    const std::string& original) const {
  DCHECK(!original.empty());

  icu::UnicodeString icu_str(original.c_str(),
                             static_cast<int32_t>(original.length()));
  int32_t sort_key_size = 0;
  if (collator_)
    collator_->getSortKey(icu_str, &buffer_[0], buffer_size());
  DCHECK_LT(0, sort_key_size);

  if (sort_key_size > buffer_size()) {
    buffer_.resize(sort_key_size * 2, 0);
    sort_key_size = collator_->getSortKey(icu_str, &buffer_[0], buffer_size());
    DCHECK_LT(0, sort_key_size);
    DCHECK_GT(buffer_size(), sort_key_size);
  }

  return buffer_;
}

int32_t InputSuggester::StringCanonicalizer::buffer_size() const {
  return static_cast<int32_t>(buffer_.size());
}

// All sub-regions of a COUNTRY level region, organized into tries for lookup by
// region name or key.
class InputSuggester::SubRegionData {
 public:
  SubRegionData()
      : initialized_(false),
        smallest_region_size_(COUNTRY),
        canonicalizer_(NULL) {}

  ~SubRegionData() {}

  bool is_initialized() const { return initialized_; }

  // Adds the sub-regions of |country_region| into tries. Uses
  // |shared_canonicalizer| for case and diacritic insensitive lookup of the
  // sub-regions. Should be called at most once.
  void Initialize(const RegionData& country_region,
                  const StringCanonicalizer& shared_canonicalizer) {
    DCHECK(!initialized_);
    DCHECK(!country_region.has_parent());

    initialized_ = true;
    canonicalizer_ = &shared_canonicalizer;

    if (!country_region.sub_regions().empty())
      AddSubRegionsOf(country_region, COUNTRY);
  }

  // Adds the suggestions for |user_input| into |suggestions| when user is
  // typing in |focused_field|.
  void BuildSuggestions(const AddressData& user_input,
                        AddressField focused_field,
                        std::vector<Suggestion>* suggestions) {
    DCHECK(initialized_);

    // Do not suggest anything if there's no suggestion data for the focused
    // field.
    if (focused_field != POSTAL_CODE && smallest_region_size_ < focused_field)
      return;

    // Non-owned regions that match a field value by region key.
    std::set<const RegionData*> regions_match_key;

    // Non-owned regions that match a field value by region name.
    std::set<const RegionData*> regions_match_name;

    AddressSuggestions address_suggestions;
    for (int i = ADMIN_AREA; i <= focused_field && i <= DEPENDENT_LOCALITY;
         ++i) {
      AddressField address_field = static_cast<AddressField>(i);
      AddressField parent_address_field = static_cast<AddressField>(i - 1);

      const std::string& field_value = user_input.GetFieldValue(address_field);
      const std::string& parent_field_value =
          user_input.GetFieldValue(parent_address_field);

      if (field_value.empty() &&
          (address_field == ADMIN_AREA || parent_field_value.empty())) {
        address_suggestions.AllRegionsMatchForField(address_field);
        continue;
      }

      if (field_value.empty()) {
        DCHECK_NE(address_field, focused_field);
        continue;
      }

      regions_match_key.clear();
      regions_match_name.clear();

      const FieldTries& field_tries = field_tries_[address_field];

      const std::vector<uint8_t>& canonicalized_value =
          canonicalizer_->Canonicalize(field_value);

      field_tries.keys.FindDataForKeyPrefix(canonicalized_value,
                                            &regions_match_key);
      field_tries.names.FindDataForKeyPrefix(canonicalized_value,
                                             &regions_match_name);

      bool added_suggestions = address_suggestions.AddRegions(
          address_field, regions_match_key, regions_match_name);

      // Do not suggest anything if the focused field does not have suggestions.
      if (address_field == focused_field && !added_suggestions)
        return;
    }

    address_suggestions.SwapSmallestSubRegionSuggestions(suggestions);
  }

 private:
  // The tries to lookup regions for a specific field by keys and names. For
  // example, the FieldTries for ADMIN_AREA in US will have keys for "AL", "AK",
  // "AS", etc and names for "Alabama", "Alaska", "American Samoa", etc. The
  // struct is uncopyable due to Trie objects being uncopyable.
  struct FieldTries {
    Trie<const RegionData*> keys;
    Trie<const RegionData*> names;
  };

  // Adds the sub-regions of |parent_region| into tries.
  void AddSubRegionsOf(const RegionData& parent_region,
                       AddressField parent_field) {
    DCHECK(!parent_region.sub_regions().empty());

    AddressField address_field = static_cast<AddressField>(parent_field + 1);
    DCHECK(address_field >= ADMIN_AREA);
    DCHECK(address_field <= DEPENDENT_LOCALITY);

    FieldTries* field_tries = &field_tries_[address_field];
    for (std::vector<const RegionData*>::const_iterator it =
             parent_region.sub_regions().begin();
         it != parent_region.sub_regions().end();
         ++it) {
      const RegionData* region = *it;
      DCHECK(region);
      DCHECK(!region->key().empty());
      DCHECK(!region->name().empty());

      field_tries->keys.AddDataForKey(
          canonicalizer_->Canonicalize(region->key()), region);

      field_tries->names.AddDataForKey(
          canonicalizer_->Canonicalize(region->name()), region);

      if (smallest_region_size_ < address_field)
        smallest_region_size_ = address_field;

      if (!region->sub_regions().empty())
        AddSubRegionsOf(*region, address_field);
    }
  }

  // True after Initialize() has been called.
  bool initialized_;

  // The tries to lookup regions for ADMIN_AREA, LOCALITY, and
  // DEPENDENT_LOCALITY.
  std::map<AddressField, FieldTries> field_tries_;

  // The smallest size of a sub-region that has data. For example, this is
  // ADMIN_AREA in US, but DEPENDENT_LOCALITY in CN.
  AddressField smallest_region_size_;

  // A shared instance of string canonicalizer for case and diacritic comparison
  // of region keys and names.
  const StringCanonicalizer* canonicalizer_;
};

InputSuggester::InputSuggester(PreloadSupplier* supplier)
    : region_data_builder_(supplier),
      input_helper_(supplier),
      validator_(supplier),
      validated_(BuildCallback(this, &InputSuggester::Validated)) {}

InputSuggester::~InputSuggester() {}

void InputSuggester::GetSuggestions(const AddressData& user_input,
                                    AddressField focused_field,
                                    size_t suggestions_limit,
                                    std::vector<AddressData>* suggestions) {
  DCHECK(suggestions);
  DCHECK(focused_field == POSTAL_CODE ||
         (focused_field >= ADMIN_AREA && focused_field <= DEPENDENT_LOCALITY));

  AddressData address_copy = user_input;

  // Do not suggest anything if the user input is empty.
  if (address_copy.IsFieldEmpty(focused_field))
    return;

  if (focused_field == POSTAL_CODE) {
    // Do not suggest anything if the user is typing an invalid postal code.
    FieldProblemMap problems;
    FieldProblemMap filter;
    filter.insert(std::make_pair(POSTAL_CODE, INVALID_FORMAT));
    validator_.Validate(address_copy,
                        true,   // Allow postal office boxes.
                        false,  // Do not require recipient name.
                        &filter,
                        &problems,
                        *validated_);
    if (!problems.empty())
      return;

    // Fill in the sub-regions based on the postal code.
    input_helper_.FillAddress(&address_copy);
  }

  // Lazily initialize the mapping from COUNTRY level regions to all of their
  // sub-regions with metadata for generating suggestions.
  std::string unused_best_language;
  const RegionData& region_data =
      region_data_builder_.Build(address_copy.region_code,
                                 address_copy.language_code,
                                 &unused_best_language);
  SubRegionData* sub_region_data = &sub_regions_[&region_data];
  if (!sub_region_data->is_initialized())
    sub_region_data->Initialize(region_data, canonicalizer_);

  // Build the list of regions that match |address_copy| when the user is typing
  // in the |focused_field|.
  std::vector<Suggestion> suggested_regions;
  sub_region_data->BuildSuggestions(
      address_copy, focused_field, &suggested_regions);

  FieldProblemMap problems;
  FieldProblemMap filter;
  filter.insert(std::make_pair(POSTAL_CODE, MISMATCHING_VALUE));

  // Generate suggestions based on the regions.
  for (std::vector<Suggestion>::const_iterator suggested_region_it =
           suggested_regions.begin();
       suggested_region_it != suggested_regions.end();
       ++suggested_region_it) {
    AddressData address;
    address.region_code = address_copy.region_code;
    address.postal_code = address_copy.postal_code;

    // Traverse the tree of regions from the smallest |region_to_suggest| to the
    // country-wide "root" of the tree. Use the region names or keys found at
    // each of the levels of the tree to build the |address| to suggest.
    AddressField address_field = suggested_region_it->matching_address_field;
    for (const RegionData* region = suggested_region_it->region_to_suggest;
         region->has_parent();
         region = &region->parent()) {
      address.SetFieldValue(address_field,
                            suggested_region_it->region_key_matches
                                ? region->key()
                                : region->name());
      address_field = static_cast<AddressField>(address_field - 1);
    }

    // Do not suggest an address with a mismatching postal code.
    problems.clear();
    validator_.Validate(address,
                        true,   // Allow postal office boxes.
                        false,  // Do not require recipient name.
                        &filter,
                        &problems,
                        *validated_);
    if (!problems.empty())
      continue;

    // Do not add more suggestions than |suggestions_limit|.
    if (suggestions->size() >= suggestions_limit) {
      suggestions->clear();
      return;
    }

    suggestions->push_back(address);
  }
}

void InputSuggester::Validated(bool success,
                               const AddressData&,
                               const FieldProblemMap&) {
  DCHECK(success);
}

}  // namespace autofill
