// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/common/status.h"
#include "core/common/logging/logging.h"
#include "core/framework/op_kernel.h"
#include "core/framework/kernel_def_builder.h"
#include "core/framework/kernel_registry.h"

#if !defined(ORT_MINIMAL_BUILD)
#include "core/graph/schema_registry.h"
#endif

namespace onnxruntime {

/**
   Represents a registry that contains both custom kernels and custom schemas.
*/
class CustomRegistry final {
 public:
  CustomRegistry()
      : kernel_registry_(std::make_shared<KernelRegistry>())
#if !defined(ORT_MINIMAL_BUILD)
        ,
        opschema_registry_(std::make_shared<onnxruntime::OnnxRuntimeOpSchemaRegistry>())
#endif
  {
  }

  /**
   * Register a kernel definition together with kernel factory method to this session.
   * If any conflict happened between registered kernel def and built-in kernel def,
   * registered kernel will have higher priority.
   * Call this before invoking Initialize().
   * @return OK if success.
   */
  common::Status RegisterCustomKernel(KernelDefBuilder& kernel_def_builder, const KernelCreateFn& kernel_creator);

  common::Status RegisterCustomKernel(KernelCreateInfo&);

  const std::shared_ptr<KernelRegistry>& GetKernelRegistry();

#if !defined(ORT_MINIMAL_BUILD)
  common::Status RegisterOpSet(std::vector<ONNX_NAMESPACE::OpSchema>& schemas, const std::string& domain,
                               int baseline_opset_version, int opset_version);

  const std::shared_ptr<onnxruntime::OnnxRuntimeOpSchemaRegistry>& GetOpschemaRegistry();
#endif

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(CustomRegistry);
  std::shared_ptr<KernelRegistry> kernel_registry_;
#if !defined(ORT_MINIMAL_BUILD)
  std::shared_ptr<onnxruntime::OnnxRuntimeOpSchemaRegistry> opschema_registry_;
#endif
};

}  // namespace onnxruntime
