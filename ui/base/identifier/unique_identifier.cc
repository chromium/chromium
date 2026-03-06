// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/identifier/unique_identifier.h"

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/no_destructor.h"

namespace ui::internal {

std::string UniqueIdentifier::GetName() const {
  if (!handle_) {
    return std::string();
  }
  RegisterKnownIdentifier(*this);
  return handle_->name;
}

intptr_t UniqueIdentifier::GetRawValue() const {
  if (!handle_) {
    return 0;
  }
  RegisterKnownIdentifier(*this);
  return reinterpret_cast<intptr_t>(handle_);
}

// static
UniqueIdentifier UniqueIdentifier::FromRawValue(intptr_t value) {
  if (!value) {
    return UniqueIdentifier();
  }
  const auto* impl =
      reinterpret_cast<const internal::UniqueIdentifierProvider*>(value);
  CHECK(GetKnownIdentifiers().contains(impl));
  return UniqueIdentifier(impl);
}

// static
UniqueIdentifier UniqueIdentifier::FromName(const char* name) {
  for (const auto* impl : GetKnownIdentifiers()) {
    if (std::string_view(impl->name) == name) {
      return UniqueIdentifier(impl);
    }
  }
  return UniqueIdentifier();
}

// static
void UniqueIdentifier::RegisterKnownIdentifier(
    UniqueIdentifier element_identifier) {
  CHECK(element_identifier);

#if DCHECK_IS_ON()
  // Enforce uniqueness in DCHECK builds.
  const UniqueIdentifier existing = FromName(element_identifier.handle_->name);
  DCHECK(!existing || existing == element_identifier)
      << "Duplicate identifier: " << element_identifier.handle_->name;
#endif

  GetKnownIdentifiers().insert(element_identifier.handle_);
}

void UniqueIdentifier::ClearKnownIdentifiersForTesting() {
  GetKnownIdentifiers().clear();
}

// static
UniqueIdentifier::KnownIdentifiers& UniqueIdentifier::GetKnownIdentifiers() {
  static base::NoDestructor<KnownIdentifiers> known_identifiers;
  return *known_identifiers.get();
}

void PrintTo(UniqueIdentifier element_identifier, std::ostream* os) {
  *os << "UniqueIdentifier " << element_identifier.GetName();
}

std::ostream& operator<<(std::ostream& os,
                         UniqueIdentifier element_identifier) {
  PrintTo(element_identifier, &os);
  return os;
}

}  // namespace ui::internal
