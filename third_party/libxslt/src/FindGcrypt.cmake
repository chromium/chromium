include(FindPackageHandleStandardArgs)
include(SelectLibraryConfigurations)

find_path(GCRYPT_INCLUDE_DIRS NAMES gcrypt.h)

mark_as_advanced(GCRYPT_INCLUDE_DIRS)

find_library(GCRYPT_LIBRARY_DEBUG NAMES gcryptd)
find_library(GCRYPT_LIBRARY_RELEASE NAMES gcrypt)

select_library_configurations(GCRYPT)

if(GCRYPT_INCLUDE_DIRS AND EXISTS "${GCRYPT_INCLUDE_DIRS}/gcrypt.h")
	file(STRINGS "${GCRYPT_INCLUDE_DIRS}/gcrypt.h" _GCRYPT_VERSION_DEFINE REGEX "#define[\t ]+GCRYPT_VERSION[\t ]+\"[^\"]*\".*")
	string(REGEX REPLACE "#define[\t ]+GCRYPT_VERSION[\t ]+\"([^\"]*)\".*" "\\1" GCRYPT_VERSION "${_GCRYPT_VERSION_DEFINE}")
	unset(_GCRYPT_VERSION_DEFINE)
endif()

find_package_handle_standard_args(
	Gcrypt
	FOUND_VAR GCRYPT_FOUND
	REQUIRED_VARS GCRYPT_INCLUDE_DIRS GCRYPT_LIBRARIES
	VERSION_VAR GCRYPT_VERSION
)

if(GCRYPT_FOUND AND NOT TARGET Gcrypt::Gcrypt)
	add_library(Gcrypt::Gcrypt UNKNOWN IMPORTED)
	if(GCRYPT_LIBRARY_RELEASE)
		set_property(TARGET Gcrypt::Gcrypt APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
		set_target_properties(Gcrypt::Gcrypt PROPERTIES IMPORTED_LOCATION_RELEASE "${GCRYPT_LIBRARY_RELEASE}")
	endif()
	if(GCRYPT_LIBRARY_DEBUG)
		set_property(TARGET Gcrypt::Gcrypt APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
		set_target_properties(Gcrypt::Gcrypt PROPERTIES IMPORTED_LOCATION_DEBUG "${GCRYPT_LIBRARY_DEBUG}")
	endif()
	set_target_properties(
		Gcrypt::Gcrypt PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${GCRYPT_INCLUDE_DIRS}"
	)
endif()
