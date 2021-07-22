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

#include "maldoca/ole/ooxml_properties_extract.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/ret_check.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/base/statusor.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/proto/ole.pb.h"

namespace maldoca {
using utils::XmlCharDeleter;

namespace {
// Enum storing the two possible property_set_stream types.
enum class PropertyStreamType {
  kSummaryInformation,
  kDocumentSummaryInformation,
};

// Some properties have some appended text to them from time
// to time. For instance, the `Properties` tag can be named
// `ap:Properties`. We do not need it and want to remove it
// for identifying the properties.
absl::string_view RemoveTextBeforeColon(absl::string_view word) {
  size_t pos = word.find_last_of(':');
  if (pos == std::string::npos) {
    return word;
  }
  return word.substr(pos + 1);
}

// Converts a string to the matching variant.
// E.g. StringToOfficeVariantType("i4") = kVtI4.
StatusOr<ole::VariantType> StringToOfficeVariantType(
    absl::string_view type_view) {
  std::string type = absl::AsciiStrToLower(RemoveTextBeforeColon(type_view));
  if (type == "empty") {
    return ole::VariantType::kVtEmpty;
  } else if (type == "null") {
    return ole::VariantType::kVtNull;
  } else if (type == "i2") {
    return ole::VariantType::kVtI2;
  } else if (type == "i4") {
    return ole::VariantType::kVtI4;
  } else if (type == "i8") {
    return ole::VariantType::kVtI8;
  } else if (type == "r4") {
    return ole::VariantType::kVtR4;
  } else if (type == "r8") {
    return ole::VariantType::kVtR8;
  } else if (type == "cy") {
    return ole::VariantType::kVtCy;
  } else if (type == "date") {
    return ole::VariantType::kVtDate;
  } else if (type == "bstr") {
    return ole::VariantType::kVtBstr;
  } else if (type == "error") {
    return ole::VariantType::kVtError;
  } else if (type == "bool") {
    return ole::VariantType::kVtBool;
  } else if (type == "variant") {
    return ole::VariantType::kVtVariant;
  } else if (type == "i1") {
    return ole::VariantType::kVtI1;
  } else if (type == "ui1") {
    return ole::VariantType::kVtUI1;
  } else if (type == "ui2") {
    return ole::VariantType::kVtUI2;
  } else if (type == "ui4") {
    return ole::VariantType::kVtUI4;
  } else if (type == "ui8") {
    return ole::VariantType::kVtUI8;
  } else if (type == "lpstr") {
    return ole::VariantType::kVtLpstr;
  } else if (type == "lpwstr") {
    return ole::VariantType::kVtLpwstr;
  } else if (type == "filetime") {
    return ole::VariantType::kVtFiletime;
  } else if (type == "blob") {
    return ole::VariantType::kVtBlob;
  } else if (type == "vector") {
    return ole::VariantType::kVtVector;
  }
  return absl::InternalError("Type " + type + " does not exist!");
}

// Extract a vector property from an XML node.
StatusOr<ole::VtValue> ExtractVtVectorFromXML(
    ole::VariantType type, const xmlNode *property_xml_node) {
  // The vector should be like:
  // <vt:vector size="1" baseType="lpstr">
  // <vt:lpstr>text</vt:lpstr>
  // </vt:vector>

  if (type != ole::VariantType::kVtVectorVtVariant &&
      type != ole::VariantType::kVtVectorVtLpstr) {
    return absl::InternalError("Tried to parse an invalid type as vector!");
  }
  ole::VtValue vtvalue_proto;
  auto vector_proto = vtvalue_proto.mutable_vector();
  int vector_size = -1;
  std::string vector_type;

  // Look for the `size` and `baseType` attribute of the vector tag.
  auto attribute = property_xml_node->properties;
  bool found_size = false;
  bool found_type = false;
  for (; attribute; attribute = attribute->next) {
    absl::string_view name = utils::XmlCharPointerToString(attribute->name);
    name = RemoveTextBeforeColon(name);
    std::unique_ptr<xmlChar, utils::XmlCharDeleter> prop(
        xmlGetProp(property_xml_node, attribute->name));
    absl::string_view value = utils::XmlCharPointerToString(prop.get());

    if (!found_size && name == "size") {
      // Sets the vector_size. If `value` can't be parsed, then
      // `vector_size` is left untouched.
      if (!absl::SimpleAtoi(value, &vector_size)) {
        LOG(WARNING) << "Can't parse " << value << " as size type";
      }
      found_size = true;
    } else if (!found_type && name == "baseType") {
      vector_type = std::string(value);
      found_type = true;
    }
    if (found_type && found_size) {
      break;
    }
  }

  // If type is kVtVectorVtLpstr then entries of the vector are strings,
  // otherwise entries have a variant type.
  ole::VariantType vector_type_enum = ole::VariantType::kVtLpstr;
  if (type == ole::VariantType::kVtVectorVtVariant) {
    vector_type_enum = ole::VariantType::kVtVariant;
  }

  auto status_or_type = StringToOfficeVariantType(vector_type);
  if (!status_or_type.ok()) {
    LOG(WARNING) << "Unable to extract vector type: Expected `Lpstr` or "
                    "`Variant`, received "
                 << vector_type << ": " << status_or_type.status().message();
  } else if (status_or_type.value() != vector_type_enum) {
    LOG(WARNING) << "Expected vector to be of type " << vector_type_enum
                 << " but received type " << status_or_type.value();
  }
  vector_proto->set_type(vector_type_enum);

  const xmlNode *xml_vector_element = property_xml_node->children;
  for (; xml_vector_element; xml_vector_element = xml_vector_element->next) {
    auto status_or_value =
        ExtractVtValueFromXML(vector_type_enum, xml_vector_element);
    if (!status_or_value.ok()) {
      return status_or_value.status();
    }
    *(vector_proto->add_value()) = status_or_value.value();
  }
  if (vector_proto->value_size() != vector_size) {
    DLOG(INFO)
        << "While parsing, XML property vector expected a vector size of "
        << vector_size << " but received a size of "
        << vector_proto->value_size() << '!';
  }
  // Return the VtValue containing the vector.
  return vtvalue_proto;
}

// Extracts a variant value from an XML node.
StatusOr<ole::VtValue> ExtractVtVariantFromXML(
    const xmlNode *property_xml_node) {
  // The xml code should be like:
  // <variant>
  //    <new_type>value</new_type>
  // </variant>
  absl::string_view node_name =
      utils::XmlCharPointerToString(property_xml_node->name);
  node_name = RemoveTextBeforeColon(node_name);
  if (node_name != "variant") {
    return absl::InternalError(
        absl::StrCat("Expected variant value but got ", node_name));
  }

  // The name of the tag should be the new type and its content the new
  // value.
  const xmlNode *variant_xml_node = property_xml_node->children;
  // The node should exist, and it shouldn't have any siblings.
  if (variant_xml_node == nullptr || variant_xml_node->next) {
    return absl::InternalError("Invalid variant type syntax!");
  }

  absl::string_view new_type_name =
      utils::XmlCharPointerToString(variant_xml_node->name);
  new_type_name = RemoveTextBeforeColon(new_type_name);

  auto status_or_new_type = StringToOfficeVariantType(new_type_name);
  if (!status_or_new_type.ok()) {
    return status_or_new_type.status();
  }

  ole::VariantType new_type = status_or_new_type.value();

  // Return the extracted value for the new node and new type.
  return ExtractVtValueFromXML(new_type, variant_xml_node);
}
}  // namespace

StatusOr<ole::VtValue> ExtractVtValueFromXML(ole::VariantType type,
                                             const xmlNode *property_xml_node) {
  // If `property_xml_node` has no children then it doesn't have any value.
  if (!property_xml_node->children) {
    return absl::UnavailableError("Property has no asociated value!");
  }
  // If `propert_xml_node` has two children, then it's not a valid property.
  if (property_xml_node->children->next) {
    return absl::InternalError("Property has two asociated values!");
  }

  // Property is stored inside a vector container.
  if (type == ole::VariantType::kVtVectorVtVariant ||
      type == ole::VariantType::kVtVectorVtLpstr) {
    return ExtractVtVectorFromXML(type, property_xml_node->children);
  }
  if (type == ole::VariantType::kVtVariant) {
    return ExtractVtVariantFromXML(property_xml_node);
  }

  std::string content = std::string(
      utils::XmlCharPointerToString(property_xml_node->children->content));
  ole::VtValue value;

  switch (type) {
    case ole::VariantType::kVtI1:
    case ole::VariantType::kVtI2:
    case ole::VariantType::kVtI4:
    case ole::VariantType::kVtI8: {
      int64_t int_v;
      if (!absl::SimpleAtoi(content, &int_v)) {
        return absl::InternalError("Invalid number!");
      }
      value.set_int_(int_v);
      break;
    }
    case ole::VariantType::kVtUI1:
    case ole::VariantType::kVtUI2:
    case ole::VariantType::kVtUI4:
    case ole::VariantType::kVtUI8: {
      uint64_t uint_v;
      if (!absl::SimpleAtoi(content, &uint_v)) {
        return absl::InternalError("Invalid unsigned!");
      }
      value.set_uint(uint_v);
      break;
    }
    case ole::VariantType::kVtBool:
      absl::AsciiStrToLower(&content);
      if (content != "true" && content != "false") {
        return absl::InternalError("Invalid Boolean!");
      }
      value.set_boolean(content == "true");
      break;
    case ole::VariantType::kVtBlob:
      value.set_blob(content);
      value.set_blob_hash(Sha256HexString(content));
      break;
    case ole::VariantType::kVtLpstr:
    case ole::VariantType::kVtLpwstr:
    case ole::VariantType::kVtBstr:
      value.set_str(content);
      break;
    case ole::VariantType::kVtFiletime: {
      absl::Time timestamp;
      std::string error_message;
      if (!absl::ParseTime(absl::RFC3339_full, content, &timestamp,
                           &error_message)) {
        return absl::InternalError(error_message);
      }
      constexpr int64_t kTicksFrom1600 = 504911232000000000;
      uint64_t timestamp_uint64 = absl::ToUniversal(timestamp) - kTicksFrom1600;
      value.set_uint(timestamp_uint64);
      break;
    }
    default:
      return absl::UnimplementedError("Property type is not supported!");
  }
  return value;
}

// Tries to parse an OOXML property from the libXML node `property` and store it
// in the `OOXMLFile` proto.
absl::Status ExtractOOXMLPropertyToProto(const xmlNode *property,
                                         ooxml::OOXMLFile *proto,
                                         int *property_id_counter) {
  // The property has no value. Just return without doing anything.
  if (!property->children) return absl::OkStatus();

  // Extract the name of the current tag.
  absl::string_view tag_name = utils::XmlCharPointerToString(property->name);
  tag_name = RemoveTextBeforeColon(tag_name);

  // Static lookup dictionary matching a property name with:
  // 1. Its type (ole::VariantType)
  // 2. The property stream it has to be saved into
  // (PropertyStreamType::kSummaryInformation /
  // PropertyStreamType::kDocumentSummaryInformation)
  // 3. Its full name.
  static const auto *properties_lookup = new absl::flat_hash_map<
      std::string,
      std::tuple<ole::VariantType, PropertyStreamType, std::string>>{
      {"Application",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:Application"}},
      {"Template",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:Template"}},
      {"TotalTime",
       {ole::VariantType::kVtUI8, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:TotalTime"}},
      {"Words",
       {ole::VariantType::kVtI4, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:Words"}},
      {"Characters",
       {ole::VariantType::kVtI4, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:Characters"}},
      {"DocSecurity",
       {ole::VariantType::kVtI4, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:DocSecurity"}},
      {"RevisionNumber",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/ooxml:Properties/ooxml:RevisionNumber"}},
      {"Lines",
       {ole::VariantType::kVtI4,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Lines"}},
      {"Paragraphs",
       {ole::VariantType::kVtI4,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Paragraphs"}},
      {"Slides",
       {ole::VariantType::kVtI4,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Slides"}},
      {"Notes",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Notes"}},
      {"HiddenSlides",
       {ole::VariantType::kVtI4,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:HiddenSlides"}},
      {"MMClips",
       {ole::VariantType::kVtI4,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:MMClips"}},
      {"Company",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Company"}},
      {"Manager",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Manager"}},
      {"Language",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Language"}},
      {"Category",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:Category"}},
      {"ScaleCrop",
       {ole::VariantType::kVtBool,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:ScaleCrop"}},
      {"CharactersWithSpaces",
       {ole::VariantType::kVtI4,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:CharactersWithSpaces"}},
      {"SharedDoc",
       {ole::VariantType::kVtBool,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:SharedDoc"}},
      {"HyperlinksChanged",
       {ole::VariantType::kVtBool,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:HyperlinksChanged"}},
      {"AppVersion",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:AppVersion"}},
      {"PresentationFormat",
       {ole::VariantType::kVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:PresentationFormat"}},
      {"HeadingPairs",
       {ole::VariantType::kVtVectorVtVariant,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:HeadingPairs"}},
      {"TitlesOfParts",
       {ole::VariantType::kVtVectorVtLpstr,
        PropertyStreamType::kDocumentSummaryInformation,
        "/ooxml:Properties/ooxml:TitlesOfParts"}},
      {"title",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/dc:title"}},
      {"subject",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/dc:subject"}},
      {"description",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/dc:description"}},
      {"creator",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/dc:creator"}},
      {"lastModifiedBy",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/cp:lastModifiedBy"}},
      {"revision",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/cp:revision"}},
      {"keywords",
       {ole::VariantType::kVtLpstr, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/cp:keywords"}},
      {"created",
       {ole::VariantType::kVtFiletime, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/dcterms:created"}},
      {"modified",
       {ole::VariantType::kVtFiletime, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/dcterms:modified"}},
      {"lastPrinted",
       {ole::VariantType::kVtFiletime, PropertyStreamType::kSummaryInformation,
        "/cp:coreProperties/cp:lastPrinted"}},
  };

  auto property_iterator = properties_lookup->find(tag_name);
  if (property_iterator == properties_lookup->end()) {
    return absl::UnimplementedError(
        absl::StrCat("Property ", tag_name, " not processed!"));
  }

  auto variant_type = std::get<0>(property_iterator->second);
  auto stream_type = std::get<1>(property_iterator->second);
  const auto &property_name = std::get<2>(property_iterator->second);
  ole::PropertySetStream *property_set_stream =
      (stream_type == PropertyStreamType::kSummaryInformation
           ? proto->mutable_summary_information()
           : proto->mutable_document_summary_information());

  // Extract VtValue from the `property` XML node.
  MALDOCA_ASSIGN_OR_RETURN(auto value,
                           ExtractVtValueFromXML(variant_type, property));

  // Property stream must have exactly one property set.
  MALDOCA_RET_CHECK_EQ(property_set_stream->property_set_size(), 1,
                       _ << "Property streams are not properly initialized!");

  auto property_set = property_set_stream->mutable_property_set(0);

  MALDOCA_RET_CHECK_EQ(property_set->dictionary_size(), 1,
                       _ << "Property sets are not properly initialized!");

  // Now add the property to the *property_set_stream proto.
  auto dict_entry = property_set->mutable_dictionary(0)->add_entry();
  dict_entry->set_name(property_name);
  dict_entry->set_property_id(*property_id_counter);

  auto property_entry = property_set->add_property();
  property_entry->set_property_id(*property_id_counter);
  property_entry->set_type(variant_type);
  *(property_entry->mutable_value()) = value;

  // Incrementing property id. Note that this variable is a reference to the
  // counter created in 'ooxml_to_proto.cc'.
  ++(*property_id_counter);
  return absl::OkStatus();
}

}  // namespace maldoca
