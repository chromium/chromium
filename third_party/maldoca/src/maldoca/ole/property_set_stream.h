/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Read PropertySetStream structure (and all sub-structures) form OLE stream
//
// PropertySetStream is the main structure used to define document metadata
// (author, title etc) in OLE2 files. Microsoft provides thorough description in
// the document named:
// "[MS-OLEPS]: Object Linking and Embedding (OLE) Property Set Data Structures"
// It can be accessed through this link:
// https://msdn.microsoft.com/en-us/library/dd942421.aspx
//
// Main use case:
//   maldoca::OLEPropertySetStream property_set_stream;
//   property_set_stream.Read(string_view,
//                            maldoca::ole::PropertySetStream);
// it will parse whole PropertySetStream structure and put it into
// maldoca::ole::PropertySetStream proto buf
//
// Additional public functions are provided to read just the specific fields
// of the PropertySetStream (string_view has to point to the buffer with the
// specific structure):
//  - PropertySetReader() - PropertySet structure
//  - DictionaryPacketReader() - Dictionary structure
//  - TypedPropertyValueReader() - TypedPropertyValue structure
//  - VtValueReader() - similar to VARIANT type, according to specification
//                      inside TypedPropertyValue
//  - VtVectorReader() - Vector and Array Property  structure
//  - CodePageStringReader() - CodePageString structure
//  - UnicodeStringReader() - UnicodeString structure

#ifndef MALDOCA_OLE_PROPERTY_SET_STREAM_H_
#define MALDOCA_OLE_PROPERTY_SET_STREAM_H_

#include "maldoca/base/statusor.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/proto/ole.pb.h"

namespace maldoca {

class OLEPropertySetStream {
 public:
  enum PropertyIdentifier {
    kDictionary,
    kNormal,
    kCodePage,
    kLocale,
    kBehavior
  };
  // enum based on VARENUM definition from Windows SDK (wtypes.h):
  // windows_sdk/windows_sdk_10/files/include/10.0.10586.0/shared/wtypes.h
  enum VariantType : uint16_t {
    kVtEmpty = 0x0000,
    kVtNull = 0x0001,
    kVtI2 = 0x0002,
    kVtI4 = 0x0003,
    kVtR4 = 0x0004,
    kVtR8 = 0x0005,
    kVtCy = 0x0006,
    kVtDate = 0x0007,
    kVtBstr = 0x0008,
    kVtError = 0x000A,
    kVtBool = 0x000B,
    kVtVariant = 0x000C,
    kVtI1 = 0x0010,
    kVtUI1 = 0x0011,
    kVtUI2 = 0x0012,
    kVtUI4 = 0x0013,
    kVtI8 = 0x0014,
    kVtUI8 = 0x0015,
    kVtLpstr = 0x001E,
    kVtLpwstr = 0x001F,
    kVtFiletime = 0x0040,
    kVtBlob = 0x0041,
    kVtVector = 0x1000,
    // below types are basically VT_VECTOR | base type, they are defined mostly
    // for the convenience, since only those two vector types are used within
    // OLE2 objects.
    kVtVectorVtVariant = kVtVector | kVtVariant,
    kVtVectorVtLpstr = kVtVector | kVtLpstr,
  };
  enum CodePage {
    kCpWinUnicode = 0x04B0,
  };

  OLEPropertySetStream();
  // reads PropertySetStream as defined here:
  // https://msdn.microsoft.com/en-us/library/dd942207.aspx
  // data occupied by the PropertySetStream is NOT removed from the stream
  bool Read(absl::string_view stream,
            maldoca::ole::PropertySetStream *prop_set_stream);
  // reads PropertySet as defined here:
  // https://msdn.microsoft.com/en-us/library/dd942379.aspx
  // data occupied by the PropertySet is NOT removed from the stream
  bool PropertySetReader(absl::string_view stream,
                         maldoca::ole::PropertySet *prop_set);
  // reads Dictionary as defined here:
  // https://msdn.microsoft.com/en-us/library/dd942320.aspx
  // data occupied by the Dictionary is NOT removed from the stream
  bool DictionaryPacketReader(absl::string_view stream,
                              maldoca::ole::Dictionary *dict);
  // reads TypedPropertyValue as defined here:
  // https://msdn.microsoft.com/en-us/library/dd942532.aspx
  // data occupied by the TypedPropertyValue is removed from the stream
  // returns type and VtValue in the output parameters
  bool TypedValueReader(absl::string_view *stream, uint16_t *type,
                        maldoca::ole::VtValue *value);
  // reads TypedPropertyValue and stores it inside Property message, it is very
  // similar to the TypedValueReader(), but data occupied by the
  // TypedPropertyValue is NOT removed from the stream
  bool TypedPropertyValueReader(absl::string_view stream,
                                maldoca::ole::Property *property);
  // reads VARIANT value as defined here:
  // https://msdn.microsoft.com/en-us/library/dd942532.aspx
  // data occupied by VARIANT value is removed from the stream string_view
  bool VtValueReader(absl::string_view *stream, uint16_t type,
                     maldoca::ole::VtValue *value);
  // reads VtVector according to the specification:
  // https://msdn.microsoft.com/en-us/library/dd942011.aspx
  // data occupied by the VtVector is NOT removed from the stream
  bool VtVectorReader(absl::string_view *stream, uint16_t type,
                      maldoca::ole::VtVector *vec);
  // reads TypedPropertyValue of type VT_I4
  bool VtInt4Reader(absl::string_view *stream, int32_t *value);
  // reads TypedPropertyValue of type VT_LPWSTR
  bool VtLpwstrReader(absl::string_view *stream, std::string *value);
  // reads VecVtHyperlink structure as defined here:
  // https://msdn.microsoft.com/en-us/library/dd908491.aspx
  // data occupied by VecVtHyperlink is removed from the stream string_view
  // results are stored in the links output parameter
  void VecVtHyperlinkReader(
      absl::string_view *stream,
      google::protobuf::RepeatedPtrField<maldoca::ole::VtHyperlink> *links);
  // reads the CodePageString structure as defined here:
  // https://msdn.microsoft.com/en-us/library/dd942354.aspx
  // data occupied by CodePageString is removed from the stream string_view
  // returns proper utf-8 encoded string
  bool CodePageStringReader(absl::string_view *stream, std::string *output);
  // reads the UnicodeString/Lpwstr as defined here:
  // https://msdn.microsoft.com/en-us/library/dd910574.aspx
  // data occupied by UnicodeString is removed from the stream string_view
  // returns proper utf-8 encoded string
  bool UnicodeStringReader(absl::string_view *stream, std::string *output);
  // facilitates extracting Property value for the given user defined property
  // name from the DocumentSummaryinformation PropertySetStream
  // https://msdn.microsoft.com/en-us/library/dd946752.aspx
  static StatusOr<const maldoca::ole::Property *> GetPropertyFromDictionary(
      absl::string_view name, const maldoca::ole::PropertySetStream &stream);

 private:
  StatusOr<maldoca::ole::VariantType> ConvertTypeToProtoVariantType(
      uint16_t type);
  static PropertyIdentifier GetPropertyIdentifier(uint32_t v);
  void ConsumePaddingBytes(uint32_t size, uint32_t padd_to,
                           absl::string_view *stream);
  std::string encoding_name_;
};

}  // namespace maldoca

#endif  // MALDOCA_OLE_PROPERTY_SET_STREAM_H_
