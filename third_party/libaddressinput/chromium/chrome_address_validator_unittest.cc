// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_problem.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressProblem;
using ::i18n::addressinput::BuildCallback;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::GetRegionCodes;
using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::TestdataSource;

using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::STREET_ADDRESS;
using ::i18n::addressinput::RECIPIENT;

using ::i18n::addressinput::INVALID_FORMAT;
using ::i18n::addressinput::MISMATCHING_VALUE;
using ::i18n::addressinput::MISSING_REQUIRED_FIELD;
using ::i18n::addressinput::UNEXPECTED_FIELD;
using ::i18n::addressinput::UNKNOWN_VALUE;
using ::i18n::addressinput::UNSUPPORTED_FIELD;
using ::i18n::addressinput::USES_P_O_BOX;

// This class should always succeed in getting the rules.
class AddressValidatorTest : public testing::Test, LoadRulesListener {
 protected:
  AddressValidatorTest()
      : validator_(new AddressValidator(
            std::unique_ptr<Source>(new TestdataSource(true)),
            std::unique_ptr<Storage>(new NullStorage),
            this)) {
    validator_->LoadRules("US");
  }

  void set_expected_status(AddressValidator::Status expected_status) {
    expected_status_ = expected_status;
  }

  virtual ~AddressValidatorTest() {}

  const std::unique_ptr<AddressValidator> validator_;

 private:
  // LoadRulesListener implementation.
  void OnAddressValidationRulesLoaded(const std::string& country_code,
                                      bool success) override {
    AddressData address_data;
    address_data.region_code = country_code;
    FieldProblemMap dummy;
    AddressValidator::Status status =
        validator_->ValidateAddress(address_data, NULL, &dummy);
    ASSERT_EQ(expected_status_, status);
  }

  AddressValidator::Status expected_status_ = AddressValidator::SUCCESS;

  DISALLOW_COPY_AND_ASSIGN(AddressValidatorTest);
};

// Use this test fixture if you're going to use a region with a large set of
// validation rules. All rules should be loaded in SetUpTestCase().
class LargeAddressValidatorTest : public testing::Test {
 protected:
  LargeAddressValidatorTest() {}
  virtual ~LargeAddressValidatorTest() {}

  static void SetUpTestCase() {
    validator_ =
        new AddressValidator(std::unique_ptr<Source>(new TestdataSource(true)),
                             std::unique_ptr<Storage>(new NullStorage), NULL);
    validator_->LoadRules("CN");
    validator_->LoadRules("KR");
    validator_->LoadRules("TW");
  }

  static void TearDownTestcase() {
    delete validator_;
    validator_ = NULL;
  }

  // Owned shared instance of validator with large sets validation rules.
  static AddressValidator* validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LargeAddressValidatorTest);
};

AddressValidator* LargeAddressValidatorTest::validator_ = NULL;

TEST_F(AddressValidatorTest, SubKeysLoaded) {
  const std::string country_code = "US";
  const std::string state_code = "AL";
  const std::string state_name = "Alabama";
  const std::string language = "en";

  validator_->LoadRules(country_code);
  std::vector<std::pair<std::string, std::string>> sub_keys =
      validator_->GetRegionSubKeys(country_code, language);
  ASSERT_FALSE(sub_keys.empty());
  ASSERT_EQ(state_code, sub_keys[0].first);
  ASSERT_EQ(state_name, sub_keys[0].second);
}

TEST_F(AddressValidatorTest, SubKeysLoaded_DefaultLanguage) {
  const std::string country_code = "CA";
  const std::string province_code = "BC";
  const std::string province_name = "British Columbia";
  const std::string language = "en";

  validator_->LoadRules(country_code);
  std::vector<std::pair<std::string, std::string>> sub_keys =
      validator_->GetRegionSubKeys(country_code, language);
  ASSERT_FALSE(sub_keys.empty());
  ASSERT_EQ(province_code, sub_keys[1].first);
  ASSERT_EQ(province_name, sub_keys[1].second);
}

TEST_F(AddressValidatorTest, SubKeysLoaded_NonDefaultLanguage) {
  const std::string country_code = "CA";
  const std::string province_code = "BC";
  const std::string province_name = "Colombie-Britannique";
  const std::string language = "fr";

  validator_->LoadRules(country_code);
  std::vector<std::pair<std::string, std::string>> sub_keys =
      validator_->GetRegionSubKeys(country_code, language);
  ASSERT_FALSE(sub_keys.empty());
  ASSERT_EQ(province_code, sub_keys[1].first);
  ASSERT_EQ(province_name, sub_keys[1].second);
}

TEST_F(AddressValidatorTest, SubKeysLoaded_LanguageNotAvailable) {
  const std::string country_code = "CA";
  const std::string province_code = "BC";
  const std::string province_name = "British Columbia";
  const std::string language = "es";

  validator_->LoadRules(country_code);
  std::vector<std::pair<std::string, std::string>> sub_keys =
      validator_->GetRegionSubKeys(country_code, language);
  ASSERT_FALSE(sub_keys.empty());
  ASSERT_EQ(province_code, sub_keys[1].first);
  ASSERT_EQ(province_name, sub_keys[1].second);
}

TEST_F(AddressValidatorTest, SubKeysLoaded_NamesNotAvailable) {
  const std::string country_code = "ES";
  const std::string province_code = "A Coruña";
  const std::string province_name = "A Coruña";
  const std::string language = "es";

  validator_->LoadRules(country_code);
  std::vector<std::pair<std::string, std::string>> sub_keys =
      validator_->GetRegionSubKeys(country_code, language);
  ASSERT_FALSE(sub_keys.empty());
  ASSERT_EQ(province_code, sub_keys[0].first);
  ASSERT_EQ(province_name, sub_keys[0].second);
}

TEST_F(AddressValidatorTest, SubKeysNotExist) {
  const std::string country_code = "OZ";
  const std::string language = "en";

  set_expected_status(AddressValidator::RULES_UNAVAILABLE);

  validator_->LoadRules(country_code);
  std::vector<std::pair<std::string, std::string>> sub_keys =
      validator_->GetRegionSubKeys(country_code, language);
  ASSERT_TRUE(sub_keys.empty());
}

TEST_F(AddressValidatorTest, RegionHasRules) {
  const std::vector<std::string>& region_codes = GetRegionCodes();
  AddressData address;
  for (size_t i = 0; i < region_codes.size(); ++i) {
    SCOPED_TRACE("For region: " + region_codes[i]);
    validator_->LoadRules(region_codes[i]);
    address.region_code = region_codes[i];
    FieldProblemMap dummy;
    EXPECT_EQ(AddressValidator::SUCCESS,
              validator_->ValidateAddress(address, NULL, &dummy));
  }
}

TEST_F(AddressValidatorTest, EmptyAddressNoFatalFailure) {
  AddressData address;
  address.region_code = "US";

  FieldProblemMap dummy;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &dummy));
}

TEST_F(AddressValidatorTest, UsStateNamesAreValidEntries) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "California";

  FieldProblemMap filter;
  filter.insert(std::make_pair(ADMIN_AREA, UNKNOWN_VALUE));
  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, &filter, &problems));
  EXPECT_TRUE(problems.empty());
}

TEST_F(AddressValidatorTest, USZipCode) {
  AddressData address;
  address.recipient = "Mr. Smith";
  address.address_line.push_back("340 Main St.");
  address.locality = "Venice";
  address.administrative_area = "CA";
  address.region_code = "US";

  // Valid Californian zip code.
  address.postal_code = "90291";
  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));

  FieldProblemMap expected;
  expected.emplace(LOCALITY, UNSUPPORTED_FIELD);
  expected.emplace(DEPENDENT_LOCALITY, UNSUPPORTED_FIELD);
  EXPECT_EQ(expected, problems);
  problems.clear();

  // An extended, valid Californian zip code.
  address.postal_code = "90210-1234";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);
  problems.clear();

  // New York zip code (which is invalid for California).
  address.postal_code = "12345";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  expected.emplace(POSTAL_CODE, MISMATCHING_VALUE);
  EXPECT_EQ(expected, problems);
  problems.clear();

  // A zip code with a "90" in the middle.
  address.postal_code = "12903";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);
  problems.clear();

  // Invalid zip code (too many digits).
  address.postal_code = "902911";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  expected.clear();
  expected.emplace(LOCALITY, UNSUPPORTED_FIELD);
  expected.emplace(DEPENDENT_LOCALITY, UNSUPPORTED_FIELD);
  expected.emplace(POSTAL_CODE, INVALID_FORMAT);
  EXPECT_EQ(expected, problems);
  problems.clear();

  // Invalid zip code (too few digits).
  address.postal_code = "9029";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);
}

TEST_F(AddressValidatorTest, BasicValidation) {
  // US rules should always be available, even though this load call fails.
  validator_->LoadRules("US");
  AddressData address;
  address.region_code = "US";
  address.language_code = "en";
  address.administrative_area = "TX";
  address.locality = "Paris";
  address.postal_code = "75461";
  address.address_line.push_back("123 Main St");
  address.recipient = "Mr. Smith";
  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));

  FieldProblemMap expected;
  expected.emplace(LOCALITY, UNSUPPORTED_FIELD);
  expected.emplace(DEPENDENT_LOCALITY, UNSUPPORTED_FIELD);
  EXPECT_EQ(expected, problems);

  // The display name works as well as the key.
  address.administrative_area = "Texas";
  problems.clear();
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);

  // Ignore capitalization.
  address.administrative_area = "tx";
  problems.clear();
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);

  // Ignore capitalization.
  address.administrative_area = "teXas";
  problems.clear();
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);

  // Ignore diacriticals.
  address.administrative_area = base::WideToUTF8(L"T\u00E9xas");
  problems.clear();
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_EQ(expected, problems);
}

TEST_F(AddressValidatorTest, BasicValidationFailure) {
  // US rules should always be available, even though this load call fails.
  validator_->LoadRules("US");
  AddressData address;
  address.region_code = "US";
  address.language_code = "en";
  address.administrative_area = "XT";
  address.locality = "Paris";
  address.postal_code = "75461";
  address.address_line.push_back("123 Main St");
  address.recipient = "Mr. Smith";
  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(address, NULL, &problems));

  ASSERT_EQ(3U, problems.size());

  FieldProblemMap expected;
  expected.emplace(ADMIN_AREA, UNKNOWN_VALUE);
  expected.emplace(LOCALITY, UNSUPPORTED_FIELD);
  expected.emplace(DEPENDENT_LOCALITY, UNSUPPORTED_FIELD);
  EXPECT_EQ(expected, problems);
}

TEST_F(AddressValidatorTest, NoNullSuggestionsCrash) {
  AddressData address;
  address.region_code = "US";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, COUNTRY, 1, NULL));
}

TEST_F(AddressValidatorTest, SuggestAdminAreaForPostalCode) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90291";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("CA", suggestions[0].administrative_area);
  EXPECT_EQ("90291", suggestions[0].postal_code);
}

TEST_F(LargeAddressValidatorTest, SuggestLocalityForPostalCodeWithAdminArea) {
  AddressData address;
  address.region_code = "TW";
  address.postal_code = "515";
  address.administrative_area = "Changhua";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Dacun Township", suggestions[0].locality);
  EXPECT_EQ("Changhua County", suggestions[0].administrative_area);
  EXPECT_EQ("515", suggestions[0].postal_code);
}

TEST_F(LargeAddressValidatorTest, SuggestAdminAreaForPostalCodeWithLocality) {
  AddressData address;
  address.region_code = "TW";
  address.postal_code = "515";
  address.locality = "Dacun";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Dacun Township", suggestions[0].locality);
  EXPECT_EQ("Changhua County", suggestions[0].administrative_area);
  EXPECT_EQ("515", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, NoSuggestForPostalCodeWithWrongAdminArea) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90066";
  address.postal_code = "TX";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(LargeAddressValidatorTest, SuggestForLocality) {
  AddressData address;
  address.region_code = "CN";
  address.locality = "Anqin";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, LOCALITY, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Anqing Shi", suggestions[0].locality);
  EXPECT_EQ("Anhui Sheng", suggestions[0].administrative_area);
}

TEST_F(LargeAddressValidatorTest, SuggestForLocalityAndAdminArea) {
  AddressData address;
  address.region_code = "CN";
  address.locality = "Anqing";
  address.administrative_area = "Anhui";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, LOCALITY, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_TRUE(suggestions[0].dependent_locality.empty());
  EXPECT_EQ("Anqing Shi", suggestions[0].locality);
  EXPECT_EQ("Anhui Sheng", suggestions[0].administrative_area);
}

TEST_F(LargeAddressValidatorTest, SuggestForAdminAreaAndLocality) {
  AddressData address;
  address.region_code = "CN";
  address.locality = "Anqing";
  address.administrative_area = "Anhui";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_TRUE(suggestions[0].dependent_locality.empty());
  EXPECT_TRUE(suggestions[0].locality.empty());
  EXPECT_EQ("Anhui Sheng", suggestions[0].administrative_area);
}

TEST_F(LargeAddressValidatorTest, SuggestForDependentLocality) {
  AddressData address;
  address.region_code = "CN";
  address.dependent_locality = "Zongyang";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(
                address, DEPENDENT_LOCALITY, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Zongyang Xian", suggestions[0].dependent_locality);
  EXPECT_EQ("Anqing Shi", suggestions[0].locality);
  EXPECT_EQ("Anhui Sheng", suggestions[0].administrative_area);
}

TEST_F(LargeAddressValidatorTest,
       NoSuggestForDependentLocalityWithWrongAdminArea) {
  AddressData address;
  address.region_code = "CN";
  address.dependent_locality = "Zongyang";
  address.administrative_area = "Sichuan Sheng";
  address.language_code = "zh-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(
                address, DEPENDENT_LOCALITY, 10, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, EmptySuggestionsOverLimit) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "A";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 1, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, PreferShortSuggestions) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "CA";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("CA", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestTheSingleMatchForFullMatchName) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "Texas";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Texas", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestAdminArea) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "Cali";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("California", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, MultipleSuggestions) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "MA";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  EXPECT_LT(1U, suggestions.size());

  // Massachusetts should not be a suggestion, because it's already covered
  // under MA.
  std::set<std::string> expected_suggestions;
  expected_suggestions.insert("MA");
  expected_suggestions.insert("Maine");
  expected_suggestions.insert("Marshall Islands");
  expected_suggestions.insert("Maryland");
  for (std::vector<AddressData>::const_iterator it = suggestions.begin();
       it != suggestions.end();
       ++it) {
    expected_suggestions.erase(it->administrative_area);
  }
  EXPECT_TRUE(expected_suggestions.empty());
}

TEST_F(LargeAddressValidatorTest, SuggestNonLatinKeyWhenLanguageMatches) {
  AddressData address;
  address.language_code = "ko";
  address.region_code = "KR";
  address.postal_code = "210-210";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("강원도", suggestions[0].administrative_area);
  EXPECT_EQ("210-210", suggestions[0].postal_code);
}

TEST_F(LargeAddressValidatorTest, SuggestNonLatinKeyWhenUserInputIsNotLatin) {
  AddressData address;
  address.language_code = "en";
  address.region_code = "KR";
  address.administrative_area = "강원";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("강원도", suggestions[0].administrative_area);
}

TEST_F(LargeAddressValidatorTest,
       SuggestLatinNameWhenLanguageDiffersAndLatinNameAvailable) {
  AddressData address;
  address.language_code = "ko-Latn";
  address.region_code = "KR";
  address.postal_code = "210-210";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Gangwon", suggestions[0].administrative_area);
  EXPECT_EQ("210-210", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, NoSuggestionsForEmptyAddress) {
  AddressData address;
  address.region_code = "US";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->GetSuggestions(address, POSTAL_CODE, 999, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, SuggestionIncludesCountry) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90291";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("US", suggestions[0].region_code);
}

TEST_F(AddressValidatorTest, InvalidPostalCodeNoSuggestions) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "0";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->GetSuggestions(address, POSTAL_CODE, 999, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, MismatchedPostalCodeNoSuggestions) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "TX";
  address.postal_code = "90291";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->GetSuggestions(address, POSTAL_CODE, 999, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, SuggestOnlyForAdministrativeAreasAndPostalCode) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "CA";
  address.locality = "Los Angeles";
  address.dependent_locality = "Venice";
  address.postal_code = "90291";
  address.sorting_code = "123";
  address.address_line.push_back("123 Main St");
  address.recipient = "Jon Smith";

  // Fields that should not have suggestions in US.
  static const AddressField kNoSugestFields[] = {
    COUNTRY,
    LOCALITY,
    DEPENDENT_LOCALITY,
    SORTING_CODE,
    STREET_ADDRESS,
    RECIPIENT
  };

  static const size_t kNumNoSuggestFields =
      sizeof kNoSugestFields / sizeof (AddressField);

  for (size_t i = 0; i < kNumNoSuggestFields; ++i) {
    std::vector<AddressData> suggestions;
    EXPECT_EQ(AddressValidator::SUCCESS,
              validator_->GetSuggestions(
                  address, kNoSugestFields[i], 999, &suggestions));
    EXPECT_TRUE(suggestions.empty());
  }
}

TEST_F(AddressValidatorTest, SuggestionsAreCleared) {
  AddressData address;
  address.region_code = "US";

  std::vector<AddressData> suggestions(1, address);
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, NormalizeUsAdminAreaName) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "cALIFORNIa";
  EXPECT_TRUE(validator_->NormalizeAddress(&address));
  EXPECT_EQ("CA", address.administrative_area);
}

TEST_F(AddressValidatorTest, NormalizeUsAdminAreaKey) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "CA";
  EXPECT_TRUE(validator_->NormalizeAddress(&address));
  EXPECT_EQ("CA", address.administrative_area);
}

TEST_F(AddressValidatorTest, NormalizeJpAdminAreaKey) {
  validator_->LoadRules("JP");
  AddressData address;
  address.region_code = "JP";
  address.administrative_area = "東京都";
  EXPECT_TRUE(validator_->NormalizeAddress(&address));
  EXPECT_EQ("東京都", address.administrative_area);
}

TEST_F(AddressValidatorTest, NormalizeJpAdminAreaLatinName) {
  validator_->LoadRules("JP");
  AddressData address;
  address.region_code = "JP";
  address.administrative_area = "tOKYo";
  EXPECT_TRUE(validator_->NormalizeAddress(&address));
  EXPECT_EQ("TOKYO", address.administrative_area);
}

TEST_F(AddressValidatorTest, AreRulesLoadedForRegion_NotLoaded) {
  EXPECT_FALSE(validator_->AreRulesLoadedForRegion("JP"));
}

TEST_F(AddressValidatorTest, AreRulesLoadedForRegion_Loaded) {
  validator_->LoadRules("JP");
  EXPECT_TRUE(validator_->AreRulesLoadedForRegion("JP"));
}

TEST_F(AddressValidatorTest, TokushimaSuggestionIsValid) {
  validator_->LoadRules("JP");
  AddressData address;
  address.region_code = "JP";
  address.administrative_area = "Toku";
  address.language_code = "ja-Latn";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("TOKUSHIMA", suggestions[0].administrative_area);

  FieldProblemMap filter;
  for (int i = UNEXPECTED_FIELD; i <= USES_P_O_BOX; ++i)
    filter.insert(std::make_pair(ADMIN_AREA, static_cast<AddressProblem>(i)));

  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->ValidateAddress(suggestions[0], &filter, &problems));
  EXPECT_TRUE(problems.empty());
}

TEST_F(AddressValidatorTest, ValidPostalCodeInSuggestion) {
  validator_->LoadRules("US");
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "New";
  address.postal_code = "13699";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 999, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("New York", suggestions[0].administrative_area);

  address.administrative_area = "New";
  address.postal_code = "03755";

  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 999, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("New Hampshire", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, ValidateRequiredFieldsWithoutRules) {
  // Do not load the rules for JP.
  AddressData address;
  address.region_code = "JP";

  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::RULES_UNAVAILABLE,
            validator_->ValidateAddress(address, NULL, &problems));
  EXPECT_FALSE(problems.empty());

  for (FieldProblemMap::const_iterator it = problems.begin();
       it != problems.end();
       ++it) {
    EXPECT_EQ(MISSING_REQUIRED_FIELD, it->second);
  }
}

TEST_F(AddressValidatorTest,
       DoNotValidateRequiredFieldsWithoutRulesWhenErrorIsFiltered) {
  // Do not load the rules for JP.
  AddressData address;
  address.region_code = "JP";

  FieldProblemMap filter;
  filter.insert(std::make_pair(COUNTRY, UNKNOWN_VALUE));

  FieldProblemMap problems;
  EXPECT_EQ(AddressValidator::RULES_UNAVAILABLE,
            validator_->ValidateAddress(address, &filter, &problems));
  EXPECT_TRUE(problems.empty());
}

// Use this test fixture for configuring the number of failed attempts to load
// rules.
class FailingAddressValidatorTest : public testing::Test, LoadRulesListener {
 protected:
  // A validator that retries loading rules without delay.
  class TestAddressValidator : public AddressValidator {
   public:
    // Takes ownership of |source| and |storage|.
    TestAddressValidator(std::unique_ptr<::i18n::addressinput::Source> source,
                         std::unique_ptr<::i18n::addressinput::Storage> storage,
                         LoadRulesListener* load_rules_listener)
        : AddressValidator(std::move(source),
                           std::move(storage),
                           load_rules_listener) {}

    virtual ~TestAddressValidator() {}

   protected:
    base::TimeDelta GetBaseRetryPeriod() const override {
      return base::TimeDelta::FromSeconds(0);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestAddressValidator);
  };

  // A source that always fails |failures_number| times before downloading
  // data.
  class FailingSource : public Source {
   public:
    FailingSource()
        : failures_number_(0), attempts_number_(0), actual_source_(true) {}
    virtual ~FailingSource() {}

    // Sets the number of times to fail before downloading data.
    void set_failures_number(int failures_number) {
      failures_number_ = failures_number;
    }

    // Source implementation.
    // Always fails for the first |failures_number| times.
    void Get(const std::string& url, const Callback& callback) const override {
      ++attempts_number_;
      // |callback| takes ownership of the |new std::string|.
      if (failures_number_-- > 0)
        callback(false, url, new std::string);
      else
        actual_source_.Get(url, callback);
    }

    // Returns the number of download attempts.
    int attempts_number() const { return attempts_number_; }

   private:
    // The number of times to fail before downloading data.
    mutable int failures_number_;

    // The number of times Get was called.
    mutable int attempts_number_;

    // The source to use for successful downloads.
    TestdataSource actual_source_;

    DISALLOW_COPY_AND_ASSIGN(FailingSource);
  };

  FailingAddressValidatorTest()
      : source_(new FailingSource),
        validator_(
            new TestAddressValidator(std::unique_ptr<Source>(source_),
                                     std::unique_ptr<Storage>(new NullStorage),
                                     this)),
        load_rules_success_(false) {}

  virtual ~FailingAddressValidatorTest() {}

  FailingSource* source_;  // Owned by |validator_|.
  std::unique_ptr<AddressValidator> validator_;
  bool load_rules_success_;

 private:
  // LoadRulesListener implementation.
  void OnAddressValidationRulesLoaded(const std::string&,
                                      bool success) override {
    load_rules_success_ = success;
  }

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(FailingAddressValidatorTest);
};

// The validator will attempt to load rules at most 8 times.
TEST_F(FailingAddressValidatorTest, RetryLoadingRulesHasLimit) {
  source_->set_failures_number(99);
  validator_->LoadRules("CH");
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(load_rules_success_);
  EXPECT_EQ(8, source_->attempts_number());
}

// The validator will load rules successfully if the source returns data
// before the maximum number of retries.
TEST_F(FailingAddressValidatorTest, RuleRetryingWillSucceed) {
  source_->set_failures_number(4);
  validator_->LoadRules("CH");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(load_rules_success_);
  EXPECT_EQ(5, source_->attempts_number());
}

// The delayed task to retry loading rules should stop (instead of crashing) if
// the validator is destroyed before it fires.
TEST_F(FailingAddressValidatorTest, DestroyedValidatorStopsRetries) {
  source_->set_failures_number(4);
  validator_->LoadRules("CH");

  // Destroy the validator.
  validator_.reset();

  // Fire the delayed task to retry loading rules.
  EXPECT_NO_FATAL_FAILURE(base::RunLoop().RunUntilIdle());
}

// Each call to LoadRules should reset the number of retry attempts. If the
// first call to LoadRules exceeded the maximum number of retries, the second
// call to LoadRules should start counting the retries from zero.
TEST_F(FailingAddressValidatorTest, LoadingRulesSecondTimeSucceeds) {
  source_->set_failures_number(11);
  validator_->LoadRules("CH");
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(load_rules_success_);
  EXPECT_EQ(8, source_->attempts_number());

  validator_->LoadRules("CH");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(load_rules_success_);
  EXPECT_EQ(12, source_->attempts_number());
}

// Calling LoadRules("CH") and LoadRules("GB") simultaneously should attempt to
// load both rules up to the maximum number of attempts for each region.
TEST_F(FailingAddressValidatorTest, RegionsShouldRetryIndividually) {
  source_->set_failures_number(99);
  validator_->LoadRules("CH");
  validator_->LoadRules("GB");
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(load_rules_success_);
  EXPECT_EQ(16, source_->attempts_number());
}

}  // namespace autofill
