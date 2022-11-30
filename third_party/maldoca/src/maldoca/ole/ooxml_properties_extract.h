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

#ifndef MALDOCA_OLE_OOXML_PROPERTIES_EXTRACT_
#define MALDOCA_OLE_OOXML_PROPERTIES_EXTRACT_

#include "absl/status/status.h"
#include "libxml/parser.h"
#include "maldoca/base/statusor.h"
#include "maldoca/ole/proto/ooxml.pb.h"

namespace maldoca {
// Extracts the VtValue from the XML node `property_xml_node` and returns it. If
// there is an error while parsing the value, then the error message is returned
// instead. `type` represents the type of the parsed value, and
// `property_xml_node` the location inside the XML document.
StatusOr<ole::VtValue> ExtractVtValueFromXML(ole::VariantType type,
                                             const xmlNode *property_xml_node);

// Tries to parse an OOXML property from the libXML node `property` and store it
// in the `OOXMLFile` proto.
// `property_id_counter` is used for giving unique ids to the extracted
// properties.
absl::Status ExtractOOXMLPropertyToProto(const xmlNode *property,
                                         ooxml::OOXMLFile *proto,
                                         int *property_id_counter);
}  // namespace maldoca

#endif  // MALDOCA_OLE_OOXML_PROPERTIES_EXTRACT_
