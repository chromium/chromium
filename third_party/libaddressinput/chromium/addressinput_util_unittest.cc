// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/addressinput_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"

namespace autofill {
namespace addressinput {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressProblem;

TEST(AddressinputUtilTest, AddressRequiresRegionCode) {
  AddressData address;
  EXPECT_FALSE(HasAllRequiredFields(address));
}

TEST(AddressinputUtilTest, UsRequiresState) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90291";
  // Leave state empty.
  address.locality = "Los Angeles";
  address.address_line.push_back("340 Main St.");
  EXPECT_FALSE(HasAllRequiredFields(address));
}

TEST(AddressinputUtilTest, CompleteAddressReturnsTrue) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90291";
  address.administrative_area = "CA";
  address.locality = "Los Angeles";
  address.address_line.push_back("340 Main St.");
  EXPECT_TRUE(HasAllRequiredFields(address));
}

TEST(AddressinputUtilTest, MissingFieldsAreAddedToProblems) {
  AddressData address;
  address.region_code = "US";
  // Leave postal code empty.
  // Leave state empty.
  address.locality = "Los Angeles";
  address.address_line.push_back("340 Main St.");

  std::multimap<AddressField, AddressProblem> empty_filter;
  std::multimap<AddressField, AddressProblem> problems;

  ValidateRequiredFields(address, &empty_filter, &problems);
  EXPECT_EQ(problems.size(), 2);
}

TEST(AddressinputUtilTest, OnlyFieldsContainedInFilterAreAddedToProblems) {
  AddressData address;
  address.region_code = "US";
  // Leave postal code empty.
  // Leave state empty.
  address.locality = "Los Angeles";
  address.address_line.push_back("340 Main St.");

  std::multimap<AddressField, AddressProblem> include_postal_code_filter;
  include_postal_code_filter.insert(std::make_pair(
      AddressField::POSTAL_CODE, AddressProblem::MISSING_REQUIRED_FIELD));
  std::multimap<AddressField, AddressProblem> problems;

  ValidateRequiredFields(address, &include_postal_code_filter, &problems);
  ASSERT_EQ(problems.size(), 1);
  EXPECT_EQ(problems.begin()->first, AddressField::POSTAL_CODE);
  EXPECT_EQ(problems.begin()->second, AddressProblem::MISSING_REQUIRED_FIELD);
}

TEST(AddressinputUtilTest, AllMissingFieldsAreAddedToProblems) {
  AddressData address;
  address.region_code = "US";
  // Leave postal code empty.
  // Leave state empty.
  address.locality = "Los Angeles";
  address.address_line.push_back("340 Main St.");

  std::multimap<AddressField, AddressProblem> empty_filter;
  std::multimap<AddressField, AddressProblem> problems;

  ValidateRequiredFieldsExceptFilteredOut(address, &empty_filter, &problems);
  EXPECT_EQ(problems.size(), 2);
}

TEST(AddressinputUtilTest, FieldsContainedInFilterAreExcludedFromProblems) {
  AddressData address;
  address.region_code = "US";
  // Leave postal code empty.
  // Leave state empty.
  address.locality = "Los Angeles";
  address.address_line.push_back("340 Main St.");

  std::multimap<AddressField, AddressProblem> exclude_postal_code_filter;
  exclude_postal_code_filter.insert(std::make_pair(
      AddressField::POSTAL_CODE, AddressProblem::MISSING_REQUIRED_FIELD));
  std::multimap<AddressField, AddressProblem> problems;

  ValidateRequiredFieldsExceptFilteredOut(address, &exclude_postal_code_filter,
                                          &problems);
  ASSERT_EQ(problems.size(), 1);
  EXPECT_EQ(problems.begin()->first, AddressField::ADMIN_AREA);
  EXPECT_EQ(problems.begin()->second, AddressProblem::MISSING_REQUIRED_FIELD);
}

}  // namespace addressinput
}  // namespace autofill
