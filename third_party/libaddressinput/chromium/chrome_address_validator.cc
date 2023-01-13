// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

#include <cmath>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_normalizer.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/src/rule.h"

namespace autofill {
namespace {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressNormalizer;
using ::i18n::addressinput::BuildCallback;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::PreloadSupplier;
using ::i18n::addressinput::Rule;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::POSTAL_CODE;

// The maximum number attempts to load rules.
static const int kMaxAttemptsNumber = 8;

}  // namespace

AddressValidator::AddressValidator(std::unique_ptr<Source> source,
                                   std::unique_ptr<Storage> storage,
                                   LoadRulesListener* load_rules_listener)
    : supplier_(new PreloadSupplier(source.release(), storage.release())),
      normalizer_(new AddressNormalizer(supplier_.get())),
      validator_(new ::i18n::addressinput::AddressValidator(supplier_.get())),
      validated_(BuildCallback(this, &AddressValidator::Validated)),
      rules_loaded_(BuildCallback(this, &AddressValidator::RulesLoaded)),
      load_rules_listener_(load_rules_listener) {}

AddressValidator::~AddressValidator() {}

void AddressValidator::LoadRules(const std::string& region_code) {
  attempts_number_[region_code] = 0;
  supplier_->LoadRules(region_code, *rules_loaded_);
}

std::vector<std::pair<std::string, std::string>>
AddressValidator::GetRegionSubKeys(const std::string& region_code,
                                   const std::string& language) {
  std::vector<std::pair<std::string, std::string>> subkeys_codes_names;
  if (!AreRulesLoadedForRegion(region_code))
    return subkeys_codes_names;

  auto rules = supplier_->GetRulesForRegion(region_code);
  auto rule_iterator = rules.find("data/" + region_code);
  // This happens if the rules are bad.
  if (rule_iterator == rules.end() || !rule_iterator->second)
    return subkeys_codes_names;

  auto subkeys_codes = rule_iterator->second->GetSubKeys();

  // If the device language is available, show the names in that language.
  // Otherwise, show the default names.
  std::string lang_suffix = "";
  if (rules.find("data/" + region_code + "--" + language) != rules.end())
    lang_suffix = "--" + language;  // ex: --fr

  for (auto subkey_code : subkeys_codes) {
    auto rule = rules.find("data/" + region_code + '/' + subkey_code +
                           lang_suffix);  // exp: data/CA/QC--fr
    // This happens if the rules are bad.
    if (rule == rules.end() || !rule_iterator->second)
      continue;
    auto subkey_name = rule->second->GetName();
    if (subkey_name.empty())  // For some cases, the name is not available.
      subkey_name = subkey_code;
    subkeys_codes_names.push_back(make_pair(subkey_code, subkey_name));
  }
  return subkeys_codes_names;
}

AddressValidator::Status AddressValidator::ValidateAddress(
    const AddressData& address,
    const FieldProblemMap* filter,
    FieldProblemMap* problems) const {
  if (supplier_->IsPending(address.region_code)) {
    if (problems)
      addressinput::ValidateRequiredFields(address, filter, problems);
    return RULES_NOT_READY;
  }

  if (!supplier_->IsLoaded(address.region_code)) {
    if (problems)
      addressinput::ValidateRequiredFields(address, filter, problems);
    return RULES_UNAVAILABLE;
  }

  if (!problems)
    return SUCCESS;

  validator_->Validate(address,
                       true,  // Allow postal office boxes.
                       true,  // Require recipient name.
                       filter,
                       problems,
                       *validated_);

  return SUCCESS;
}

bool AddressValidator::NormalizeAddress(AddressData* address) const {
  if (!supplier_->IsLoaded(address->region_code))
    return false;

  normalizer_->Normalize(address);
  return true;
}

bool AddressValidator::AreRulesLoadedForRegion(const std::string& region_code) {
  return supplier_->IsLoaded(region_code);
}

AddressValidator::AddressValidator() : load_rules_listener_(nullptr) {}

base::TimeDelta AddressValidator::GetBaseRetryPeriod() const {
  return base::Seconds(8);
}

void AddressValidator::Validated(bool success,
                                 const AddressData&,
                                 const FieldProblemMap&) {
  DCHECK(success);
}

void AddressValidator::RulesLoaded(bool success,
                                   const std::string& region_code,
                                   int) {
  if (load_rules_listener_)
    load_rules_listener_->OnAddressValidationRulesLoaded(region_code, success);

  // Count the first failed attempt to load rules as well.
  if (success || attempts_number_[region_code] + 1 >= kMaxAttemptsNumber)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AddressValidator::RetryLoadRules,
                     weak_factory_.GetWeakPtr(), region_code),
      GetBaseRetryPeriod() * pow(2, attempts_number_[region_code]++));
}

void AddressValidator::RetryLoadRules(const std::string& region_code) {
  // Do not reset retry count.
  supplier_->LoadRules(region_code, *rules_loaded_);
}

}  // namespace autofill
