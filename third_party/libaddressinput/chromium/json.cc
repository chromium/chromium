// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/util/json.h"

#include <map>
#include <memory>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace i18n {
namespace addressinput {

namespace {

// Returns |json| parsed into a JSON dictionary. Sets |parser_error| to true if
// parsing failed.
::std::unique_ptr<const base::DictionaryValue> Parse(const std::string& json,
                                                     bool* parser_error) {
  DCHECK(parser_error);
  ::std::unique_ptr<const base::DictionaryValue> result;

  // |json| is converted to a |c_str()| here because rapidjson and other parts
  // of the standalone library use char* rather than std::string.
  ::std::unique_ptr<const base::Value> parsed(
      base::JSONReader::ReadDeprecated(json.c_str()));
  *parser_error = !parsed || !parsed->is_dict();

  if (*parser_error)
    result.reset(new base::DictionaryValue);
  else
    result.reset(static_cast<const base::DictionaryValue*>(parsed.release()));

  return result;
}

}  // namespace

// Implementation of JSON parser for libaddressinput using JSON parser in
// Chrome.
class Json::JsonImpl {
 public:
  explicit JsonImpl(const std::string& json)
      : owned_(Parse(json, &parser_error_)),
        dict_(*owned_) {}

  JsonImpl(const JsonImpl&) = delete;
  JsonImpl& operator=(const JsonImpl&) = delete;

  ~JsonImpl() {}

  bool parser_error() const { return parser_error_; }

  const std::vector<const Json*>& GetSubDictionaries() {
    if (sub_dicts_.empty()) {
      for (base::DictionaryValue::Iterator it(dict_); !it.IsAtEnd();
           it.Advance()) {
        if (it.value().is_dict()) {
          const base::DictionaryValue* sub_dict = NULL;
          it.value().GetAsDictionary(&sub_dict);
          owned_sub_dicts_.push_back(
              base::WrapUnique(new Json(new JsonImpl(*sub_dict))));
          sub_dicts_.push_back(owned_sub_dicts_.back().get());
        }
      }
    }
    return sub_dicts_;
  }

  bool GetStringValueForKey(const std::string& key, std::string* value) const {
    const std::string* value_str = dict_.FindStringKey(key);
    if (!value_str)
      return false;

    DCHECK(value);
    *value = *value_str;
    return true;
  }

 private:
  explicit JsonImpl(const base::DictionaryValue& dict)
      : parser_error_(false), dict_(dict) {}

  const ::std::unique_ptr<const base::DictionaryValue> owned_;
  bool parser_error_;
  const base::DictionaryValue& dict_;
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
