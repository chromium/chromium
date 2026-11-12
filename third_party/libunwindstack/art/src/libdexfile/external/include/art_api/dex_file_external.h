/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_EXTERNAL_H_
#define ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_EXTERNAL_H_

// Dex file external API
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

// This is the stable C ABI that backs art_api::dex below. Structs and functions
// may only be added here. C++ users should use dex_file_support.h instead.

struct ADexFile;
typedef struct ADexFile ADexFile; // NOLINT

struct ADexFile_Method;
typedef struct ADexFile_Method ADexFile_Method; // NOLINT

enum ADexFile_Error : uint32_t {
  ADEXFILE_ERROR_OK = 0,
  ADEXFILE_ERROR_INVALID_DEX = 1,
  ADEXFILE_ERROR_INVALID_HEADER = 2,
  ADEXFILE_ERROR_NOT_ENOUGH_DATA = 3,
};
typedef enum ADexFile_Error ADexFile_Error; // NOLINT

// Callback used to return information about a dex method.
// The method information is valid only during the callback.
// NOLINTNEXTLINE
typedef void ADexFile_MethodCallback(void* _Nullable callback_data,
                                     const ADexFile_Method* _Nonnull method);

// Interprets a chunk of memory as a dex file.
//
// @param address Pointer to the start of dex file data.
//                The caller must retain the memory until the object is destroyed.
// @param size Size of the memory range. If the size is too small, the method returns
//             ADEXFILE_ERROR_NOT_ENOUGH_DATA and sets new_size to some larger size
//             (which still might large enough, so several retries might be needed).
// @param new_size On successful load, this contains exact dex file size from header.
// @param location A string that describes the dex file. Preferably its path.
//                 It is mostly used just for log messages and may be "".
// @param dex_file The created dex file object, or nullptr on error.
//                 It must be later freed with ADexFile_Destroy.
//
// @return ADEXFILE_ERROR_OK if successful.
// @return ADEXFILE_ERROR_NOT_ENOUGH_DATA if the provided dex file is too short (truncated).
// @return ADEXFILE_ERROR_INVALID_HEADER if the memory does not seem to represent DEX file.
// @return ADEXFILE_ERROR_INVALID_DEX if any other non-specific error occurs.
//
// Thread-safe (creates new object).
ADexFile_Error ADexFile_create(const void* _Nonnull address,
                               size_t size,
                               size_t* _Nullable new_size,
                               const char* _Nonnull location,
                               /*out*/ ADexFile* _Nullable * _Nonnull out_dex_file);

// Find method at given offset and call callback with information about the method.
//
// @param dex_offset Offset relative to the start of the dex file header.
// @param callback The callback to call when method is found. Any data that needs to
//                 outlive the execution of the callback must be copied by the user.
// @param callback_data Extra user-specified argument for the callback.
//
// @return Number of methods found (0 or 1).
//
// Not thread-safe for calls on the same ADexFile instance.
size_t ADexFile_findMethodAtOffset(ADexFile* _Nonnull self,
                                   size_t dex_offset,
                                   ADexFile_MethodCallback* _Nonnull callback,
                                   void* _Nullable callback_data);

// Call callback for all methods in the DEX file.
//
// @param flags Specifies which information should be obtained.
// @param callback The callback to call for all methods. Any data that needs to
//                 outlive the execution of the callback must be copied by the user.
// @param callback_data Extra user-specified argument for the callback.
//
// @return Number of methods found.
//
// Not thread-safe for calls on the same ADexFile instance.
size_t ADexFile_forEachMethod(ADexFile* _Nonnull self,
                              ADexFile_MethodCallback* _Nonnull callback,
                              void* _Nullable callback_data);

// Free the given object.
//
// Thread-safe, can be called only once for given instance.
void ADexFile_destroy(ADexFile* _Nullable self);

// @return Offset of byte-code of the method relative to start of the dex file.
// @param out_size Optionally return size of byte-code in bytes.
// Not thread-safe for calls on the same ADexFile instance.
size_t ADexFile_Method_getCodeOffset(const ADexFile_Method* _Nonnull self,
                                     size_t* _Nullable out_size);

// @return Method name only (without class).
//         The encoding is slightly modified UTF8 (see Dex specification).
// @param out_size Optionally return string size (excluding null-terminator).
//
// Returned data may be short lived: it must be copied before calling
// this method again within the same ADexFile.
// (it is currently long lived, but this is not guaranteed in the future).
//
// Not thread-safe for calls on the same ADexFile instance.
const char* _Nonnull ADexFile_Method_getName(const ADexFile_Method* _Nonnull self,
                                             size_t* _Nullable out_size);

// @return Method name (with class name).
//         The encoding is slightly modified UTF8 (see Dex specification).
// @param out_size Optionally return string size (excluding null-terminator).
// @param with_params Whether to include method parameters and return type.
//
// Returned data may be short lived: it must be copied before calling
// this method again within the same ADexFile.
// (it points to pretty printing buffer within the ADexFile instance)
//
// Not thread-safe for calls on the same ADexFile instance.
const char* _Nonnull ADexFile_Method_getQualifiedName(const ADexFile_Method* _Nonnull self,
                                                      int with_params,
                                                      size_t* _Nullable out_size);

// @return Class descriptor (mangled class name).
//         The encoding is slightly modified UTF8 (see Dex specification).
// @param out_size Optionally return string size (excluding null-terminator).
//
// Returned data may be short lived: it must be copied before calling
// this method again within the same ADexFile.
// (it is currently long lived, but this is not guaranteed in the future).
//
// Not thread-safe for calls on the same ADexFile instance.
const char* _Nonnull ADexFile_Method_getClassDescriptor(const ADexFile_Method* _Nonnull self,
                                                        size_t* _Nullable out_size);

// @return Compile-time literal or nullptr on error.
const char* _Nullable ADexFile_Error_toString(ADexFile_Error self);

__END_DECLS

#endif  // ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_EXTERNAL_H_
