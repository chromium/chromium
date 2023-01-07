// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Don't warn for unscoped enums (for now).
enum UnscopedEnumIgnored {
  kMaxValue = -1,
  kOops = 0,
};

// Warn if kMaxValue doesn't have the highest enumerator value.
enum class NotHighest {
  kNegative = -1,
  kZero = 0,
  kMaxValue = kNegative,
};

// Also warn if kMaxValue has a unique value: it should share the highest value
// to avoid polluting switch statements.
enum class MaxValueIsUnique {
  kNegative = -1,
  kZero = 0,
  kMaxValue,
};

// No warning if everything is right.
enum class CorrectMaxValue {
  kNegative = -1,
  kZero = 0,
  kMaxValue = kZero,
};

// No warning if the enum does not contain kMaxValue.
enum class NoMaxValue {
  kNegative = -1,
  kZero = 0,
  kNotMaxValue = 1,
};
