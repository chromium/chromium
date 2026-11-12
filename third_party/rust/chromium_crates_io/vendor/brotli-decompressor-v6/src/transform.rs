#![allow(non_upper_case_globals)]

const kIdentity: u8 = 0;
const kOmitLast1: u8 = 1;
const kOmitLast2: u8 = 2;
const kOmitLast3: u8 = 3;
const kOmitLast4: u8 = 4;
const kOmitLast5: u8 = 5;
const kOmitLast6: u8 = 6;
const kOmitLast7: u8 = 7;
const kOmitLast8: u8 = 8;
const kOmitLast9: u8 = 9;
const kUppercaseFirst: u8 = 10;
const kUppercaseAll: u8 = 11;
const kOmitFirst1: u8 = 12;
const kOmitFirst2: u8 = 13;
const kOmitFirst3: u8 = 14;
const kOmitFirst4: u8 = 15;
const kOmitFirst5: u8 = 16;
const kOmitFirst6: u8 = 17;
const kOmitFirst7: u8 = 18;
// const  kOmitFirst8     : u8 = 19; // <-- unused (reserved)
const kOmitFirst9: u8 = 20;


pub struct Transform {
  pub prefix_id: u8,
  pub transform: u8,
  pub suffix_id: u8,
}

const kPrefixSuffix: [u8; 208] =
  [0x00, 0x20, 0x00, 0x2c, 0x20, 0x00, 0x20, 0x6f, 0x66, 0x20, 0x74, 0x68, 0x65, 0x20, 0x00, 0x20,
   0x6f, 0x66, 0x20, 0x00, 0x73, 0x20, 0x00, 0x2e, 0x00, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x00, 0x20,
   0x69, 0x6e, 0x20, 0x00, 0x22, 0x00, 0x20, 0x74, 0x6f, 0x20, 0x00, 0x22, 0x3e, 0x00, 0x0a, 0x00,
   0x2e, 0x20, 0x00, 0x5d, 0x00, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x00, 0x20, 0x61, 0x20, 0x00, 0x20,
   0x74, 0x68, 0x61, 0x74, 0x20, 0x00, 0x27, 0x00, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x00, 0x20,
   0x66, 0x72, 0x6f, 0x6d, 0x20, 0x00, 0x20, 0x62, 0x79, 0x20, 0x00, 0x28, 0x00, 0x2e, 0x20, 0x54,
   0x68, 0x65, 0x20, 0x00, 0x20, 0x6f, 0x6e, 0x20, 0x00, 0x20, 0x61, 0x73, 0x20, 0x00, 0x20, 0x69,
   0x73, 0x20, 0x00, 0x69, 0x6e, 0x67, 0x20, 0x00, 0x0a, 0x09, 0x00, 0x3a, 0x00, 0x65, 0x64, 0x20,
   0x00, 0x3d, 0x22, 0x00, 0x20, 0x61, 0x74, 0x20, 0x00, 0x6c, 0x79, 0x20, 0x00, 0x2c, 0x00, 0x3d,
   0x27, 0x00, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x00, 0x2e, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20, 0x00,
   0x20, 0x6e, 0x6f, 0x74, 0x20, 0x00, 0x65, 0x72, 0x20, 0x00, 0x61, 0x6c, 0x20, 0x00, 0x66, 0x75,
   0x6c, 0x20, 0x00, 0x69, 0x76, 0x65, 0x20, 0x00, 0x6c, 0x65, 0x73, 0x73, 0x20, 0x00, 0x65, 0x73,
   0x74, 0x20, 0x00, 0x69, 0x7a, 0x65, 0x20, 0x00, 0xc2, 0xa0, 0x00, 0x6f, 0x75, 0x73, 0x20, 0x00];
//    "\0 \0, \0 of the \0 of \0s \0.\0 and \0 in \0\"\0 to \0\">\0\n\0. \0]\0"
//    " for \0 a \0 that \0\'\0 with \0 from \0 by \0(\0. The \0 on \0 as \0"
//    " is \0ing \0\n\t\0:\0ed \0=\"\0 at \0ly \0,\0=\'\0.com/\0. This \0"
//    " not \0er \0al \0ful \0ive \0less \0est \0ize \0\xc2\xa0\0ous ";

// EMPTY = ""
// SP = " "
// DQUOT = "\""
// SQUOT = "'"
// CLOSEBR = "]"
// OPEN = "("
// SLASH = "/"
// NBSP = non-breaking space "\0xc2\xa0"
//
const kPFix_EMPTY: u8 = 0;
const kPFix_SP: u8 = 1;
const kPFix_COMMASP: u8 = 3;
const kPFix_SPofSPtheSP: u8 = 6;
const kPFix_SPtheSP: u8 = 9;
const kPFix_eSP: u8 = 12;
const kPFix_SPofSP: u8 = 15;
const kPFix_sSP: u8 = 20;
const kPFix_DOT: u8 = 23;
const kPFix_SPandSP: u8 = 25;
const kPFix_SPinSP: u8 = 31;
const kPFix_DQUOT: u8 = 36;
const kPFix_SPtoSP: u8 = 38;
const kPFix_DQUOTGT: u8 = 43;
const kPFix_NEWLINE: u8 = 46;
const kPFix_DOTSP: u8 = 48;
const kPFix_CLOSEBR: u8 = 51;
const kPFix_SPforSP: u8 = 53;
const kPFix_SPaSP: u8 = 59;
const kPFix_SPthatSP: u8 = 63;
const kPFix_SQUOT: u8 = 70;
const kPFix_SPwithSP: u8 = 72;
const kPFix_SPfromSP: u8 = 79;
const kPFix_SPbySP: u8 = 86;
const kPFix_OPEN: u8 = 91;
const kPFix_DOTSPTheSP: u8 = 93;
const kPFix_SPonSP: u8 = 100;
const kPFix_SPasSP: u8 = 105;
const kPFix_SPisSP: u8 = 110;
const kPFix_ingSP: u8 = 115;
const kPFix_NEWLINETAB: u8 = 120;
const kPFix_COLON: u8 = 123;
const kPFix_edSP: u8 = 125;
const kPFix_EQDQUOT: u8 = 129;
const kPFix_SPatSP: u8 = 132;
const kPFix_lySP: u8 = 137;
const kPFix_COMMA: u8 = 141;
const kPFix_EQSQUOT: u8 = 143;
const kPFix_DOTcomSLASH: u8 = 146;
const kPFix_DOTSPThisSP: u8 = 152;
const kPFix_SPnotSP: u8 = 160;
const kPFix_erSP: u8 = 166;
const kPFix_alSP: u8 = 170;
const kPFix_fulSP: u8 = 174;
const kPFix_iveSP: u8 = 179;
const kPFix_lessSP: u8 = 184;
const kPFix_estSP: u8 = 190;
const kPFix_izeSP: u8 = 195;
const kPFix_NBSP: u8 = 200;
const kPFix_ousSP: u8 = 203;

pub const kNumTransforms: i32 = 121;
pub const kTransforms: [Transform; kNumTransforms as usize] = [Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst1,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPtheSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_sSP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPofSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPandSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst2,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast1,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_COMMASP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_COMMASP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPinSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPtoSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_eSP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DQUOTGT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_NEWLINE,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast3,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_CLOSEBR,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPforSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst3,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast2,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPaSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPthatSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DOTSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_DOT,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_COMMASP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst4,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPwithSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPfromSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPbySP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst5,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst6,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SPtheSP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast4,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DOTSPTheSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPonSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPasSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPisSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast7,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast1,
                                                                 suffix_id: kPFix_ingSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_NEWLINETAB,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_COLON,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DOTSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_edSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst9,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitFirst7,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast6,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_OPEN,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_COMMASP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast8,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPatSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_lySP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SPtheSP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPofSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast5,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kOmitLast9,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_COMMASP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_DQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_DOT,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_OPEN,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_DQUOTGT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EQDQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_DOTcomSLASH,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SPtheSP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPofSPtheSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_SQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_DOTSPThisSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_COMMA,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_DOT,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_OPEN,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_DOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_SPnotSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EQDQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_erSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_SP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_alSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EQSQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_DQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_DOTSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_OPEN,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_fulSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_DOTSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_iveSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_lessSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_SQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_estSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_DOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_DQUOTGT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EQSQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_COMMA,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_izeSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_DOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_NBSP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_EMPTY,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_COMMA,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_EQDQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_EQDQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kIdentity,
                                                                 suffix_id: kPFix_ousSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_COMMASP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_EQSQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_COMMA,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_EQDQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_COMMASP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_COMMA,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_OPEN,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_DOTSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_DOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_EMPTY,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_EQSQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_DOTSP,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_EQDQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseAll,
                                                                 suffix_id: kPFix_EQSQUOT,
                                                               },
                                                               Transform {
                                                                 prefix_id: kPFix_SP,
                                                                 transform: kUppercaseFirst,
                                                                 suffix_id: kPFix_EQSQUOT,
                                                               }];



// Copies src to dst, which are equally long. Dictionary words are mostly 4 to
// 16 bytes, too short for a memcpy call to pay off: two overlapping copies of
// their first and last 4 or 8 bytes cover them. Other lengths are rare.
#[inline(always)]
pub(crate) fn copy_bytes(dst: &mut [u8], src: &[u8]) {
  let n = src.len();
  let dst = &mut dst[..n];
  match n {
    4..=7 => {
      dst[..4].copy_from_slice(&src[..4]);
      dst[n - 4..].copy_from_slice(&src[n - 4..]);
    }
    8..=16 => {
      dst[..8].copy_from_slice(&src[..8]);
      dst[n - 8..].copy_from_slice(&src[n - 8..]);
    }
    17..=32 => {
      dst[..16].copy_from_slice(&src[..16]);
      dst[n - 16..].copy_from_slice(&src[n - 16..]);
    }
    _ => dst.copy_from_slice(src),
  }
}

pub(crate) fn ToUpperCase(p: &mut [u8]) -> i32 {
  if (fast!((p)[0]) < 0xc0) {
    if (fast!((p)[0]) >= b'a' && fast!((p)[0]) <= b'z') {
      fast_mut!((p)[0]) ^= 32;
    }
    return 1;
  }
  // An overly simplified uppercasing model for utf-8.
  if (fast!((p)[0]) < 0xe0) {
    fast_mut!((p)[1]) ^= 32;
    return 2;
  }
  // An arbitrary transform for three byte characters.
  fast_mut!((p)[2]) ^= 5;
  3
}

pub fn TransformDictionaryWord(dst: &mut [u8],
                               mut word: &[u8],
                               mut len: i32,
                               transform: i32)
                               -> i32 {
  let mut idx: i32 = 0;
  {
    let prefix =
      &fast!((kPrefixSuffix)[fast_inner!((kTransforms)[transform as usize]).prefix_id as usize;]);
    while (fast!((prefix)[idx as usize]) != 0) {
      fast_mut!((dst)[idx as usize]) = fast!((prefix)[idx as usize]);
      idx += 1;
    }
  }
  {
    let t = fast_ref!((kTransforms)[transform as usize]).transform;
    let mut skip: i32 = if t < kOmitFirst1 {
      0
    } else {
      t as i32 - (kOmitFirst1 - 1) as i32
    };
    if (skip > len) {
      skip = len;
    }
    word = fast!((word)[skip as usize;]);
    len -= skip;
    if (t <= kOmitLast9) {
      len -= t as i32;
    }
    if len > 0 {
      let start = idx as usize;
      copy_bytes(fast_mut!((dst)[start ; start + len as usize]), fast!((word)[0 ; len as usize]));
      idx += len;
    }
    let uppercase = &mut fast_mut!((dst)[(idx - len) as usize ;]);
    if (t == kUppercaseFirst) {
      ToUpperCase(uppercase);
    } else if (t == kUppercaseAll) {
      let mut uppercase_offset: usize = 0;
      while (len > 0) {
        let step = ToUpperCase(&mut fast_mut!((uppercase)[uppercase_offset;]));
        uppercase_offset += step as usize;
        len -= step;
      }
    }
  }
  {
    let suffix =
      &fast!((kPrefixSuffix)[fast_inner!((kTransforms)[transform as usize]).suffix_id as usize ; ]);
    let mut i: usize = 0;
    while (fast!((suffix)[i as usize]) != 0) {
      fast_mut!((dst)[idx as usize]) = fast!((suffix)[i]);
      idx += 1;
      i += 1;
    }
    idx
  }
}

#[cfg(test)]
mod tests {
  use super::{copy_bytes, kNumTransforms, kPrefixSuffix, kTransforms, kOmitFirst1, kOmitLast9,
              kUppercaseAll, kUppercaseFirst, ToUpperCase, TransformDictionaryWord};

  // `TransformDictionaryWord` as it was written before it copied the word
  // slice with `copy_bytes`: copying the word byte by byte.
  fn reference(dst: &mut [u8], mut word: &[u8], mut len: i32, transform: i32) -> i32 {
    let mut idx: i32 = 0;
    let prefix = &kPrefixSuffix[kTransforms[transform as usize].prefix_id as usize..];
    while prefix[idx as usize] != 0 {
      dst[idx as usize] = prefix[idx as usize];
      idx += 1;
    }
    let t = kTransforms[transform as usize].transform;
    let mut skip: i32 = if t < kOmitFirst1 { 0 } else { t as i32 - (kOmitFirst1 - 1) as i32 };
    let mut i: i32 = 0;
    if skip > len {
      skip = len;
    }
    word = &word[skip as usize..];
    len -= skip;
    if t <= kOmitLast9 {
      len -= t as i32;
    }
    while i < len {
      dst[idx as usize] = word[i as usize];
      idx += 1;
      i += 1;
    }
    let uppercase = &mut dst[(idx - len) as usize..];
    if t == kUppercaseFirst {
      ToUpperCase(uppercase);
    } else if t == kUppercaseAll {
      let mut uppercase_offset: usize = 0;
      while len > 0 {
        let step = ToUpperCase(&mut uppercase[uppercase_offset..]);
        uppercase_offset += step as usize;
        len -= step;
      }
    }
    let suffix = &kPrefixSuffix[kTransforms[transform as usize].suffix_id as usize..];
    let mut i: usize = 0;
    while suffix[i] != 0 {
      dst[idx as usize] = suffix[i];
      idx += 1;
      i += 1;
    }
    idx
  }

  #[test]
  fn transforms_match_the_byte_by_byte_reference() {
    // ASCII, two- and three-byte UTF-8 lead bytes, so that every uppercase
    // branch runs.
    let source: [u8; 24] = [b'w', 0xc3, 0xa9, b'r', b'd', 0xe2, 0x82, 0xac, b'a', b'b', b'c',
                            b'D', b'e', b'f', b'g', b'h', b'i', b'j', b'k', b'l', b'm', b'n',
                            b'o', b'p'];
    for transform in 0..kNumTransforms {
      for len in 0..=source.len() {
        let word = &source[..len];
        let mut expected = [0x5au8; 64];
        let mut actual = [0x5au8; 64];
        let expected_len = reference(&mut expected, word, len as i32, transform);
        let actual_len = TransformDictionaryWord(&mut actual, word, len as i32, transform);
        assert_eq!(actual_len, expected_len, "transform {} len {}", transform, len);
        assert_eq!(&actual[..], &expected[..], "transform {} len {}", transform, len);
      }
    }
  }

  #[test]
  fn copy_bytes_copies_every_length() {
    let mut source = [0u8; 40];
    for (i, b) in source.iter_mut().enumerate() {
      *b = (i * 7 + 1) as u8;
    }
    for len in 0..=source.len() {
      let mut dst = [0u8; 48];
      copy_bytes(&mut dst[3..3 + len], &source[..len]);
      assert_eq!(&dst[3..3 + len], &source[..len]);
      assert!(dst[..3].iter().chain(dst[3 + len..].iter()).all(|b| *b == 0));
    }
  }
}
