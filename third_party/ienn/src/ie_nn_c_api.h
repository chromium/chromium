// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IE_NN_C_API_H
#define IE_NN_C_API_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
#define OPENCV_C_EXTERN extern "C"
#else
#define OPENCV_C_EXTERN
#endif

#if defined(__GNUC__) && (__GNUC__ < 4)
#define BUILD_NETWORK_C_WRAPPER(...) OPENCV_C_EXTERN __VA_ARGS__
#else
#if defined(_WIN32)
#ifdef opencv_c_wraper_EXPORTS
#define BUILD_NETWORK_C_WRAPPER(...) \
  OPENCV_C_EXTERN __declspec(dllexport) __VA_ARGS__ __cdecl
#else
#define BUILD_NETWORK_C_WRAPPER(...) \
  OPENCV_C_EXTERN __declspec(dllimport) __VA_ARGS__ __cdecl
#endif
#else
#define BUILD_NETWORK_C_WRAPPER(...) \
  OPENCV_C_EXTERN __attribute__((visibility("default"))) __VA_ARGS__
#endif
#endif

typedef struct ie_model ie_model_t;
typedef struct ie_compilation ie_compilation_t;
typedef struct ie_execution ie_execution_t;

typedef struct ie_operand {
  int32_t type;
  int32_t dimensionCount;
  const uint32_t* dimensions;
  float scale;
  int32_t zeroPoint;
} ie_operand_t;

/**
 * @brief Constructs Inference Engine Model instance. Use the ie_model_free()
 * method to free memory.
 * @ingroup Model
 * @param model A pointer to the newly created ie_model_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t) ie_model_create(ie_model_t** model);

/**
 * @brief Releases memory occupied by model.
 * @ingroup Model
 * @param model A pointer to the model to free memory.
 */
BUILD_NETWORK_C_WRAPPER(void) ie_model_free(ie_model_t* model);

/**
 * @brief Add operand to model.
 * @ingroup Model
 * @param model A pointer to the specified ie_model_t.
 * @param operand A pointer to ie_operand_t that will be add to model.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_model_add_operand(ie_model_t* model, ie_operand_t* operand);

/**
 * @brief Set operand value.
 * @ingroup Model
 * @param model A pointer to the specified ie_model_t.
 * @param index the index of operand.
 * @param buffer the buffer of operand.
 * @param length the length of operand.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_model_set_operand_value(ie_model_t* model,
                           uint32_t index,
                           const void* buffer,
                           size_t length);

/**
 * @brief Add operation to model.
 * @ingroup Model
 * @param model A pointer to the specified ie_model_t.
 * @param inputCount the count of input.
 * @param inputs the operand of index for inputs.
 * @param outputCount the count of output.
 * @param outputs the operand of index for outputs.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_model_add_operation(ie_model_t* model,
                       int32_t type,
                       uint32_t inputCount,
                       const uint32_t* inputs,
                       uint32_t outputCount,
                       const uint32_t* outputs);

/**
 * @brief Add operation to model.
 * @ingroup Model
 * @param model A pointer to the specified ie_model_t.
 * @param inputCount the count of input.
 * @param inputs the operand of index for inputs.
 * @param outputCount the count of output.
 * @param outputs the operand of index for outputs.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_model_identify_inputs_outputs(ie_model_t* model,
                                 uint32_t inputCount,
                                 const uint32_t* inputs,
                                 uint32_t outputCount,
                                 const uint32_t* outputs);

/**
 * @brief Create Compilation for the model.
 * @ingroup Compilation
 * @param model A pointer to the newly created ie_compilation_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_compilation_create(ie_model_t* model, ie_compilation_t** compliation);

/**
 * @brief Set prefence to the Compilation.
 * @ingroup Compilation
 * @param compliation A pointer to the specified ie_compilation_t.
 * @param preference preference.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_compilation_set_preference(ie_compilation_t* compliation,
                              int32_t preference);

/**
 * @brief Start to compile the model.
 * @ingroup Compilation
 * @param compliation A pointer to the specified ie_compilation_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_compilation_finish(ie_compilation_t* compliation);

/**
 * @brief Releases memory occupied by compilation.
 * @ingroup Model
 * @param model A pointer to the compilation to free memory.
 */
BUILD_NETWORK_C_WRAPPER(void)
ie_compilation_free(ie_compilation_t* compilation);

/**
 * @brief Create execution for the model.
 * @ingroup Execution
 * @param compliation A pointer to the specified ie_compilation_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_execution_create(ie_compilation_t* compliation, ie_execution_t** execution);

/**
 * @brief Set input data for the model.
 * @ingroup Execution
 * @param compliation A pointer to the specified ie_execution_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_execution_set_input(ie_execution_t* execution,
                       uint32_t index,
                       void* buffer,
                       uint32_t length);

/**
 * @brief Set output data for the model.
 * @ingroup Execution
 * @param compliation A pointer to the specified ie_execution_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_execution_set_output(ie_execution_t* execution,
                        uint32_t index,
                        void* buffer,
                        uint32_t length);

/**
 * @brief Start compute the model.
 * @ingroup Execution
 * @param compliation A pointer to the specified ie_execution_t.
 * @return Status code of the operation: OK(0) for success.
 */
BUILD_NETWORK_C_WRAPPER(int32_t)
ie_execution_start_compute(ie_execution_t* execution);

/**
 * @brief Releases memory occupied by execution.
 * @ingroup Model
 * @param model A pointer to the execution to free memory.
 */
BUILD_NETWORK_C_WRAPPER(void) ie_execution_free(ie_execution_t* execution);
#endif  // IE_NN_C_API_H