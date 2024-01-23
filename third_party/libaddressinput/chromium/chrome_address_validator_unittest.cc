// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
 public:
  AddressValidatorTest(const AddressValidatorTest&) = delete;
  AddressValidatorTest& operator=(const AddressValidatorTest&) = delete;

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
};

// Use this test fixture if you're going to use a region with a large set of
// validation rules. All rules should be loaded in SetUpTestCase().
class LargeAddressValidatorTest : public testing::Test {
 public:
  LargeAddressValidatorTest(const LargeAddressValidatorTest&) = delete;
  LargeAddressValidatorTest& operator=(const LargeAddressValidatorTest&) =
      delete;

 protected:
  LargeAddressValidatorTest() {}
  virtual ~LargeAddressValidatorTest() {}

  static void SetUpTestSuite() {
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
 public:
  FailingAddressValidatorTest(const FailingAddressValidatorTest&) = delete;
  FailingAddressValidatorTest& operator=(const FailingAddressValidatorTest&) =
      delete;

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

    TestAddressValidator(const TestAddressValidator&) = delete;
    TestAddressValidator& operator=(const TestAddressValidator&) = delete;

    virtual ~TestAddressValidator() {}

   protected:
    base::TimeDelta GetBaseRetryPeriod() const override {
      return base::Seconds(0);
    }
  };

  // A source that always fails |failures_number| times before downloading
  // data.
  class FailingSource : public Source {
   public:
    FailingSource()
        : failures_number_(0), attempts_number_(0), actual_source_(true) {}

    FailingSource(const FailingSource&) = delete;
    FailingSource& operator=(const FailingSource&) = delete;

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
