# create zip_err_str.c from zip.h and zipint.h
file(READ ${PROJECT_SOURCE_DIR}/lib/zip.h zip_h)
string(REGEX MATCHALL "#define ZIP_ER_([A-Z0-9_]+) ([0-9]+)[ \t]+/([-*0-9a-zA-Z, ']*)/" zip_h_err ${zip_h})
file(READ ${PROJECT_SOURCE_DIR}/lib/zipint.h zipint_h)
string(REGEX MATCHALL "#define ZIP_ER_DETAIL_([A-Z0-9_]+) ([0-9]+)[ \t]+/([-*0-9a-zA-Z, ']*)/" zipint_h_err ${zipint_h})
set(zip_err_str [=[
/*
  This file was generated automatically by CMake
  from zip.h and zipint.h\; make changes there.
*/

#include "zipint.h"

#define L ZIP_ET_LIBZIP
#define N ZIP_ET_NONE
#define S ZIP_ET_SYS
#define Z ZIP_ET_ZLIB

#define E ZIP_DETAIL_ET_ENTRY
#define G ZIP_DETAIL_ET_GLOBAL

const struct _zip_err_info _zip_err_str[] = {
]=])
set(zip_err_type)
foreach(errln ${zip_h_err})
  string(REGEX MATCH "#define ZIP_ER_([A-Z0-9_]+) ([0-9]+)[ \t]+/([-*0-9a-zA-Z, ']*)/" err_t_tt ${errln})
  string(REGEX MATCH "([L|N|S|Z]+) ([-0-9a-zA-Z,, ']*)" err_t_tt "${CMAKE_MATCH_3}")
  string(STRIP "${CMAKE_MATCH_2}" err_t_tt)
  string(APPEND zip_err_str "    { ${CMAKE_MATCH_1}, \"${err_t_tt}\" },\n")
endforeach()
string(APPEND zip_err_str [=[}\;

const int _zip_err_str_count = sizeof(_zip_err_str)/sizeof(_zip_err_str[0])\;

const struct _zip_err_info _zip_err_details[] = {
]=])
foreach(errln ${zipint_h_err})
  string(REGEX MATCH "#define ZIP_ER_DETAIL_([A-Z0-9_]+) ([0-9]+)[ \t]+/([-*0-9a-zA-Z, ']*)/" err_t_tt ${errln})
  string(REGEX MATCH "([E|G]+) ([-0-9a-zA-Z, ']*)" err_t_tt "${CMAKE_MATCH_3}")
  string(STRIP "${CMAKE_MATCH_2}" err_t_tt)
  string(APPEND zip_err_str "    { ${CMAKE_MATCH_1}, \"${err_t_tt}\" },\n")
endforeach()
string(APPEND zip_err_str [=[}\;

const int _zip_err_details_count = sizeof(_zip_err_details)/sizeof(_zip_err_details[0])\;
]=])
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/zip_err_str.c ${zip_err_str})
