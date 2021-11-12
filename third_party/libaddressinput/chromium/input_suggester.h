// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_INPUT_SUGGESTER_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_INPUT_SUGGESTER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "third_party/icu/source/i18n/unicode/coll.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_input_helper.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data_builder.h"

namespace i18n {
namespace addressinput {
class PreloadSupplier;
class RegionData;
struct AddressData;
}
}

namespace autofill {

// Suggests address completions for a partially entered address from the user.
class InputSuggester {
 public:
  // Does not take ownership of |supplier|, which should not be NULL.
  explicit InputSuggester(::i18n::addressinput::PreloadSupplier* supplier);

  InputSuggester(const InputSuggester&) = delete;
  InputSuggester& operator=(const InputSuggester&) = delete;

  ~InputSuggester();

  // Fills in |suggestions| for the partially typed in |user_input|, assuming
  // the user is typing in the |focused_field|. If the number of |suggestions|
  // is over the |suggestion_limit|, then returns no |suggestions| at all.
  //
  // Sample user input 1:
  //   country code = "US"
  //   postal code = "90066"
  //   focused field = POSTAL_CODE
  //   suggestions limit = 1
  // Suggestion:
  //   [{administrative_area: "CA"}]
  //
  // Sample user input 2:
  //   country code = "CN"
  //   dependent locality = "Zongyang"
  //   focused field = DEPENDENT_LOCALITY
  //   suggestions limit = 10
  // Suggestion:
  //   [{dependent_locality: "Zongyang Xian",
  //     locality: "Anqing Shi",
  //     administrative_area: "Anhui Sheng"}]
  //
  // Builds the index for generating suggestions lazily.
  //
  // The |suggestions| parameter should not be NULL. The |focused_field|
  // parameter should be either POSTAL_CODE or between ADMIN_AREA and
  // DEPENDENT_LOCALITY inclusively.
  void GetSuggestions(
      const ::i18n::addressinput::AddressData& user_input,
      ::i18n::addressinput::AddressField focused_field,
      size_t suggestion_limit,
      std::vector< ::i18n::addressinput::AddressData>* suggestions);

 private:
  class SubRegionData;

  // Canonicalizes strings for case and diacritic insensitive comparison.
  class StringCanonicalizer {
   public:
    // Initializes the canonicalizer. This is slow, so avoid calling it more
    // often than necessary.
    StringCanonicalizer();

    StringCanonicalizer(const StringCanonicalizer&) = delete;
    StringCanonicalizer& operator=(const StringCanonicalizer&) = delete;

    ~StringCanonicalizer();

    // Returns a 0-terminated canonical version of the string that can be used
    // for comparing strings regardless of diacritics and capitalization.
    //    Canonicalize("Texas") == Canonicalize("T\u00E9xas");
    //    Canonicalize("Texas") == Canonicalize("teXas");
    //    Canonicalize("Texas") != Canonicalize("California");
    //
    // The output is not human-readable.
    //    Canonicalize("Texas") != "Texas";
    //
    // The |original| parameter should not be empty.
    const std::vector<uint8_t>& Canonicalize(const std::string& original) const;

   private:
    int32_t buffer_size() const;

    mutable std::vector<uint8_t> buffer_;
    std::unique_ptr<icu::Collator> collator_;
  };

  // The method to be invoked by |validated_| callback.
  void Validated(bool success,
                 const ::i18n::addressinput::AddressData&,
                 const ::i18n::addressinput::FieldProblemMap&);

  // Data source for region data.
  ::i18n::addressinput::RegionDataBuilder region_data_builder_;

  // Suggests sub-regions based on postal code.
  const ::i18n::addressinput::AddressInputHelper input_helper_;

  // Verifies that suggested sub-regions match the postal code.
  ::i18n::addressinput::AddressValidator validator_;

  // The callback for |validator_| to invoke when validation finishes.
  const std::unique_ptr<const ::i18n::addressinput::AddressValidator::Callback>
      validated_;

  // A mapping from a COUNTRY level region to a collection of all of its
  // sub-regions along with metadata used to construct suggestions.
  std::map<const ::i18n::addressinput::RegionData*, SubRegionData> sub_regions_;

  // Canonicalizes strings for case and diacritic insensitive search of
  // sub-region names.
  StringCanonicalizer canonicalizer_;
};

}  // namespace autofill

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_INPUT_SUGGESTER_H_
