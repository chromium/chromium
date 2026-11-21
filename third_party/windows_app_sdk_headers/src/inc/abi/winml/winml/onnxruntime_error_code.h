// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

/** \addtogroup Global
 * ONNX Runtime C API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Error codes reported by ONNX Runtime.
 *
 * The error code associated with an ::OrtStatus.
 */
typedef enum OrtErrorCode {
  /**
   * Success. No error occurred.
   */
  ORT_OK,
  /**
   * Generic failure that does not map to a more specific error code. Consult the error message for details.
   */
  ORT_FAIL,
  /**
   * A caller-supplied argument was invalid (e.g. NULL pointer, out-of-range value, mismatched shape/rank, or bad
   * configuration).
   */
  ORT_INVALID_ARGUMENT,
  /**
   * A required file (such as a model file) does not exist.
   */
  ORT_NO_SUCHFILE,
  /**
   * Legacy/unused but retained for ABI compatibility. Historically returned when a model could not be found by name in
   * the ONNX Runtime Server (removed in 2022).
   */
  ORT_NO_MODEL,
  /**
   * A hardware accelerator or backend engine reported a failure (e.g. a device crash or other device-level error).
   */
  ORT_ENGINE_ERROR,
  /**
   * A generic runtime exception was caught. The error message is the primary source of detail.
   */
  ORT_RUNTIME_EXCEPTION,
  /**
   * Protobuf parsing or serialization failed.
   */
  ORT_INVALID_PROTOBUF,
  /**
   * Invalid session state for the requested operation. Despite the name, this code does not mean "success, model
   * loaded"; it is returned when the session is in the wrong state for the requested call (e.g. a model is already
   * loaded, the session is already initialized, or no model has been loaded yet). The name is historical and is
   * retained for ABI compatibility; consult the error message for the specific condition.
   */
  ORT_MODEL_LOADED,
  /**
   * The requested functionality is not implemented in this build.
   */
  ORT_NOT_IMPLEMENTED,
  /**
   * The model graph is structurally invalid (e.g. recursive function definitions, invalid tensor dimensions, or
   * malformed nodes).
   */
  ORT_INVALID_GRAPH,
  /**
   * An execution provider reported a generic failure.
   */
  ORT_EP_FAIL,
  /**
   * Model loading or session initialization was canceled at the caller's request.
   */
  ORT_MODEL_LOAD_CANCELED,
  /**
   * The model requires compilation by an execution provider, but compilation was disabled via session options.
   */
  ORT_MODEL_REQUIRES_COMPILATION,
  /**
   * A requested resource could not be found.
   */
  ORT_NOT_FOUND,
  /**
   * The execution provider's hardware device has been reset.
   *
   * The caller should stop using the existing session (and release it) and create a new session.
   */
  ORT_DEVICE_RESET,
} OrtErrorCode;

#ifdef __cplusplus
}
#endif

/// @}
