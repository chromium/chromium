#ifndef IE_EXECUTION_H
#define IE_EXECUTION_H

#include <memory>
#include <vector>

#include "ie_compilation.h"

namespace InferenceEngine {

class Execution {
 public:
  Execution(std::unique_ptr<Compilation> compilation);
  ~Execution();

  int32_t Init();
  int32_t SetInputOperandValue(void* buffer, uint32_t length);
  int32_t SetOutputOperandValue(void* buffer, uint32_t length);

  int32_t StartCompute();

 private:
  bool initialized_;

  std::unique_ptr<Compilation> compilation_;
  std::vector<OperandValue> input_data_;
  std::vector<OutputData> output_data_;

  std::unique_ptr<InferRequest> infer_request_;
  std::unique_ptr<InferencePlugin> plugin_;
  std::unique_ptr<ExecutableNetwork> execution_;
  std::unique_ptr<Core> ie_core_;

  DISALLOW_COPY_AND_ASSIGN(Execution);
};

}  // namespace InferenceEngine

#endif // IE_EXECUTION_H