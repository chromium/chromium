// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

namespace onnxruntime {
class IExecutionFrame;
class Stream;
namespace concurrency {
class ThreadPool;
}

class OpKernelContext {
 public:
  using ArgMap = std::unordered_map<std::string, size_t>;

  OpKernelContext(_Inout_ IExecutionFrame* frame, _In_ const OpKernel* kernel,
                  _In_ Stream* stream,
                  _In_opt_ concurrency::ThreadPool* threadpool, _In_ const logging::Logger& logger);

  virtual ~OpKernelContext() = default;

  /**
  Return the number of inputs for a variadic argument.
  @param arg_num The operator argument number.
  @returns Number of inputs the argument has.
  */
  virtual int NumVariadicInputs(size_t arg_num) const;

  virtual MLDataType InputType(int index) const;
  virtual MLDataType OutputType(int index) const;

  const OrtValue* GetInputOrtValue(int index) const {
    return GetInputMLValue(index);
  }

  template <typename T>
  const T* Input(int index) const {
    const OrtValue* p_ml_value = GetInputMLValue(index);
    ORT_TRY {
      return p_ml_value ? &(p_ml_value->Get<T>()) : nullptr;
    }
    ORT_CATCH(const std::exception& /*e*/) {
      ORT_THROW("Missing Input: " + kernel_->Node().InputDefs()[index]->Name());
    }
  }

  // Fetch a required input, enforcing that it is present.
  template <typename T>
  const T& RequiredInput(int index) const {
    const T* input_ptr = Input<T>(index);
    ORT_ENFORCE(input_ptr, "Required input at index ", index, " is not present.");
    return *input_ptr;
  }

  // Fetch output (non-tensor) with specified index.
  template <typename T>
  T* Output(int index) {
    if (index < 0 || index >= OutputCount())
      return nullptr;

    OrtValue* p_ml_value = GetOrCreateOutputMLValue(index);
    return p_ml_value ? p_ml_value->GetMutable<T>() : nullptr;
  }

  // In the case that memory allocation has not been done for an output tensor,
  // The memory allocation will be done on-the-fly with given tensor shape.
  // Return nullptr if the output is an unused optional output.
  Tensor* Output(int index, const TensorShape& shape);
  Tensor* Output(int index, const std::vector<int64_t>& shape);
  Tensor* Output(int index, const std::initializer_list<int64_t>& shape);

  // Fetch a required tensor output, enforcing that it is present.
  Tensor& RequiredOutput(int index, const TensorShape& shape) {
    Tensor* output_ptr = Output(index, shape);
    ORT_ENFORCE(output_ptr, "Required output at index ", index, " is not present.");
    return *output_ptr;
  }

#if !defined(DISABLE_SPARSE_TENSORS)
  // Fetch a sparse-tensor output corresponding to the specified index.
  // shape must specify the shape of the underlying dense-tensor.
  // Memory allocation for the output may happen when this method is invoked,
  // unless static optimization pre-allocates it.
  SparseTensor* OutputSparse(int index, const TensorShape& shape);
#endif

#if !defined(DISABLE_OPTIONAL_TYPE)
  // Use this API to output a "None" of a specific type (e.g. Tensor) at specified index
  template <typename T>
  void OutputOptionalWithoutData(int index) {
    auto* output_ort_value = GetOutputMLValue(index);

    auto type = DataTypeImpl::GetType<T>();

    output_ort_value->Init(nullptr,  // This OrtValue is "None" and has no data
                           type,
                           type->GetDeleteFunc());
  }
#endif

  // Retrieve indexed shape obtained from memory planning before actual
  // computation. If the indexed shape cannot be inferred, this function returns
  // false.
  virtual bool TryGetInferredInputShape(int index, TensorShape& shape) const;

  // Retrieve indexed shape obtained from memory planning before actual
  // computation. If the indexed shape cannot be inferred, this function returns
  // false.
  virtual bool TryGetInferredOutputShape(int index, TensorShape& shape) const;

  const logging::Logger& Logger() const {
    return *logger_;
  }

  // always >= 0
  virtual int InputCount() const {
    return static_cast<int>(kernel_->Node().InputDefs().size());
  }

  // always >= 0
  virtual int ImplicitInputCount() const {
    return static_cast<int>(kernel_->Node().ImplicitInputDefs().size());
  }

  // always >= 0
  virtual int OutputCount() const {
    return static_cast<int>(kernel_->Node().OutputDefs().size());
  }

  /**
   Return an allocator on device 0, with memtype of OrtMemTypeDefault.
   @remarks Use SafeInt when calculating the size of memory to allocate using AllocatorPtr->Alloc.
   */
  [[nodiscard]] virtual Status GetTempSpaceAllocator(AllocatorPtr* output) const;

  /**
   Return the allocator associated with the CPU EP with memtype of OrtMemTypeDefault.
   @remarks Use SafeInt when calculating the size of memory to allocate using AllocatorPtr->Alloc.
   */
  [[nodiscard]] Status GetTempSpaceCPUAllocator(AllocatorPtr* output) const;

  /**
  Return the device id that current kernel runs on.
  */
  virtual int GetDeviceId() const {
    return kernel_->Info().GetExecutionProvider()->GetDeviceId();
  }

  /**
  Return the compute stream associated with the EP that the kernel is partitioned to.
  For EPs that do not have a compute stream (e.g. CPU EP), a nullptr is returned.
  */
  [[nodiscard]] virtual Stream* GetComputeStream() const {
    return stream_;
  }

  /**
  Returns the opset domain of the underlying kernel
  **/
  const std::string& GetOpDomain() const;

  /**
  Returns the optype of the underlying kernel
  **/
  const std::string& GetOpType() const;

  /**
  Returns the node name of the underlying kernel
  **/
  const std::string& GetNodeName() const;

  /**
  Returns the intra-op threadpool, if available.
  */
  _Ret_maybenull_ onnxruntime::concurrency::ThreadPool* GetOperatorThreadPool() const { return threadpool_; }

  /**
  Returns whether deterministic computation is preferred.
  */
  virtual bool GetUseDeterministicCompute() const {
    return true;
  }

  /**
  Returns Allocator from a specific OrtMemoryInfo object.
  TODO(leca): Replace GetTempSpaceAllocator() and GetTempSpaceCPUAllocator() with this API in the future
  */
  AllocatorPtr GetAllocator(const OrtDevice& device) const;

 protected:
  OpKernelContext(concurrency::ThreadPool* threadpool, const logging::Logger& logger, Stream* stream);

  onnxruntime::NodeIndex GetNodeIndex() const;

  virtual const OrtValue* GetInputMLValue(int index) const;
  const OrtValue* GetImplicitInputMLValue(int index) const;
  OrtValue* GetOutputMLValue(int index);

#ifdef ENABLE_ATEN
  Status SetOutputMLValue(int index, const OrtValue& ort_value);
#endif

  // Creates the OrtValue* based on the shape, if it does not exist
  virtual OrtValue* OutputMLValue(int index, const TensorShape& shape);

  virtual OrtValue* GetOrCreateOutputMLValue(int index);

 private:
  ORT_DISALLOW_COPY_AND_ASSIGNMENT(OpKernelContext);
  int GetInputArgIndex(int index) const;
  int GetImplicitInputArgIndex(int index) const;
  int GetOutputArgIndex(int index) const;

  IExecutionFrame* const execution_frame_{};
  const OpKernel* const kernel_{};
  concurrency::ThreadPool* const threadpool_{};
  const logging::Logger* const logger_{};

  // The argument starting index in ExecutionFrame.
  int node_input_start_index_{-1};
  int node_implicit_input_start_index_{-1};
  int node_output_start_index_{-1};

  Stream* stream_;
};

// Fetching output tensor without shape is not allowed except when it already exists
template <>
inline Tensor* OpKernelContext::Output<Tensor>(int index) {
  OrtValue* p_ml_value = GetOutputMLValue(index);
  ORT_ENFORCE(p_ml_value, "Please fetch output tensor with specified shape.");
  return p_ml_value->GetMutable<Tensor>();
}

#if !defined(DISABLE_SPARSE_TENSORS)
template <>
inline SparseTensor* OpKernelContext::Output<SparseTensor>(int index) {
  OrtValue* p_ml_value = GetOutputMLValue(index);
  ORT_ENFORCE(p_ml_value, "Please fetch output sparse tensor with specified shape.");
  return p_ml_value->GetMutable<SparseTensor>();
}
#endif

}  // namespace onnxruntime
