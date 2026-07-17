# Copyright (C) 2020 Dieter Baron and Thomas Klausner
#
# The authors can be contacted at <info@libzip.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
#
# 3. The names of the authors may not be used to endorse or promote
#   products derived from this software without specific prior
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
Findzstd
-------

Finds the Zstandard (zstd) library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``zstd::libzstd_shared``
  The shared Zstandard library
``zstd::libzstd_static``
  The shared Zstandard library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``zstd_FOUND``
  True if the system has the Zstandard library.
``zstd_VERSION``
  The version of the Zstandard library which was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``zstd_INCLUDE_DIR``
  The directory containing ``zstd.h``.
``zstd_STATIC_LIBRARY``
  The path to the Zstandard static library.
``zstd_SHARED_LIBRARY``
  The path to the Zstandard shared library.
``zstd_DLL``
  The path to the Zstandard DLL.

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_zstd QUIET libzstd)

find_path(zstd_INCLUDE_DIR
  NAMES zstd.h
  HINTS ${PC_zstd_INCLUDE_DIRS}
)

find_file(zstd_DLL
  NAMES libzstd.dll zstd.dll
  PATH_SUFFIXES bin
  HINTS ${PC_zstd_PREFIX}
)

# On Windows, we manually define the library names to avoid mistaking the
# implib for the static library
if(zstd_DLL)
  set(_zstd_win_static_name zstd-static)
  set(_zstd_win_shared_name zstd)
else()
  # vcpkg removes the -static suffix in static builds
  set(_zstd_win_static_name zstd zstd_static)
  set(_zstd_win_shared_name)
endif()

set(_previous_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".dylib" ".dll.a" ".lib")
find_library(zstd_SHARED_LIBRARY
  NAMES zstd ${_zstd_win_shared_name}
  HINTS ${PC_zstd_LIBDIR}
)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".lib")
find_library(zstd_STATIC_LIBRARY
  NAMES zstd ${_zstd_win_static_name}
  HINTS ${PC_zstd_LIBDIR}
)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_previous_suffixes})

# Set zstd_LIBRARY to the shared library or fall back to the static library
if(zstd_SHARED_LIBRARY)
  set(_zstd_LIBRARY ${zstd_SHARED_LIBRARY})
else()
  set(_zstd_LIBRARY ${zstd_STATIC_LIBRARY})
endif()

# Extract version information from the header file
if(zstd_INCLUDE_DIR)
  file(STRINGS ${zstd_INCLUDE_DIR}/zstd.h _ver_major_line
    REGEX "^#define ZSTD_VERSION_MAJOR  *[0-9]+"
    LIMIT_COUNT 1)
  string(REGEX MATCH "[0-9]+"
    zstd_MAJOR_VERSION "${_ver_major_line}")
  file(STRINGS ${zstd_INCLUDE_DIR}/zstd.h _ver_minor_line
    REGEX "^#define ZSTD_VERSION_MINOR  *[0-9]+"
    LIMIT_COUNT 1)
  string(REGEX MATCH "[0-9]+"
    zstd_MINOR_VERSION "${_ver_minor_line}")
  file(STRINGS ${zstd_INCLUDE_DIR}/zstd.h _ver_release_line
    REGEX "^#define ZSTD_VERSION_RELEASE  *[0-9]+"
    LIMIT_COUNT 1)
  string(REGEX MATCH "[0-9]+"
    zstd_RELEASE_VERSION "${_ver_release_line}")
  set(Zstd_VERSION "${zstd_MAJOR_VERSION}.${zstd_MINOR_VERSION}.${zstd_RELEASE_VERSION}")
  unset(_ver_major_line)
  unset(_ver_minor_line)
  unset(_ver_release_line)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(zstd
  FOUND_VAR zstd_FOUND
  REQUIRED_VARS
    _zstd_LIBRARY
    zstd_INCLUDE_DIR
  VERSION_VAR zstd_VERSION
)

if(zstd_FOUND AND zstd_SHARED_LIBRARY AND NOT TARGET zstd::libzstd_shared)
  add_library(zstd::libzstd_shared SHARED IMPORTED)
  if(WIN32 OR CYGWIN)
    set_target_properties(zstd::libzstd_shared PROPERTIES
      IMPORTED_LOCATION "${zstd_DLL}"
      IMPORTED_IMPLIB "${zstd_SHARED_LIBRARY}"
    )
  else()
    set_target_properties(zstd::libzstd_shared PROPERTIES
      IMPORTED_LOCATION "${zstd_SHARED_LIBRARY}"
    )
  endif()

  set_target_properties(zstd::libzstd_shared PROPERTIES
    INTERFACE_COMPILE_OPTIONS "${PC_zstd_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${zstd_INCLUDE_DIR}"
  )
endif()

if(zstd_FOUND AND zstd_STATIC_LIBRARY AND NOT TARGET zstd::libzstd_static)
  add_library(zstd::libzstd_static STATIC IMPORTED)
  set_target_properties(zstd::libzstd_static PROPERTIES
    IMPORTED_LOCATION "${zstd_STATIC_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_zstd_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${zstd_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  zstd_INCLUDE_DIR
  zstd_DLL
  zstd_SHARED_LIBRARY
  zstd_STATIC_LIBRARY
)
