// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace {

constexpr size_t kMaxFieldLength = 128;

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  i18n::addressinput::AddressData address;
  address.region_code = provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.administrative_area =
      provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.locality = provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.dependent_locality =
      provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.postal_code = provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.sorting_code = provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.language_code = provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.organization = provider.ConsumeRandomLengthString(kMaxFieldLength);
  address.recipient = provider.ConsumeRandomLengthString(kMaxFieldLength);

  while (provider.remaining_bytes() > 0) {
    address.address_line.push_back(
        provider.ConsumeRandomLengthString(kMaxFieldLength));
  }

  std::vector<std::string> output_multiline;
  i18n::addressinput::GetFormattedNationalAddress(address, &output_multiline);

  std::string output;
  i18n::addressinput::GetFormattedNationalAddressLine(address, &output);
  i18n::addressinput::GetStreetAddressLinesAsSingleLine(address, &output);
  return 0;
}
