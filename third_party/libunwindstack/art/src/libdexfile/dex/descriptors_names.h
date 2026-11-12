/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_DESCRIPTORS_NAMES_H_
#define ART_LIBDEXFILE_DEX_DESCRIPTORS_NAMES_H_

#include <string>

#include "dex/primitive.h"

namespace art {

// Used to implement PrettyClass, PrettyField, PrettyMethod, and PrettyTypeOf,
// one of which is probably more useful to you.
// Returns a human-readable equivalent of 'descriptor'. So "I" would be "int",
// "[[I" would be "int[][]", "[Ljava/lang/String;" would be
// "java.lang.String[]", and so forth.
void AppendPrettyDescriptor(const char* descriptor, std::string* result);
std::string PrettyDescriptor(const char* descriptor);
std::string PrettyDescriptor(Primitive::Type type);

// Used to convert user-specified ignored types ("java.lang.String[]",
// "byte[][]") to the form returned by art::mirror::Class->GetDescriptor()
// ("[Ljava/lang/String;", "[[B").
std::string InversePrettyDescriptor(const std::string& pretty_descriptor);

// Performs JNI name mangling as described in section 11.3 "Linking Native Methods"
// of the JNI spec.
std::string MangleForJni(const std::string& s);

std::string GetJniShortName(const std::string& class_name, const std::string& method_name);

// Turn "java.lang.String" into "Ljava/lang/String;".
std::string DotToDescriptor(const char* class_name);

// Turn "Ljava/lang/String;" into "java.lang.String" using the conventions of
// java.lang.Class.getName().
std::string DescriptorToDot(const char* descriptor);

// Turn "Ljava/lang/String;" into "java/lang/String" using the opposite conventions of
// java.lang.Class.getName().
std::string DescriptorToName(const char* descriptor);

// Tests for whether 's' is a valid class name in the three common forms:
bool IsValidBinaryClassName(const char* s);  // "java.lang.String"
bool IsValidJniClassName(const char* s);     // "java/lang/String"
bool IsValidDescriptor(const char* s);       // "Ljava/lang/String;"

// Returns whether the given string is a valid field or method name,
// additionally allowing names that begin with '<' and end with '>'.
bool IsValidMemberName(const char* s);

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DESCRIPTORS_NAMES_H_
