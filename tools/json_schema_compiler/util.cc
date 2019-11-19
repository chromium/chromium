// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
                 base::string16* error) {
  if (!error->empty())
    error->append(base::ASCIIToUTF16("; "));
  error->append(base::ASCIIToUTF16(base::StringPrintf(
      "expected %s, got %s", base::Value::GetTypeName(expected),
      base::Value::GetTypeName(from.type()))));
  return false;  // Always false on purpose.
}

}  // namespace

bool PopulateItem(const base::Value& from, int* out) {
  return from.GetAsInteger(out);
}

bool PopulateItem(const base::Value& from, int* out, base::string16* error) {
  if (!from.GetAsInteger(out))
    return ReportError(from, base::Value::Type::INTEGER, error);
  return true;
}

bool PopulateItem(const base::Value& from, bool* out) {
  return from.GetAsBoolean(out);
}

bool PopulateItem(const base::Value& from, bool* out, base::string16* error) {
  if (!from.GetAsBoolean(out))
    return ReportError(from, base::Value::Type::BOOLEAN, error);
  return true;
}

bool PopulateItem(const base::Value& from, double* out) {
  return from.GetAsDouble(out);
}

bool PopulateItem(const base::Value& from, double* out, base::string16* error) {
  if (!from.GetAsDouble(out))
    return ReportError(from, base::Value::Type::DOUBLE, error);
  return true;
}

bool PopulateItem(const base::Value& from, std::string* out) {
  return from.GetAsString(out);
}

bool PopulateItem(const base::Value& from,
                  std::string* out,
                  base::string16* error) {
  if (!from.GetAsString(out))
    return ReportError(from, base::Value::Type::STRING, error);
  return true;
}

bool PopulateItem(const base::Value& from, std::vector<uint8_t>* out) {
  if (!from.is_blob())
    return false;
  *out = from.GetBlob();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::vector<uint8_t>* out,
                  base::string16* error) {
  if (!from.is_blob())
    return ReportError(from, base::Value::Type::BINARY, error);
  *out = from.GetBlob();
  return true;
}

bool PopulateItem(const base::Value& from, std::unique_ptr<base::Value>* out) {
  *out = from.CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::Value>* out,
                  base::string16* error) {
  *out = from.CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::DictionaryValue>* out) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict))
    return false;
  *out = dict->CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::DictionaryValue>* out,
                  base::string16* error) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict))
    return ReportError(from, base::Value::Type::DICTIONARY, error);
  *out = dict->CreateDeepCopy();
  return true;
}

void AddItemToList(const int from, base::ListValue* out) {
  out->AppendInteger(from);
}

void AddItemToList(const bool from, base::ListValue* out) {
  out->AppendBoolean(from);
}

void AddItemToList(const double from, base::ListValue* out) {
  out->AppendDouble(from);
}

void AddItemToList(const std::string& from, base::ListValue* out) {
  out->AppendString(from);
}

void AddItemToList(const std::vector<uint8_t>& from, base::ListValue* out) {
  out->Append(base::Value(from));
}

void AddItemToList(const std::unique_ptr<base::Value>& from,
                   base::ListValue* out) {
  out->Append(from->CreateDeepCopy());
}

void AddItemToList(const std::unique_ptr<base::DictionaryValue>& from,
                   base::ListValue* out) {
  out->Append(from->CreateDeepCopy());
}

}  // namespace util
}  // namespace json_schema_compiler
