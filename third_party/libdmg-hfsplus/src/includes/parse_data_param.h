#ifndef _LIBDMG_HFSPLUS_PARSE_DATA_PARAM_H
#define _LIBDMG_HFSPLUS_PARSE_DATA_PARAM_H

#include "sizedbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

// `DataParamParserPtr` points to some function for interpreting a C string
// (typically provided via `argv`) as some byte sequence. Unlike a raw C string,
// the decoded version may contain bytes with value 0.
//
// Ownership of the returned `SizedBuf` is transfered to the caller.
//
// A `DataParamParser` never returns `NULL`. If it receives an unparseable
// encoding or a `NULL` pointer, the program exits with a nonzero return value.
typedef SizedBuf* (*DataParamParserPtr)(const char*);

// `DataParamParseFormat` is a named algorithm for parsing C strings into byte
// sequences.
typedef struct {
  const char* name;
  const DataParamParserPtr parser;
} DataParamParseFormat;

// Scans kLibdmgHfsplusDataParseFormats for the parser matching the name
// provided. If it can't find one, the program exits with return value 2.
DataParamParserPtr dataParamParserForFormat(const char* format_flag);

// Writes a C string containing a comma-delimited printable list of known data
// format names into the provided buffer of the specified length. If not all
// names fit, it writes as many complete names as it can. If `out_len` is
// nonzero, `out_buf` will be the head of a valid C string when this function
// returns, regardless of whether all formats were named. (If no names at all
// fit, this string will be empty.)
//
// If `out_len` is 0, `*out_buf` is not evaluated. It is valid to provide a
// NULL pointer for `out_buf`, provided that `out_len` is 0, to ask how large
// a buffer would be required to capture all names.
size_t dataParamFormats(char* out_buf, size_t out_len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // _LIBDMG_HFSPLUS_PARSE_DATA_PARAM_H
