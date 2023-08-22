// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing normal case
#line 6 "raw_ptr_fields_macro.cpp"
#define USR_INT int
#define USR_INTP int*
#define USR_CONST const
#define USR_ATTR [[fake_attribute]]
#define USR_INTP_FIELD() int* macro_ptr
#define USR_TYPE_WITH_SUFFIX(TYP) TYP##Suffix
#define USR_SYMBOL(SYM) SYM
#define USR_SYMBOL_WITH_SUFFIX(SYM) SYM##_suffix
#define USR_EQ =
#define USR_NULLPTR nullptr

// Testing isInThirdPartyLocation()
#line 19 "/src/tools/clang/plugins/tests/third_party/fake_location.cpp"
#define TP_INT int
#define TP_INTP int*
#define TP_CONST const
#define TP_ATTR [[fake_attribute]]
#define TP_INTP_FIELD() int* macro_ptr
#define TP_TYPE_WITH_SUFFIX(TYP) TYP##Suffix
#define TP_SYMBOL(SYM) SYM
#define TP_SYMBOL_WITH_SUFFIX(SYM) SYM##_suffix
#define TP_EQ =
#define TP_NULLPTR nullptr

// Testing isInLocationListedInFilterFile()
#line 32 "/src/tools/clang/plugins/tests/internal/fake_location.cpp"
#define IG_INT int
#define IG_INTP int*
#define IG_CONST const
#define IG_ATTR [[fake_attribute]]
#define IG_INTP_FIELD() int* macro_ptr
#define IG_TYPE_WITH_SUFFIX(TYP) TYP##Suffix
#define IG_SYMBOL(SYM) SYM
#define IG_SYMBOL_WITH_SUFFIX(SYM) SYM##_suffix
#define IG_EQ =
#define IG_NULLPTR nullptr

// Testing isInGeneratedLocation()
#line 45 "/src/tools/clang/plugins/tests/gen/fake_location.cpp"
#define GEN_INT int
#define GEN_INTP int*
#define GEN_CONST const
#define GEN_ATTR [[fake_attribute]]
#define GEN_INTP_FIELD() int* macro_ptr
#define GEN_TYPE_WITH_SUFFIX(TYP) TYP##Suffix
#define GEN_SYMBOL(SYM) SYM
#define GEN_SYMBOL_WITH_SUFFIX(SYM) SYM##_suffix
#define GEN_EQ =
#define GEN_NULLPTR nullptr

// Testing isSpellingInSystemHeader()
#include <raw_ptr_system_test.h>

#line 60 "raw_ptr_fields_macro.cpp"
class UsrTypSuffix;

// These `SYS_***` macro should be defined
// in `//tools/clang/plugins/tests/system/raw_ptr_system_test.h`.
struct UsrStructWithSysMacro {
  // Error.
  int* ptr0;
  // Error: typeLoc is macro but identifier is written here.
  SYS_INT* ptr1;
  // Error: typeLoc is macro but identifier is written here.
  SYS_INTP ptr2;
  // Error: typeLoc is macro but identifier is written here.
  int* SYS_CONST ptr3;
  // Error: attribute is macro but identifier is written here.
  int* SYS_ATTR ptr4;
  // OK: code owner has no control over fieldDecl.
  SYS_INTP_FIELD();
  // Error: typeLoc is macro but identifier is written here.
  SYS_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;
  // Error: identifier is defined with macro but it is written here.
  int* SYS_SYMBOL(ptr6);
  // OK: the source location for this field declaration will be "<scratch
  // space>" and the real file path cannot be detected.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr7);
  // Error: field is initialized with macro but identifier is written here.
  int* ptr8 SYS_EQ nullptr;
  // Error: field is initialized with macro but identifier is written here.
  int* ptr9 = SYS_NULLPTR;
  // OK: the source location for this field declaration will be "<scratch
  // space>" and the real file path cannot be detected.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;
};

// These `CMD_***` macro should be defined in command line arguments.
// Same as for UsrStructWithSysMacro.
struct UsrStructWithCmdMacro {
  int* ptr0;                                     // Error.
  CMD_INT* ptr1;                                 // Error.
  CMD_INTP ptr2;                                 // Error.
  int* CMD_CONST ptr3;                           // Error.
  int* CMD_ATTR ptr4;                            // Error.
  CMD_INTP_FIELD();                              // OK.
  CMD_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;           // Error.
  int* CMD_SYMBOL(ptr6);                         // Error.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 CMD_EQ nullptr;                      // Error.
  int* ptr9 = CMD_NULLPTR;                       // Error.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct UsrStructWithUsrMacro {
  // Error.
  int* ptr0;
  // Error: typeLoc is macro but identifier is written here.
  USR_INT* ptr1;
  // Error: typeLoc is macro but identifier is written here.
  USR_INTP ptr2;
  // Error: typeLoc is macro but identifier is written here.
  int* USR_CONST ptr3;
  // Error: attribute is macro but identifier is written here.
  int* USR_ATTR ptr4;
  // Error: user has control over the macro.
  USR_INTP_FIELD();
  // Error: user has control over the macro.
  USR_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;
  // Error: identifier is defined with macro but it is written here.
  int* USR_SYMBOL(ptr6);
  // OK: the source location for this field declaration will be "<scratch
  // space>" and the real file path cannot be detected.
  int* USR_SYMBOL_WITH_SUFFIX(ptr7);
  // Error: field is initialized with macro but identifier is written here.
  int* ptr8 USR_EQ nullptr;
  // Error: field is initialized with macro but identifier is written here.
  int* ptr9 = USR_NULLPTR;
  // OK: the source location for this field declaration will be "<scratch
  // space>" and the real file path cannot be detected.
  int* USR_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;
};

// Same as for UsrStructWithSysMacro.
struct UsrStructWithThirdPartyMacro {
  int* ptr0;                                    // Error.
  TP_INT* ptr1;                                 // Error.
  TP_INTP ptr2;                                 // Error.
  int* TP_CONST ptr3;                           // Error.
  int* TP_ATTR ptr4;                            // Error.
  TP_INTP_FIELD();                              // OK.
  TP_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;           // Error.
  int* TP_SYMBOL(ptr6);                         // Error.
  int* TP_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 TP_EQ nullptr;                      // Error.
  int* ptr9 = TP_NULLPTR;                       // Error.
  int* TP_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

// Same as for UsrStructWithSysMacro.
struct UsrStructWithManuallyIgnoredMacro {
  int* ptr0;                                    // Error.
  IG_INT* ptr1;                                 // Error.
  IG_INTP ptr2;                                 // Error.
  int* IG_CONST ptr3;                           // Error.
  int* IG_ATTR ptr4;                            // Error.
  IG_INTP_FIELD();                              // OK.
  IG_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;           // Error.
  int* IG_SYMBOL(ptr6);                         // Error.
  int* IG_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 IG_EQ nullptr;                      // Error.
  int* ptr9 = IG_NULLPTR;                       // Error.
  int* IG_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

// Same as for UsrStructWithSysMacro.
struct UsrStructWithGeneratedMacro {
  int* ptr0;                                     // Error.
  GEN_INT* ptr1;                                 // Error.
  GEN_INTP ptr2;                                 // Error.
  int* GEN_CONST ptr3;                           // Error.
  int* GEN_ATTR ptr4;                            // Error.
  GEN_INTP_FIELD();                              // OK.
  GEN_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;           // Error.
  int* GEN_SYMBOL(ptr6);                         // Error.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 GEN_EQ nullptr;                      // Error.
  int* ptr9 = GEN_NULLPTR;                       // Error.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

#line 188 "/src/tools/clang/plugins/tests/third_party/fake_location.cpp"
class ThirdPartyTypSuffix;

struct ThirdPartyStructWithSysMacro {
  int* ptr0;                                     // OK.
  SYS_INT* ptr1;                                 // OK.
  SYS_INTP ptr2;                                 // OK.
  int* SYS_CONST ptr3;                           // OK.
  int* SYS_ATTR ptr4;                            // OK.
  SYS_INTP_FIELD();                              // OK.
  SYS_TYPE_WITH_SUFFIX(ThirdPartyTyp) * ptr5;    // OK.
  int* SYS_SYMBOL(ptr6);                         // OK.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 SYS_EQ nullptr;                      // OK.
  int* ptr9 = SYS_NULLPTR;                       // OK.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct ThirdPartyStructWithCmdMacro {
  int* ptr0;                                     // OK.
  CMD_INT* ptr1;                                 // OK.
  CMD_INTP ptr2;                                 // OK.
  int* CMD_CONST ptr3;                           // OK.
  int* CMD_ATTR ptr4;                            // OK.
  CMD_INTP_FIELD();                              // OK.
  CMD_TYPE_WITH_SUFFIX(ThirdPartyTyp) * ptr5;    // OK.
  int* CMD_SYMBOL(ptr6);                         // OK.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 CMD_EQ nullptr;                      // OK.
  int* ptr9 = CMD_NULLPTR;                       // OK.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct ThirdPartyStructWithUsrMacro {
  int* ptr0;                                     // OK.
  USR_INT* ptr1;                                 // OK.
  USR_INTP ptr2;                                 // OK.
  int* USR_CONST ptr3;                           // OK.
  int* USR_ATTR ptr4;                            // OK.
  USR_INTP_FIELD();                              // Error.
  USR_TYPE_WITH_SUFFIX(ThirdPartyTyp) * ptr5;    // OK.
  int* USR_SYMBOL(ptr6);                         // OK.
  int* USR_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 USR_EQ nullptr;                      // OK.
  int* ptr9 = USR_NULLPTR;                       // OK.
  int* USR_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct ThirdPartyStructWithThirdPartyMacro {
  int* ptr0;                                    // OK.
  TP_INT* ptr1;                                 // OK.
  TP_INTP ptr2;                                 // OK.
  int* TP_CONST ptr3;                           // OK.
  int* TP_ATTR ptr4;                            // OK.
  TP_INTP_FIELD();                              // OK.
  TP_TYPE_WITH_SUFFIX(ThirdPartyTyp) * ptr5;    // OK.
  int* TP_SYMBOL(ptr6);                         // OK.
  int* TP_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 TP_EQ nullptr;                      // OK.
  int* ptr9 = TP_NULLPTR;                       // OK.
  int* TP_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct ThirdPartyStructWithManuallyIgnoredMacro {
  int* ptr0;                                    // OK.
  IG_INT* ptr1;                                 // OK.
  IG_INTP ptr2;                                 // OK.
  int* IG_CONST ptr3;                           // OK.
  int* IG_ATTR ptr4;                            // OK.
  IG_INTP_FIELD();                              // OK.
  IG_TYPE_WITH_SUFFIX(ThirdPartyTyp) * ptr5;    // OK.
  int* IG_SYMBOL(ptr6);                         // OK.
  int* IG_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 IG_EQ nullptr;                      // OK.
  int* ptr9 = IG_NULLPTR;                       // OK.
  int* IG_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct ThirdPartyStructWithGeneratedMacro {
  int* ptr0;                                     // OK.
  GEN_INT* ptr1;                                 // OK.
  GEN_INTP ptr2;                                 // OK.
  int* GEN_CONST ptr3;                           // OK.
  int* GEN_ATTR ptr4;                            // OK.
  GEN_INTP_FIELD();                              // OK.
  GEN_TYPE_WITH_SUFFIX(ThirdPartyTyp) * ptr5;    // OK.
  int* GEN_SYMBOL(ptr6);                         // OK.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 GEN_EQ nullptr;                      // OK.
  int* ptr9 = GEN_NULLPTR;                       // OK.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

#line 281 "/src/tools/clang/plugins/tests/internal/fake_location.cpp"
class ManuallyIgnoredTypSuffix;

struct ManuallyIgnoredStructWithSysMacro {
  int* ptr0;                                        // OK.
  SYS_INT* ptr1;                                    // OK.
  SYS_INTP ptr2;                                    // OK.
  int* SYS_CONST ptr3;                              // OK.
  int* SYS_ATTR ptr4;                               // OK.
  SYS_INTP_FIELD();                                 // OK.
  SYS_TYPE_WITH_SUFFIX(ManuallyIgnoredTyp) * ptr5;  // OK.
  int* SYS_SYMBOL(ptr6);                            // OK.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr7);                // OK.
  int* ptr8 SYS_EQ nullptr;                         // OK.
  int* ptr9 = SYS_NULLPTR;                          // OK.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;     // OK.
};

struct ManuallyIgnoredStructWithCmdMacro {
  int* ptr0;                                        // OK.
  CMD_INT* ptr1;                                    // OK.
  CMD_INTP ptr2;                                    // OK.
  int* CMD_CONST ptr3;                              // OK.
  int* CMD_ATTR ptr4;                               // OK.
  CMD_INTP_FIELD();                                 // OK.
  CMD_TYPE_WITH_SUFFIX(ManuallyIgnoredTyp) * ptr5;  // OK.
  int* CMD_SYMBOL(ptr6);                            // OK.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr7);                // OK.
  int* ptr8 CMD_EQ nullptr;                         // OK.
  int* ptr9 = CMD_NULLPTR;                          // OK.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;     // OK.
};

struct ManuallyIgnoredStructWithUsrMacro {
  int* ptr0;                                        // OK.
  USR_INT* ptr1;                                    // OK.
  USR_INTP ptr2;                                    // OK.
  int* USR_CONST ptr3;                              // OK.
  int* USR_ATTR ptr4;                               // OK.
  USR_INTP_FIELD();                                 // Error.
  USR_TYPE_WITH_SUFFIX(ManuallyIgnoredTyp) * ptr5;  // OK.
  int* USR_SYMBOL(ptr6);                            // OK.
  int* USR_SYMBOL_WITH_SUFFIX(ptr7);                // OK.
  int* ptr8 USR_EQ nullptr;                         // OK.
  int* ptr9 = USR_NULLPTR;                          // OK.
  int* USR_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;     // OK.
};

struct ManuallyIgnoredStructWithThirdPartyMacro {
  int* ptr0;                                       // OK.
  TP_INT* ptr1;                                    // OK.
  TP_INTP ptr2;                                    // OK.
  int* TP_CONST ptr3;                              // OK.
  int* TP_ATTR ptr4;                               // OK.
  TP_INTP_FIELD();                                 // OK.
  TP_TYPE_WITH_SUFFIX(ManuallyIgnoredTyp) * ptr5;  // OK.
  int* TP_SYMBOL(ptr6);                            // OK.
  int* TP_SYMBOL_WITH_SUFFIX(ptr7);                // OK.
  int* ptr8 TP_EQ nullptr;                         // OK.
  int* ptr9 = TP_NULLPTR;                          // OK.
  int* TP_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;     // OK.
};

struct ManuallyIgnoredStructWithManuallyIgnoredMacro {
  int* ptr0;                                       // OK.
  IG_INT* ptr1;                                    // OK.
  IG_INTP ptr2;                                    // OK.
  int* IG_CONST ptr3;                              // OK.
  int* IG_ATTR ptr4;                               // OK.
  IG_INTP_FIELD();                                 // OK.
  IG_TYPE_WITH_SUFFIX(ManuallyIgnoredTyp) * ptr5;  // OK.
  int* IG_SYMBOL(ptr6);                            // OK.
  int* IG_SYMBOL_WITH_SUFFIX(ptr7);                // OK.
  int* ptr8 IG_EQ nullptr;                         // OK.
  int* ptr9 = IG_NULLPTR;                          // OK.
  int* IG_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;     // OK.
};

struct ManuallyIgnoredStructWithGeneratedMacro {
  int* ptr0;                                        // OK.
  GEN_INT* ptr1;                                    // OK.
  GEN_INTP ptr2;                                    // OK.
  int* GEN_CONST ptr3;                              // OK.
  int* GEN_ATTR ptr4;                               // OK.
  GEN_INTP_FIELD();                                 // OK.
  GEN_TYPE_WITH_SUFFIX(ManuallyIgnoredTyp) * ptr5;  // OK.
  int* GEN_SYMBOL(ptr6);                            // OK.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr7);                // OK.
  int* ptr8 GEN_EQ nullptr;                         // OK.
  int* ptr9 = GEN_NULLPTR;                          // OK.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;     // OK.
};

#line 374 "/src/tools/clang/plugins/tests/gen/fake_location.cpp"
class GeneratedTypSuffix;

struct GeneratedStructWithSysMacro {
  int* ptr0;                                     // OK.
  SYS_INT* ptr1;                                 // OK.
  SYS_INTP ptr2;                                 // OK.
  int* SYS_CONST ptr3;                           // OK.
  int* SYS_ATTR ptr4;                            // OK.
  SYS_INTP_FIELD();                              // OK.
  SYS_TYPE_WITH_SUFFIX(GeneratedTyp) * ptr5;     // OK.
  int* SYS_SYMBOL(ptr6);                         // OK.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 SYS_EQ nullptr;                      // OK.
  int* ptr9 = SYS_NULLPTR;                       // OK.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct GeneratedStructWithCmdMacro {
  int* ptr0;                                     // OK.
  CMD_INT* ptr1;                                 // OK.
  CMD_INTP ptr2;                                 // OK.
  int* CMD_CONST ptr3;                           // OK.
  int* CMD_ATTR ptr4;                            // OK.
  CMD_INTP_FIELD();                              // OK.
  CMD_TYPE_WITH_SUFFIX(GeneratedTyp) * ptr5;     // OK.
  int* CMD_SYMBOL(ptr6);                         // OK.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 CMD_EQ nullptr;                      // OK.
  int* ptr9 = CMD_NULLPTR;                       // OK.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct GeneratedStructWithUsrMacro {
  int* ptr0;                                     // OK.
  USR_INT* ptr1;                                 // OK.
  USR_INTP ptr2;                                 // OK.
  int* USR_CONST ptr3;                           // OK.
  int* USR_ATTR ptr4;                            // OK.
  USR_INTP_FIELD();                              // Error.
  USR_TYPE_WITH_SUFFIX(GeneratedTyp) * ptr5;     // OK.
  int* USR_SYMBOL(ptr6);                         // OK.
  int* USR_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 USR_EQ nullptr;                      // OK.
  int* ptr9 = USR_NULLPTR;                       // OK.
  int* USR_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct GeneratedStructWithThirdPartyMacro {
  int* ptr0;                                    // OK.
  TP_INT* ptr1;                                 // OK.
  TP_INTP ptr2;                                 // OK.
  int* TP_CONST ptr3;                           // OK.
  int* TP_ATTR ptr4;                            // OK.
  TP_INTP_FIELD();                              // OK.
  TP_TYPE_WITH_SUFFIX(GeneratedTyp) * ptr5;     // OK.
  int* TP_SYMBOL(ptr6);                         // OK.
  int* TP_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 TP_EQ nullptr;                      // OK.
  int* ptr9 = TP_NULLPTR;                       // OK.
  int* TP_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct GeneratedStructWithManuallyIgnoredMacro {
  int* ptr0;                                    // OK.
  IG_INT* ptr1;                                 // OK.
  IG_INTP ptr2;                                 // OK.
  int* IG_CONST ptr3;                           // OK.
  int* IG_ATTR ptr4;                            // OK.
  IG_INTP_FIELD();                              // OK.
  IG_TYPE_WITH_SUFFIX(GeneratedTyp) * ptr5;     // OK.
  int* IG_SYMBOL(ptr6);                         // OK.
  int* IG_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 IG_EQ nullptr;                      // OK.
  int* ptr9 = IG_NULLPTR;                       // OK.
  int* IG_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};

struct GeneratedStructWithGeneratedMacro {
  int* ptr0;                                     // OK.
  GEN_INT* ptr1;                                 // OK.
  GEN_INTP ptr2;                                 // OK.
  int* GEN_CONST ptr3;                           // OK.
  int* GEN_ATTR ptr4;                            // OK.
  GEN_INTP_FIELD();                              // OK.
  GEN_TYPE_WITH_SUFFIX(GeneratedTyp) * ptr5;     // OK.
  int* GEN_SYMBOL(ptr6);                         // OK.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr7);             // OK.
  int* ptr8 GEN_EQ nullptr;                      // OK.
  int* ptr9 = GEN_NULLPTR;                       // OK.
  int* GEN_SYMBOL_WITH_SUFFIX(ptr10) = nullptr;  // OK.
};
