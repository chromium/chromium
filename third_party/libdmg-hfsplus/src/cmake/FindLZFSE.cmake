include(FindPackageHandleStandardArgs)

find_library(LZFSE_LIBRARY NAMES lzfse)
find_path(LZFSE_INCLUDE_DIR NAMES lzfse.h)

find_package_handle_standard_args(LZFSE
  REQUIRED_VARS LZFSE_LIBRARY LZFSE_INCLUDE_DIR)
