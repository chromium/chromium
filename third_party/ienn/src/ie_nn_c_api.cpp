// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ie_nn_c_api.h"

#include <map>
#include <memory>
#include <vector>

#include <inference_engine.hpp>
#include "constants.h"
#include "ie_compilation.h"
#include "ie_execution.h"
#include "utils.h"

namespace IE = InferenceEngine;

/**
 * @struct ie_model
 * @brief Represents model information that reflects the set of supported
 * operations
 */
struct ie_model {
  std::shared_ptr<IE::ModelInfo> object;
};

struct ie_compilation {
  std::unique_ptr<IE::Compilation> object;
};

struct ie_execution {
  std::unique_ptr<IE::Execution> object;
};

int32_t ie_model_create(ie_model_t** model) {
  if (model == nullptr) {
    return IE::error_t::BAD_STATE;
  }

  *model = new ie_model_t;
  (*model)->object.reset(new IE::ModelInfo());
  return IE::error_t::NOT_ERROR;
}

void ie_model_free(ie_model_t* model) {
  if (model) {
    delete model;
    model = NULL;
  }
}

int32_t ie_model_add_operand(ie_model_t* model, ie_operand_t* ie_operand) {
  if (model == nullptr || ie_operand == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  IE::Operand operand;
  operand.type = ie_operand->type;
  for (size_t i = 0; i < ie_operand->dimensionCount; ++i) {
    operand.dimensions.push_back(ie_operand->dimensions[i]);
  }
  operand.scale = ie_operand->scale;
  operand.zeroPoint = ie_operand->zeroPoint;

  model->object->operands.push_back(operand);
  return IE::error_t::NOT_ERROR;
}

int32_t ie_model_set_operand_value(ie_model_t* model,
                                   uint32_t index,
                                   const void* buffer,
                                   size_t length) {
  if (model == nullptr || buffer == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  model->object->values.insert({index, IE::OperandValue(buffer, length)});

  return IE::error_t::NOT_ERROR;
}

int32_t ie_model_add_operation(ie_model_t* model,
                               int32_t type,
                               uint32_t inputCount,
                               const uint32_t* inputs,
                               uint32_t outputCount,
                               const uint32_t* outputs) {
  if (model == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  IE::Operation operation;
  operation.type = type;
  for (size_t i = 0; i < inputCount; ++i) {
    operation.inputs.push_back(inputs[i]);
  }
  for (size_t i = 0; i < outputCount; ++i) {
    operation.outputs.push_back(outputs[i]);
  }
  model->object->operations.push_back(operation);
  return IE::error_t::NOT_ERROR;
}

int32_t ie_model_identify_inputs_outputs(ie_model_t* model,
                                         uint32_t inputCount,
                                         const uint32_t* inputs,
                                         uint32_t outputCount,
                                         const uint32_t* outputs) {
  if (model == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  for (size_t i = 0; i < inputCount; ++i) {
    model->object->inputs.push_back(inputs[i]);
  }
  for (size_t i = 0; i < outputCount; ++i) {
    model->object->outputs.push_back(outputs[i]);
  }
  return IE::error_t::NOT_ERROR;
}

int32_t ie_compilation_create(ie_model_t* model,
                              ie_compilation_t** compliation) {
  if (model == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  *compliation = new ie_compilation_t;
  (*compliation)->object.reset(new IE::Compilation(model->object));
  return IE::error_t::NOT_ERROR;
}

int32_t ie_compilation_set_preference(ie_compilation_t* compliation,
                                      int32_t preference) {
  if (compliation == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  compliation->object->SetPreference(preference);
  return IE::error_t::NOT_ERROR;
}

int32_t ie_compilation_finish(ie_compilation_t* compliation) {
  if (compliation == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  return compliation->object->Compile();
}

void ie_compilation_free(ie_compilation_t* compilation) {
  if (compilation) {
    delete compilation;
    compilation = NULL;
  }
}

int32_t ie_execution_create(ie_compilation_t* compliation,
                            ie_execution_t** execution) {
  if (compliation == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  *execution = new ie_execution_t;
  (*execution)->object.reset(new IE::Execution(std::move(compliation->object)));
  return (*execution)->object->Init();
}

int32_t ie_execution_set_input(ie_execution_t* execution,
                               uint32_t index,
                               void* buffer,
                               uint32_t length) {
  if (execution == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  execution->object->SetInputOperandValue(buffer, length);
  return IE::error_t::NOT_ERROR;
}

int32_t ie_execution_set_output(ie_execution_t* execution,
                                uint32_t index,
                                void* buffer,
                                uint32_t length) {
  if (execution == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  execution->object->SetOutputOperandValue(buffer, length);
  return IE::error_t::NOT_ERROR;
}

int32_t ie_execution_start_compute(ie_execution_t* execution) {
  if (execution == nullptr) {
    return IE::error_t::BAD_DATA;
  }
  return execution->object->StartCompute();
}

void ie_execution_free(ie_execution_t* execution) {
  if (execution) {
    delete execution;
    execution = NULL;
  }
}
