// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace util {

namespace {

bool ReportError(const base::Value& from,
                 base::Value::Type expected,
                 std::u16string& error) {
  DCHECK(error.empty());
  error = base::ASCIIToUTF16(base::StringPrintf(
      "expected %s, got %s", base::Value::GetTypeName(expected),
      base::Value::GetTypeName(from.type())));
  return false;  // Always false on purpose.
}

}  // namespace

bool PopulateItem(const base::Value& from, int& out) {
  if (!from.is_int()) {
    return false;
  }
  out = from.GetInt();
  return true;
}

bool PopulateItem(const base::Value& from, int& out, std::u16string& error) {
  if (!PopulateItem(from, out))
    return ReportError(from, base::Value::Type::INTEGER, error);
  return true;
}

bool PopulateItem(const base::Value& from, bool& out) {
  if (!from.is_bool()) {
    return false;
  }
  out = from.GetBool();
  return true;
}

bool PopulateItem(const base::Value& from, bool& out, std::u16string& error) {
  if (!from.is_bool()) {
    return ReportError(from, base::Value::Type::BOOLEAN, error);
  }

  out = from.GetBool();
  return true;
}

bool PopulateItem(const base::Value& from, double& out) {
  if (!from.is_double()) {
    return false;
  }
  out = from.GetDouble();
  return true;
}

bool PopulateItem(const base::Value& from, double& out, std::u16string& error) {
  if (!from.is_double()) {
    return ReportError(from, base::Value::Type::DOUBLE, error);
  }

  out = from.GetDouble();
  return true;
}

bool PopulateItem(const base::Value& from, std::string& out) {
  if (!from.is_string())
    return false;
  out = from.GetString();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::string& out,
                  std::u16string& error) {
  if (!from.is_string()) {
    return ReportError(from, base::Value::Type::STRING, error);
  }
  out = from.GetString();
  return true;
}

bool PopulateItem(const base::Value& from, std::vector<uint8_t>& out) {
  if (!from.is_blob()) {
    return false;
  }
  out = from.GetBlob();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::vector<uint8_t>& out,
                  std::u16string& error) {
  if (!from.is_blob()) {
    return ReportError(from, base::Value::Type::BINARY, error);
  }
  out = from.GetBlob();
  return true;
}

bool PopulateItem(const base::Value& from, base::Value& out) {
  out = from.Clone();
  return true;
}

bool PopulateItem(const base::Value& from,
                  base::Value& out,
                  std::u16string& error) {
  out = from.Clone();
  return true;
}

void AddItemToList(const int from, base::Value::List& out) {
  out.Append(from);
}

void AddItemToList(const bool from, base::Value::List& out) {
  out.Append(from);
}

void AddItemToList(const double from, base::Value::List& out) {
  out.Append(from);
}

void AddItemToList(const std::string& from, base::Value::List& out) {
  out.Append(from);
}

void AddItemToList(const std::vector<uint8_t>& from, base::Value::List& out) {
  out.Append(base::Value(from));
}

void AddItemToList(const base::Value& from, base::Value::List& out) {
  out.Append(from.Clone());
}

}  // namespace util
}  // namespace json_schema_compiler
