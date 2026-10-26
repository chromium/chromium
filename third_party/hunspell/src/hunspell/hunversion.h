#ifndef HUNSPELL_VERSION_H_
#define HUNSPELL_VERSION_H_

#define HUNSPELL_VERSION_MAJOR 1
#define HUNSPELL_VERSION_MINOR 7
#define HUNSPELL_VERSION_PATCH 4
#define HUNSPELL_VERSION_STRING "1.7.4"

/* Major, minor and patch packed one byte each so versions compare in order:
 *   #if HUNSPELL_VERSION >= HUNSPELL_VERSION_CHECK(1, 4, 1) */
#define HUNSPELL_VERSION                                                  \
  ((HUNSPELL_VERSION_MAJOR << 16) | (HUNSPELL_VERSION_MINOR << 8) |       \
   HUNSPELL_VERSION_PATCH)
#define HUNSPELL_VERSION_CHECK(major, minor, patch)                       \
  (((major) << 16) | ((minor) << 8) | (patch))

#endif
