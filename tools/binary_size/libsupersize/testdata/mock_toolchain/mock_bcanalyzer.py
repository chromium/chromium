# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

# C++ source to generate data for test.o (note that changing "uint" to "int"
# would still produces the same output):
# ----- test.cc -----
# #include <stdint.h>
# #include <string>
#
# int test() {
#   static auto s8a = "Test1a";
#   static auto s8b = "Test1b";
#   static struct {
#     const char* x;
#     const char* y;
#   } s8s = {"Test2a", "Test2b"};
#   static auto s16a = u"Test3a";
#   static auto s16b = u"Test3b";
#   static auto s32a = U"Test4a";
#   static auto s32b = U"Test4b";
#   static bool i1a[] = {true, false, false, true, true, false};
#   static bool i1b[] = {true, false, false, true, true, true};
#   static uint8_t i8a[] = {'T', 'e', 's', 't', '5', 'a', 0};
#   static uint8_t i8b[] = {'T', 'e', 's', 't', '5', 'b', 1};
#   static uint16_t i16a[] = {'T', 'e', 's', 't', '6', 'a', 0};
#   static uint16_t i16b[] = {'T', 'e', 's', 't', '6', 'b', 1};
#   static uint32_t i32a[] = {'T', 'e', 's', 't', '7', 'a', 0};
#   static uint32_t i32b[] = {'T', 'e', 's', 't', '7', 'b', 1};
#   static std::string ssa = "Test8a";
#   static std::string ssb = "Test8b";
#   static uint8_t u8a = 0;
#   static uint8_t u8b = 10;
#   static uint16_t u16a = 0;
#   static uint16_t u16b = 1000;
#   static uint32_t u32a = 0;
#   static uint32_t u32b = 1000000;
#   static uint64_t u64a = 0;
#   static uint64_t u64b = 1000000000000LL;
#   static auto s8empty = "";
#   static auto s16empty = u"";
#   static auto s32empty = U"";
#   static auto s8a_suffix = "1a";
#   static uint8_t zeros[256] = {0};
#   return 0;
# }
# -------------------

# Commands to generate dump:
#   clang -c -std=c++11 -emit-llvm -O0 test.cc -o test.o
#   llvm-bcanalyzer -dump --disable-histogram test.o

_OUTPUTS = {
    'test.o': [
        '<IDENTIFICATION_BLOCK_ID NumWords=5 BlockCodeSize=5>',
        ('  <STRING abbrevid=4 op0=76 op1=76 op2=86 op3=77 op4=52 op5=46 '
         'op6=48 op7=46 op8=49/> record string = \'LLVM4.0.1\''),
        '  <EPOCH abbrevid=5 op0=0/>',
        '</IDENTIFICATION_BLOCK_ID>',
        '<MODULE_BLOCK NumWords=1288 BlockCodeSize=3>',
        '  <VERSION op0=1/>',
        '  <BLOCKINFO_BLOCK/>',
        '  <PARAMATTR_GROUP_BLOCK_ID NumWords=442 BlockCodeSize=3>',
        # The following <ENTRY> is abridged (has 423 items that don't get used).
        '    <ENTRY op0=1 op1=4294967295 op2=0/>',
        # Omit more <ENTRY> lines that don't get used.
        '  </PARAMATTR_GROUP_BLOCK_ID>',
        '  <PARAMATTR_BLOCK NumWords=4 BlockCodeSize=3>',
        '    <ENTRY op0=1/>',
        '    <ENTRY op0=2/>',
        '    <ENTRY op0=3/>',
        '    <ENTRY op0=4 op1=5/>',
        '    <ENTRY op0=4/>',
        '  </PARAMATTR_BLOCK>',
        # Key target: <TYPE_BLOCK_ID>.
        '  <TYPE_BLOCK_ID NumWords=103 BlockCodeSize=4>',
        # Meta data: <NUMENTRY>: There are 62 types, with ids in [0, 61].
        '    <NUMENTRY op0=62/>',
        # Type id = 0: int8.
        '    <INTEGER op0=8/>',
        # Type id = 1: Pointer to type id = 0 ==> *int8.
        '    <POINTER abbrevid=4 op0=0 op1=0/>',
        '    <POINTER abbrevid=4 op0=1 op1=0/>',
        # Type id = 3: 7-element array of type id = 0 ==> int8[7].
        '    <ARRAY abbrevid=9 op0=7 op1=0/>',
        '    <POINTER abbrevid=4 op0=3 op1=0/>',
        # Type id = 5 for <STRUCT_NAME> + <STRUCT_NAMED>.
        ('    <STRUCT_NAME abbrevid=7 op0=115 op1=116 op2=114 op3=117 op4=99 '
         'op5=116 op6=46 op7=97 op8=110 op9=111 op10=110/> record string = '
         '\'struct.anon\''),
        '    <STRUCT_NAMED abbrevid=8 op0=0 op1=1 op2=1/>',
        '    <POINTER abbrevid=4 op0=5 op1=0/>',
        # Type id = 7: int16.
        '    <INTEGER op0=16/>',
        '    <POINTER abbrevid=4 op0=7 op1=0/>',
        '    <POINTER abbrevid=4 op0=8 op1=0/>',
        # Type id = 10: 7-element array of type id = 7 ==> int16[7].
        '    <ARRAY abbrevid=9 op0=7 op1=7/>',
        '    <POINTER abbrevid=4 op0=10 op1=0/>',
        # Type id = 12: int32.
        '    <INTEGER op0=32/>',
        '    <POINTER abbrevid=4 op0=12 op1=0/>',
        '    <POINTER abbrevid=4 op0=13 op1=0/>',
        # Type id = 15: 7-element array of type id = 12 ==> int32[7].
        '    <ARRAY abbrevid=9 op0=7 op1=12/>',
        '    <POINTER abbrevid=4 op0=15 op1=0/>',
        # Type id = 17: 6-element array of type id = 0 ==> int8[6].
        '    <ARRAY abbrevid=9 op0=6 op1=0/>',
        '    <POINTER abbrevid=4 op0=17 op1=0/>',
        # Type id = 19 for <STRUCT_NAME> + <STRUCT_NAMED>.
        ('    <STRUCT_NAME op0=115 op1=116 op2=114 op3=117 op4=99 op5=116 '
         'op6=46 op7=115 op8=116 op9=100 op10=58 op11=58 op12=95 op13=95 '
         'op14=99 op15=120 op16=120 op17=49 op18=49 op19=58 op20=58 op21=98 '
         'op22=97 op23=115 op24=105 op25=99 op26=95 op27=115 op28=116 op29=114 '
         'op30=105 op31=110 op32=103 op33=60 op34=99 op35=104 op36=97 op37=114 '
         'op38=44 op39=32 op40=115 op41=116 op42=100 op43=58 op44=58 op45=99 '
         'op46=104 op47=97 op48=114 op49=95 op50=116 op51=114 op52=97 op53=105 '
         'op54=116 op55=115 op56=60 op57=99 op58=104 op59=97 op60=114 op61=62 '
         'op62=44 op63=32 op64=115 op65=116 op66=100 op67=58 op68=58 op69=97 '
         'op70=108 op71=108 op72=111 op73=99 op74=97 op75=116 op76=111 '
         'op77=114 op78=60 op79=99 op80=104 op81=97 op82=114 op83=62 op84=32 '
         'op85=62 op86=58 op87=58 op88=95 op89=65 op90=108 op91=108 op92=111 '
         'op93=99 op94=95 op95=104 op96=105 op97=100 op98=101 op99=114/>'),
        '    <STRUCT_NAMED abbrevid=8 op0=0 op1=1/>',
        # Type id = 20: int64.
        '    <INTEGER op0=64/>',
        # Type id = 21: 8-element aray of type id = 0 ==> int8[8].
        '    <ARRAY abbrevid=9 op0=8 op1=0/>',
        # Type id = 22 for <STRUCT_NAME> + <STRUCT_NAMED>.
        ('    <STRUCT_NAME abbrevid=7 op0=117 op1=110 op2=105 op3=111 op4=110 '
         'op5=46 op6=97 op7=110 op8=111 op9=110/> record string = '
         '\'union.anon\''),
        '    <STRUCT_NAMED abbrevid=8 op0=0 op1=20 op2=21/>',
        # Type id = 23 for <STRUCT_NAME> + <STRUCT_NAMED>.
        ('    <STRUCT_NAME op0=99 op1=108 op2=97 op3=115 op4=115 op5=46 '
         'op6=115 op7=116 op8=100 op9=58 op10=58 op11=95 op12=95 op13=99 '
         'op14=120 op15=120 op16=49 op17=49 op18=58 op19=58 op20=98 op21=97 '
         'op22=115 op23=105 op24=99 op25=95 op26=115 op27=116 op28=114 '
         'op29=105 op30=110 op31=103/>'),
        '    <STRUCT_NAMED abbrevid=8 op0=0 op1=19 op2=20 op3=22/>',
        '    <POINTER abbrevid=4 op0=23 op1=0/>',
        '    <POINTER abbrevid=4 op0=20 op1=0/>',
        # Type id = 26: 1-element array of type id = 0 ==> int8[1].
        '    <ARRAY abbrevid=9 op0=1 op1=0/>',
        '    <POINTER abbrevid=4 op0=26 op1=0/>',
        # Type id = 28: 1-element array of type id = 7 ==> int16[1].
        '    <ARRAY abbrevid=9 op0=1 op1=7/>',
        '    <POINTER abbrevid=4 op0=28 op1=0/>',
        # Type id = 30: 1-element array of type id = 12 ==> int32[1].
        '    <ARRAY abbrevid=9 op0=1 op1=12/>',
        '    <POINTER abbrevid=4 op0=30 op1=0/>',
        # Type id = 32: 3-element array of type id = 0 ==> int8[3].
        '    <ARRAY abbrevid=9 op0=3 op1=0/>',
        '    <POINTER abbrevid=4 op0=32 op1=0/>',
        # Type id = 34: 256-element array of type id = 0 ==> int8[256].
        '    <ARRAY abbrevid=9 op0=256 op1=0/>',
        '    <POINTER abbrevid=4 op0=34 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=12/>',
        '    <POINTER abbrevid=4 op0=36 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=12 op2=25/>',
        '    <POINTER abbrevid=4 op0=38 op1=0/>',
        # Type id = 40.
        '    <VOID/>',
        # Type id = 41 for <STRUCT_NAME> + <STRUCT_NAMED>.
        ('    <STRUCT_NAME op0=99 op1=108 op2=97 op3=115 op4=115 op5=46 '
         'op6=115 op7=116 op8=100 op9=58 op10=58 op11=97 op12=108 op13=108 '
         'op14=111 op15=99 op16=97 op17=116 op18=111 op19=114/>'),
        '    <STRUCT_NAMED abbrevid=8 op0=0 op1=0/>',
        '    <POINTER abbrevid=4 op0=41 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=40 op2=42/> record string = \'(*\'',
        '    <POINTER abbrevid=4 op0=43 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=40 op2=24 op3=1 op4=42/>',
        '    <POINTER abbrevid=4 op0=45 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=1 op1=12/>',
        '    <POINTER abbrevid=4 op0=47 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=40 op2=24/>',
        # Type id = 50.
        '    <POINTER abbrevid=4 op0=49 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=40 op2=1/>',
        '    <POINTER abbrevid=4 op0=51 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=12 op2=52 op3=1 op4=1/>',
        '    <POINTER abbrevid=4 op0=53 op1=0/>',
        '    <FUNCTION abbrevid=5 op0=0 op1=40 op2=25/>',
        '    <POINTER abbrevid=4 op0=55 op1=0/>',
        # Type id = 57: 1-bit integer (but not bool!).
        '    <INTEGER op0=1/>',
        '    <POINTER abbrevid=4 op0=57 op1=0/>',
        '    <METADATA/>',
        '    <LABEL/>',
        # Type id = 61.
        '    <STRUCT_ANON abbrevid=6 op0=0 op1=1 op2=12/>',
        '  </TYPE_BLOCK_ID>',
        ('  <TRIPLE op0=120 op1=56 op2=54 op3=95 op4=54 op5=52 op6=45 op7=112 '
         'op8=99 op9=45 op10=108 op11=105 op12=110 op13=117 op14=120 op15=45 '
         'op16=103 op17=110 op18=117/>'),
        ('  <DATALAYOUT op0=101 op1=45 op2=109 op3=58 op4=101 op5=45 op6=105 '
         'op7=54 op8=52 op9=58 op10=54 op11=52 op12=45 op13=102 op14=56 '
         'op15=48 op16=58 op17=49 op18=50 op19=56 op20=45 op21=110 op22=56 '
         'op23=58 op24=49 op25=54 op26=58 op27=51 op28=50 op29=58 op30=54 '
         'op31=52 op32=45 op33=83 op34=49 op35=50 op36=56/>'),
        '  <GLOBALVAR abbrevid=4 op0=1 op1=2 op2=59 op3=3 op4=4 op5=0/>',
        # Omit <GLOBALVAR> lines that we don't care about.
        '  <GLOBALVAR abbrevid=4 op0=34 op1=2 op2=102 op3=3 op4=5 op5=0/>',
        ('  <FUNCTION op0=36 op1=0 op2=0 op3=0 op4=1 op5=0 op6=0 op7=0 op8=0 '
         'op9=0 op10=0 op11=0 op12=0 op13=0 op14=103/>'),
        # Omit <FUNCTION> lines that we don't care about.
        ('  <FUNCTION op0=55 op1=0 op2=1 op3=0 op4=2 op5=0 op6=0 op7=0 op8=0 '
         'op9=0 op10=0 op11=0 op12=0 op13=0 op14=0/>'),
        ('  <SOURCE_FILENAME abbrevid=5 op0=116 op1=101 op2=115 op3=116 op4=46 '
         'op5=99 op6=99/> record string = \'test.cc\''),
        '  <VSTOFFSET abbrevid=6 op0=1095/>',
        # Key target: <CONSTANTS_BLOCK>.
        '  <CONSTANTS_BLOCK NumWords=93 BlockCodeSize=4>',
        # <SETTYPE> changes "current type" state. abbrevid=4 is just redundant
        # data; within <CONSTANTS_BLOCK>, 4 = <SETTYPE>, 11 = <CSTRING>, etc.
        # Current type id := 12: int32.
        '    <SETTYPE abbrevid=4 op0=12/>',
        '    <NULL/>',  # |u32a| = 0, probably shared elsewhere.
        '    <SETTYPE abbrevid=4 op0=1/>',
        '    <CE_INBOUNDS_GEP op0=3 op1=4 op2=1 op3=12 op4=57 op5=12 op6=57/>',
        # Current type id := 3: int8[7].
        '    <SETTYPE abbrevid=4 op0=3/>',
        # For <CSTRING>, op6=0 is implicit! Also, "record string" gives hint,
        # but cannot be relied upon since it disappears if unprintable character
        # exists.
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=49 '
         'op5=97/> record string = \'Test1a\''),
        '    <SETTYPE abbrevid=4 op0=1/>',
        '    <CE_INBOUNDS_GEP op0=3 op1=4 op2=3 op3=12 op4=57 op5=12 op6=57/>',
        # Current type id := 3: int8[7].
        '    <SETTYPE abbrevid=4 op0=3/>',
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=49 '
         'op5=98/> record string = \'Test1b\''),
        '    <SETTYPE abbrevid=4 op0=1/>',
        '    <CE_INBOUNDS_GEP op0=3 op1=4 op2=5 op3=12 op4=57 op5=12 op6=57/>',
        '    <CE_INBOUNDS_GEP op0=3 op1=4 op2=6 op3=12 op4=57 op5=12 op6=57/>',
        '    <SETTYPE abbrevid=4 op0=5/>',
        '    <AGGREGATE abbrevid=8 op0=62 op1=63/> record string = \'>?\'',
        # Current type id := 3: int8[7].
        '    <SETTYPE abbrevid=4 op0=3/>',
        # The next 2 <CSTRING>s have the same length, so share type id.
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=50 '
         'op5=97/> record string = \'Test2a\''),
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=50 '
         'op5=98/> record string = \'Test2b\''),
        '    <SETTYPE abbrevid=4 op0=8/>',
        ('    <CE_INBOUNDS_GEP op0=10 op1=11 op2=8 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 10: int16[7].
        '    <SETTYPE abbrevid=4 op0=10/>',
        # <DATA> specifies u"Test3a", with implicit terminating null op6=0.
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=51 op5=97 op6=0/>',
        '    <SETTYPE abbrevid=4 op0=8/>',
        ('    <CE_INBOUNDS_GEP op0=10 op1=11 op2=10 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 10: int16[7].
        '    <SETTYPE abbrevid=4 op0=10/>',
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=51 op5=98 op6=0/>',
        '    <SETTYPE abbrevid=4 op0=13/>',
        ('    <CE_INBOUNDS_GEP op0=15 op1=16 op2=12 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 15: int32[7].
        '    <SETTYPE abbrevid=4 op0=15/>',
        # <DATA> specifies U"Test4a", with implicit terminating null op6=0.
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=52 op5=97 op6=0/>',
        '    <SETTYPE abbrevid=4 op0=13/>',
        ('    <CE_INBOUNDS_GEP op0=15 op1=16 op2=14 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 15: int32[7].
        '    <SETTYPE abbrevid=4 op0=15/>',
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=52 op5=98 op6=0/>',
        # Current type id := 17: int8[6], to represent bool[6].
        '    <SETTYPE abbrevid=4 op0=17/>',
        # {0 = false, 1 = true}, with explicit terminating null.
        '    <STRING abbrevid=9 op0=1 op1=0 op2=0 op3=1 op4=1 op5=0/>',
        '    <STRING abbrevid=9 op0=1 op1=0 op2=0 op3=1 op4=1 op5=1/>',
        # Current type id := 3: int8[7].
        '    <SETTYPE abbrevid=4 op0=3/>',
        # |i8a| = {'T','e','s','t','5','a',0} indistinguishable from "Test5a".
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=53 '
         'op5=97/> record string = \'Test5a\''),
        # |i8b| = {'T','e','s','t','5','a',1}: Not ending with '\0', so use
        # <STRING> instead of <CSTRING>!
        ('    <STRING abbrevid=9 op0=84 op1=101 op2=115 op3=116 op4=53 op5=98 '
         'op6=1/>'),
        # Current type id := 10: int16[7].
        '    <SETTYPE abbrevid=4 op0=10/>',
        # |i16a| and |i16b|: Both use <DATA>.
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=54 op5=97 op6=0/>',
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=54 op5=98 op6=1/>',
        # Current type id := 15: int32[7].
        '    <SETTYPE abbrevid=4 op0=15/>',
        # |i32a| and |i32b|: Both use <DATA>.
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=55 op5=97 op6=0/>',
        '    <DATA op0=84 op1=101 op2=115 op3=116 op4=55 op5=98 op6=1/>',
        '    <SETTYPE abbrevid=4 op0=23/>',
        '    <NULL/>',
        # Current type id := 20: int64.
        '    <SETTYPE abbrevid=4 op0=20/>',
        '    <NULL/>',  # |u64a| = 0.
        # Current type id := 3: int8[7].
        '    <SETTYPE abbrevid=4 op0=3/>',
        # Initializers for std::string, and has same form as int8[7].
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=56 '
         'op5=97/> record string = \'Test8a\''),
        ('    <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=56 '
         'op5=98/> record string = \'Test8b\''),
        # Current type id := 0: int8.
        '    <SETTYPE abbrevid=4 op0=0/>',
        '    <NULL/>',  # |u8a| = 0.
        # |u8a| = 10: Encoded as 20, since bit 0 stores sign bit (so -10 => 21).
        '    <INTEGER abbrevid=5 op0=20/>',
        # Current type id := 7: int16.
        '    <SETTYPE abbrevid=4 op0=7/>',
        '    <NULL/>',  # |u16a| = 0.
        # |u16b| = 1000.
        '    <INTEGER abbrevid=5 op0=2000/>',
        # Current type id := 12: int32.
        '    <SETTYPE abbrevid=4 op0=12/>',
        # |u32b| = 1000000.
        '    <INTEGER abbrevid=5 op0=2000000/>',
        # Current type id := 20: int64.
        '    <SETTYPE abbrevid=4 op0=20/>',
        # |u64b| = 1000000000000.
        '    <INTEGER abbrevid=5 op0=2000000000000/>',
        '    <SETTYPE abbrevid=4 op0=1/>',
        ('    <CE_INBOUNDS_GEP op0=26 op1=27 op2=39 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 26: int8[1].
        '    <SETTYPE abbrevid=4 op0=26/>',
        '    <NULL/>',  # |s8empty| = "".
        '    <SETTYPE abbrevid=4 op0=8/>',
        ('    <CE_INBOUNDS_GEP op0=28 op1=29 op2=41 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 28: int16[1].
        '    <SETTYPE abbrevid=4 op0=28/>',
        '    <NULL/>',  # |s16empty| = u"".
        '    <SETTYPE abbrevid=4 op0=13/>',
        ('    <CE_INBOUNDS_GEP op0=30 op1=31 op2=43 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 30: int32[1].
        '    <SETTYPE abbrevid=4 op0=30/>',
        '    <NULL/>',  # |s32empty| = U"".
        '    <SETTYPE abbrevid=4 op0=1/>',
        ('    <CE_INBOUNDS_GEP op0=32 op1=33 op2=45 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        # Current type id := 32: int8[3].
        '    <SETTYPE abbrevid=4 op0=32/>',
        # |s8a_suffix| = "1a" is a suffix of "Test1a", but not combined in BC.
        '    <CSTRING abbrevid=11 op0=49 op1=97/> record string = \'1a\'',
        # Current type id := 32: int8[256].
        '    <SETTYPE abbrevid=4 op0=34/>',
        '    <NULL/>',  # |zeros| = {0}. Will be in .bss section.
        '    <SETTYPE abbrevid=4 op0=1/>',
        '    <CE_CAST abbrevid=6 op0=11 op1=48 op2=51/>',
        '    <SETTYPE abbrevid=4 op0=56/>',
        '    <NULL/>',
        '  </CONSTANTS_BLOCK>',
        '  <METADATA_KIND_BLOCK NumWords=104 BlockCodeSize=3>',
        '    <KIND op0=0 op1=100 op2=98 op3=103/>',
        # Omit <KIND> lines that don't get used.
        ('    <KIND op0=21 op1=97 op2=98 op3=115 op4=111 op5=108 op6=117 '
         'op7=116 op8=101 op9=95 op10=115 op11=121 op12=109 op13=98 op14=111 '
         'op15=108/>'),
        '  </METADATA_KIND_BLOCK>',
        '  <METADATA_BLOCK NumWords=28 BlockCodeSize=4>',
        '    <STRINGS abbrevid=8 op0=1 op1=4/> num-strings = 1 {',
        '      \'clang version 4.0.1-10 (tags/RELEASE_401/final)\'',
        '    }',
        '    <NODE op0=1/>',
        ('    <NAME abbrevid=9 op0=108 op1=108 op2=118 op3=109 op4=46 op5=105 '
         'op6=100 op7=101 op8=110 op9=116/> record string = \'llvm.ident\''),
        '    <NAMED_NODE op0=1/>',
        '  </METADATA_BLOCK>',
        '  <OPERAND_BUNDLE_TAGS_BLOCK NumWords=11 BlockCodeSize=3>',
        '    <OPERAND_BUNDLE_TAG op0=100 op1=101 op2=111 op3=112 op4=116/>',
        ('    <OPERAND_BUNDLE_TAG op0=102 op1=117 op2=110 op3=99 op4=108 '
         'op5=101 op6=116/>'),
        ('    <OPERAND_BUNDLE_TAG op0=103 op1=99 op2=45 op3=116 op4=114 op5=97 '
         'op6=110 op7=115 op8=105 op9=116 op10=105 op11=111 op12=110/>'),
        '  </OPERAND_BUNDLE_TAGS_BLOCK>',
        '  <FUNCTION_BLOCK NumWords=122 BlockCodeSize=4>',
        '    <DECLAREBLOCKS op0=12/>',
        # Another <CONSTANTS_BLOCK>! This one does not get used.
        '    <CONSTANTS_BLOCK NumWords=11 BlockCodeSize=4>',
        '      <SETTYPE abbrevid=4 op0=12/>',
        '      <INTEGER abbrevid=5 op0=2/>',
        '      <SETTYPE abbrevid=4 op0=1/>',
        '      <CE_CAST abbrevid=6 op0=11 op1=25 op2=24/>',
        ('      <CE_INBOUNDS_GEP op0=3 op1=4 op2=25 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        '      <SETTYPE abbrevid=4 op0=52/>',
        '      <CE_CAST abbrevid=6 op0=11 op1=50 op2=53/>',
        '      <SETTYPE abbrevid=4 op0=1/>',
        '      <CE_CAST abbrevid=6 op0=11 op1=24 op2=23/>',
        '      <CE_CAST abbrevid=6 op0=11 op1=25 op2=28/>',
        ('      <CE_INBOUNDS_GEP op0=3 op1=4 op2=29 op3=12 op4=57 op5=12 '
         'op6=57/>'),
        '      <CE_CAST abbrevid=6 op0=11 op1=24 op2=27/>',
        '      <SETTYPE abbrevid=4 op0=61/>',
        '      <UNDEF/>',
        '    </CONSTANTS_BLOCK>',
        '    <INST_ALLOCA op0=41 op1=12 op2=104 op3=65/>',
        # Omit <INST_*> lines that don't get used.
        '    <UnknownCode39 op0=1/>',
        '    <USELIST_BLOCK_ID NumWords=21 BlockCodeSize=3>',
        '      <USELIST_CODE_DEFAULT op0=1 op1=0 op2=2 op3=115/>',
        # Omit <USELIST_CODE_DEFAULT> lines that don't get used.
        '      <USELIST_CODE_DEFAULT op0=1 op1=0 op2=2 op3=3 op4=24/>',
        '    </USELIST_BLOCK_ID>',
        '  </FUNCTION_BLOCK>    ',
        '  <VALUE_SYMTAB NumWords=200 BlockCodeSize=4>',
        ('    <ENTRY abbrevid=6 op0=24 op1=95 op2=90 op3=71 op4=86 op5=90 '
         'op6=52 op7=116 op8=101 op9=115 op10=116 op11=118 op12=69 op13=51 '
         'op14=115 op15=115 op16=97 op17=66 op18=53 op19=99 op20=120 op21=120 '
         'op22=49 op23=49/> record string = \'_ZGVZ4testvE3ssaB5cxx11\''),
        # Omit <ENTRY> lines that don't get used.
        ('    <ENTRY abbrevid=6 op0=30 op1=95 op2=90 op3=90 op4=52 op5=116 '
         'op6=101 op7=115 op8=116 op9=118 op10=69 op11=51 op12=117 op13=56 '
         'op14=97/> record string = \'_ZZ4testvE3u8a\''),
        '  </VALUE_SYMTAB>',
        '</MODULE_BLOCK>',
        # Trailing data that don't get used.
        '',
        '',
        'Summary of test.o:',
        '         Total size: 41536b/5192.00B/1298W',
        '        Stream type: LLVM IR',
        '  # Toplevel Blocks: 2',
        '',
        'Per-block Summary:',
        '  Block ID #0 (BLOCKINFO_BLOCK):',
        '      Num Instances: 1',
        '         Total Size: 672b/84.00B/21W',
        '    Percent of file: 1.6179%',
        '      Num SubBlocks: 0',
        '        Num Abbrevs: 16',
        '        Num Records: 3',
        '    Percent Abbrevs: 0.0000%',
        '',
        # Omit "Block ID #1 to #21.
        '  Block ID #22 (METADATA_KIND_BLOCK):',
        '      Num Instances: 1',
        '         Total Size: 3381b/422.62B/105W',
        '    Percent of file: 8.1399%',
        '      Num SubBlocks: 0',
        '        Num Abbrevs: 0',
        '        Num Records: 22',
        '    Percent Abbrevs: 0.0000%',
        '',
        '',
    ]
}


def _PrintOutput(path):
  lines = _OUTPUTS.get(os.path.normpath(path))
  assert lines, 'No mock_bcanalyzer.py entry for: ' + path
  sys.stdout.write('\n'.join(lines))
  sys.stdout.write('\n')


def main():
  paths = [p for p in sys.argv[1:] if not p.startswith('-')]
  _PrintOutput(paths[0])


if __name__ == '__main__':
  main()
