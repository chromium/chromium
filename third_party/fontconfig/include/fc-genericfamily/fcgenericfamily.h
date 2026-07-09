/* ANSI-C code produced by gperf version 3.3 */
/* Command-line: gperf --pic -m 100 --output-file fc-genericfamily/fcgenericfamily.h fc-genericfamily/fcgenericfamily.gperf  */
/* Computed positions: -k'1-2,4-5,8,10,12,14-17,20,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 5 "fc-genericfamily/fcgenericfamily.gperf"

#include <string.h>
#include <stdint.h>

struct FcGenericFamilyEntry {
    int      name;
    uint32_t classification;  /* Bit field of FC_FAMILY_* values */
};
#line 26 "fc-genericfamily/fcgenericfamily.gperf"
struct FcGenericFamilyEntry;
#include <string.h>

#define TOTAL_KEYWORDS 873
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 31
#define MIN_HASH_VALUE 40
#define MAX_HASH_VALUE 5938
/* maximum key range = 5899, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static const unsigned char gperf_downcase[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
     45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255
  };
#endif

#ifndef GPERF_CASE_STRCMP
#define GPERF_CASE_STRCMP 1
static int
gperf_case_strcmp (register const char *s1, register const char *s2)
{
  for (;;)
    {
      unsigned char c1 = gperf_downcase[(unsigned char)*s1++];
      unsigned char c2 = gperf_downcase[(unsigned char)*s2++];
      if (c1 != 0 && c1 == c2)
        continue;
      return (int)c1 - (int)c2;
    }
}
#endif

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
fc_generic_family_hash (register const char *str, register size_t len)
{
  static const unsigned short asso_values[] =
    {
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939,  185, 1235, 5939, 5939, 5939, 5939, 5939, 5939,
        12,   12, 5939,   39, 5939,   38, 5939, 5939,   11,   14,
        13,   18,   47,   35,   11, 5939,   11,   12, 5939, 5939,
      5939, 5939, 5939, 5939, 5939,   11, 1140,  403,   99,   28,
       156,  146,  583,   12, 1406, 1075,   25,   19,   12,   14,
       530,   14,   94,   80,   41,   13, 1709,  433,   71,  987,
       398,   11, 5939, 5939, 5939,   11,   11,   11, 1140,  403,
        99,   28,  156,  146,  583,   12, 1406, 1075,   25,   19,
        12,   14,  530,   14,   94,   80,   41,   13, 1709,  433,
        71,  987,  398,   11, 5939, 5939, 5939, 5939, 5939, 5939,
        11,   12, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939,   11, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939,   11,   11, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,   11,   12,
      5939, 5939, 5939, 5939, 5939,   11, 5939, 5939, 5939, 5939,
        11, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,   11,
        11, 5939, 5939, 5939, 5939,   11, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939,   11, 5939, 5939,
        11, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,   11,
      5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939, 5939,
      5939, 5939, 5939, 5939, 5939, 5939, 5939
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[19]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 19:
      case 18:
      case 17:
        hval += asso_values[(unsigned char)str[16]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 16:
        hval += asso_values[(unsigned char)str[15]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 15:
        hval += asso_values[(unsigned char)str[14]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 14:
        hval += asso_values[(unsigned char)str[13]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 13:
      case 12:
        hval += asso_values[(unsigned char)str[11]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 11:
      case 10:
        hval += asso_values[(unsigned char)str[9]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 9:
      case 8:
        hval += asso_values[(unsigned char)str[7]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 7:
      case 6:
      case 5:
        hval += asso_values[(unsigned char)str[4]+1];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
#if (defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang__ && __clang_major__ + (__clang_minor__ >= 9) > 3))) || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L && ((defined __GNUC__ && __GNUC__ >= 10) || (defined __clang__ && __clang_major__ >= 9)))
      [[fallthrough]];
#elif (defined __GNUC__ && __GNUC__ >= 7) || (defined __clang__ && __clang_major__ >= 10)
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct FcConstFamilyNamePool_t
  {
    char FcConstFamilyNamePool_str40[sizeof("nu")];
    char FcConstFamilyNamePool_str53[sizeof("aqui")];
    char FcConstFamilyNamePool_str60[sizeof("muli")];
    char FcConstFamilyNamePool_str64[sizeof("untaza")];
    char FcConstFamilyNamePool_str65[sizeof("loma")];
    char FcConstFamilyNamePool_str68[sizeof("lato")];
    char FcConstFamilyNamePool_str70[sizeof("nunito")];
    char FcConstFamilyNamePool_str71[sizeof("nasim")];
    char FcConstFamilyNamePool_str72[sizeof("madan")];
    char FcConstFamilyNamePool_str73[sizeof("navilu")];
    char FcConstFamilyNamePool_str74[sizeof("madan2")];
    char FcConstFamilyNamePool_str75[sizeof("unbom")];
    char FcConstFamilyNamePool_str77[sizeof("andika")];
    char FcConstFamilyNamePool_str78[sizeof("undotum")];
    char FcConstFamilyNamePool_str79[sizeof("tiza")];
    char FcConstFamilyNamePool_str81[sizeof("unvada")];
    char FcConstFamilyNamePool_str84[sizeof("unpen")];
    char FcConstFamilyNamePool_str85[sizeof("undinaru")];
    char FcConstFamilyNamePool_str86[sizeof("unpilgia")];
    char FcConstFamilyNamePool_str88[sizeof("uniol")];
    char FcConstFamilyNamePool_str93[sizeof("mononoki")];
    char FcConstFamilyNamePool_str94[sizeof("unjamosora")];
    char FcConstFamilyNamePool_str95[sizeof("tahoma")];
    char FcConstFamilyNamePool_str96[sizeof("norasi")];
    char FcConstFamilyNamePool_str97[sizeof("lime")];
    char FcConstFamilyNamePool_str98[sizeof("alkalami")];
    char FcConstFamilyNamePool_str100[sizeof("elham")];
    char FcConstFamilyNamePool_str101[sizeof("\357\275\215\357\275\223 \346\230\216\346\234\235")];
    char FcConstFamilyNamePool_str102[sizeof("m+ 1mn")];
    char FcConstFamilyNamePool_str103[sizeof("ungungseo")];
    char FcConstFamilyNamePool_str105[sizeof("unjamodotum")];
    char FcConstFamilyNamePool_str106[sizeof("molot")];
    char FcConstFamilyNamePool_str107[sizeof("m+ 2m")];
    char FcConstFamilyNamePool_str108[sizeof("m+ 1m")];
    char FcConstFamilyNamePool_str109[sizeof("andale mono")];
    char FcConstFamilyNamePool_str117[sizeof("asea")];
    char FcConstFamilyNamePool_str118[sizeof("sina")];
    char FcConstFamilyNamePool_str122[sizeof("unpenheulim")];
    char FcConstFamilyNamePool_str123[sizeof("lohit tamil")];
    char FcConstFamilyNamePool_str124[sizeof("unyetgul")];
    char FcConstFamilyNamePool_str125[sizeof("titillium")];
    char FcConstFamilyNamePool_str126[sizeof("unjamonovel")];
    char FcConstFamilyNamePool_str134[sizeof("roya")];
    char FcConstFamilyNamePool_str137[sizeof("lohit gujarati")];
    char FcConstFamilyNamePool_str139[sizeof("lohit nepali")];
    char FcConstFamilyNamePool_str140[sizeof("inconsolata")];
    char FcConstFamilyNamePool_str143[sizeof("lohit telugu")];
    char FcConstFamilyNamePool_str148[sizeof("aroania")];
    char FcConstFamilyNamePool_str150[sizeof("tabassom")];
    char FcConstFamilyNamePool_str153[sizeof("\357\275\215\357\275\223 \343\202\264\343\202\267\343\203\203\343\202\257")];
    char FcConstFamilyNamePool_str155[sizeof("roboto")];
    char FcConstFamilyNamePool_str160[sizeof("musica")];
    char FcConstFamilyNamePool_str161[sizeof("lohit malayalam")];
    char FcConstFamilyNamePool_str164[sizeof("albany amt")];
    char FcConstFamilyNamePool_str165[sizeof("arial")];
    char FcConstFamilyNamePool_str166[sizeof("lohit maithili")];
    char FcConstFamilyNamePool_str170[sizeof("edwin")];
    char FcConstFamilyNamePool_str171[sizeof("nsimsun")];
    char FcConstFamilyNamePool_str172[sizeof("smoothansi")];
    char FcConstFamilyNamePool_str175[sizeof("literata")];
    char FcConstFamilyNamePool_str180[sizeof("d-din")];
    char FcConstFamilyNamePool_str183[sizeof("leland")];
    char FcConstFamilyNamePool_str188[sizeof("impact")];
    char FcConstFamilyNamePool_str190[sizeof("gohu")];
    char FcConstFamilyNamePool_str191[sizeof("roboto mono")];
    char FcConstFamilyNamePool_str193[sizeof("tinos")];
    char FcConstFamilyNamePool_str195[sizeof("lohit odia")];
    char FcConstFamilyNamePool_str198[sizeof("lohit hindi")];
    char FcConstFamilyNamePool_str203[sizeof("david clm")];
    char FcConstFamilyNamePool_str207[sizeof("times")];
    char FcConstFamilyNamePool_str210[sizeof("lohit kannada")];
    char FcConstFamilyNamePool_str211[sizeof("lohit sindhi")];
    char FcConstFamilyNamePool_str215[sizeof("garuda")];
    char FcConstFamilyNamePool_str216[sizeof("mingliu")];
    char FcConstFamilyNamePool_str223[sizeof("agbalumo")];
    char FcConstFamilyNamePool_str229[sizeof("oswald")];
    char FcConstFamilyNamePool_str231[sizeof("inter")];
    char FcConstFamilyNamePool_str234[sizeof("analecta")];
    char FcConstFamilyNamePool_str236[sizeof("mingzat")];
    char FcConstFamilyNamePool_str238[sizeof("irannastaliq")];
    char FcConstFamilyNamePool_str242[sizeof("leland text")];
    char FcConstFamilyNamePool_str243[sizeof("liberation mono")];
    char FcConstFamilyNamePool_str245[sizeof("titr")];
    char FcConstFamilyNamePool_str253[sizeof("lohit assamese")];
    char FcConstFamilyNamePool_str258[sizeof("aurulentsansmono")];
    char FcConstFamilyNamePool_str259[sizeof("lohit devanagari")];
    char FcConstFamilyNamePool_str260[sizeof("tlwgmono")];
    char FcConstFamilyNamePool_str261[sizeof("tlwgtypo")];
    char FcConstFamilyNamePool_str267[sizeof("stix")];
    char FcConstFamilyNamePool_str274[sizeof("infofont")];
    char FcConstFamilyNamePool_str281[sizeof("edges")];
    char FcConstFamilyNamePool_str284[sizeof("latin modern roman")];
    char FcConstFamilyNamePool_str288[sizeof("lohit bengali")];
    char FcConstFamilyNamePool_str291[sizeof("inconsolatago")];
    char FcConstFamilyNamePool_str292[sizeof("alegreya")];
    char FcConstFamilyNamePool_str294[sizeof("monofur")];
    char FcConstFamilyNamePool_str295[sizeof("annapurna sil")];
    char FcConstFamilyNamePool_str301[sizeof("montserrat")];
    char FcConstFamilyNamePool_str303[sizeof("anorexia")];
    char FcConstFamilyNamePool_str305[sizeof("sniglet")];
    char FcConstFamilyNamePool_str310[sizeof("segoe ui")];
    char FcConstFamilyNamePool_str313[sizeof("geist")];
    char FcConstFamilyNamePool_str319[sizeof("geist mono")];
    char FcConstFamilyNamePool_str320[sizeof("exo 2")];
    char FcConstFamilyNamePool_str324[sizeof("intel one mono")];
    char FcConstFamilyNamePool_str326[sizeof("freemono")];
    char FcConstFamilyNamePool_str327[sizeof("libertinus")];
    char FcConstFamilyNamePool_str328[sizeof("fixed")];
    char FcConstFamilyNamePool_str329[sizeof("tlwgtypist")];
    char FcConstFamilyNamePool_str333[sizeof("mints mild")];
    char FcConstFamilyNamePool_str340[sizeof("signfont")];
    char FcConstFamilyNamePool_str341[sizeof("arundina sans")];
    char FcConstFamilyNamePool_str342[sizeof("anka/coder")];
    char FcConstFamilyNamePool_str345[sizeof("inconsolata go")];
    char FcConstFamilyNamePool_str349[sizeof("unbatang")];
    char FcConstFamilyNamePool_str352[sizeof("alef")];
    char FcConstFamilyNamePool_str358[sizeof("grand hotel")];
    char FcConstFamilyNamePool_str363[sizeof("material icons")];
    char FcConstFamilyNamePool_str370[sizeof("segoe ui emoji")];
    char FcConstFamilyNamePool_str374[sizeof("unjamobatang")];
    char FcConstFamilyNamePool_str375[sizeof("tobecontinued")];
    char FcConstFamilyNamePool_str382[sizeof("lateef")];
    char FcConstFamilyNamePool_str383[sizeof("fantezi")];
    char FcConstFamilyNamePool_str385[sizeof("dror")];
    char FcConstFamilyNamePool_str386[sizeof("garamond")];
    char FcConstFamilyNamePool_str388[sizeof("latin modern roman demi")];
    char FcConstFamilyNamePool_str392[sizeof("fixedsys")];
    char FcConstFamilyNamePool_str394[sizeof("latin modern roman unslanted")];
    char FcConstFamilyNamePool_str395[sizeof("great vibes")];
    char FcConstFamilyNamePool_str398[sizeof("droid sans mono")];
    char FcConstFamilyNamePool_str399[sizeof("times new roman")];
    char FcConstFamilyNamePool_str404[sizeof("latin modern roman dunhill")];
    char FcConstFamilyNamePool_str408[sizeof("drift")];
    char FcConstFamilyNamePool_str410[sizeof("nafees nastaleeq")];
    char FcConstFamilyNamePool_str413[sizeof("nanumgothic_coding")];
    char FcConstFamilyNamePool_str414[sizeof("droid sans")];
    char FcConstFamilyNamePool_str415[sizeof("droid sans thai")];
    char FcConstFamilyNamePool_str436[sizeof("liberation sans")];
    char FcConstFamilyNamePool_str437[sizeof("droid sans armenian")];
    char FcConstFamilyNamePool_str438[sizeof("namdhinggo sil")];
    char FcConstFamilyNamePool_str440[sizeof("droid serif")];
    char FcConstFamilyNamePool_str442[sizeof("c059")];
    char FcConstFamilyNamePool_str443[sizeof("oxygen mono")];
    char FcConstFamilyNamePool_str448[sizeof("foundation icons")];
    char FcConstFamilyNamePool_str449[sizeof("z003")];
    char FcConstFamilyNamePool_str451[sizeof("tscu_paranar")];
    char FcConstFamilyNamePool_str458[sizeof("alegreya sans")];
    char FcConstFamilyNamePool_str459[sizeof("latin modern roman slanted")];
    char FcConstFamilyNamePool_str462[sizeof("droid sans tamil")];
    char FcConstFamilyNamePool_str468[sizeof("segoe ui symbol")];
    char FcConstFamilyNamePool_str470[sizeof("clean")];
    char FcConstFamilyNamePool_str471[sizeof("caladea")];
    char FcConstFamilyNamePool_str472[sizeof("mints strong")];
    char FcConstFamilyNamePool_str474[sizeof("laconic")];
    char FcConstFamilyNamePool_str476[sizeof("cure")];
    char FcConstFamilyNamePool_str478[sizeof("malayalam")];
    char FcConstFamilyNamePool_str487[sizeof("freesans")];
    char FcConstFamilyNamePool_str493[sizeof("code2000")];
    char FcConstFamilyNamePool_str496[sizeof("freeserif")];
    char FcConstFamilyNamePool_str499[sizeof("code2001")];
    char FcConstFamilyNamePool_str500[sizeof("tinos nerd font")];
    char FcConstFamilyNamePool_str505[sizeof("iosevka")];
    char FcConstFamilyNamePool_str506[sizeof("zar")];
    char FcConstFamilyNamePool_str510[sizeof("arundina sans mono")];
    char FcConstFamilyNamePool_str514[sizeof("unifrakturmaguntia")];
    char FcConstFamilyNamePool_str515[sizeof("nanumgothic")];
    char FcConstFamilyNamePool_str529[sizeof("d-din condensed")];
    char FcConstFamilyNamePool_str530[sizeof("elliniaclm")];
    char FcConstFamilyNamePool_str536[sizeof("manchu2005")];
    char FcConstFamilyNamePool_str556[sizeof("inconsolata bold")];
    char FcConstFamilyNamePool_str558[sizeof("red hat mono")];
    char FcConstFamilyNamePool_str564[sizeof("fangsong ti")];
    char FcConstFamilyNamePool_str565[sizeof("montserrat alternates")];
    char FcConstFamilyNamePool_str570[sizeof("oxygen-sans")];
    char FcConstFamilyNamePool_str571[sizeof("p052")];
    char FcConstFamilyNamePool_str573[sizeof("meiryo ui")];
    char FcConstFamilyNamePool_str574[sizeof("aurulentsansmono nerd font")];
    char FcConstFamilyNamePool_str579[sizeof("gfs didot")];
    char FcConstFamilyNamePool_str582[sizeof("motoyalmaru")];
    char FcConstFamilyNamePool_str583[sizeof("constantia")];
    char FcConstFamilyNamePool_str585[sizeof("montserrat underline")];
    char FcConstFamilyNamePool_str589[sizeof("andika compact")];
    char FcConstFamilyNamePool_str590[sizeof("tlwgtypewriter")];
    char FcConstFamilyNamePool_str595[sizeof("source sans 3")];
    char FcConstFamilyNamePool_str598[sizeof("notomono nerd font")];
    char FcConstFamilyNamePool_str599[sizeof("perizia")];
    char FcConstFamilyNamePool_str603[sizeof("gomono nerd font")];
    char FcConstFamilyNamePool_str605[sizeof("mononoki nerd font")];
    char FcConstFamilyNamePool_str608[sizeof("codenewroman")];
    char FcConstFamilyNamePool_str611[sizeof("gfs nicefore")];
    char FcConstFamilyNamePool_str615[sizeof("gfs porson")];
    char FcConstFamilyNamePool_str618[sizeof("petaluma")];
    char FcConstFamilyNamePool_str621[sizeof("meslo")];
    char FcConstFamilyNamePool_str622[sizeof("comic neue")];
    char FcConstFamilyNamePool_str623[sizeof("homa")];
    char FcConstFamilyNamePool_str625[sizeof("comicshannsmono")];
    char FcConstFamilyNamePool_str626[sizeof("red hat text")];
    char FcConstFamilyNamePool_str628[sizeof("inconsolata regular")];
    char FcConstFamilyNamePool_str630[sizeof("lpfont")];
    char FcConstFamilyNamePool_str636[sizeof("hanamin")];
    char FcConstFamilyNamePool_str642[sizeof("lekton")];
    char FcConstFamilyNamePool_str647[sizeof("gfs solomos")];
    char FcConstFamilyNamePool_str651[sizeof("hasida")];
    char FcConstFamilyNamePool_str652[sizeof("iosevkaterm")];
    char FcConstFamilyNamePool_str655[sizeof("ms serif")];
    char FcConstFamilyNamePool_str656[sizeof("arundina serif")];
    char FcConstFamilyNamePool_str661[sizeof("waree")];
    char FcConstFamilyNamePool_str662[sizeof("\303\251colier court")];
    char FcConstFamilyNamePool_str669[sizeof("lotoos")];
    char FcConstFamilyNamePool_str670[sizeof("ume mincho")];
    char FcConstFamilyNamePool_str671[sizeof("nachlieli")];
    char FcConstFamilyNamePool_str672[sizeof("liberationmono nerd font")];
    char FcConstFamilyNamePool_str673[sizeof("arimo")];
    char FcConstFamilyNamePool_str675[sizeof("umpush")];
    char FcConstFamilyNamePool_str680[sizeof("widelands")];
    char FcConstFamilyNamePool_str683[sizeof("liberation serif")];
    char FcConstFamilyNamePool_str684[sizeof("daddytimemono")];
    char FcConstFamilyNamePool_str685[sizeof("elephant")];
    char FcConstFamilyNamePool_str686[sizeof("ume p mincho")];
    char FcConstFamilyNamePool_str688[sizeof("mplus")];
    char FcConstFamilyNamePool_str690[sizeof("arial unicode")];
    char FcConstFamilyNamePool_str694[sizeof("lohit marathi")];
    char FcConstFamilyNamePool_str700[sizeof("clear sans")];
    char FcConstFamilyNamePool_str706[sizeof("songti tc")];
    char FcConstFamilyNamePool_str708[sizeof("lohit kashmiri")];
    char FcConstFamilyNamePool_str713[sizeof("ume hy gothic o5")];
    char FcConstFamilyNamePool_str721[sizeof("petalumatext")];
    char FcConstFamilyNamePool_str725[sizeof("source serif 4")];
    char FcConstFamilyNamePool_str730[sizeof("go mono")];
    char FcConstFamilyNamePool_str731[sizeof("comic sans ms")];
    char FcConstFamilyNamePool_str732[sizeof("lucida math")];
    char FcConstFamilyNamePool_str733[sizeof("delphine")];
    char FcConstFamilyNamePool_str737[sizeof("firamono nerd font")];
    char FcConstFamilyNamePool_str745[sizeof("songti sc")];
    char FcConstFamilyNamePool_str750[sizeof("iosevka term")];
    char FcConstFamilyNamePool_str751[sizeof("gfs pyrsos")];
    char FcConstFamilyNamePool_str752[sizeof("anonymous pro")];
    char FcConstFamilyNamePool_str754[sizeof("dejavu sans")];
    char FcConstFamilyNamePool_str757[sizeof("ume mincho s3")];
    char FcConstFamilyNamePool_str760[sizeof("latin modern roman caps")];
    char FcConstFamilyNamePool_str761[sizeof("smonohand")];
    char FcConstFamilyNamePool_str767[sizeof("droid sans georgian")];
    char FcConstFamilyNamePool_str773[sizeof("d-din exp")];
    char FcConstFamilyNamePool_str783[sizeof("inconsolata nerd font")];
    char FcConstFamilyNamePool_str785[sizeof("sazanami mincho")];
    char FcConstFamilyNamePool_str786[sizeof("monofur nerd font")];
    char FcConstFamilyNamePool_str789[sizeof("ia writer duo")];
    char FcConstFamilyNamePool_str791[sizeof("ume p mincho s3")];
    char FcConstFamilyNamePool_str794[sizeof("thorndale")];
    char FcConstFamilyNamePool_str800[sizeof("glisp")];
    char FcConstFamilyNamePool_str803[sizeof("inconsolatago nerd font")];
    char FcConstFamilyNamePool_str805[sizeof("ia writer mono")];
    char FcConstFamilyNamePool_str811[sizeof("robotomono nerd font")];
    char FcConstFamilyNamePool_str815[sizeof("droidsansmono nerd font")];
    char FcConstFamilyNamePool_str823[sizeof("gfs theokritos")];
    char FcConstFamilyNamePool_str830[sizeof("crete round")];
    char FcConstFamilyNamePool_str832[sizeof("ferdosi")];
    char FcConstFamilyNamePool_str835[sizeof("firacode nerd font mono")];
    char FcConstFamilyNamePool_str837[sizeof("old standard sfd")];
    char FcConstFamilyNamePool_str838[sizeof("firacode nerd font")];
    char FcConstFamilyNamePool_str843[sizeof("lucida sans unicode")];
    char FcConstFamilyNamePool_str844[sizeof("ume hy gothic")];
    char FcConstFamilyNamePool_str848[sizeof("calibri")];
    char FcConstFamilyNamePool_str855[sizeof("simsong")];
    char FcConstFamilyNamePool_str869[sizeof("georgia")];
    char FcConstFamilyNamePool_str870[sizeof("shimenkan")];
    char FcConstFamilyNamePool_str872[sizeof("codenewroman nerd font mono")];
    char FcConstFamilyNamePool_str880[sizeof("apparatus sil")];
    char FcConstFamilyNamePool_str886[sizeof("hadasim clm")];
    char FcConstFamilyNamePool_str892[sizeof("nachlieli clm")];
    char FcConstFamilyNamePool_str894[sizeof("codenewroman nerd font")];
    char FcConstFamilyNamePool_str900[sizeof("roboto condensed")];
    char FcConstFamilyNamePool_str902[sizeof("comic neue angular")];
    char FcConstFamilyNamePool_str912[sizeof("droidsansm nerd font")];
    char FcConstFamilyNamePool_str914[sizeof("domestic manners")];
    char FcConstFamilyNamePool_str918[sizeof("dejavu sans mono")];
    char FcConstFamilyNamePool_str919[sizeof("frank ruehl")];
    char FcConstFamilyNamePool_str921[sizeof("applemyungjo")];
    char FcConstFamilyNamePool_str924[sizeof("letters laughing")];
    char FcConstFamilyNamePool_str931[sizeof("gfs philostratos")];
    char FcConstFamilyNamePool_str936[sizeof("nanumgothiccoding")];
    char FcConstFamilyNamePool_str938[sizeof("gfs bodoni")];
    char FcConstFamilyNamePool_str945[sizeof("dai banna sil")];
    char FcConstFamilyNamePool_str956[sizeof("source han mono")];
    char FcConstFamilyNamePool_str958[sizeof("comicshannsmono nerd font")];
    char FcConstFamilyNamePool_str962[sizeof("sourcecodepro nerd font")];
    char FcConstFamilyNamePool_str963[sizeof("inconsolatalgc")];
    char FcConstFamilyNamePool_str968[sizeof("tai heritage pro")];
    char FcConstFamilyNamePool_str983[sizeof("ia writer quattro")];
    char FcConstFamilyNamePool_str988[sizeof("rit rachana")];
    char FcConstFamilyNamePool_str994[sizeof("meslo nerd font")];
    char FcConstFamilyNamePool_str995[sizeof("mplus nerd font")];
    char FcConstFamilyNamePool_str1006[sizeof("geistmono nerd font")];
    char FcConstFamilyNamePool_str1008[sizeof("pcfont")];
    char FcConstFamilyNamePool_str1010[sizeof("anka/coder condensed")];
    char FcConstFamilyNamePool_str1015[sizeof("thorndale amt")];
    char FcConstFamilyNamePool_str1018[sizeof("twitter color emoji")];
    char FcConstFamilyNamePool_str1021[sizeof("inconsolatalgc nerd font")];
    char FcConstFamilyNamePool_str1028[sizeof("cormorant")];
    char FcConstFamilyNamePool_str1029[sizeof("arial unicode ms")];
    char FcConstFamilyNamePool_str1031[sizeof("infofont z")];
    char FcConstFamilyNamePool_str1034[sizeof("mgopen modata")];
    char FcConstFamilyNamePool_str1039[sizeof("lekton nerd font")];
    char FcConstFamilyNamePool_str1046[sizeof("arimo nerd font")];
    char FcConstFamilyNamePool_str1047[sizeof("gfs olga")];
    char FcConstFamilyNamePool_str1051[sizeof("sparks dot small")];
    char FcConstFamilyNamePool_str1052[sizeof("caladingsclm")];
    char FcConstFamilyNamePool_str1054[sizeof("albany")];
    char FcConstFamilyNamePool_str1056[sizeof("rit meera new")];
    char FcConstFamilyNamePool_str1061[sizeof("entypo")];
    char FcConstFamilyNamePool_str1062[sizeof("console")];
    char FcConstFamilyNamePool_str1067[sizeof("sparks dot medium")];
    char FcConstFamilyNamePool_str1069[sizeof("dejavu serif")];
    char FcConstFamilyNamePool_str1071[sizeof("yudit")];
    char FcConstFamilyNamePool_str1072[sizeof("iosevka nerd font mono")];
    char FcConstFamilyNamePool_str1074[sizeof("microsoft yahei")];
    char FcConstFamilyNamePool_str1078[sizeof("gf zemen unicode")];
    char FcConstFamilyNamePool_str1080[sizeof("iosevka nerd font")];
    char FcConstFamilyNamePool_str1084[sizeof("spectral")];
    char FcConstFamilyNamePool_str1087[sizeof("liberation sans narrow")];
    char FcConstFamilyNamePool_str1092[sizeof("simple clm")];
    char FcConstFamilyNamePool_str1097[sizeof("signfont z")];
    char FcConstFamilyNamePool_str1105[sizeof("gfs fleischman")];
    char FcConstFamilyNamePool_str1106[sizeof("spark dot-line medium")];
    char FcConstFamilyNamePool_str1109[sizeof("yehudaclm")];
    char FcConstFamilyNamePool_str1110[sizeof("petalumascript")];
    char FcConstFamilyNamePool_str1111[sizeof("source code pro")];
    char FcConstFamilyNamePool_str1126[sizeof("gfs eustace")];
    char FcConstFamilyNamePool_str1127[sizeof("pt serif")];
    char FcConstFamilyNamePool_str1128[sizeof("lilex")];
    char FcConstFamilyNamePool_str1131[sizeof("gfs galatea")];
    char FcConstFamilyNamePool_str1134[sizeof("comfortaa")];
    char FcConstFamilyNamePool_str1138[sizeof("linux biolinum")];
    char FcConstFamilyNamePool_str1141[sizeof("pt mono")];
    char FcConstFamilyNamePool_str1143[sizeof("anaktoria")];
    char FcConstFamilyNamePool_str1146[sizeof("frank ruehl clm")];
    char FcConstFamilyNamePool_str1147[sizeof("motoyalcedar")];
    char FcConstFamilyNamePool_str1148[sizeof("sazanami gothic")];
    char FcConstFamilyNamePool_str1152[sizeof("spark dot-line extrathin")];
    char FcConstFamilyNamePool_str1153[sizeof("opendyslexicmono")];
    char FcConstFamilyNamePool_str1156[sizeof("snap")];
    char FcConstFamilyNamePool_str1157[sizeof("akkadian")];
    char FcConstFamilyNamePool_str1165[sizeof("dejavusansmono nerd font")];
    char FcConstFamilyNamePool_str1171[sizeof("gfs gazis")];
    char FcConstFamilyNamePool_str1174[sizeof("gfs complutum")];
    char FcConstFamilyNamePool_str1178[sizeof("pmingliu")];
    char FcConstFamilyNamePool_str1180[sizeof("bola")];
    char FcConstFamilyNamePool_str1181[sizeof("b612")];
    char FcConstFamilyNamePool_str1186[sizeof("ar pl new sung")];
    char FcConstFamilyNamePool_str1190[sizeof("lohit konkani")];
    char FcConstFamilyNamePool_str1194[sizeof("sparks dot extrasmall")];
    char FcConstFamilyNamePool_str1195[sizeof("consolas")];
    char FcConstFamilyNamePool_str1196[sizeof("daddytimemono nerd font")];
    char FcConstFamilyNamePool_str1197[sizeof("ubuntu")];
    char FcConstFamilyNamePool_str1198[sizeof("raleway")];
    char FcConstFamilyNamePool_str1199[sizeof("ms gothic")];
    char FcConstFamilyNamePool_str1201[sizeof("linux libertine")];
    char FcConstFamilyNamePool_str1203[sizeof("tex gyre termes")];
    char FcConstFamilyNamePool_str1204[sizeof("spacemono")];
    char FcConstFamilyNamePool_str1206[sizeof("sparks dot large")];
    char FcConstFamilyNamePool_str1208[sizeof("aegyptus")];
    char FcConstFamilyNamePool_str1209[sizeof("lohit gurmukhi")];
    char FcConstFamilyNamePool_str1210[sizeof("openmoji color")];
    char FcConstFamilyNamePool_str1211[sizeof("kacstpen")];
    char FcConstFamilyNamePool_str1221[sizeof("space mono")];
    char FcConstFamilyNamePool_str1224[sizeof("rubik")];
    char FcConstFamilyNamePool_str1225[sizeof("aegean")];
    char FcConstFamilyNamePool_str1227[sizeof("minion math")];
    char FcConstFamilyNamePool_str1239[sizeof("miriam mono")];
    char FcConstFamilyNamePool_str1240[sizeof("kates")];
    char FcConstFamilyNamePool_str1243[sizeof("kacstone")];
    char FcConstFamilyNamePool_str1250[sizeof("notcouriersans")];
    char FcConstFamilyNamePool_str1256[sizeof("accanthis adf std")];
    char FcConstFamilyNamePool_str1257[sizeof("envycoder")];
    char FcConstFamilyNamePool_str1259[sizeof("mukta vaani")];
    char FcConstFamilyNamePool_str1263[sizeof("sparks dot extralarge")];
    char FcConstFamilyNamePool_str1269[sizeof("kacstart")];
    char FcConstFamilyNamePool_str1270[sizeof("silkscreen")];
    char FcConstFamilyNamePool_str1275[sizeof("microsoft yahei ui")];
    char FcConstFamilyNamePool_str1277[sizeof("ar pl new sung mono")];
    char FcConstFamilyNamePool_str1281[sizeof("mitra")];
    char FcConstFamilyNamePool_str1284[sizeof("kacsttitlel")];
    char FcConstFamilyNamePool_str1286[sizeof("kacsttitle")];
    char FcConstFamilyNamePool_str1287[sizeof("iosevkaterm nerd font")];
    char FcConstFamilyNamePool_str1291[sizeof("mukta mahee")];
    char FcConstFamilyNamePool_str1293[sizeof("kacstqura")];
    char FcConstFamilyNamePool_str1294[sizeof("kacstqurn")];
    char FcConstFamilyNamePool_str1297[sizeof("meera")];
    char FcConstFamilyNamePool_str1300[sizeof("source code vf")];
    char FcConstFamilyNamePool_str1302[sizeof("go mono nerd font")];
    char FcConstFamilyNamePool_str1307[sizeof("kacstfarsi")];
    char FcConstFamilyNamePool_str1308[sizeof("terafik")];
    char FcConstFamilyNamePool_str1320[sizeof("luxi mono")];
    char FcConstFamilyNamePool_str1323[sizeof("noto emoji")];
    char FcConstFamilyNamePool_str1324[sizeof("kacstscreen")];
    char FcConstFamilyNamePool_str1325[sizeof("beteckna")];
    char FcConstFamilyNamePool_str1327[sizeof("dejavusansm nerd font")];
    char FcConstFamilyNamePool_str1328[sizeof("im writing nerd font mono")];
    char FcConstFamilyNamePool_str1330[sizeof("ume p gothic o5")];
    char FcConstFamilyNamePool_str1335[sizeof("anka/coder narrow")];
    char FcConstFamilyNamePool_str1340[sizeof("mukta malar")];
    char FcConstFamilyNamePool_str1342[sizeof("mint mono")];
    char FcConstFamilyNamePool_str1343[sizeof("badr")];
    char FcConstFamilyNamePool_str1350[sizeof("im writing nerd font")];
    char FcConstFamilyNamePool_str1352[sizeof("gfs garaldus")];
    char FcConstFamilyNamePool_str1353[sizeof("kacstletter")];
    char FcConstFamilyNamePool_str1358[sizeof("profont")];
    char FcConstFamilyNamePool_str1362[sizeof("armnet helvetica")];
    char FcConstFamilyNamePool_str1365[sizeof("inter variable")];
    char FcConstFamilyNamePool_str1370[sizeof("latin modern math")];
    char FcConstFamilyNamePool_str1374[sizeof("sharetechmono")];
    char FcConstFamilyNamePool_str1375[sizeof("kacst-qr")];
    char FcConstFamilyNamePool_str1376[sizeof("noto sans")];
    char FcConstFamilyNamePool_str1377[sizeof("roboto slab")];
    char FcConstFamilyNamePool_str1378[sizeof("alexander")];
    char FcConstFamilyNamePool_str1383[sizeof("leelawadee ui")];
    char FcConstFamilyNamePool_str1386[sizeof("luxi sans")];
    char FcConstFamilyNamePool_str1388[sizeof("hiragino sans")];
    char FcConstFamilyNamePool_str1391[sizeof("cooper std")];
    char FcConstFamilyNamePool_str1392[sizeof("kacstposter")];
    char FcConstFamilyNamePool_str1393[sizeof("opendyslexic nerd font")];
    char FcConstFamilyNamePool_str1394[sizeof("rit keraleeyam")];
    char FcConstFamilyNamePool_str1396[sizeof("ume p gothic s5")];
    char FcConstFamilyNamePool_str1400[sizeof("petaluma script")];
    char FcConstFamilyNamePool_str1402[sizeof("nirmala ui")];
    char FcConstFamilyNamePool_str1408[sizeof("mukta devanagari")];
    char FcConstFamilyNamePool_str1410[sizeof("linux libertine mono")];
    char FcConstFamilyNamePool_str1412[sizeof("proggyclean")];
    char FcConstFamilyNamePool_str1414[sizeof("ibm 3270")];
    char FcConstFamilyNamePool_str1420[sizeof("ume p gothic s4")];
    char FcConstFamilyNamePool_str1428[sizeof("kacstdigital")];
    char FcConstFamilyNamePool_str1429[sizeof("b compset")];
    char FcConstFamilyNamePool_str1435[sizeof("pingfang tc")];
    char FcConstFamilyNamePool_str1439[sizeof("abyssinica sil")];
    char FcConstFamilyNamePool_str1440[sizeof("source han serif cn")];
    char FcConstFamilyNamePool_str1444[sizeof("lilex nerd font")];
    char FcConstFamilyNamePool_str1445[sizeof("jura")];
    char FcConstFamilyNamePool_str1447[sizeof("conakry")];
    char FcConstFamilyNamePool_str1449[sizeof("fira mono")];
    char FcConstFamilyNamePool_str1460[sizeof("lucidatypewriter")];
    char FcConstFamilyNamePool_str1464[sizeof("zysong18030")];
    char FcConstFamilyNamePool_str1469[sizeof("opendyslexicmono nerd font")];
    char FcConstFamilyNamePool_str1470[sizeof("tex gyre chorus")];
    char FcConstFamilyNamePool_str1471[sizeof("nazli")];
    char FcConstFamilyNamePool_str1474[sizeof("pingfang sc")];
    char FcConstFamilyNamePool_str1477[sizeof("miriam mono clm")];
    char FcConstFamilyNamePool_str1480[sizeof("ibm plex mono")];
    char FcConstFamilyNamePool_str1489[sizeof("fontawesome")];
    char FcConstFamilyNamePool_str1495[sizeof("scheherazade new")];
    char FcConstFamilyNamePool_str1496[sizeof("mukti")];
    char FcConstFamilyNamePool_str1498[sizeof("opendyslexic")];
    char FcConstFamilyNamePool_str1508[sizeof("noto sans ui")];
    char FcConstFamilyNamePool_str1511[sizeof("keter yg")];
    char FcConstFamilyNamePool_str1518[sizeof("paktype tehreer")];
    char FcConstFamilyNamePool_str1521[sizeof("nanummyeongjo")];
    char FcConstFamilyNamePool_str1522[sizeof("source han serif kr")];
    char FcConstFamilyNamePool_str1525[sizeof("ipapmincho")];
    char FcConstFamilyNamePool_str1528[sizeof("noto sans mono")];
    char FcConstFamilyNamePool_str1543[sizeof("fantasquesansmono")];
    char FcConstFamilyNamePool_str1547[sizeof("amiri")];
    char FcConstFamilyNamePool_str1550[sizeof("fira code")];
    char FcConstFamilyNamePool_str1552[sizeof("terminal")];
    char FcConstFamilyNamePool_str1558[sizeof("monoid")];
    char FcConstFamilyNamePool_str1561[sizeof("jadid")];
    char FcConstFamilyNamePool_str1563[sizeof("mnmlicons")];
    char FcConstFamilyNamePool_str1568[sizeof("jomolhari")];
    char FcConstFamilyNamePool_str1574[sizeof("source han mono tc")];
    char FcConstFamilyNamePool_str1577[sizeof("amiri quran")];
    char FcConstFamilyNamePool_str1579[sizeof("tex gyre cursor")];
    char FcConstFamilyNamePool_str1580[sizeof("yudit unicode")];
    char FcConstFamilyNamePool_str1586[sizeof("mint mono 35")];
    char FcConstFamilyNamePool_str1589[sizeof("gelly")];
    char FcConstFamilyNamePool_str1590[sizeof("tex gyre schola")];
    char FcConstFamilyNamePool_str1593[sizeof("ubuntu nerd font")];
    char FcConstFamilyNamePool_str1595[sizeof("dustismo")];
    char FcConstFamilyNamePool_str1596[sizeof("haettenschweiler")];
    char FcConstFamilyNamePool_str1598[sizeof("share tech mono")];
    char FcConstFamilyNamePool_str1600[sizeof("tuffy")];
    char FcConstFamilyNamePool_str1601[sizeof("ipaex\346\230\216\346\234\235")];
    char FcConstFamilyNamePool_str1607[sizeof("stix two text")];
    char FcConstFamilyNamePool_str1608[sizeof("fantasquesansmono nerd font")];
    char FcConstFamilyNamePool_str1611[sizeof("ar pl shanheisun uni")];
    char FcConstFamilyNamePool_str1613[sizeof("source han mono sc")];
    char FcConstFamilyNamePool_str1629[sizeof("tex gyre pagella")];
    char FcConstFamilyNamePool_str1634[sizeof("miriam clm")];
    char FcConstFamilyNamePool_str1642[sizeof("ipagothic")];
    char FcConstFamilyNamePool_str1646[sizeof("ume p gothic")];
    char FcConstFamilyNamePool_str1652[sizeof("cambria")];
    char FcConstFamilyNamePool_str1653[sizeof("apple sd gothic neo")];
    char FcConstFamilyNamePool_str1654[sizeof("cantarell")];
    char FcConstFamilyNamePool_str1658[sizeof("ethiopic washra")];
    char FcConstFamilyNamePool_str1660[sizeof("noto sans kannada ui")];
    char FcConstFamilyNamePool_str1661[sizeof("spark dot-line thin")];
    char FcConstFamilyNamePool_str1662[sizeof("terminus")];
    char FcConstFamilyNamePool_str1663[sizeof("ipaex\343\202\264\343\202\267\343\203\203\343\202\257")];
    char FcConstFamilyNamePool_str1664[sizeof("laconic-shadow")];
    char FcConstFamilyNamePool_str1665[sizeof("terminus (ttf)")];
    char FcConstFamilyNamePool_str1666[sizeof("3270 nerd font")];
    char FcConstFamilyNamePool_str1668[sizeof("anonymicepro nerd font mono")];
    char FcConstFamilyNamePool_str1669[sizeof("aharoniclm")];
    char FcConstFamilyNamePool_str1671[sizeof("candara")];
    char FcConstFamilyNamePool_str1672[sizeof("source han sans cn")];
    char FcConstFamilyNamePool_str1683[sizeof("ungraphic")];
    char FcConstFamilyNamePool_str1685[sizeof("inter display")];
    char FcConstFamilyNamePool_str1688[sizeof("noto sans gujarati ui")];
    char FcConstFamilyNamePool_str1690[sizeof("anonymicepro nerd font")];
    char FcConstFamilyNamePool_str1691[sizeof("noto serif")];
    char FcConstFamilyNamePool_str1696[sizeof("saysettha unicode")];
    char FcConstFamilyNamePool_str1701[sizeof("luxi serif")];
    char FcConstFamilyNamePool_str1703[sizeof("gfs artemisia")];
    char FcConstFamilyNamePool_str1704[sizeof("gfs orpheus")];
    char FcConstFamilyNamePool_str1706[sizeof("ipamonagothic")];
    char FcConstFamilyNamePool_str1713[sizeof("ms sans serif")];
    char FcConstFamilyNamePool_str1716[sizeof("tex gyre heros")];
    char FcConstFamilyNamePool_str1717[sizeof("padmaa")];
    char FcConstFamilyNamePool_str1718[sizeof("mgopen canonica")];
    char FcConstFamilyNamePool_str1719[sizeof("ume p gothic c5")];
    char FcConstFamilyNamePool_str1721[sizeof("noto sans lao ui")];
    char FcConstFamilyNamePool_str1723[sizeof("farnaz")];
    char FcConstFamilyNamePool_str1726[sizeof("gargi")];
    char FcConstFamilyNamePool_str1727[sizeof("noto sans devanagari ui")];
    char FcConstFamilyNamePool_str1729[sizeof("pigiarniq")];
    char FcConstFamilyNamePool_str1730[sizeof("ezra sil")];
    char FcConstFamilyNamePool_str1731[sizeof("source han sans tw")];
    char FcConstFamilyNamePool_str1732[sizeof("ipamonamincho")];
    char FcConstFamilyNamePool_str1734[sizeof("gfs didot classic")];
    char FcConstFamilyNamePool_str1738[sizeof("noto sans bengali ui")];
    char FcConstFamilyNamePool_str1741[sizeof("bellota")];
    char FcConstFamilyNamePool_str1743[sizeof("ume p gothic c4")];
    char FcConstFamilyNamePool_str1744[sizeof("corbel")];
    char FcConstFamilyNamePool_str1748[sizeof("noto sans tamil ui")];
    char FcConstFamilyNamePool_str1750[sizeof("ar pl uming tw")];
    char FcConstFamilyNamePool_str1764[sizeof("fkp")];
    char FcConstFamilyNamePool_str1765[sizeof("pcfont z")];
    char FcConstFamilyNamePool_str1766[sizeof("adwaita mono")];
    char FcConstFamilyNamePool_str1769[sizeof("gfs g\303\266schen")];
    char FcConstFamilyNamePool_str1777[sizeof("kacstoffice")];
    char FcConstFamilyNamePool_str1785[sizeof("harmattan")];
    char FcConstFamilyNamePool_str1788[sizeof("gfs decker")];
    char FcConstFamilyNamePool_str1797[sizeof("gohu nerd font")];
    char FcConstFamilyNamePool_str1798[sizeof("khmer ui")];
    char FcConstFamilyNamePool_str1800[sizeof("noto nastaliq urdu")];
    char FcConstFamilyNamePool_str1804[sizeof("urw bookman")];
    char FcConstFamilyNamePool_str1805[sizeof("vemana2000")];
    char FcConstFamilyNamePool_str1817[sizeof("b davat")];
    char FcConstFamilyNamePool_str1824[sizeof("news cycle")];
    char FcConstFamilyNamePool_str1828[sizeof("urdu nastaliq unicode")];
    char FcConstFamilyNamePool_str1829[sizeof("avdira")];
    char FcConstFamilyNamePool_str1834[sizeof("ar pl zenkai uni")];
    char FcConstFamilyNamePool_str1835[sizeof("gfs ambrosia")];
    char FcConstFamilyNamePool_str1841[sizeof("hasklig")];
    char FcConstFamilyNamePool_str1846[sizeof("rachana")];
    char FcConstFamilyNamePool_str1847[sizeof("lklug")];
    char FcConstFamilyNamePool_str1848[sizeof("ume gothic o5")];
    char FcConstFamilyNamePool_str1852[sizeof("paktype naqsh")];
    char FcConstFamilyNamePool_str1854[sizeof("ubuntumono nerd font")];
    char FcConstFamilyNamePool_str1855[sizeof("bitstream vera sans")];
    char FcConstFamilyNamePool_str1860[sizeof("nuosu sil")];
    char FcConstFamilyNamePool_str1861[sizeof("source han serif tw")];
    char FcConstFamilyNamePool_str1862[sizeof("3270 nerd font mono")];
    char FcConstFamilyNamePool_str1866[sizeof("carlito")];
    char FcConstFamilyNamePool_str1868[sizeof("monoid nerd font")];
    char FcConstFamilyNamePool_str1874[sizeof("vazirmatn")];
    char FcConstFamilyNamePool_str1878[sizeof("pt sans")];
    char FcConstFamilyNamePool_str1881[sizeof("envycoder nerd font")];
    char FcConstFamilyNamePool_str1882[sizeof("digna's handwriting")];
    char FcConstFamilyNamePool_str1886[sizeof("sharetechmono nerd font")];
    char FcConstFamilyNamePool_str1888[sizeof("noto sans telugu ui")];
    char FcConstFamilyNamePool_str1892[sizeof("open sans")];
    char FcConstFamilyNamePool_str1895[sizeof("adwaita sans")];
    char FcConstFamilyNamePool_str1896[sizeof("segoe ui historic")];
    char FcConstFamilyNamePool_str1899[sizeof("simsun")];
    char FcConstFamilyNamePool_str1901[sizeof("darkgarden")];
    char FcConstFamilyNamePool_str1903[sizeof("profont nerd font")];
    char FcConstFamilyNamePool_str1908[sizeof("spacemono nerd font")];
    char FcConstFamilyNamePool_str1909[sizeof("yu gothic ui")];
    char FcConstFamilyNamePool_str1913[sizeof("droid sans ethiopic")];
    char FcConstFamilyNamePool_str1914[sizeof("ume gothic s5")];
    char FcConstFamilyNamePool_str1917[sizeof("khmer os muol")];
    char FcConstFamilyNamePool_str1923[sizeof("gfs bodoni classic")];
    char FcConstFamilyNamePool_str1926[sizeof("ume gothic s4")];
    char FcConstFamilyNamePool_str1927[sizeof("ibm 3270 nerd font")];
    char FcConstFamilyNamePool_str1929[sizeof("cumberland amt")];
    char FcConstFamilyNamePool_str1931[sizeof("cumberland")];
    char FcConstFamilyNamePool_str1934[sizeof("khmer os")];
    char FcConstFamilyNamePool_str1938[sizeof("cousine")];
    char FcConstFamilyNamePool_str1941[sizeof("gfs ignacio")];
    char FcConstFamilyNamePool_str1943[sizeof("ubuntu condensed")];
    char FcConstFamilyNamePool_str1948[sizeof("lao ui")];
    char FcConstFamilyNamePool_str1958[sizeof("source han serif jp")];
    char FcConstFamilyNamePool_str1962[sizeof("pt serif caption")];
    char FcConstFamilyNamePool_str1969[sizeof("overpass mono")];
    char FcConstFamilyNamePool_str1970[sizeof("gfs neohellenic")];
    char FcConstFamilyNamePool_str1979[sizeof("bitstream vera sans mono")];
    char FcConstFamilyNamePool_str1988[sizeof("awami nastaliq")];
    char FcConstFamilyNamePool_str1991[sizeof("new athena unicode")];
    char FcConstFamilyNamePool_str1992[sizeof("zapfino")];
    char FcConstFamilyNamePool_str1997[sizeof("gfs orpheus sans")];
    char FcConstFamilyNamePool_str1999[sizeof("overpass")];
    char FcConstFamilyNamePool_str2003[sizeof("gentium plus")];
    char FcConstFamilyNamePool_str2004[sizeof("amiri quran colored")];
    char FcConstFamilyNamePool_str2010[sizeof("keyfont v2")];
    char FcConstFamilyNamePool_str2018[sizeof("courier")];
    char FcConstFamilyNamePool_str2024[sizeof("ipaexgothic")];
    char FcConstFamilyNamePool_str2026[sizeof("cascadia mono")];
    char FcConstFamilyNamePool_str2031[sizeof("xits math")];
    char FcConstFamilyNamePool_str2040[sizeof("gillius adf")];
    char FcConstFamilyNamePool_str2046[sizeof("silkscreen expanded")];
    char FcConstFamilyNamePool_str2048[sizeof("unshinmun")];
    char FcConstFamilyNamePool_str2050[sizeof("khmer mondulkiri")];
    char FcConstFamilyNamePool_str2054[sizeof("proggyclean nerd font")];
    char FcConstFamilyNamePool_str2055[sizeof("agave")];
    char FcConstFamilyNamePool_str2059[sizeof("shofar")];
    char FcConstFamilyNamePool_str2062[sizeof("sampige")];
    char FcConstFamilyNamePool_str2069[sizeof("tex gyre bonum")];
    char FcConstFamilyNamePool_str2072[sizeof("hiragino sans cns")];
    char FcConstFamilyNamePool_str2074[sizeof("vazirmatn ui")];
    char FcConstFamilyNamePool_str2083[sizeof("hermit")];
    char FcConstFamilyNamePool_str2087[sizeof("khmer os system")];
    char FcConstFamilyNamePool_str2097[sizeof("stix two math")];
    char FcConstFamilyNamePool_str2100[sizeof("vazirmatn nl")];
    char FcConstFamilyNamePool_str2105[sizeof("bitstream vera serif")];
    char FcConstFamilyNamePool_str2106[sizeof("stevehand")];
    char FcConstFamilyNamePool_str2111[sizeof("arshia")];
    char FcConstFamilyNamePool_str2114[sizeof("campania")];
    char FcConstFamilyNamePool_str2115[sizeof("drugulinclm")];
    char FcConstFamilyNamePool_str2116[sizeof("source han mono hc")];
    char FcConstFamilyNamePool_str2120[sizeof("beteckna small caps")];
    char FcConstFamilyNamePool_str2126[sizeof("raghindi")];
    char FcConstFamilyNamePool_str2127[sizeof("vazirmatn ui nl")];
    char FcConstFamilyNamePool_str2130[sizeof("ms mincho")];
    char FcConstFamilyNamePool_str2144[sizeof("fontawesome 7 free")];
    char FcConstFamilyNamePool_str2155[sizeof("heuristica")];
    char FcConstFamilyNamePool_str2157[sizeof("public sans")];
    char FcConstFamilyNamePool_str2158[sizeof("noto sans sinhala ui")];
    char FcConstFamilyNamePool_str2161[sizeof("sparks bar medium")];
    char FcConstFamilyNamePool_str2167[sizeof("gfs baskerville")];
    char FcConstFamilyNamePool_str2171[sizeof("goudy bookletter 1911")];
    char FcConstFamilyNamePool_str2177[sizeof("ipaexmincho")];
    char FcConstFamilyNamePool_str2183[sizeof("umeplus gothic")];
    char FcConstFamilyNamePool_str2189[sizeof("vazirmatn rd ui")];
    char FcConstFamilyNamePool_str2199[sizeof("ume gothic")];
    char FcConstFamilyNamePool_str2205[sizeof("copperplate gothic std")];
    char FcConstFamilyNamePool_str2206[sizeof("stam sefarad clm")];
    char FcConstFamilyNamePool_str2214[sizeof("vazirmatn rd nl")];
    char FcConstFamilyNamePool_str2216[sizeof("spark dot-line extrathick")];
    char FcConstFamilyNamePool_str2228[sizeof("ar pl ukai tw")];
    char FcConstFamilyNamePool_str2237[sizeof("ume gothic c5")];
    char FcConstFamilyNamePool_str2240[sizeof("ar pl mingti2l big5")];
    char FcConstFamilyNamePool_str2245[sizeof("gentium basic")];
    char FcConstFamilyNamePool_str2247[sizeof("palatino linotype")];
    char FcConstFamilyNamePool_str2248[sizeof("vazirmatn rd")];
    char FcConstFamilyNamePool_str2249[sizeof("ume gothic c4")];
    char FcConstFamilyNamePool_str2251[sizeof("tex gyre heros cn")];
    char FcConstFamilyNamePool_str2253[sizeof("courier std")];
    char FcConstFamilyNamePool_str2258[sizeof("kinnari")];
    char FcConstFamilyNamePool_str2274[sizeof("urw gothic")];
    char FcConstFamilyNamePool_str2277[sizeof("kacstbook")];
    char FcConstFamilyNamePool_str2281[sizeof("hasklug nerd font")];
    char FcConstFamilyNamePool_str2282[sizeof("android emoji")];
    char FcConstFamilyNamePool_str2293[sizeof("droid sans devanagari")];
    char FcConstFamilyNamePool_str2294[sizeof("antykwatorunska")];
    char FcConstFamilyNamePool_str2301[sizeof("terminess nerd font")];
    char FcConstFamilyNamePool_str2306[sizeof("noto sans thai ui")];
    char FcConstFamilyNamePool_str2307[sizeof("khmer os muol light")];
    char FcConstFamilyNamePool_str2313[sizeof("pothana2000")];
    char FcConstFamilyNamePool_str2338[sizeof("kamran")];
    char FcConstFamilyNamePool_str2339[sizeof("proggysquarettsz")];
    char FcConstFamilyNamePool_str2341[sizeof("merriweather")];
    char FcConstFamilyNamePool_str2344[sizeof("victormono")];
    char FcConstFamilyNamePool_str2348[sizeof("victor mono")];
    char FcConstFamilyNamePool_str2358[sizeof("droid sans japanese")];
    char FcConstFamilyNamePool_str2375[sizeof("saab")];
    char FcConstFamilyNamePool_str2378[sizeof("microsoft jhenghei")];
    char FcConstFamilyNamePool_str2382[sizeof("umeplus p gothic")];
    char FcConstFamilyNamePool_str2394[sizeof("microsoft jhenghei ui")];
    char FcConstFamilyNamePool_str2397[sizeof("ibmplexmono nerd font")];
    char FcConstFamilyNamePool_str2399[sizeof("khmer os content")];
    char FcConstFamilyNamePool_str2402[sizeof("vazirmatn rd ui nl")];
    char FcConstFamilyNamePool_str2405[sizeof("noto sans khmer ui")];
    char FcConstFamilyNamePool_str2410[sizeof("ume ui gothic o5")];
    char FcConstFamilyNamePool_str2414[sizeof("agave nerd font")];
    char FcConstFamilyNamePool_str2415[sizeof("apple color emoji")];
    char FcConstFamilyNamePool_str2416[sizeof("gfs orpheus classic")];
    char FcConstFamilyNamePool_str2426[sizeof("source han sans kr")];
    char FcConstFamilyNamePool_str2430[sizeof("asana math")];
    char FcConstFamilyNamePool_str2434[sizeof("b612 mono")];
    char FcConstFamilyNamePool_str2435[sizeof("kacstnaskh")];
    char FcConstFamilyNamePool_str2436[sizeof("hurmit nerd font")];
    char FcConstFamilyNamePool_str2443[sizeof("overpass nerd font")];
    char FcConstFamilyNamePool_str2447[sizeof("patrick hand")];
    char FcConstFamilyNamePool_str2462[sizeof("mukti narrow")];
    char FcConstFamilyNamePool_str2469[sizeof("prociono")];
    char FcConstFamilyNamePool_str2480[sizeof("baekmuk gulim")];
    char FcConstFamilyNamePool_str2482[sizeof("baekmuk dotum")];
    char FcConstFamilyNamePool_str2492[sizeof("itc zapf chancery std")];
    char FcConstFamilyNamePool_str2496[sizeof("cousine nerd font")];
    char FcConstFamilyNamePool_str2504[sizeof("beteckna lower case")];
    char FcConstFamilyNamePool_str2511[sizeof("cascadia code")];
    char FcConstFamilyNamePool_str2515[sizeof("merriweather sans")];
    char FcConstFamilyNamePool_str2523[sizeof("league gothic")];
    char FcConstFamilyNamePool_str2524[sizeof("cascadia mono nf")];
    char FcConstFamilyNamePool_str2525[sizeof("antykwatorunska medium")];
    char FcConstFamilyNamePool_str2534[sizeof("bigblueterminal")];
    char FcConstFamilyNamePool_str2541[sizeof("ume ui gothic")];
    char FcConstFamilyNamePool_str2559[sizeof("andika new basic")];
    char FcConstFamilyNamePool_str2566[sizeof("sparks bar wide")];
    char FcConstFamilyNamePool_str2573[sizeof("ipamincho")];
    char FcConstFamilyNamePool_str2574[sizeof("courier new")];
    char FcConstFamilyNamePool_str2590[sizeof("hiragino mincho pron")];
    char FcConstFamilyNamePool_str2600[sizeof("gb_ss_gb18030")];
    char FcConstFamilyNamePool_str2610[sizeof("agave nerd font mono")];
    char FcConstFamilyNamePool_str2615[sizeof("dancing script")];
    char FcConstFamilyNamePool_str2626[sizeof("charis sil")];
    char FcConstFamilyNamePool_str2639[sizeof("noto sans myanmar ui")];
    char FcConstFamilyNamePool_str2646[sizeof("sparks bar extrawide")];
    char FcConstFamilyNamePool_str2647[sizeof("vdrsymbols sans")];
    char FcConstFamilyNamePool_str2648[sizeof("lohit punjabi")];
    char FcConstFamilyNamePool_str2649[sizeof("pingfang hk")];
    char FcConstFamilyNamePool_str2660[sizeof("khmer os muol pali")];
    char FcConstFamilyNamePool_str2663[sizeof("noto sans math")];
    char FcConstFamilyNamePool_str2665[sizeof("stam ashkenaz clm")];
    char FcConstFamilyNamePool_str2674[sizeof("gfs jackson")];
    char FcConstFamilyNamePool_str2698[sizeof("sparks bar extranarrow")];
    char FcConstFamilyNamePool_str2706[sizeof("m yuppy gb")];
    char FcConstFamilyNamePool_str2722[sizeof("gubbi")];
    char FcConstFamilyNamePool_str2736[sizeof("noto sans malayalam ui")];
    char FcConstFamilyNamePool_str2748[sizeof("hack")];
    char FcConstFamilyNamePool_str2751[sizeof("baekmuk batang")];
    char FcConstFamilyNamePool_str2755[sizeof("symbola")];
    char FcConstFamilyNamePool_str2775[sizeof("droid sans fallback")];
    char FcConstFamilyNamePool_str2777[sizeof("finale broadway text")];
    char FcConstFamilyNamePool_str2779[sizeof("heavydata")];
    char FcConstFamilyNamePool_str2780[sizeof("cascadia mono pl")];
    char FcConstFamilyNamePool_str2792[sizeof("noto sans oriya ui")];
    char FcConstFamilyNamePool_str2803[sizeof("antykwatorunskacond")];
    char FcConstFamilyNamePool_str2834[sizeof("vl gothic")];
    char FcConstFamilyNamePool_str2839[sizeof("britannic")];
    char FcConstFamilyNamePool_str2841[sizeof("hapax berb\303\250re")];
    char FcConstFamilyNamePool_str2848[sizeof("jg laotimes")];
    char FcConstFamilyNamePool_str2851[sizeof("khmer os freehand")];
    char FcConstFamilyNamePool_str2856[sizeof("bigblueterminal nerd font")];
    char FcConstFamilyNamePool_str2857[sizeof("tex gyre adventor")];
    char FcConstFamilyNamePool_str2858[sizeof("work sans")];
    char FcConstFamilyNamePool_str2873[sizeof("artsounk")];
    char FcConstFamilyNamePool_str2878[sizeof("noto fangsong kss vertical")];
    char FcConstFamilyNamePool_str2882[sizeof("noto color emoji")];
    char FcConstFamilyNamePool_str2892[sizeof("twentieth century")];
    char FcConstFamilyNamePool_str2899[sizeof("helvetica")];
    char FcConstFamilyNamePool_str2903[sizeof("khmer os fasthand")];
    char FcConstFamilyNamePool_str2907[sizeof("jetbrains mono")];
    char FcConstFamilyNamePool_str2908[sizeof("emoji one")];
    char FcConstFamilyNamePool_str2915[sizeof("antykwatorunskacond medium")];
    char FcConstFamilyNamePool_str2929[sizeof("nimbus mono")];
    char FcConstFamilyNamePool_str2930[sizeof("jg lao old arial")];
    char FcConstFamilyNamePool_str2933[sizeof("ms ui gothic")];
    char FcConstFamilyNamePool_str2936[sizeof("antykwatorunskacond light")];
    char FcConstFamilyNamePool_str2937[sizeof("noto fangsong kss rotated")];
    char FcConstFamilyNamePool_str2947[sizeof("gb_ss_gb18030_extended")];
    char FcConstFamilyNamePool_str2968[sizeof("victormono nerd font")];
    char FcConstFamilyNamePool_str2979[sizeof("gentium plus compact")];
    char FcConstFamilyNamePool_str2994[sizeof("verdana")];
    char FcConstFamilyNamePool_str2995[sizeof("cascadia code nf")];
    char FcConstFamilyNamePool_str3008[sizeof("cambria math")];
    char FcConstFamilyNamePool_str3022[sizeof("nimbus roman")];
    char FcConstFamilyNamePool_str3034[sizeof("ar pl uming hk")];
    char FcConstFamilyNamePool_str3046[sizeof("red hat display")];
    char FcConstFamilyNamePool_str3052[sizeof("droid sans hebrew")];
    char FcConstFamilyNamePool_str3054[sizeof("ipapgothic")];
    char FcConstFamilyNamePool_str3056[sizeof("nimbus sans")];
    char FcConstFamilyNamePool_str3060[sizeof("sparks bar narrow")];
    char FcConstFamilyNamePool_str3123[sizeof("antykwatorunska light")];
    char FcConstFamilyNamePool_str3127[sizeof("nimbus mono l")];
    char FcConstFamilyNamePool_str3154[sizeof("hanyisong")];
    char FcConstFamilyNamePool_str3155[sizeof("droid arabic kufi")];
    char FcConstFamilyNamePool_str3164[sizeof("khmer os siemreap")];
    char FcConstFamilyNamePool_str3188[sizeof("nimbus sans l")];
    char FcConstFamilyNamePool_str3193[sizeof("source han sans jp")];
    char FcConstFamilyNamePool_str3203[sizeof("proggytinyttsz")];
    char FcConstFamilyNamePool_str3210[sizeof("bauhaus std")];
    char FcConstFamilyNamePool_str3221[sizeof("noto sans gurmukhi ui")];
    char FcConstFamilyNamePool_str3230[sizeof("yanone kaffeesatz")];
    char FcConstFamilyNamePool_str3236[sizeof("jetbrainsmono nerd font")];
    char FcConstFamilyNamePool_str3245[sizeof("fontawesome 7 brands")];
    char FcConstFamilyNamePool_str3251[sizeof("cascadia code pl")];
    char FcConstFamilyNamePool_str3263[sizeof("nimbus mono ps")];
    char FcConstFamilyNamePool_str3264[sizeof("nimbus roman no9 l")];
    char FcConstFamilyNamePool_str3271[sizeof("sophia nubian")];
    char FcConstFamilyNamePool_str3279[sizeof("source han mono k")];
    char FcConstFamilyNamePool_str3298[sizeof("khmer os metal chrieng")];
    char FcConstFamilyNamePool_str3315[sizeof("emoji two")];
    char FcConstFamilyNamePool_str3323[sizeof("ektype baloo 2")];
    char FcConstFamilyNamePool_str3326[sizeof("bitstreamverasansmono nerd font")];
    char FcConstFamilyNamePool_str3337[sizeof("noto sans arabic ui")];
    char FcConstFamilyNamePool_str3342[sizeof("padauk")];
    char FcConstFamilyNamePool_str3369[sizeof("trebuchet ms")];
    char FcConstFamilyNamePool_str3392[sizeof("emojione mozilla")];
    char FcConstFamilyNamePool_str3409[sizeof("koodak")];
    char FcConstFamilyNamePool_str3412[sizeof("ar pl ukai hk")];
    char FcConstFamilyNamePool_str3419[sizeof("ektype baloo tamma 2")];
    char FcConstFamilyNamePool_str3421[sizeof("ektype baloo tammudu 2")];
    char FcConstFamilyNamePool_str3425[sizeof("bpg glaho international")];
    char FcConstFamilyNamePool_str3440[sizeof("helvetica lt std")];
    char FcConstFamilyNamePool_str3443[sizeof("tribun adf std")];
    char FcConstFamilyNamePool_str3451[sizeof("finale broadway")];
    char FcConstFamilyNamePool_str3452[sizeof("ar pl sungtil gb")];
    char FcConstFamilyNamePool_str3470[sizeof("joypixels")];
    char FcConstFamilyNamePool_str3486[sizeof("heavydata nerd font")];
    char FcConstFamilyNamePool_str3497[sizeof("kacstdecorative")];
    char FcConstFamilyNamePool_str3501[sizeof("biz udmincho")];
    char FcConstFamilyNamePool_str3533[sizeof("kochi mincho")];
    char FcConstFamilyNamePool_str3551[sizeof("source han code jp")];
    char FcConstFamilyNamePool_str3559[sizeof("noto naskh arabic ui")];
    char FcConstFamilyNamePool_str3587[sizeof("bonvenocf")];
    char FcConstFamilyNamePool_str3621[sizeof("ektype baloo da 2")];
    char FcConstFamilyNamePool_str3623[sizeof("bpg utf8 m")];
    char FcConstFamilyNamePool_str3649[sizeof("droid arabic naskh")];
    char FcConstFamilyNamePool_str3661[sizeof("biz udpgothic")];
    char FcConstFamilyNamePool_str3670[sizeof("kerkis")];
    char FcConstFamilyNamePool_str3684[sizeof("noto kufi arabic")];
    char FcConstFamilyNamePool_str3687[sizeof("biz udpmincho")];
    char FcConstFamilyNamePool_str3694[sizeof("rit tn joy")];
    char FcConstFamilyNamePool_str3762[sizeof("tagmukay")];
    char FcConstFamilyNamePool_str3783[sizeof("century gothic")];
    char FcConstFamilyNamePool_str3800[sizeof("spark dot-line thick")];
    char FcConstFamilyNamePool_str3890[sizeof("zilla slab")];
    char FcConstFamilyNamePool_str3895[sizeof("openmoji black")];
    char FcConstFamilyNamePool_str3904[sizeof("paktype naskh basic wide")];
    char FcConstFamilyNamePool_str3909[sizeof("paktype naskh basic semi wide")];
    char FcConstFamilyNamePool_str3922[sizeof("hiragino sans gb")];
    char FcConstFamilyNamePool_str3924[sizeof("gb_ss_gb18030_extendedk")];
    char FcConstFamilyNamePool_str3926[sizeof("noto sans cjk tc")];
    char FcConstFamilyNamePool_str3935[sizeof("noto naskh arabic")];
    char FcConstFamilyNamePool_str3959[sizeof("noto sans mono cjk tc")];
    char FcConstFamilyNamePool_str3965[sizeof("noto sans cjk sc")];
    char FcConstFamilyNamePool_str3998[sizeof("noto sans mono cjk sc")];
    char FcConstFamilyNamePool_str4016[sizeof("charis sil compact")];
    char FcConstFamilyNamePool_str4052[sizeof("noto serif cjk tc")];
    char FcConstFamilyNamePool_str4089[sizeof("paktype naskh basic")];
    char FcConstFamilyNamePool_str4091[sizeof("noto serif cjk sc")];
    char FcConstFamilyNamePool_str4156[sizeof("ektype baloo thambi 2")];
    char FcConstFamilyNamePool_str4173[sizeof("unikurd web")];
    char FcConstFamilyNamePool_str4191[sizeof("zilla slab highlight")];
    char FcConstFamilyNamePool_str4221[sizeof("wenquanyi zen hei")];
    char FcConstFamilyNamePool_str4246[sizeof("vl pgothic")];
    char FcConstFamilyNamePool_str4263[sizeof("khmer os bokor")];
    char FcConstFamilyNamePool_str4275[sizeof("wenquanyi micro hei")];
    char FcConstFamilyNamePool_str4292[sizeof("wenquanyi bitmap song")];
    char FcConstFamilyNamePool_str4342[sizeof("noto sans cjk kr")];
    char FcConstFamilyNamePool_str4373[sizeof("khmer os battambang")];
    char FcConstFamilyNamePool_str4385[sizeof("ektype baloo chettan 2")];
    char FcConstFamilyNamePool_str4461[sizeof("biz udgothic")];
    char FcConstFamilyNamePool_str4467[sizeof("wenquanyi micro hei mono")];
    char FcConstFamilyNamePool_str4468[sizeof("noto serif cjk kr")];
    char FcConstFamilyNamePool_str4493[sizeof("kochi gothic")];
    char FcConstFamilyNamePool_str4501[sizeof("padauk book")];
    char FcConstFamilyNamePool_str4533[sizeof("ukij tuz")];
    char FcConstFamilyNamePool_str4559[sizeof("gentium book basic")];
    char FcConstFamilyNamePool_str4670[sizeof("bravura")];
    char FcConstFamilyNamePool_str4684[sizeof("noto sans mono cjk kr")];
    char FcConstFamilyNamePool_str4816[sizeof("bravuratext")];
    char FcConstFamilyNamePool_str5061[sizeof("ektype baloo bhai 2")];
    char FcConstFamilyNamePool_str5173[sizeof("noto sans mono cjk hk")];
    char FcConstFamilyNamePool_str5248[sizeof("ektype baloo bhaina 2")];
    char FcConstFamilyNamePool_str5270[sizeof("caskaydiacove nerd font mono")];
    char FcConstFamilyNamePool_str5287[sizeof("ektype baloo paaji 2")];
    char FcConstFamilyNamePool_str5292[sizeof("caskaydiacove nerd font")];
    char FcConstFamilyNamePool_str5328[sizeof("wenquanyi zen hei sharp")];
    char FcConstFamilyNamePool_str5451[sizeof("noto sans mono cjk jp")];
    char FcConstFamilyNamePool_str5545[sizeof("noto sans cjk jp")];
    char FcConstFamilyNamePool_str5671[sizeof("noto serif cjk jp")];
    char FcConstFamilyNamePool_str5812[sizeof("noto sans cjk hk")];
    char FcConstFamilyNamePool_str5938[sizeof("noto serif cjk hk")];
  };
static const struct FcConstFamilyNamePool_t FcConstFamilyNamePool_contents =
  {
    "nu",
    "aqui",
    "muli",
    "untaza",
    "loma",
    "lato",
    "nunito",
    "nasim",
    "madan",
    "navilu",
    "madan2",
    "unbom",
    "andika",
    "undotum",
    "tiza",
    "unvada",
    "unpen",
    "undinaru",
    "unpilgia",
    "uniol",
    "mononoki",
    "unjamosora",
    "tahoma",
    "norasi",
    "lime",
    "alkalami",
    "elham",
    "\357\275\215\357\275\223 \346\230\216\346\234\235",
    "m+ 1mn",
    "ungungseo",
    "unjamodotum",
    "molot",
    "m+ 2m",
    "m+ 1m",
    "andale mono",
    "asea",
    "sina",
    "unpenheulim",
    "lohit tamil",
    "unyetgul",
    "titillium",
    "unjamonovel",
    "roya",
    "lohit gujarati",
    "lohit nepali",
    "inconsolata",
    "lohit telugu",
    "aroania",
    "tabassom",
    "\357\275\215\357\275\223 \343\202\264\343\202\267\343\203\203\343\202\257",
    "roboto",
    "musica",
    "lohit malayalam",
    "albany amt",
    "arial",
    "lohit maithili",
    "edwin",
    "nsimsun",
    "smoothansi",
    "literata",
    "d-din",
    "leland",
    "impact",
    "gohu",
    "roboto mono",
    "tinos",
    "lohit odia",
    "lohit hindi",
    "david clm",
    "times",
    "lohit kannada",
    "lohit sindhi",
    "garuda",
    "mingliu",
    "agbalumo",
    "oswald",
    "inter",
    "analecta",
    "mingzat",
    "irannastaliq",
    "leland text",
    "liberation mono",
    "titr",
    "lohit assamese",
    "aurulentsansmono",
    "lohit devanagari",
    "tlwgmono",
    "tlwgtypo",
    "stix",
    "infofont",
    "edges",
    "latin modern roman",
    "lohit bengali",
    "inconsolatago",
    "alegreya",
    "monofur",
    "annapurna sil",
    "montserrat",
    "anorexia",
    "sniglet",
    "segoe ui",
    "geist",
    "geist mono",
    "exo 2",
    "intel one mono",
    "freemono",
    "libertinus",
    "fixed",
    "tlwgtypist",
    "mints mild",
    "signfont",
    "arundina sans",
    "anka/coder",
    "inconsolata go",
    "unbatang",
    "alef",
    "grand hotel",
    "material icons",
    "segoe ui emoji",
    "unjamobatang",
    "tobecontinued",
    "lateef",
    "fantezi",
    "dror",
    "garamond",
    "latin modern roman demi",
    "fixedsys",
    "latin modern roman unslanted",
    "great vibes",
    "droid sans mono",
    "times new roman",
    "latin modern roman dunhill",
    "drift",
    "nafees nastaleeq",
    "nanumgothic_coding",
    "droid sans",
    "droid sans thai",
    "liberation sans",
    "droid sans armenian",
    "namdhinggo sil",
    "droid serif",
    "c059",
    "oxygen mono",
    "foundation icons",
    "z003",
    "tscu_paranar",
    "alegreya sans",
    "latin modern roman slanted",
    "droid sans tamil",
    "segoe ui symbol",
    "clean",
    "caladea",
    "mints strong",
    "laconic",
    "cure",
    "malayalam",
    "freesans",
    "code2000",
    "freeserif",
    "code2001",
    "tinos nerd font",
    "iosevka",
    "zar",
    "arundina sans mono",
    "unifrakturmaguntia",
    "nanumgothic",
    "d-din condensed",
    "elliniaclm",
    "manchu2005",
    "inconsolata bold",
    "red hat mono",
    "fangsong ti",
    "montserrat alternates",
    "oxygen-sans",
    "p052",
    "meiryo ui",
    "aurulentsansmono nerd font",
    "gfs didot",
    "motoyalmaru",
    "constantia",
    "montserrat underline",
    "andika compact",
    "tlwgtypewriter",
    "source sans 3",
    "notomono nerd font",
    "perizia",
    "gomono nerd font",
    "mononoki nerd font",
    "codenewroman",
    "gfs nicefore",
    "gfs porson",
    "petaluma",
    "meslo",
    "comic neue",
    "homa",
    "comicshannsmono",
    "red hat text",
    "inconsolata regular",
    "lpfont",
    "hanamin",
    "lekton",
    "gfs solomos",
    "hasida",
    "iosevkaterm",
    "ms serif",
    "arundina serif",
    "waree",
    "\303\251colier court",
    "lotoos",
    "ume mincho",
    "nachlieli",
    "liberationmono nerd font",
    "arimo",
    "umpush",
    "widelands",
    "liberation serif",
    "daddytimemono",
    "elephant",
    "ume p mincho",
    "mplus",
    "arial unicode",
    "lohit marathi",
    "clear sans",
    "songti tc",
    "lohit kashmiri",
    "ume hy gothic o5",
    "petalumatext",
    "source serif 4",
    "go mono",
    "comic sans ms",
    "lucida math",
    "delphine",
    "firamono nerd font",
    "songti sc",
    "iosevka term",
    "gfs pyrsos",
    "anonymous pro",
    "dejavu sans",
    "ume mincho s3",
    "latin modern roman caps",
    "smonohand",
    "droid sans georgian",
    "d-din exp",
    "inconsolata nerd font",
    "sazanami mincho",
    "monofur nerd font",
    "ia writer duo",
    "ume p mincho s3",
    "thorndale",
    "glisp",
    "inconsolatago nerd font",
    "ia writer mono",
    "robotomono nerd font",
    "droidsansmono nerd font",
    "gfs theokritos",
    "crete round",
    "ferdosi",
    "firacode nerd font mono",
    "old standard sfd",
    "firacode nerd font",
    "lucida sans unicode",
    "ume hy gothic",
    "calibri",
    "simsong",
    "georgia",
    "shimenkan",
    "codenewroman nerd font mono",
    "apparatus sil",
    "hadasim clm",
    "nachlieli clm",
    "codenewroman nerd font",
    "roboto condensed",
    "comic neue angular",
    "droidsansm nerd font",
    "domestic manners",
    "dejavu sans mono",
    "frank ruehl",
    "applemyungjo",
    "letters laughing",
    "gfs philostratos",
    "nanumgothiccoding",
    "gfs bodoni",
    "dai banna sil",
    "source han mono",
    "comicshannsmono nerd font",
    "sourcecodepro nerd font",
    "inconsolatalgc",
    "tai heritage pro",
    "ia writer quattro",
    "rit rachana",
    "meslo nerd font",
    "mplus nerd font",
    "geistmono nerd font",
    "pcfont",
    "anka/coder condensed",
    "thorndale amt",
    "twitter color emoji",
    "inconsolatalgc nerd font",
    "cormorant",
    "arial unicode ms",
    "infofont z",
    "mgopen modata",
    "lekton nerd font",
    "arimo nerd font",
    "gfs olga",
    "sparks dot small",
    "caladingsclm",
    "albany",
    "rit meera new",
    "entypo",
    "console",
    "sparks dot medium",
    "dejavu serif",
    "yudit",
    "iosevka nerd font mono",
    "microsoft yahei",
    "gf zemen unicode",
    "iosevka nerd font",
    "spectral",
    "liberation sans narrow",
    "simple clm",
    "signfont z",
    "gfs fleischman",
    "spark dot-line medium",
    "yehudaclm",
    "petalumascript",
    "source code pro",
    "gfs eustace",
    "pt serif",
    "lilex",
    "gfs galatea",
    "comfortaa",
    "linux biolinum",
    "pt mono",
    "anaktoria",
    "frank ruehl clm",
    "motoyalcedar",
    "sazanami gothic",
    "spark dot-line extrathin",
    "opendyslexicmono",
    "snap",
    "akkadian",
    "dejavusansmono nerd font",
    "gfs gazis",
    "gfs complutum",
    "pmingliu",
    "bola",
    "b612",
    "ar pl new sung",
    "lohit konkani",
    "sparks dot extrasmall",
    "consolas",
    "daddytimemono nerd font",
    "ubuntu",
    "raleway",
    "ms gothic",
    "linux libertine",
    "tex gyre termes",
    "spacemono",
    "sparks dot large",
    "aegyptus",
    "lohit gurmukhi",
    "openmoji color",
    "kacstpen",
    "space mono",
    "rubik",
    "aegean",
    "minion math",
    "miriam mono",
    "kates",
    "kacstone",
    "notcouriersans",
    "accanthis adf std",
    "envycoder",
    "mukta vaani",
    "sparks dot extralarge",
    "kacstart",
    "silkscreen",
    "microsoft yahei ui",
    "ar pl new sung mono",
    "mitra",
    "kacsttitlel",
    "kacsttitle",
    "iosevkaterm nerd font",
    "mukta mahee",
    "kacstqura",
    "kacstqurn",
    "meera",
    "source code vf",
    "go mono nerd font",
    "kacstfarsi",
    "terafik",
    "luxi mono",
    "noto emoji",
    "kacstscreen",
    "beteckna",
    "dejavusansm nerd font",
    "im writing nerd font mono",
    "ume p gothic o5",
    "anka/coder narrow",
    "mukta malar",
    "mint mono",
    "badr",
    "im writing nerd font",
    "gfs garaldus",
    "kacstletter",
    "profont",
    "armnet helvetica",
    "inter variable",
    "latin modern math",
    "sharetechmono",
    "kacst-qr",
    "noto sans",
    "roboto slab",
    "alexander",
    "leelawadee ui",
    "luxi sans",
    "hiragino sans",
    "cooper std",
    "kacstposter",
    "opendyslexic nerd font",
    "rit keraleeyam",
    "ume p gothic s5",
    "petaluma script",
    "nirmala ui",
    "mukta devanagari",
    "linux libertine mono",
    "proggyclean",
    "ibm 3270",
    "ume p gothic s4",
    "kacstdigital",
    "b compset",
    "pingfang tc",
    "abyssinica sil",
    "source han serif cn",
    "lilex nerd font",
    "jura",
    "conakry",
    "fira mono",
    "lucidatypewriter",
    "zysong18030",
    "opendyslexicmono nerd font",
    "tex gyre chorus",
    "nazli",
    "pingfang sc",
    "miriam mono clm",
    "ibm plex mono",
    "fontawesome",
    "scheherazade new",
    "mukti",
    "opendyslexic",
    "noto sans ui",
    "keter yg",
    "paktype tehreer",
    "nanummyeongjo",
    "source han serif kr",
    "ipapmincho",
    "noto sans mono",
    "fantasquesansmono",
    "amiri",
    "fira code",
    "terminal",
    "monoid",
    "jadid",
    "mnmlicons",
    "jomolhari",
    "source han mono tc",
    "amiri quran",
    "tex gyre cursor",
    "yudit unicode",
    "mint mono 35",
    "gelly",
    "tex gyre schola",
    "ubuntu nerd font",
    "dustismo",
    "haettenschweiler",
    "share tech mono",
    "tuffy",
    "ipaex\346\230\216\346\234\235",
    "stix two text",
    "fantasquesansmono nerd font",
    "ar pl shanheisun uni",
    "source han mono sc",
    "tex gyre pagella",
    "miriam clm",
    "ipagothic",
    "ume p gothic",
    "cambria",
    "apple sd gothic neo",
    "cantarell",
    "ethiopic washra",
    "noto sans kannada ui",
    "spark dot-line thin",
    "terminus",
    "ipaex\343\202\264\343\202\267\343\203\203\343\202\257",
    "laconic-shadow",
    "terminus (ttf)",
    "3270 nerd font",
    "anonymicepro nerd font mono",
    "aharoniclm",
    "candara",
    "source han sans cn",
    "ungraphic",
    "inter display",
    "noto sans gujarati ui",
    "anonymicepro nerd font",
    "noto serif",
    "saysettha unicode",
    "luxi serif",
    "gfs artemisia",
    "gfs orpheus",
    "ipamonagothic",
    "ms sans serif",
    "tex gyre heros",
    "padmaa",
    "mgopen canonica",
    "ume p gothic c5",
    "noto sans lao ui",
    "farnaz",
    "gargi",
    "noto sans devanagari ui",
    "pigiarniq",
    "ezra sil",
    "source han sans tw",
    "ipamonamincho",
    "gfs didot classic",
    "noto sans bengali ui",
    "bellota",
    "ume p gothic c4",
    "corbel",
    "noto sans tamil ui",
    "ar pl uming tw",
    "fkp",
    "pcfont z",
    "adwaita mono",
    "gfs g\303\266schen",
    "kacstoffice",
    "harmattan",
    "gfs decker",
    "gohu nerd font",
    "khmer ui",
    "noto nastaliq urdu",
    "urw bookman",
    "vemana2000",
    "b davat",
    "news cycle",
    "urdu nastaliq unicode",
    "avdira",
    "ar pl zenkai uni",
    "gfs ambrosia",
    "hasklig",
    "rachana",
    "lklug",
    "ume gothic o5",
    "paktype naqsh",
    "ubuntumono nerd font",
    "bitstream vera sans",
    "nuosu sil",
    "source han serif tw",
    "3270 nerd font mono",
    "carlito",
    "monoid nerd font",
    "vazirmatn",
    "pt sans",
    "envycoder nerd font",
    "digna's handwriting",
    "sharetechmono nerd font",
    "noto sans telugu ui",
    "open sans",
    "adwaita sans",
    "segoe ui historic",
    "simsun",
    "darkgarden",
    "profont nerd font",
    "spacemono nerd font",
    "yu gothic ui",
    "droid sans ethiopic",
    "ume gothic s5",
    "khmer os muol",
    "gfs bodoni classic",
    "ume gothic s4",
    "ibm 3270 nerd font",
    "cumberland amt",
    "cumberland",
    "khmer os",
    "cousine",
    "gfs ignacio",
    "ubuntu condensed",
    "lao ui",
    "source han serif jp",
    "pt serif caption",
    "overpass mono",
    "gfs neohellenic",
    "bitstream vera sans mono",
    "awami nastaliq",
    "new athena unicode",
    "zapfino",
    "gfs orpheus sans",
    "overpass",
    "gentium plus",
    "amiri quran colored",
    "keyfont v2",
    "courier",
    "ipaexgothic",
    "cascadia mono",
    "xits math",
    "gillius adf",
    "silkscreen expanded",
    "unshinmun",
    "khmer mondulkiri",
    "proggyclean nerd font",
    "agave",
    "shofar",
    "sampige",
    "tex gyre bonum",
    "hiragino sans cns",
    "vazirmatn ui",
    "hermit",
    "khmer os system",
    "stix two math",
    "vazirmatn nl",
    "bitstream vera serif",
    "stevehand",
    "arshia",
    "campania",
    "drugulinclm",
    "source han mono hc",
    "beteckna small caps",
    "raghindi",
    "vazirmatn ui nl",
    "ms mincho",
    "fontawesome 7 free",
    "heuristica",
    "public sans",
    "noto sans sinhala ui",
    "sparks bar medium",
    "gfs baskerville",
    "goudy bookletter 1911",
    "ipaexmincho",
    "umeplus gothic",
    "vazirmatn rd ui",
    "ume gothic",
    "copperplate gothic std",
    "stam sefarad clm",
    "vazirmatn rd nl",
    "spark dot-line extrathick",
    "ar pl ukai tw",
    "ume gothic c5",
    "ar pl mingti2l big5",
    "gentium basic",
    "palatino linotype",
    "vazirmatn rd",
    "ume gothic c4",
    "tex gyre heros cn",
    "courier std",
    "kinnari",
    "urw gothic",
    "kacstbook",
    "hasklug nerd font",
    "android emoji",
    "droid sans devanagari",
    "antykwatorunska",
    "terminess nerd font",
    "noto sans thai ui",
    "khmer os muol light",
    "pothana2000",
    "kamran",
    "proggysquarettsz",
    "merriweather",
    "victormono",
    "victor mono",
    "droid sans japanese",
    "saab",
    "microsoft jhenghei",
    "umeplus p gothic",
    "microsoft jhenghei ui",
    "ibmplexmono nerd font",
    "khmer os content",
    "vazirmatn rd ui nl",
    "noto sans khmer ui",
    "ume ui gothic o5",
    "agave nerd font",
    "apple color emoji",
    "gfs orpheus classic",
    "source han sans kr",
    "asana math",
    "b612 mono",
    "kacstnaskh",
    "hurmit nerd font",
    "overpass nerd font",
    "patrick hand",
    "mukti narrow",
    "prociono",
    "baekmuk gulim",
    "baekmuk dotum",
    "itc zapf chancery std",
    "cousine nerd font",
    "beteckna lower case",
    "cascadia code",
    "merriweather sans",
    "league gothic",
    "cascadia mono nf",
    "antykwatorunska medium",
    "bigblueterminal",
    "ume ui gothic",
    "andika new basic",
    "sparks bar wide",
    "ipamincho",
    "courier new",
    "hiragino mincho pron",
    "gb_ss_gb18030",
    "agave nerd font mono",
    "dancing script",
    "charis sil",
    "noto sans myanmar ui",
    "sparks bar extrawide",
    "vdrsymbols sans",
    "lohit punjabi",
    "pingfang hk",
    "khmer os muol pali",
    "noto sans math",
    "stam ashkenaz clm",
    "gfs jackson",
    "sparks bar extranarrow",
    "m yuppy gb",
    "gubbi",
    "noto sans malayalam ui",
    "hack",
    "baekmuk batang",
    "symbola",
    "droid sans fallback",
    "finale broadway text",
    "heavydata",
    "cascadia mono pl",
    "noto sans oriya ui",
    "antykwatorunskacond",
    "vl gothic",
    "britannic",
    "hapax berb\303\250re",
    "jg laotimes",
    "khmer os freehand",
    "bigblueterminal nerd font",
    "tex gyre adventor",
    "work sans",
    "artsounk",
    "noto fangsong kss vertical",
    "noto color emoji",
    "twentieth century",
    "helvetica",
    "khmer os fasthand",
    "jetbrains mono",
    "emoji one",
    "antykwatorunskacond medium",
    "nimbus mono",
    "jg lao old arial",
    "ms ui gothic",
    "antykwatorunskacond light",
    "noto fangsong kss rotated",
    "gb_ss_gb18030_extended",
    "victormono nerd font",
    "gentium plus compact",
    "verdana",
    "cascadia code nf",
    "cambria math",
    "nimbus roman",
    "ar pl uming hk",
    "red hat display",
    "droid sans hebrew",
    "ipapgothic",
    "nimbus sans",
    "sparks bar narrow",
    "antykwatorunska light",
    "nimbus mono l",
    "hanyisong",
    "droid arabic kufi",
    "khmer os siemreap",
    "nimbus sans l",
    "source han sans jp",
    "proggytinyttsz",
    "bauhaus std",
    "noto sans gurmukhi ui",
    "yanone kaffeesatz",
    "jetbrainsmono nerd font",
    "fontawesome 7 brands",
    "cascadia code pl",
    "nimbus mono ps",
    "nimbus roman no9 l",
    "sophia nubian",
    "source han mono k",
    "khmer os metal chrieng",
    "emoji two",
    "ektype baloo 2",
    "bitstreamverasansmono nerd font",
    "noto sans arabic ui",
    "padauk",
    "trebuchet ms",
    "emojione mozilla",
    "koodak",
    "ar pl ukai hk",
    "ektype baloo tamma 2",
    "ektype baloo tammudu 2",
    "bpg glaho international",
    "helvetica lt std",
    "tribun adf std",
    "finale broadway",
    "ar pl sungtil gb",
    "joypixels",
    "heavydata nerd font",
    "kacstdecorative",
    "biz udmincho",
    "kochi mincho",
    "source han code jp",
    "noto naskh arabic ui",
    "bonvenocf",
    "ektype baloo da 2",
    "bpg utf8 m",
    "droid arabic naskh",
    "biz udpgothic",
    "kerkis",
    "noto kufi arabic",
    "biz udpmincho",
    "rit tn joy",
    "tagmukay",
    "century gothic",
    "spark dot-line thick",
    "zilla slab",
    "openmoji black",
    "paktype naskh basic wide",
    "paktype naskh basic semi wide",
    "hiragino sans gb",
    "gb_ss_gb18030_extendedk",
    "noto sans cjk tc",
    "noto naskh arabic",
    "noto sans mono cjk tc",
    "noto sans cjk sc",
    "noto sans mono cjk sc",
    "charis sil compact",
    "noto serif cjk tc",
    "paktype naskh basic",
    "noto serif cjk sc",
    "ektype baloo thambi 2",
    "unikurd web",
    "zilla slab highlight",
    "wenquanyi zen hei",
    "vl pgothic",
    "khmer os bokor",
    "wenquanyi micro hei",
    "wenquanyi bitmap song",
    "noto sans cjk kr",
    "khmer os battambang",
    "ektype baloo chettan 2",
    "biz udgothic",
    "wenquanyi micro hei mono",
    "noto serif cjk kr",
    "kochi gothic",
    "padauk book",
    "ukij tuz",
    "gentium book basic",
    "bravura",
    "noto sans mono cjk kr",
    "bravuratext",
    "ektype baloo bhai 2",
    "noto sans mono cjk hk",
    "ektype baloo bhaina 2",
    "caskaydiacove nerd font mono",
    "ektype baloo paaji 2",
    "caskaydiacove nerd font",
    "wenquanyi zen hei sharp",
    "noto sans mono cjk jp",
    "noto sans cjk jp",
    "noto serif cjk jp",
    "noto sans cjk hk",
    "noto serif cjk hk"
  };
#define FcConstFamilyNamePool ((const char *) &FcConstFamilyNamePool_contents)
const struct FcGenericFamilyEntry *
fc_generic_family_lookup (register const char *str, register size_t len)
{
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 6) > 4) || (defined __clang__ && __clang_major__ >= 3)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
  static const struct FcGenericFamilyEntry wordlist[] =
    {
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 628 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str40, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 77 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str53, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 558 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str60, 0x00000002},
      {-1}, {-1}, {-1},
#line 856 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str64, 0x00000004},
#line 491 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str65, 0x00000002},
      {-1}, {-1},
#line 451 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str68, 0x00000002},
      {-1},
#line 629 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str70, 0x00000002},
#line 568 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str71, 0x00000002},
#line 504 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str72, 0x00000002},
#line 569 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str73, 0x00000002},
#line 505 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str74, 0x00000002},
#line 840 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str75, 0x00000010},
      {-1},
#line 55 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str77, 0x00000002},
#line 842 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str78, 0x00000002},
#line 799 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str79, 0x00000010},
      {-1},
#line 857 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str81, 0x00000010},
      {-1}, {-1},
#line 852 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str84, 0x00000008},
#line 841 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str85, 0x00000010},
#line 854 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str86, 0x00000008},
      {-1},
#line 847 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str88, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 538 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str93, 0x00000004},
#line 851 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str94, 0x00000010},
#line 775 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str95, 0x00000002},
#line 581 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str96, 0x00000001},
#line 467 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str97, 0x00000002},
#line 48 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str98, 0x00000001},
      {-1},
#line 236 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str100, 0x00000002},
#line 900 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str101, 0x00000001},
#line 502 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str102, 0x00000004},
#line 844 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str103, 0x00000008},
      {-1},
#line 849 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str105, 0x00000010},
#line 533 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str106, 0x00000002},
#line 503 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str107, 0x00000004},
#line 501 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str108, 0x00000004},
#line 54 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str109, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 101 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str117, 0x00000001},
#line 720 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str118, 0x00000002},
      {-1}, {-1}, {-1},
#line 853 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str122, 0x00000008},
#line 489 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str123, 0x00000007},
#line 858 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str124, 0x00000010},
#line 797 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str125, 0x00000002},
#line 850 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str126, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 696 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str134, 0x00000002},
      {-1}, {-1},
#line 476 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str137, 0x00000007},
      {-1},
#line 485 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str139, 0x00000007},
#line 359 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str140, 0x00000004},
      {-1}, {-1},
#line 490 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str143, 0x00000007},
      {-1}, {-1}, {-1}, {-1},
#line 94 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str148, 0x00000002},
      {-1},
#line 773 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str150, 0x00000010},
      {-1}, {-1},
#line 899 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str153, 0x00000002},
      {-1},
#line 691 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str155, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 559 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str160, 0x00000001},
#line 483 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str161, 0x00000007},
      {-1}, {-1},
#line 43 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str164, 0x00000002},
#line 88 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str165, 0x00000002},
#line 482 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str166, 0x00000007},
      {-1}, {-1}, {-1},
#line 225 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str170, 0x00000003},
#line 627 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str171, 0x00000004},
#line 722 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str172, 0x00000002},
      {-1}, {-1},
#line 471 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str175, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 187 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str180, 0x00000002},
      {-1}, {-1},
#line 456 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str183, 0x00000010},
      {-1}, {-1}, {-1}, {-1},
#line 358 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str188, 0x00000010},
      {-1},
#line 320 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str190, 0x00000004},
#line 693 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str191, 0x00000004},
      {-1},
#line 795 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str193, 0x00000001},
      {-1},
#line 486 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str195, 0x00000007},
      {-1}, {-1},
#line 478 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str198, 0x00000007},
      {-1}, {-1}, {-1}, {-1},
#line 195 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str203, 0x00000001},
      {-1}, {-1}, {-1},
#line 793 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str207, 0x00000001},
      {-1}, {-1},
#line 479 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str210, 0x00000007},
#line 488 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str211, 0x00000007},
      {-1}, {-1}, {-1},
#line 274 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str215, 0x00000002},
#line 521 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str216, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 39 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str223, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 639 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str229, 0x00000002},
      {-1},
#line 371 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str231, 0x00000002},
      {-1}, {-1},
#line 53 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str234, 0x00000010},
      {-1},
#line 522 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str236, 0x00000002},
      {-1},
#line 390 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str238, 0x00000008},
      {-1}, {-1}, {-1},
#line 457 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str242, 0x00000002},
#line 459 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str243, 0x00000004},
      {-1},
#line 798 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str245, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 473 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str253, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 102 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str258, 0x00000004},
#line 475 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str259, 0x00000006},
#line 800 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str260, 0x00000004},
#line 803 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str261, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 769 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str267, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 368 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str274, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 224 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str281, 0x00000002},
      {-1}, {-1},
#line 445 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str284, 0x00000001},
      {-1}, {-1}, {-1},
#line 474 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str288, 0x00000007},
      {-1}, {-1},
#line 364 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str291, 0x00000004},
#line 45 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str292, 0x00000001},
      {-1},
#line 534 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str294, 0x00000004},
#line 62 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str295, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 540 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str301, 0x00000002},
      {-1},
#line 66 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str303, 0x00000002},
      {-1},
#line 724 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str305, 0x00000010},
      {-1}, {-1}, {-1}, {-1},
#line 704 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str310, 0x00000020},
      {-1}, {-1},
#line 278 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str313, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 279 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str319, 0x00000004},
#line 245 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str320, 0x00000002},
      {-1}, {-1}, {-1},
#line 370 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str324, 0x00000004},
      {-1},
#line 269 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str326, 0x00000004},
#line 464 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str327, 0x00000001},
#line 260 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str328, 0x00000004},
#line 802 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str329, 0x00000004},
      {-1}, {-1}, {-1},
#line 526 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str333, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 713 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str340, 0x00000002},
#line 97 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str341, 0x00000002},
#line 59 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str342, 0x00000004},
      {-1}, {-1},
#line 361 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str345, 0x00000004},
      {-1}, {-1}, {-1},
#line 839 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str349, 0x00000001},
      {-1}, {-1},
#line 44 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str352, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 324 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str358, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 508 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str363, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 705 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str370, 0x00000400},
      {-1}, {-1}, {-1},
#line 848 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str374, 0x00000010},
#line 804 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str375, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 443 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str382, 0x00000001},
#line 250 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str383, 0x00000010},
      {-1},
#line 221 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str385, 0x00000001},
#line 272 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str386, 0x00000001},
      {-1},
#line 447 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str388, 0x00000001},
      {-1}, {-1}, {-1},
#line 261 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str392, 0x00000004},
      {-1},
#line 450 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str394, 0x00000001},
#line 325 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str395, 0x00000008},
      {-1}, {-1},
#line 215 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str398, 0x00000004},
#line 794 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str399, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 448 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str404, 0x00000001},
      {-1}, {-1}, {-1},
#line 204 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str408, 0x00000002},
      {-1},
#line 562 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str410, 0x00000008},
      {-1}, {-1},
#line 565 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str413, 0x00000004},
#line 207 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str414, 0x00000002},
#line 217 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str415, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 460 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str436, 0x00000002},
#line 208 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str437, 0x00000002},
#line 563 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str438, 0x00000001},
      {-1},
#line 218 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str440, 0x00000001},
      {-1},
#line 136 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str442, 0x00000001},
#line 643 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str443, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 266 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str448, 0x00000010},
#line 892 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str449, 0x00000001},
      {-1},
#line 807 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str451, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 46 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str458, 0x00000002},
#line 449 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str459, 0x00000001},
      {-1}, {-1},
#line 216 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str462, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 707 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str468, 0x00000020},
      {-1},
#line 157 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str470, 0x00000004},
#line 137 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str471, 0x00000001},
#line 527 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str472, 0x00000002},
      {-1},
#line 440 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str474, 0x00000002},
      {-1},
#line 186 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str476, 0x00000002},
      {-1},
#line 506 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str478, 0x00000003},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 270 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str487, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 159 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str493, 0x00000001},
      {-1}, {-1},
#line 271 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str496, 0x00000001},
      {-1}, {-1},
#line 160 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str499, 0x00000001},
#line 796 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str500, 0x00000005},
      {-1}, {-1}, {-1}, {-1},
#line 374 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str505, 0x00000004},
#line 894 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str506, 0x00000001},
      {-1}, {-1}, {-1},
#line 98 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str510, 0x00000004},
      {-1}, {-1}, {-1},
#line 845 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str514, 0x00000010},
#line 564 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str515, 0x00000006},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 188 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str529, 0x00000002},
#line 237 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str530, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 507 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str536, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 360 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str556, 0x00000004},
      {-1},
#line 685 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str558, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 247 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str564, 0x00000004},
#line 541 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str565, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 644 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str570, 0x00000002},
#line 645 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str571, 0x00000001},
      {-1},
#line 510 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str573, 0x00000020},
#line 103 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str574, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 295 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str579, 0x00000001},
      {-1}, {-1},
#line 544 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str582, 0x00000004},
#line 173 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str583, 0x00000001},
      {-1},
#line 542 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str585, 0x00000002},
      {-1}, {-1}, {-1},
#line 56 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str589, 0x00000002},
#line 801 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str590, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 744 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str595, 0x00000002},
      {-1}, {-1},
#line 626 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str598, 0x00000004},
#line 658 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str599, 0x00000010},
      {-1}, {-1}, {-1},
#line 322 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str603, 0x00000004},
      {-1},
#line 539 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str605, 0x00000004},
      {-1}, {-1},
#line 161 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str608, 0x00000004},
      {-1}, {-1},
#line 306 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str611, 0x00000010},
      {-1}, {-1}, {-1},
#line 312 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str615, 0x00000010},
      {-1}, {-1},
#line 659 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str618, 0x00000010},
      {-1}, {-1},
#line 513 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str621, 0x00000004},
#line 165 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str622, 0x00000002},
#line 347 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str623, 0x00000010},
      {-1},
#line 168 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str625, 0x00000004},
#line 686 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str626, 0x00000002},
      {-1},
#line 363 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str628, 0x00000004},
      {-1},
#line 493 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str630, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 330 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str636, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 454 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str642, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 314 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str647, 0x00000010},
      {-1}, {-1}, {-1},
#line 334 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str651, 0x00000004},
#line 378 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str652, 0x00000004},
      {-1}, {-1},
#line 550 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str655, 0x00000001},
#line 99 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str656, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 878 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str661, 0x00000002},
#line 898 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str662, 0x00000008},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 492 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str669, 0x00000001},
#line 824 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str670, 0x00000001},
#line 560 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str671, 0x00000002},
#line 463 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str672, 0x00000004},
#line 91 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str673, 0x00000002},
      {-1},
#line 838 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str675, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 884 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str680, 0x00000010},
      {-1}, {-1},
#line 462 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str683, 0x00000001},
#line 190 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str684, 0x00000004},
#line 235 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str685, 0x00000001},
#line 832 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str686, 0x00000001},
      {-1},
#line 545 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str688, 0x00000004},
      {-1},
#line 89 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str690, 0x00000002},
      {-1}, {-1}, {-1},
#line 484 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str694, 0x00000007},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 158 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str700, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 726 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str706, 0x00000001},
      {-1},
#line 480 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str708, 0x00000007},
      {-1}, {-1}, {-1}, {-1},
#line 823 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str713, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 662 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str721, 0x00000002},
      {-1}, {-1}, {-1},
#line 745 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str725, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 318 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str730, 0x00000004},
#line 167 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str731, 0x00000008},
#line 494 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str732, 0x00000800},
#line 201 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str733, 0x00000010},
      {-1}, {-1}, {-1},
#line 259 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str737, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 725 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str745, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 377 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str750, 0x00000004},
#line 313 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str751, 0x00000010},
#line 65 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str752, 0x00000004},
      {-1},
#line 196 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str754, 0x00000002},
      {-1}, {-1},
#line 825 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str757, 0x00000001},
      {-1}, {-1},
#line 446 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str760, 0x00000001},
#line 721 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str761, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 212 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str767, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 189 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str773, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 362 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str783, 0x00000004},
      {-1},
#line 702 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str785, 0x00000001},
#line 535 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str786, 0x00000004},
      {-1}, {-1},
#line 349 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str789, 0x00000004},
      {-1},
#line 833 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str791, 0x00000001},
      {-1}, {-1},
#line 791 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str794, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 317 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str800, 0x00000002},
      {-1}, {-1},
#line 365 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str803, 0x00000004},
      {-1},
#line 350 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str805, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 695 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str811, 0x00000004},
      {-1}, {-1}, {-1},
#line 220 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str815, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 315 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str823, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 183 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str830, 0x00000001},
      {-1},
#line 252 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str832, 0x00000001},
      {-1}, {-1},
#line 258 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str835, 0x00000004},
      {-1},
#line 631 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str837, 0x00000001},
#line 257 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str838, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 495 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str843, 0x00000002},
#line 822 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str844, 0x00000002},
      {-1}, {-1}, {-1},
#line 139 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str848, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 718 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str855, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 286 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str869, 0x00000001},
#line 711 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str870, 0x00000002},
      {-1},
#line 163 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str872, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 73 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str880, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 328 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str886, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 561 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str892, 0x00000002},
      {-1},
#line 162 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str894, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 692 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str900, 0x00000002},
      {-1},
#line 166 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str902, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 219 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str912, 0x00000004},
      {-1},
#line 203 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str914, 0x00000010},
      {-1}, {-1}, {-1},
#line 197 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str918, 0x00000004},
#line 267 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str919, 0x00000001},
      {-1},
#line 76 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str921, 0x00000001},
      {-1}, {-1},
#line 458 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str924, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 311 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str931, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 566 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str936, 0x00000004},
      {-1},
#line 291 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str938, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 192 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str945, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 731 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str956, 0x00000004},
      {-1},
#line 169 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str958, 0x00000004},
      {-1}, {-1}, {-1},
#line 746 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str962, 0x00000004},
#line 366 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str963, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 776 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str968, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 351 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str983, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 689 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str988, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 514 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str994, 0x00000004},
#line 546 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str995, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 280 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1006, 0x00000004},
      {-1},
#line 656 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1008, 0x00000002},
      {-1},
#line 60 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1010, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 792 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1015, 0x00000001},
      {-1}, {-1},
#line 810 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1018, 0x00000400},
      {-1}, {-1},
#line 367 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1021, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 177 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1028, 0x00000001},
#line 90 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1029, 0x00000002},
      {-1},
#line 369 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1031, 0x00000002},
      {-1}, {-1},
#line 516 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1034, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 455 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1039, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 92 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1046, 0x00000004},
#line 307 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1047, 0x00000010},
      {-1}, {-1}, {-1},
#line 764 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1051, 0x00000002},
#line 138 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1052, 0x00000010},
      {-1},
#line 42 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1054, 0x00000002},
      {-1},
#line 688 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1056, 0x00000006},
      {-1}, {-1}, {-1}, {-1},
#line 241 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1061, 0x00000010},
#line 172 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1062, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 763 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1067, 0x00000002},
      {-1},
#line 198 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1069, 0x00000001},
      {-1},
#line 890 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1071, 0x00000002},
#line 376 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1072, 0x00000004},
      {-1},
#line 519 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1074, 0x00000002},
      {-1}, {-1}, {-1},
#line 287 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1078, 0x00000006},
      {-1},
#line 375 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1080, 0x00000004},
      {-1}, {-1}, {-1},
#line 765 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1084, 0x00000001},
      {-1}, {-1},
#line 461 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1087, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 717 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1092, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 714 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1097, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 298 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1105, 0x00000010},
#line 752 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1106, 0x00000002},
      {-1}, {-1},
#line 888 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1109, 0x00000002},
#line 661 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1110, 0x00000010},
#line 728 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1111, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 297 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1126, 0x00000010},
#line 678 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1127, 0x00000001},
#line 465 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1128, 0x00000004},
      {-1}, {-1},
#line 299 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1131, 0x00000001},
      {-1}, {-1},
#line 164 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1134, 0x00000002},
      {-1}, {-1}, {-1},
#line 468 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1138, 0x00000002},
      {-1}, {-1},
#line 676 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1141, 0x00000004},
      {-1},
#line 52 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1143, 0x00000001},
      {-1}, {-1},
#line 268 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1146, 0x00000001},
#line 543 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1147, 0x00000004},
#line 701 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1148, 0x00000002},
      {-1}, {-1}, {-1},
#line 751 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1152, 0x00000002},
#line 635 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1153, 0x00000004},
      {-1}, {-1},
#line 723 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1156, 0x00000002},
#line 41 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1157, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 200 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1165, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 301 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1171, 0x00000010},
      {-1}, {-1},
#line 293 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1174, 0x00000010},
      {-1}, {-1}, {-1},
#line 667 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1178, 0x00000001},
      {-1},
#line 129 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1180, 0x00000010},
#line 108 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1181, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 79 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1186, 0x00000001},
      {-1}, {-1}, {-1},
#line 481 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1190, 0x00000007},
      {-1}, {-1}, {-1},
#line 761 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1194, 0x00000002},
#line 171 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1195, 0x00000004},
#line 191 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1196, 0x00000004},
#line 811 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1197, 0x00000002},
#line 683 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1198, 0x00000002},
#line 547 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1199, 0x00000006},
      {-1},
#line 469 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1201, 0x00000001},
      {-1},
#line 790 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1203, 0x00000001},
#line 748 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1204, 0x00000004},
      {-1},
#line 762 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1206, 0x00000002},
      {-1},
#line 35 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1208, 0x00000010},
#line 477 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1209, 0x00000002},
#line 638 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1210, 0x00000400},
#line 410 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1211, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 747 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1221, 0x00000004},
      {-1}, {-1},
#line 697 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1224, 0x00000002},
#line 34 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1225, 0x00000010},
      {-1},
#line 523 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1227, 0x00000800},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 529 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1239, 0x00000004},
#line 418 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1240, 0x00000002},
      {-1}, {-1},
#line 409 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1243, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 582 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1250, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 31 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1256, 0x00000001},
#line 242 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1257, 0x00000004},
      {-1},
#line 555 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1259, 0x00000001},
      {-1}, {-1}, {-1},
#line 760 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1263, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 401 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1269, 0x00000001},
#line 715 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1270, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 520 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1275, 0x00000020},
      {-1},
#line 80 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1277, 0x00000004},
      {-1}, {-1}, {-1},
#line 531 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1281, 0x00000001},
      {-1}, {-1},
#line 416 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1284, 0x00000002},
      {-1},
#line 415 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1286, 0x00000002},
#line 379 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1287, 0x00000004},
      {-1}, {-1}, {-1},
#line 553 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1291, 0x00000001},
      {-1},
#line 412 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1293, 0x00000003},
#line 413 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1294, 0x00000002},
      {-1}, {-1},
#line 509 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1297, 0x00000006},
      {-1}, {-1},
#line 729 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1300, 0x00000004},
      {-1},
#line 319 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1302, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 405 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1307, 0x00000002},
#line 777 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1308, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 497 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1320, 0x00000004},
      {-1}, {-1},
#line 584 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1323, 0x00000400},
#line 414 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1324, 0x00000002},
#line 116 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1325, 0x00000010},
      {-1},
#line 199 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1327, 0x00000004},
#line 357 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1328, 0x00000004},
      {-1},
#line 829 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1330, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 61 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1335, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 554 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1340, 0x00000001},
      {-1},
#line 524 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1342, 0x00000004},
#line 110 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1343, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 356 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1350, 0x00000004},
      {-1},
#line 300 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1352, 0x00000010},
#line 406 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1353, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 670 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1358, 0x00000004},
      {-1}, {-1}, {-1},
#line 93 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1362, 0x00000002},
      {-1}, {-1},
#line 373 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1365, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 444 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1370, 0x00000800},
      {-1}, {-1}, {-1},
#line 709 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1374, 0x00000004},
#line 400 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1375, 0x00000003},
#line 591 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1376, 0x00000002},
#line 694 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1377, 0x00000001},
#line 47 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1378, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 453 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1383, 0x00000020},
      {-1}, {-1},
#line 498 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1386, 0x00000002},
      {-1},
#line 344 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1388, 0x00000002},
      {-1}, {-1},
#line 174 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1391, 0x00000010},
#line 411 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1392, 0x00000002},
#line 634 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1393, 0x00000004},
#line 687 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1394, 0x00000002},
      {-1},
#line 831 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1396, 0x00000002},
      {-1}, {-1}, {-1},
#line 660 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1400, 0x00000008},
      {-1},
#line 580 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1402, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 552 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1408, 0x00000001},
      {-1},
#line 470 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1410, 0x00000004},
      {-1},
#line 672 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1412, 0x00000004},
      {-1},
#line 352 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1414, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 830 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1420, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 404 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1428, 0x00000002},
#line 106 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1429, 0x00000003},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 666 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1435, 0x00000002},
      {-1}, {-1}, {-1},
#line 30 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1439, 0x00000001},
#line 740 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1440, 0x00000001},
      {-1}, {-1}, {-1},
#line 466 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1444, 0x00000004},
#line 399 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1445, 0x00000002},
      {-1},
#line 170 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1447, 0x00000001},
      {-1},
#line 256 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1449, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 496 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1460, 0x00000004},
      {-1}, {-1}, {-1},
#line 897 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1464, 0x00000005},
      {-1}, {-1}, {-1}, {-1},
#line 636 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1469, 0x00000004},
#line 784 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1470, 0x00000008},
#line 570 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1471, 0x00000001},
      {-1}, {-1},
#line 665 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1474, 0x00000002},
      {-1}, {-1},
#line 530 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1477, 0x00000004},
      {-1}, {-1},
#line 354 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1480, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 263 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1489, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 703 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1495, 0x00000001},
#line 556 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1496, 0x00000002},
      {-1},
#line 633 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1498, 0x00000006},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 619 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1508, 0x00000020},
      {-1}, {-1},
#line 420 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1511, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 653 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1518, 0x00000002},
      {-1}, {-1},
#line 567 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1521, 0x00000001},
#line 742 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1522, 0x00000001},
      {-1}, {-1},
#line 389 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1525, 0x00000001},
      {-1}, {-1},
#line 607 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1528, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 248 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1543, 0x00000004},
      {-1}, {-1}, {-1},
#line 49 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1547, 0x00000001},
      {-1}, {-1},
#line 255 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1550, 0x00000004},
      {-1},
#line 778 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1552, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 536 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1558, 0x00000004},
      {-1}, {-1},
#line 392 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1561, 0x00000001},
      {-1},
#line 532 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1563, 0x00000010},
      {-1}, {-1}, {-1}, {-1},
#line 397 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1568, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 735 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1574, 0x00000004},
      {-1}, {-1},
#line 50 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1577, 0x00000001},
      {-1},
#line 785 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1579, 0x00000004},
#line 891 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1580, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 525 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1586, 0x00000004},
      {-1}, {-1},
#line 281 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1589, 0x00000002},
#line 789 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1590, 0x00000001},
      {-1}, {-1},
#line 813 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1593, 0x00000006},
      {-1},
#line 223 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1595, 0x00000003},
#line 329 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1596, 0x00000002},
      {-1},
#line 708 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1598, 0x00000004},
      {-1},
#line 808 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1600, 0x00000002},
#line 383 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1601, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 771 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1607, 0x00000001},
#line 249 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1608, 0x00000004},
      {-1}, {-1},
#line 81 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1611, 0x00000001},
      {-1},
#line 734 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1613, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 788 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1629, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 528 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1634, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 384 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1642, 0x00000006},
      {-1}, {-1}, {-1},
#line 826 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1646, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 140 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1652, 0x00000001},
#line 75 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1653, 0x00000002},
#line 144 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1654, 0x00000020},
      {-1}, {-1}, {-1},
#line 244 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1658, 0x00000001},
      {-1},
#line 602 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1660, 0x00000020},
#line 754 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1661, 0x00000002},
#line 780 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1662, 0x00000004},
#line 382 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1663, 0x00000002},
#line 441 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1664, 0x00000010},
#line 781 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1665, 0x00000004},
#line 28 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1666, 0x00000004},
      {-1},
#line 64 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1668, 0x00000004},
#line 40 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1669, 0x00000002},
      {-1},
#line 143 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1671, 0x00000002},
#line 736 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1672, 0x00000006},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 843 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1683, 0x00000002},
      {-1},
#line 372 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1685, 0x00000002},
      {-1}, {-1},
#line 600 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1688, 0x00000020},
      {-1},
#line 63 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1690, 0x00000004},
#line 620 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1691, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 700 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1696, 0x00000003},
      {-1}, {-1}, {-1}, {-1},
#line 499 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1701, 0x00000001},
      {-1},
#line 289 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1703, 0x00000001},
#line 308 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1704, 0x00000001},
      {-1},
#line 386 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1706, 0x00000006},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 549 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1713, 0x00000002},
      {-1}, {-1},
#line 786 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1716, 0x00000002},
#line 648 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1717, 0x00000003},
#line 515 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1718, 0x00000001},
#line 828 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1719, 0x00000002},
      {-1},
#line 604 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1721, 0x00000020},
      {-1},
#line 251 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1723, 0x00000002},
      {-1}, {-1},
#line 273 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1726, 0x00000002},
#line 599 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1727, 0x00000020},
      {-1},
#line 663 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1729, 0x00000003},
#line 246 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1730, 0x00000001},
#line 739 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1731, 0x00000006},
#line 387 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1732, 0x00000001},
      {-1},
#line 296 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1734, 0x00000001},
      {-1}, {-1}, {-1},
#line 593 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1738, 0x00000020},
      {-1}, {-1},
#line 115 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1741, 0x00000010},
      {-1},
#line 827 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1743, 0x00000002},
#line 176 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1744, 0x00000002},
      {-1}, {-1}, {-1},
#line 616 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1748, 0x00000020},
      {-1},
#line 86 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1750, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 262 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1764, 0x00000002},
#line 657 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1765, 0x00000002},
#line 32 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1766, 0x00000004},
      {-1}, {-1},
#line 302 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1769, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 408 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1777, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 333 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1785, 0x00000002},
      {-1}, {-1},
#line 294 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1788, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 321 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1797, 0x00000004},
#line 435 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1798, 0x00000020},
      {-1},
#line 590 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1800, 0x00000008},
      {-1}, {-1}, {-1},
#line 860 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1804, 0x00000001},
#line 871 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1805, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 107 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1817, 0x00000003},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 572 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1824, 0x00000002},
      {-1}, {-1}, {-1},
#line 859 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1828, 0x00000003},
#line 104 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1829, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 87 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1834, 0x00000001},
#line 288 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1835, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 335 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1841, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 681 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1846, 0x00000001},
#line 472 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1847, 0x00000007},
#line 819 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1848, 0x00000002},
      {-1}, {-1}, {-1},
#line 649 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1852, 0x00000002},
      {-1},
#line 814 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1854, 0x00000004},
#line 121 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1855, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 630 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1860, 0x00000002},
#line 743 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1861, 0x00000001},
#line 29 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1862, 0x00000004},
      {-1}, {-1}, {-1},
#line 145 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1866, 0x00000002},
      {-1},
#line 537 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1868, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 862 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1874, 0x00000006},
      {-1}, {-1}, {-1},
#line 677 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1878, 0x00000002},
      {-1}, {-1},
#line 243 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1881, 0x00000004},
#line 202 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1882, 0x00000010},
      {-1}, {-1}, {-1},
#line 710 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1886, 0x00000004},
      {-1},
#line 617 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1888, 0x00000020},
      {-1}, {-1}, {-1},
#line 632 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1892, 0x00000002},
      {-1}, {-1},
#line 33 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1895, 0x00000022},
#line 706 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1896, 0x00000020},
      {-1}, {-1},
#line 719 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1899, 0x00000001},
      {-1},
#line 194 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1901, 0x00000002},
      {-1},
#line 671 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1903, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 749 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1908, 0x00000004},
#line 889 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1909, 0x00000020},
      {-1}, {-1}, {-1},
#line 210 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1913, 0x00000002},
#line 821 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1914, 0x00000002},
      {-1}, {-1},
#line 430 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1917, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 292 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1923, 0x00000010},
      {-1}, {-1},
#line 820 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1926, 0x00000002},
#line 353 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1927, 0x00000004},
      {-1},
#line 185 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1929, 0x00000004},
      {-1},
#line 184 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1931, 0x00000004},
      {-1}, {-1},
#line 423 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1934, 0x00000003},
      {-1}, {-1}, {-1},
#line 181 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1938, 0x00000004},
      {-1}, {-1},
#line 303 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1941, 0x00000010},
      {-1},
#line 812 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1943, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 442 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1948, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 741 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1958, 0x00000001},
      {-1}, {-1}, {-1},
#line 679 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1962, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 641 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1969, 0x00000004},
#line 305 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1970, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 122 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1979, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 105 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1988, 0x00000001},
      {-1}, {-1},
#line 571 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1991, 0x00000001},
#line 893 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1992, 0x00000008},
      {-1}, {-1}, {-1}, {-1},
#line 310 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1997, 0x00000002},
      {-1},
#line 640 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str1999, 0x00000002},
      {-1}, {-1}, {-1},
#line 284 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2003, 0x00000001},
#line 51 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2004, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 421 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2010, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 178 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2018, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 380 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2024, 0x00000002},
      {-1},
#line 149 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2026, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 886 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2031, 0x00000800},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 316 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2040, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 716 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2046, 0x00000002},
      {-1},
#line 855 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2048, 0x00000004},
      {-1},
#line 422 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2050, 0x00000001},
      {-1}, {-1}, {-1},
#line 673 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2054, 0x00000004},
#line 36 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2055, 0x00000004},
      {-1}, {-1}, {-1},
#line 712 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2059, 0x00000001},
      {-1}, {-1},
#line 699 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2062, 0x00000003},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 783 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2069, 0x00000001},
      {-1}, {-1},
#line 345 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2072, 0x00000002},
      {-1},
#line 868 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2074, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 341 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2083, 0x00000004},
      {-1}, {-1}, {-1},
#line 434 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2087, 0x00000024},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 770 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2097, 0x00000800},
      {-1}, {-1},
#line 863 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2100, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 123 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2105, 0x00000001},
#line 768 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2106, 0x00000010},
      {-1}, {-1}, {-1}, {-1},
#line 95 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2111, 0x00000002},
      {-1}, {-1},
#line 142 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2114, 0x00000010},
#line 222 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2115, 0x00000001},
#line 732 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2116, 0x00000004},
      {-1}, {-1}, {-1},
#line 118 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2120, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 682 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2126, 0x00000003},
#line 869 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2127, 0x00000020},
      {-1}, {-1},
#line 548 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2130, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 265 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2144, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 342 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2155, 0x00000001},
      {-1},
#line 680 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2157, 0x00000002},
#line 615 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2158, 0x00000020},
      {-1}, {-1},
#line 757 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2161, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 290 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2167, 0x00000010},
      {-1}, {-1}, {-1},
#line 323 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2171, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 381 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2177, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 836 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2183, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 866 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2189, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 816 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2199, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 175 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2205, 0x00000010},
#line 767 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2206, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 865 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2214, 0x00000002},
      {-1},
#line 750 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2216, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 84 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2228, 0x00000005},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 818 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2237, 0x00000002},
      {-1}, {-1},
#line 78 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2240, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 282 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2245, 0x00000001},
      {-1},
#line 654 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2247, 0x00000001},
#line 864 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2248, 0x00000002},
#line 817 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2249, 0x00000002},
      {-1},
#line 787 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2251, 0x00000002},
      {-1},
#line 180 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2253, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 436 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2258, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 861 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2274, 0x00000002},
      {-1}, {-1},
#line 402 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2277, 0x00000002},
      {-1}, {-1}, {-1},
#line 336 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2281, 0x00000004},
#line 58 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2282, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 209 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2293, 0x00000002},
#line 67 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2294, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 779 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2301, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 618 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2306, 0x00000020},
#line 431 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2307, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 668 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2313, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 417 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2338, 0x00000010},
#line 674 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2339, 0x00000004},
      {-1},
#line 511 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2341, 0x00000001},
      {-1}, {-1},
#line 874 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2344, 0x00000004},
      {-1}, {-1}, {-1},
#line 873 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2348, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 214 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2358, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 698 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2375, 0x00000001},
      {-1}, {-1},
#line 517 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2378, 0x00000002},
      {-1}, {-1}, {-1},
#line 837 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2382, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 518 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2394, 0x00000020},
      {-1}, {-1},
#line 355 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2397, 0x00000004},
      {-1},
#line 426 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2399, 0x00000004},
      {-1}, {-1},
#line 867 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2402, 0x00000020},
      {-1}, {-1},
#line 603 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2405, 0x00000020},
      {-1}, {-1}, {-1}, {-1},
#line 835 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2410, 0x00000020},
      {-1}, {-1}, {-1},
#line 37 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2414, 0x00000004},
#line 74 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2415, 0x00000400},
#line 309 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2416, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 738 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2426, 0x00000002},
      {-1}, {-1}, {-1},
#line 100 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2430, 0x00000801},
      {-1}, {-1}, {-1},
#line 109 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2434, 0x00000004},
#line 407 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2435, 0x00000001},
#line 348 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2436, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 642 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2443, 0x00000004},
      {-1}, {-1}, {-1},
#line 655 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2447, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 557 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2462, 0x00000003},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 669 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2469, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 113 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2480, 0x00000004},
      {-1},
#line 112 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2482, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 391 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2492, 0x00000008},
      {-1}, {-1}, {-1},
#line 182 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2496, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 117 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2504, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 146 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2511, 0x00000004},
      {-1}, {-1}, {-1},
#line 512 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2515, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 452 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2523, 0x00000002},
#line 150 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2524, 0x00000004},
#line 69 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2525, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 119 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2534, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 834 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2541, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 57 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2559, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 759 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2566, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 385 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2573, 0x00000001},
#line 179 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2574, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 343 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2590, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 275 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2600, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 38 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2610, 0x00000004},
      {-1}, {-1}, {-1}, {-1},
#line 193 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2615, 0x00000008},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 155 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2626, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 613 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2639, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 756 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2646, 0x00000002},
#line 870 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2647, 0x00000002},
#line 487 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2648, 0x00000007},
#line 664 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2649, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 432 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2660, 0x00000004},
      {-1}, {-1},
#line 606 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2663, 0x00000800},
      {-1},
#line 766 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2665, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 304 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2674, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 755 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2698, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 500 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2706, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 326 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2722, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 605 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2736, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 327 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2748, 0x00000004},
      {-1}, {-1},
#line 111 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2751, 0x00000001},
      {-1}, {-1}, {-1},
#line 772 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2755, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 211 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2775, 0x00000002},
      {-1},
#line 254 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2777, 0x00000010},
      {-1},
#line 337 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2779, 0x00000004},
#line 151 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2780, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 614 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2792, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 70 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2803, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 876 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2834, 0x00000006},
      {-1}, {-1}, {-1}, {-1},
#line 135 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2839, 0x00000002},
      {-1},
#line 332 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2841, 0x00000007},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 396 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2848, 0x00000001},
      {-1}, {-1},
#line 428 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2851, 0x00000010},
      {-1}, {-1}, {-1}, {-1},
#line 120 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2856, 0x00000004},
#line 782 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2857, 0x00000002},
#line 885 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2858, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 96 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2873, 0x00000003},
      {-1}, {-1}, {-1}, {-1},
#line 586 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2878, 0x00001000},
      {-1}, {-1}, {-1},
#line 583 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2882, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 809 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2892, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 339 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2899, 0x00000002},
      {-1}, {-1}, {-1},
#line 427 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2903, 0x00000010},
      {-1}, {-1}, {-1},
#line 393 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2907, 0x00000004},
#line 238 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2908, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 72 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2915, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 573 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2929, 0x00000004},
#line 395 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2930, 0x00000002},
      {-1}, {-1},
#line 551 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2933, 0x00000020},
      {-1}, {-1},
#line 71 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2936, 0x00000001},
#line 585 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2937, 0x00001000},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 276 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2947, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 875 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2968, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 285 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2979, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 872 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2994, 0x00000002},
#line 147 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str2995, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 141 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3008, 0x00000800},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 576 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3022, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 85 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3034, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 684 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3046, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 213 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3052, 0x00000002},
      {-1},
#line 388 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3054, 0x00000002},
      {-1},
#line 578 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3056, 0x00000002},
      {-1}, {-1}, {-1},
#line 758 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3060, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 68 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3123, 0x00000001},
      {-1}, {-1}, {-1},
#line 574 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3127, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 331 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3154, 0x00000005},
#line 205 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3155, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 433 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3164, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 579 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3188, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 737 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3193, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 675 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3203, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 114 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3210, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 601 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3221, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 887 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3230, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 394 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3236, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 264 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3245, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 148 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3251, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 575 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3263, 0x00000004},
#line 577 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3264, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 727 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3271, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 733 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3279, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 429 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3298, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 239 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3315, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 226 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3323, 0x00000002},
      {-1}, {-1},
#line 124 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3326, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 592 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3337, 0x00000020},
      {-1}, {-1}, {-1}, {-1},
#line 646 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3342, 0x00000006},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 805 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3369, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 240 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3392, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 439 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3409, 0x00000002},
      {-1}, {-1},
#line 83 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3412, 0x00000005},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 232 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3419, 0x00000002},
      {-1},
#line 233 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3421, 0x00000002},
      {-1}, {-1}, {-1},
#line 131 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3425, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 340 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3440, 0x00000002},
      {-1}, {-1},
#line 806 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3443, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 253 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3451, 0x00000010},
#line 82 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3452, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 398 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3470, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 338 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3486, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 403 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3497, 0x00000001},
      {-1}, {-1}, {-1},
#line 126 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3501, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 438 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3533, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 730 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3551, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 589 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3559, 0x00000020},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 130 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3587, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 230 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3621, 0x00000002},
      {-1},
#line 132 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3623, 0x00000003},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 206 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3649, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 127 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3661, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 419 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3670, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 587 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3684, 0x00000008},
      {-1}, {-1},
#line 128 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3687, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 690 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3694, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 774 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3762, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 154 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3783, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 753 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3800, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 895 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3890, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 637 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3895, 0x00000400},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 652 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3904, 0x00000001},
      {-1}, {-1}, {-1}, {-1},
#line 651 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3909, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 346 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3922, 0x00000002},
      {-1},
#line 277 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3924, 0x00000001},
      {-1},
#line 598 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3926, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 588 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3935, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 612 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3959, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 597 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3965, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 611 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str3998, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 156 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4016, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 625 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4052, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 650 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4089, 0x00000001},
      {-1},
#line 624 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4091, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 234 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4156, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 846 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4173, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 896 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4191, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 882 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4221, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 877 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4246, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 425 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4263, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 880 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4275, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 879 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4292, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 596 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4342, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 424 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4373, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 229 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4385, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 125 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4461, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 881 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4467, 0x00000004},
#line 623 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4468, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 437 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4493, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 647 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4501, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 815 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4533, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 283 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4559, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 133 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4670, 0x00000010},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1},
#line 610 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4684, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 134 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str4816, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1},
#line 227 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5061, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 608 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5173, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1},
#line 228 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5248, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 153 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5270, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 231 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5287, 0x00000002},
      {-1}, {-1}, {-1}, {-1},
#line 152 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5292, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 883 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5328, 0x00000007},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 609 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5451, 0x00000004},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1},
#line 595 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5545, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 622 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5671, 0x00000001},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1},
#line 594 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5812, 0x00000002},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 621 "fc-genericfamily/fcgenericfamily.gperf"
      {(int)(size_t)&((struct FcConstFamilyNamePool_t *)0)->FcConstFamilyNamePool_str5938, 0x00000001}
    };
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 6) > 4) || (defined __clang__ && __clang_major__ >= 3)
#pragma GCC diagnostic pop
#endif

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = fc_generic_family_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register int o = wordlist[key].name;
          if (o >= 0)
            {
              register const char *s = o + FcConstFamilyNamePool;

              if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strcmp (str, s))
                return &wordlist[key];
            }
        }
    }
  return (struct FcGenericFamilyEntry *) 0;
}
#line 901 "fc-genericfamily/fcgenericfamily.gperf"

