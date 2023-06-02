// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_RAWPTRCASTINGUNSAFECHECKER_H_
#define TOOLS_CLANG_PLUGINS_RAWPTRCASTINGUNSAFECHECKER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"

// Represents a casting safety judgement |verdict_|.
// - CastingSafety::kSafe: the |type| is casting-safe.
// - CastingSafety::kUnsafe: the |type| is casting-unsafe.
// - CastingSafety::kUndetermined: This denotes the safety status
//   is not yet determined, due to cross references.
// Holds some additional information to tell reasons.
class CastingSafety {
 public:
  enum Verdict {
    kSafe,
    kUnsafe,
    // This denotes the safety status is not yet determined.
    kUndetermined,
  };

  explicit CastingSafety(const clang::Type* type) : type_(type) {}
  explicit CastingSafety(const clang::Type* type, Verdict verdict)
      : type_(type), verdict_(verdict) {}

  const clang::Type* type() const { return type_; }

  Verdict verdict() const { return this->verdict_; }

  std::shared_ptr<CastingSafety> source() const { return this->source_; }

  std::optional<clang::SourceLocation> source_loc() const {
    return this->source_loc_;
  }

 private:
  friend class CastingUnsafePredicate;

  // Merges a sub verdict into this type's verdict.
  //
  // | this   \ sub  | kSafe         | kUndetermined | kUnsafe |
  // +---------------+---------------+---------------+---------+
  // | kSafe         | kSafe         | kUndetermined | kUnsafe |
  // | kUndetermined | kUndetermined | kUndetermined | kUnsafe |
  // | kUnsafe       | kUnsafe       | kUnsafe       | kUnsafe |
  Verdict MergeSubResult(
      std::shared_ptr<CastingSafety> sub,
      std::optional<clang::SourceLocation> loc = std::nullopt) {
    if (sub->verdict_ == kUnsafe && this->verdict_ != kUnsafe) {
      this->verdict_ = kUnsafe;
      this->source_ = std::move(sub);
      this->source_loc_ = loc;
    } else if (sub->verdict_ == kUndetermined && this->verdict_ == kSafe) {
      this->verdict_ = kUndetermined;
      this->source_ = std::move(sub);
      this->source_loc_ = loc;
    }
    return this->verdict_;
  }

  // |type_| is considered to be casting-safety |verdict_|.
  // Optionally, the result contains a reason for the verdict, |source_|.
  // There can be multiple reasons (e.g. |type_| has multiple |raw_ptr| member
  // variables), but only one of them is stored. The relation between |type_|
  // and |source_| is shown at |source_loc_|.
  const clang::Type* type_;
  Verdict verdict_ = kSafe;
  std::shared_ptr<CastingSafety> source_;
  std::optional<clang::SourceLocation> source_loc_;
};

// Determines a type is "casting unsafe" or not.
// A type is considered "casting unsafe" if ref counting can be broken as a
// result of casting. We determine "casting unsafe" types by applying these
// rules recursively:
//  - |raw_ptr<T>| or |raw_ref<T>| are casting unsafe; when implemented as
//  BackupRefPtr, assignment needs to go through the C++ operators to ensure the
//  refcount is properly maintained.
//  - Pointers, references and arrays to "casting unsafe" element(s) are
//  "casting unsafe"
//  - Classes and structs having a "casting unsafe" member are "casting unsafe"
//  - Classes and structs having a "casting unsafe" base class are "casting
//  unsafe"
// `CastingUnsafePredicate` has a cache to memorize "casting unsafety" results.
class CastingUnsafePredicate {
 public:
  bool IsCastingUnsafe(const clang::Type* type) const;
  std::shared_ptr<CastingSafety> GetCastingSafety(
      const clang::Type* type,
      std::set<const clang::Type*>* visited = nullptr) const;

  // Cache to efficiently determine casting safety.
  mutable std::map<const clang::Type*, std::shared_ptr<CastingSafety>> cache_;
};

#endif  // TOOLS_CLANG_PLUGINS_RAWPTRCASTINGUNSAFECHECKER_H_
