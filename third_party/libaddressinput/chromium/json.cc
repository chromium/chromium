// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/util/json.h"

#include <map>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace i18n {
namespace addressinput {

namespace {

// Returns |json| parsed into a JSON dictionary. Sets |parser_error| to true if
// parsing failed.
base::Value::Dict Parse(const std::string& json, bool* parser_error) {
  DCHECK(parser_error);

  // |json| is converted to a |c_str()| here because rapidjson and other parts
  // of the standalone library use char* rather than std::string.
  std::optional<base::Value> parsed(base::JSONReader::Read(json.c_str()));
  *parser_error = !parsed || !parsed->is_dict();

  if (*parser_error)
    return base::Value::Dict();
  else
    return std::move(*parsed).TakeDict();
}

}  // namespace

// Implementation of JSON parser for libaddressinput using JSON parser in
// Chrome.
class Json::JsonImpl {
 public:
  explicit JsonImpl(const std::string& json)
      : owned_(Parse(json, &parser_error_)), dict_(owned_) {}

  JsonImpl(const JsonImpl&) = delete;
  JsonImpl& operator=(const JsonImpl&) = delete;

  ~JsonImpl() {}

  bool parser_error() const { return parser_error_; }

  const std::vector<const Json*>& GetSubDictionaries() {
    if (sub_dicts_.empty()) {
      for (auto kv : dict_) {
        if (kv.second.is_dict()) {
          owned_sub_dicts_.push_back(
              base::WrapUnique(new Json(new JsonImpl(kv.second.GetDict()))));
          sub_dicts_.push_back(owned_sub_dicts_.back().get());
        }
      }
    }
    return sub_dicts_;
  }

  bool GetStringValueForKey(const std::string& key, std::string* value) const {
    const std::string* value_str = dict_.FindString(key);
    if (!value_str)
      return false;

    DCHECK(value);
    *value = *value_str;
    return true;
  }

 private:
  explicit JsonImpl(const base::Value::Dict& dict)
      : parser_error_(false), dict_(dict) {}

  base::Value::Dict owned_;
  bool parser_error_;
  const base::Value::Dict& dict_;
  std::vector<const Json*> sub_dicts_;
  std::vector<std::unique_ptr<Json>> owned_sub_dicts_;
};

Json::Json() {}

Json::~Json() {}

bool Json::ParseObject(const std::string& json) {
  DCHECK(!impl_);
  impl_.reset(new JsonImpl(json));
  if (impl_->parser_error())
    impl_.reset();
  return !!impl_;
}

const std::vector<const Json*>& Json::GetSubDictionaries() const {
  return impl_->GetSubDictionaries();
}

bool Json::GetStringValueForKey(const std::string& key,
                                std::string* value) const {
  return impl_->GetStringValueForKey(key, value);
}

Json::Json(JsonImpl* impl) : impl_(impl) {}

}  // namespace addressinput
}  // namespace i18n
