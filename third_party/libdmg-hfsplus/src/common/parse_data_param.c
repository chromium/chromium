#include "parse_data_param.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "omaha_tag_format.h"
#include "sizedbuf.h"

const DataParamParseFormat kParseFormats[] = {
    {.name = "literal", .parser = AllocBufCopyString},
    {.name = "omaha-tag-zone", .parser = ParseOmahaTagZone}};

DataParamParserPtr dataParamParserForFormat(const char* format_flag) {
  size_t format_count =
      sizeof(kParseFormats) / sizeof(DataParamParseFormat);

  for (size_t i = 0; i < format_count; ++i) {
    if (strcmp(format_flag, kParseFormats[i].name) == 0) {
      return kParseFormats[i].parser;
    }
  }

  fprintf(stderr, "Unrecognized data format: %s\n", format_flag);
  exit(2);
}

size_t dataParamFormats(char* out_buf, size_t out_len) {
  size_t format_count =
      sizeof(kParseFormats) / sizeof(DataParamParseFormat);
  if (!out_buf) {
    ASSERT(out_len == 0, "dataParamFormats: NULL out_buf with nonzero out_len");
  }
  size_t total_len = 0;
  size_t written_len = 0;
  char* write_ptr = out_buf;

  // Character output limit, accounting for a null terminator and for counting
  // but not actually writing a ", " before the first item. (Pre-refunding
  // reduces conditional logic in the loop.)
  size_t remain = out_len + 2 - 1;

  for (size_t i = 0; i < format_count; ++i) {
    size_t next_len = strlen(kParseFormats[i].name) + 2;
    total_len += next_len;
    if (remain >= next_len) {
      if (write_ptr != out_buf) {
        write_ptr = stpcpy(write_ptr, ", ");
      }
      write_ptr = stpcpy(write_ptr, kParseFormats[i].name);
      remain -= next_len;
    }
  }

  // If we never wrote any output, make out_buf an empty string if possible.
  if (write_ptr == out_buf && out_len > 0) {
    *out_buf = '\0';
  }

  // Adjust total_len for the null terminator and the extra ", " for the same
  // reason as `remain` (but in the opposite direction). The overcount for ", "
  // only applies if at least one format existed, but the format list is a
  // constant, so it would be very strange for it to be empty.
  ASSERT(total_len > 1, "dataParamFormats knew no formats. Seems like a bug.");
  return total_len - 2 + 1;
}
