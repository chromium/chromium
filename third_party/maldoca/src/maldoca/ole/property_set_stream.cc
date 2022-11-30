// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/ole/property_set_stream.h"

#include "absl/strings/str_cat.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/statusor.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/oss_utils.h"

using ::maldoca::OLEPropertySetStream;
using ::maldoca::StatusOr;
using ::maldoca::ole::Dictionary;
using ::maldoca::ole::DictionaryEntry;
using ::maldoca::ole::Property;
using ::maldoca::ole::PropertySet;
using ::maldoca::ole::PropertySetStream;
using ::maldoca::ole::VtValue;
using ::maldoca::ole::VtVector;

// use default Latin1 code page (1252) if nothing else is defined in the
// PropertySetStream
OLEPropertySetStream::OLEPropertySetStream() : encoding_name_("LATIN1") {}

OLEPropertySetStream::PropertyIdentifier
OLEPropertySetStream::GetPropertyIdentifier(uint32_t v) {
  switch (v) {
    case 0:
      return kDictionary;

    case 1:
      return kCodePage;

    case 0x80000000:
      return kLocale;

    case 0x80000003:
      return kBehavior;

    default:
      return kNormal;
  }
}

StatusOr<const Property *> OLEPropertySetStream::GetPropertyFromDictionary(
    absl::string_view name, const maldoca::ole::PropertySetStream &stream) {
  constexpr int kUserDefinedPropertySet = 1;
  if ((stream.property_set_size() < 2) ||
      (stream.property_set(kUserDefinedPropertySet).dictionary().empty())) {
    return ::absl::Status(absl::StatusCode::kNotFound,
                          absl::StrCat(name, " property not found."));
  }

  for (const auto &entry :
       stream.property_set(kUserDefinedPropertySet).dictionary(0).entry()) {
    if (entry.has_property_id() && entry.has_name() && (entry.name() == name)) {
      for (const auto &property :
           stream.property_set(kUserDefinedPropertySet).property()) {
        if (property.has_property_id() &&
            (property.property_id() == entry.property_id())) {
          return &property;
        }
      }
    }
  }
  return ::absl::Status(absl::StatusCode::kNotFound,
                        absl::StrCat(name, " property not found."));
}

bool OLEPropertySetStream::CodePageStringReader(absl::string_view *stream,
                                                std::string *output) {
  uint32_t size;
  if (!LittleEndianReader::ConsumeUInt32(stream, &size)) {
    return false;
  }
  std::string str;
  if (!LittleEndianReader::ConsumeString(stream, size, &str)) {
    return false;
  }

  std::string utf8_string;
  int bytes_consumed, bytes_filled, error_char_count;
  if (utils::ConvertEncodingBufferToUTF8String(
          str, encoding_name_.c_str(), &utf8_string, &bytes_consumed,
          &bytes_filled, &error_char_count)) {
    if (error_char_count != 0) {
      LOG(WARNING) << "error_char_count is not 0, possible invalid "
                      "encoding/characters. Input string was: "
                   << str;
    }
    absl::StripAsciiWhitespace(&utf8_string);
    *output = std::move(utf8_string);
    return true;
  } else {
    LOG(WARNING) << "ConvertEncodingBufferToUTF8String failed";
    return false;
  }
}

void OLEPropertySetStream::ConsumePaddingBytes(uint32_t size, uint32_t padd_to,
                                               absl::string_view *stream) {
  uint32_t r = size % padd_to;
  if (r == 0) {
    return;
  }
  std::string dummy_str;
  LittleEndianReader::ConsumeString(stream, padd_to - r, &dummy_str);
}

bool OLEPropertySetStream::UnicodeStringReader(absl::string_view *stream,
                                               std::string *output) {
  uint32_t size = 0;
  if (!LittleEndianReader::ConsumeUInt32(stream, &size)) {
    return false;
  }
  std::string str;
  if (!LittleEndianReader::ConsumeString(stream, size * 2, &str)) {
    return false;
  }
  ConsumePaddingBytes(size * 2, 4, stream);

  int bytes_consumed, bytes_filled, error_char_count;
  std::string utf8_string;
  if (utils::ConvertEncodingBufferToUTF8String(str, "utf-16le", &utf8_string,
                                               &bytes_consumed, &bytes_filled,
                                               &error_char_count)) {
    if (error_char_count != 0) {
      LOG(WARNING) << "error_char_count is not 0, possible invalid "
                      "encoding/characters. Input string was: "
                   << str;
    }
    absl::StripAsciiWhitespace(&utf8_string);
    *output = std::move(utf8_string);
    return true;
  } else {
    LOG(WARNING) << "ConvertEncodingBufferToUTF8String failed";
    return false;
  }
}

bool OLEPropertySetStream::VtInt4Reader(absl::string_view *stream,
                                        int32_t *value) {
  uint16_t type;
  maldoca::ole::VtValue vt_value;
  if (!TypedValueReader(stream, &type, &vt_value) || (type != kVtI4)) {
    return false;
  }
  return utils::Int64To32LosslessConvert(vt_value.int_(), value);
}

bool OLEPropertySetStream::VtLpwstrReader(absl::string_view *stream,
                                          std::string *value) {
  uint16_t type;
  maldoca::ole::VtValue vt_value;
  if (!TypedValueReader(stream, &type, &vt_value) || (type != kVtLpwstr)) {
    return false;
  }
  *value = vt_value.str();
  return true;
}

void OLEPropertySetStream::VecVtHyperlinkReader(
    absl::string_view *stream,
    google::protobuf::RepeatedPtrField<maldoca::ole::VtHyperlink> *links) {
  uint32_t num_entries;
  if (!LittleEndianReader::ConsumeUInt32(stream, &num_entries)) {
    return;
  }
  // According to MSDN:
  // https://msdn.microsoft.com/en-us/library/dd908491.aspx
  // cElements (4 bytes): An unsigned integer specifying the count of elements
  // in the rgHyperlink field. The number of elements in rgHyperlink MUST be 1/6
  // of this value. This value MUST be evenly divisible by 6.
  for (uint32_t i = 0; i < num_entries / 6; ++i) {
    maldoca::ole::VtHyperlink link;
    int32_t v;
    if (!VtInt4Reader(stream, &v)) {
      break;
    }
    link.set_hash(v);
    if (!VtInt4Reader(stream, &v)) {
      break;
    }
    link.set_app(v);
    if (!VtInt4Reader(stream, &v)) {
      break;
    }
    link.set_office_art(v);
    if (!VtInt4Reader(stream, &v)) {
      break;
    }
    link.set_info(v);
    std::string s;
    if (!VtLpwstrReader(stream, &s)) {
      break;
    }
    DLOG(INFO) << s;
    link.set_hlink1(s);
    if (!VtLpwstrReader(stream, &s)) {
      break;
    }
    DLOG(INFO) << s;
    link.set_hlink2(s);
    *links->Add() = link;
  }
}

bool OLEPropertySetStream::VtVectorReader(absl::string_view *stream,
                                          uint16_t type, VtVector *vec) {
  uint32_t num_entries;
  if (!LittleEndianReader::ConsumeUInt32(stream, &num_entries)) {
    return false;
  }
  auto status_or_variant_type = ConvertTypeToProtoVariantType(type);
  if (!status_or_variant_type.ok()) {
    LOG(ERROR) << "Converting " << type << " to VariantType failed: "
               << status_or_variant_type.status().message();
    return false;
  }
  vec->set_type(status_or_variant_type.value());
  DLOG(INFO) << "vector entries: " << num_entries;
  for (uint32_t i = 0; i < num_entries; i++) {
    if (!VtValueReader(stream, type, vec->add_value())) {
      return false;
    }
  }
  return true;
}

bool OLEPropertySetStream::DictionaryPacketReader(absl::string_view stream,
                                                  Dictionary *dict) {
  uint32_t num_entries;
  if (!LittleEndianReader::ConsumeUInt32(&stream, &num_entries)) {
    return false;
  }
  DLOG(INFO) << "dictionary entries: " << num_entries;
  for (uint32_t i = 0; i < num_entries; i++) {
    uint32_t prop_id;
    if (!LittleEndianReader::ConsumeUInt32(&stream, &prop_id)) {
      return false;
    }
    std::string str;
    if (!CodePageStringReader(&stream, &str)) {
      return false;
    }
    DLOG(INFO) << "entry: " << prop_id << " -> " << str;
    DictionaryEntry *dict_entry = dict->add_entry();
    dict_entry->set_property_id(prop_id);
    dict_entry->set_name(str);
  }
  return true;
}

bool OLEPropertySetStream::VtValueReader(absl::string_view *stream,
                                         uint16_t type, VtValue *value) {
  switch (type) {
    case kVtLpstr: {
      std::string str;
      if (!CodePageStringReader(stream, &str)) {
        return false;
      }
      value->set_str(str);
      DLOG(INFO) << "value length: " << str.length() << ", value: " << str;
    } break;

    case kVtLpwstr: {
      std::string str;
      if (!UnicodeStringReader(stream, &str)) {
        return false;
      }
      value->set_str(str);
      DLOG(INFO) << "value length: " << str.length() << ", value: " << str;
    } break;

    case kVtI2: {
      int16_t v;
      if (!LittleEndianReader::ConsumeUInt16(
              stream, reinterpret_cast<uint16_t *>(&v))) {
        return false;
      }
      value->set_int_(v);
      DLOG(INFO) << "value: " << v;
    } break;

    case kVtI4: {
      int32_t v;
      if (!LittleEndianReader::ConsumeUInt32(
              stream, reinterpret_cast<uint32_t *>(&v))) {
        return false;
      }
      value->set_int_(v);
      DLOG(INFO) << "value: " << v;
    } break;

    case kVtFiletime: {
      uint64_t v;
      if (!LittleEndianReader::ConsumeUInt64(stream, &v)) {
        return false;
      }
      value->set_uint(v);
      DLOG(INFO) << "value: " << v;
    } break;

    case kVtBool: {
      uint16_t v;
      if (!LittleEndianReader::ConsumeUInt16(stream, &v)) {
        return false;
      }
      value->set_boolean(v);
      DLOG(INFO) << "value: " << (v ? "TRUE" : "FALSE");
    } break;

    case kVtVariant: {
      uint16_t vt;
      if (!LittleEndianReader::ConsumeUInt16(stream, &vt)) {
        return false;
      }
      if (stream->size() < sizeof(uint16_t)) {
        return false;
      }
      stream->remove_prefix(sizeof(uint16_t));
      return VtValueReader(stream, vt, value);
    }

    case kVtBlob: {
      uint32_t size;
      if (!LittleEndianReader::ConsumeUInt32(stream, &size)) {
        return false;
      }
      DLOG(INFO) << "blob size: " << size;
      std::string str;
      if (!LittleEndianReader::ConsumeString(stream, size, &str)) {
        return false;
      }
      value->set_blob(str.data(), str.size());
      value->set_blob_hash(Sha1Hash(str));
    } break;

    case kVtVectorVtLpstr: {
      if (!VtVectorReader(stream, kVtLpstr, value->mutable_vector())) {
        return false;
      }
    } break;

    case kVtVectorVtVariant: {
      if (!VtVectorReader(stream, kVtVariant, value->mutable_vector())) {
        return false;
      }
    } break;

    default:
      break;
  }
  return true;
}

bool OLEPropertySetStream::TypedValueReader(absl::string_view *stream,
                                            uint16_t *type,
                                            maldoca::ole::VtValue *value) {
  if (!LittleEndianReader::ConsumeUInt16(stream, type)) {
    return false;
  }
  uint16_t padding;
  if (!LittleEndianReader::ConsumeUInt16(stream, &padding)) {
    return false;
  }
  DLOG(INFO) << "type: " << *type;
  return VtValueReader(stream, *type, value);
}

bool OLEPropertySetStream::TypedPropertyValueReader(
    absl::string_view stream, maldoca::ole::Property *property) {
  uint16_t type;
  if (TypedValueReader(&stream, &type, property->mutable_value())) {
    auto status_or_variant_type = ConvertTypeToProtoVariantType(type);
    if (!status_or_variant_type.ok()) {
      LOG(ERROR) << "Converting " << type << " to VariantType failed: "
                 << status_or_variant_type.status().message();
      return false;
    }
    property->set_type(status_or_variant_type.value());
    return true;
  }
  return false;
}

bool OLEPropertySetStream::PropertySetReader(absl::string_view stream,
                                             PropertySet *prop_set) {
  absl::string_view orig_stream = stream;
  uint32_t size;
  if (!LittleEndianReader::ConsumeUInt32(&stream, &size)) {
    return false;
  }
  DLOG(INFO) << "size of properties: " << size;
  uint32_t num_props;
  if (!LittleEndianReader::ConsumeUInt32(&stream, &num_props)) {
    return false;
  }
  DLOG(INFO) << "number of properties: " << num_props;
  for (uint32_t i = 0; i < num_props; i++) {
    uint32_t prop_id;
    if (!LittleEndianReader::ConsumeUInt32(&stream, &prop_id)) {
      return false;
    }
    uint32_t offset;
    if (!LittleEndianReader::ConsumeUInt32(&stream, &offset)) {
      return false;
    }
    DLOG(INFO) << "id: " << prop_id << ", offset: " << offset;
    switch (GetPropertyIdentifier(prop_id)) {
      case kNormal:
      case kCodePage: {
        maldoca::ole::Property *prop = prop_set->add_property();
        prop->set_property_id(prop_id);
        if (!TypedPropertyValueReader(absl::ClippedSubstr(orig_stream, offset),
                                      prop)) {
          return false;
        }
        if (GetPropertyIdentifier(prop_id) == kCodePage) {
          encoding_name_ = absl::StrCat("cp", prop->value().int_());
          DLOG(INFO) << "Found encoding: " << encoding_name_;
        }
      } break;

      case kDictionary:
        if (!DictionaryPacketReader(absl::ClippedSubstr(orig_stream, offset),
                                    prop_set->add_dictionary())) {
          return false;
        }
        break;

      default:
        break;
    }
  }
  return true;
}

bool OLEPropertySetStream::Read(absl::string_view stream,
                                PropertySetStream *prop_set_stream) {
  absl::string_view orig_stream = stream;
  uint16_t byte_order;
  if (!LittleEndianReader::ConsumeUInt16(&stream, &byte_order)) {
    return false;
  }
  prop_set_stream->set_byte_order(byte_order);
  uint16_t version;
  if (!LittleEndianReader::ConsumeUInt16(&stream, &version)) {
    return false;
  }
  prop_set_stream->set_version(version);
  uint32_t system_id;
  if (!LittleEndianReader::ConsumeUInt32(&stream, &system_id)) {
    return false;
  }
  prop_set_stream->set_system_id(system_id);
  std::string clsid;
  if (!LittleEndianReader::ConsumeString(&stream, sizeof(OleGuid), &clsid)) {
    return false;
  }
  prop_set_stream->mutable_clsid()->set_data(clsid.data(), clsid.size());
  uint32_t num_props;
  if (!LittleEndianReader::ConsumeUInt32(&stream, &num_props)) {
    return false;
  }
  DLOG(INFO) << "number of property streams: " << num_props;
  if (num_props > 0) {
    std::string fmtid0;
    if (!LittleEndianReader::ConsumeString(&stream, sizeof(OleGuid), &fmtid0)) {
      return false;
    }
    PropertySet *prop_set = prop_set_stream->add_property_set();
    prop_set->mutable_fmtid()->set_data(fmtid0.data(), fmtid0.size());
    uint32_t offset0;
    if (!LittleEndianReader::ConsumeUInt32(&stream, &offset0)) {
      return false;
    }
    uint32_t offset1;
    if (num_props > 1) {
      std::string fmtid1;
      if (!LittleEndianReader::ConsumeString(&stream, sizeof(OleGuid),
                                             &fmtid1)) {
        return false;
      }
      prop_set = prop_set_stream->add_property_set();
      prop_set->mutable_fmtid()->set_data(fmtid1.data(), fmtid1.size());
      if (!LittleEndianReader::ConsumeUInt32(&stream, &offset1)) {
        return false;
      }
    }
    DLOG(INFO) << "reading PropertySet0 " << offset0;
    if (!PropertySetReader(absl::ClippedSubstr(orig_stream, offset0),
                           prop_set_stream->mutable_property_set(0))) {
      return false;
    }
    if (num_props > 1) {
      DLOG(INFO) << "reading PropertySet1 " << offset1;
      if (!PropertySetReader(absl::ClippedSubstr(orig_stream, offset1),
                             prop_set_stream->mutable_property_set(1))) {
        return false;
      }
    }
  }
  return true;
}

StatusOr<maldoca::ole::VariantType>
OLEPropertySetStream::ConvertTypeToProtoVariantType(uint16_t type) {
  switch (type) {
    case kVtEmpty:
      return ole::VariantType::kVtEmpty;
    case kVtNull:
      return ole::VariantType::kVtNull;
    case kVtI2:
      return ole::VariantType::kVtI2;
    case kVtI4:
      return ole::VariantType::kVtI4;
    case kVtR4:
      return ole::VariantType::kVtR4;
    case kVtR8:
      return ole::VariantType::kVtR8;
    case kVtCy:
      return ole::VariantType::kVtCy;
    case kVtDate:
      return ole::VariantType::kVtDate;
    case kVtBstr:
      return ole::VariantType::kVtBstr;
    case kVtError:
      return ole::VariantType::kVtError;
    case kVtBool:
      return ole::VariantType::kVtBool;
    case kVtVariant:
      return ole::VariantType::kVtVariant;
    case kVtI1:
      return ole::VariantType::kVtI1;
    case kVtUI1:
      return ole::VariantType::kVtUI1;
    case kVtUI2:
      return ole::VariantType::kVtUI2;
    case kVtUI4:
      return ole::VariantType::kVtUI4;
    case kVtI8:
      return ole::VariantType::kVtI8;
    case kVtUI8:
      return ole::VariantType::kVtUI8;
    case kVtLpstr:
      return ole::VariantType::kVtLpstr;
    case kVtLpwstr:
      return ole::VariantType::kVtLpwstr;
    case kVtFiletime:
      return ole::VariantType::kVtFiletime;
    case kVtBlob:
      return ole::VariantType::kVtBlob;
    case kVtVector:
      return ole::VariantType::kVtVector;
    case kVtVectorVtVariant:
      return ole::VariantType::kVtVectorVtVariant;
    case kVtVectorVtLpstr:
      return ole::VariantType::kVtVectorVtLpstr;
    default:
      return absl::InternalError("Unexpected type");
  }
}
