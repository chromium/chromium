// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#if !defined(DISABLE_SPARSE_TENSORS)

#include "core/framework/data_types.h"
#include "core/framework/tensor_shape.h"
#include "core/framework/tensor.h"

struct OrtValue;

namespace onnxruntime {

class IDataTransfer;
class DataTransferManager;

/**
 * @brief This is a Sparse Format enumeration
 *
 *
 */
enum class SparseFormat : uint32_t {
  kUndefined = 0x0U,        // For completeness
  kCoo = 0x1U,              // 1-D or 2-D indices
  kCsrc = 0x1U << 1,        // Both CSR(C)
  kBlockSparse = 0x1U << 2  // as in OpenAI
};

std::ostream& operator<<(std::ostream&, SparseFormat);

/**
 * @brief This class implements SparseTensor.
 * This class holds sparse non-zero data (values) and sparse format
 * specific indices. There are two main uses for the class (similar to that of Tensor)
 * - one is to re-present model sparse inputs. Such inputs typically reside
 *   in user allocated buffers that are not owned by SparseTensor instance and the instance
 *   serves as a facade to expose user allocated buffers. Such buffers should already
 *   contain proper values and format specific indices.
 *   Use the first constructor
 *   to instantiate SparseTensor and supply values_data pointer. Use*() functions can
 *   be used to supply pointers to format specific indices. These buffers are used as is
 *   and will not be modified or deallocated by the instance. However, the lifespan of the buffers
 *   must eclipse the lifespan of the SparseTensor instance.
 *
 * - Represent sparse data that is a result of format conversion or a computation result. Use second constructor
 *   to supply a desired allocator. Use Make*() format specific interfaces to supply values and format
 *   specific indices. The specified data will be copied into an internally allocated buffer.
     Internally, we will represent a SparseTensor as a single contiguous buffer that
 *   contains values followed by format specific indices. We use Tensors to project
 *   values and indices into various parts of buffer.
 */

class SparseTensor final {
 public:
  /// <summary>
  /// This constructs an instance that points to user defined buffers.
  /// Make use of Use* functions to supply format specific indices that
  /// reside in the user supplied buffers. The instance constructed this way
  /// will not copy data. The lifespan of supplied buffers is expected to eclipse
  ///  the lifespan of the sparse tensor instance.
  /// </summary>
  /// <param name="elt_type">MlDataType</param>
  /// <param name="dense_shape">a shape of original tensor in dense form</param>
  /// <param name="values_shape">shape for user supplied values. Use {0} shape for fully sparse tensors.</param>
  /// <param name="values_data">a pointer to values. Use nullptr for fully sparse tensors.</param>
  /// <param name="location">description of the user allocated memory</param>
  SparseTensor(MLDataType elt_type,
               const TensorShape& dense_shape,
               const TensorShape& values_shape,
               void* values_data,
               const OrtMemoryInfo& location);

  /// <summary>
  /// Use this constructor to hold sparse data in the buffer
  /// allocated with the specified allocator. Use Make*() methods
  /// to populate the instance with data which will be copied into the
  /// allocated buffer.
  /// </summary>
  /// <param name="elt_type"></param>
  /// <param name="dense_shape"></param>
  /// <param name="allocator"></param>
  SparseTensor(MLDataType elt_type,
               const TensorShape& dense_shape,
               std::shared_ptr<IAllocator> allocator);

  SparseTensor() noexcept;

  ~SparseTensor();

  ORT_DISALLOW_COPY_AND_ASSIGNMENT(SparseTensor);

  /// <summary>
  /// The factory function creates an instance of SparseTensor on the heap
  /// using appropriate constructor and initializes OrtValue instance wit it.
  /// </summary>
  /// <param name="elt_type">element data type</param>
  /// <param name="dense_shape">dense shape of the sparse tensor</param>
  /// <param name="values_shape">values shape. Use {0} for fully sparse tensors.</param>
  /// <param name="values_data">pointer to a user allocated buffer. Use nullptr for fully sparse tensors.</param>
  /// <param name="location">description of the user allocated buffer</param>
  /// <param name="ort_value">default constructed input/output ort_value</param>
  static void InitOrtValue(MLDataType elt_type,
                           const TensorShape& dense_shape,
                           const TensorShape& values_shape,
                           void* values_data,
                           const OrtMemoryInfo& location,
                           OrtValue& ort_value);

  /// <summary>
  /// The factory function creates an instance of SparseTensor on the heap
  /// using appropriate constructor and initializes OrtValue instance wit it.
  /// </summary>
  /// <param name="elt_type">element data type</param>
  /// <param name="dense_shape">dense shape of the sparse tensor</param>
  /// <param name="allocator">allocator to use</param>
  /// <param name="ort_value">default constructed input/output ort_value</param>
  static void InitOrtValue(MLDataType elt_type,
                           const TensorShape& dense_shape,
                           std::shared_ptr<IAllocator> allocator,
                           OrtValue& ort_value);

  /// <summary>
  /// The function will check if the OrtValue is allocated
  /// fetch the containing SparseTensor instance or throw if it
  /// does not contain one. It will check that the SparseTensor has
  /// sparse format set (i.e. fully constructed).
  /// </summary>
  /// <param name="v">OrtValue instance</param>
  /// <returns>const SparseTensor Reference</returns>
  static const SparseTensor& GetSparseTensorFromOrtValue(const OrtValue& v);

  /// <summary>
  /// /// The function will check if the OrtValue is allocated
  /// fetch the containing SparseTensor instance or throw if it
  /// does not contain one. It will check that the SparseTensor does not
  /// have sparse format set and will return non-const ref to so indices
  /// can be added to it.
  /// </summary>
  /// <param name="v">OrtValue</param>
  /// <returns>non-const reference to SparseTensor</returns>
  static SparseTensor& GetSparseTensorFromOrtValue(OrtValue& v);

  /// <summary>
  // Returns the number of non-zero values (aka "NNZ")
  // For block sparse formats this may include some zeros in the blocks
  // are considered non-zero.
  /// </summary>
  /// <returns>nnz</returns>
  size_t NumValues() const { return static_cast<size_t>(values_.Shape().Size()); }

  /// <summary>
  /// Read only accessor to non-zero values
  /// </summary>
  /// <returns></returns>
  const Tensor& Values() const noexcept {
    return values_;
  }

  SparseTensor(SparseTensor&& o) noexcept;
  SparseTensor& operator=(SparseTensor&& o) noexcept;

  /// <summary>
  /// Returns SparseFormat that the instance currently holds
  /// if the value returned in kUndefined, the instance is not populated
  /// </summary>
  /// <returns>format enum</returns>
  SparseFormat Format() const noexcept {
    return format_;
  }

  /// <summary>
  /// Returns a would be dense_shape
  /// </summary>
  /// <returns>reference to dense_shape</returns>
  const TensorShape& DenseShape() const noexcept {
    return dense_shape_;
  }

  /// <summary>
  /// Calculates and returns how much this fully initialized SparseTensor data (would)
  /// occupy in a contiguous allocation block, or, in fact, occupies if it owns the buffer.
  /// </summary>
  /// <returns>required allocation size</returns>
  int64_t RequiredAllocationSize() const noexcept;

  /// <summary>
  /// Returns Tensor element type enum.
  /// Useful for type dispatching
  /// </summary>
  /// <returns></returns>
  int32_t GetElementType() const {
    return ml_data_type_->GetDataType();
  }

  /// <summary>
  /// Return Element MLDataType
  /// </summary>
  /// <returns></returns>
  MLDataType DataType() const noexcept {
    return ml_data_type_;
  }

  /// <summary>
  /// Test for string type
  /// </summary>
  /// <returns>true if tensor values are strings</returns>
  bool IsDataTypeString() const {
    return utils::IsPrimitiveDataType<std::string>(ml_data_type_);
  }

  /// <summary>
  /// Checks if the Tensor contains data type T
  /// </summary>
  /// <typeparam name="T"></typeparam>
  /// <returns>true if tensor contains data of type T</returns>
  template <class T>
  bool IsDataType() const {
    return utils::IsPrimitiveDataType<T>(ml_data_type_);
  }

  const OrtMemoryInfo& Location() const noexcept { return location_; }

  /// <summary>
  /// Read only access to Coo indices
  /// </summary>
  class CooView {
   public:
    explicit CooView(const Tensor& indices) noexcept
        : indices_(indices) {}
    const Tensor& Indices() const noexcept { return indices_; }

   private:
    std::reference_wrapper<const Tensor> indices_;
  };

  /// <summary>
  /// Returns Coo index view
  /// </summary>
  /// <returns>CooView instance</returns>
  CooView AsCoo() const;

  /// <summary>
  /// Uses COO index contained in the user allocated buffer along with the values buffer passed on
  /// to the constructor. The buffer is used as is and its lifespan must eclipse the lifespan of the sparse
  /// tensor instance. The OrtMemoryInfo (location) of the index is assumed to be the same as values.
  ///
  /// The index size must either exactly match the number of values in which case
  /// index shape would be 1-D (values_count) or it must be twice the number of values
  /// in which case its shape would be 2-D (values_count, 2)
  /// </summary>
  /// <param name="indices">user allocated buffer span. Use empty span for fully sparse tensors.</param>
  /// <returns>Status</returns>
  Status UseCooIndices(gsl::span<int64_t> indices);

  /// <summary>
  /// The method allocates a single contiguous buffer and copies specified values
  /// and indices into it using supplied IDataTransfer.
  ///
  /// The indices size must either exactly match the number of values in which case
  /// indices shape would be 1-D (values_count) or it must be twice the number of values
  /// in which case its shape would be 2-D (values_count, 2).
  ///
  /// Values shape is supplied at construction time and its Size() must match values_count.
  /// </summary>
  /// <param name="values_count">Use 0 for fully sparse tensors.</param>
  /// <param name="values_data">pointer to a buffer to be copied. Use nullptr for fully sparse tensors.</param>
  /// <param name="indices"></param>
  /// <returns></returns>
  Status MakeCooData(const IDataTransfer& data_transfer, const OrtMemoryInfo& data_location,
                     size_t values_count, const void* values_data, gsl::span<const int64_t> indices);

  /// <summary>
  /// The method allocates a single contiguous buffer and creates instances of std::strings in it, with
  /// copies of the supplied zero-terminated strings followed by COO indices.
  /// All data is assumed to be on CPU and the allocator supplied must be
  /// a CPU based allocator.
  /// </summary>
  /// <param name="string_count">use 0 for fully sparse tensors</param>
  /// <param name="strings">array of char* pointers. use nullptr for fully sparse tensors</param>
  /// <param name="indices">span of indices. Use empty span for fully sparse tensors.</param>
  /// <returns>Status</returns>
  Status MakeCooStrings(size_t string_count, const char* const* strings, gsl::span<const int64_t> indices);

  /// <summary>
  /// Gives mutable access to Coo buffers so they can be populated
  /// </summary>
  class CooMutator {
   public:
    CooMutator(Tensor& values, Tensor& indices) noexcept : values_(values), indices_(indices) {}
    Tensor& Values() noexcept { return values_; }
    Tensor& Indices() noexcept { return indices_; }

   private:
    std::reference_wrapper<Tensor> values_;
    std::reference_wrapper<Tensor> indices_;
  };

  /// <summary>
  /// Allocates memory for values and index and returns a mutator so
  /// data can be copied into the buffer.
  /// </summary>
  /// <param name="values_count">use 0 for fully sparse tensors</param>
  /// <param name="index_count">use 0 for fully sparse tensors</param>
  /// <returns></returns>
  CooMutator MakeCooData(size_t values_count, size_t index_count);

  /// <summary>
  /// Read only access to Csr indices
  /// </summary>
  class CsrView {
   public:
    CsrView(const Tensor& inner, const Tensor& outer) noexcept
        : inner_(inner), outer_(outer) {}
    const Tensor& Inner() const noexcept { return inner_; }
    const Tensor& Outer() const noexcept { return outer_; }

   private:
    std::reference_wrapper<const Tensor> inner_;
    std::reference_wrapper<const Tensor> outer_;
  };

  /// <summary>
  /// Returns Csr indices read only view
  /// </summary>
  /// <returns></returns>
  CsrView AsCsr() const;

  /// <summary>
  /// This function will use Csr indices contained within the user allocated buffers.
  /// The lifespan of the buffers must eclipse the lifespan of sparse tensor instance.
  /// </summary>
  /// <param name="inner_index">User allocated buffer span. use empty span for fully sparse tensors</param>
  /// <param name="outer_index">User allocated buffer span. Use empty span for fully sparse tensors</param>
  /// <returns></returns>
  Status UseCsrIndices(gsl::span<int64_t> inner_index, gsl::span<int64_t> outer_index);

  /// <summary>
  /// The function will allocate a single contiguous buffer and will copy values
  /// and indices into it.
  /// </summary>
  /// <param name="data_transfer"></param>
  /// <param name="data_location"></param>
  /// <param name="values_count">use 0 for fully sparse tensors</param>
  /// <param name="values_data">pointer to data to be copied. Use nullptr for fully sparse tensors.</param>
  /// <param name="inner_index">inner index to be copied. Use empty span for fully sparse tensors.</param>
  /// <param name="outer_index">outer index to be copied. Use empty span for fully sparse tensors.</param>
  /// <returns></returns>
  Status MakeCsrData(const IDataTransfer& data_transfer,
                     const OrtMemoryInfo& data_location,
                     size_t values_count, const void* values_data,
                     gsl::span<const int64_t> inner_index,
                     gsl::span<const int64_t> outer_index);

  /// <summary>
  /// The method allocates a single contiguous buffer and creates instances of std::strings in it, with
  /// copies of the supplied zero-terminated strings followed by COO indices.
  /// All data is assumed to be on CPU and the allocator supplied must be
  /// a CPU based allocator
  /// </summary>
  /// <param name="string_count"></param>
  /// <param name="strings">array of char* pointers</param>
  /// <param name="inner_index">inner index to be copied. Use empty span for fully sparse tensors.</param>
  /// <param name="outer_index">outer index to be copied. Use empty span for fully sparse tensors.</param>
  /// <returns></returns>
  Status MakeCsrStrings(size_t string_count, const char* const* strings,
                        gsl::span<const int64_t> inner_index,
                        gsl::span<const int64_t> outer_index);

  /// <summary>
  /// Give writable access to Csr values and indices
  /// </summary>
  class CsrMutator {
   public:
    CsrMutator(Tensor& values, Tensor& inner, Tensor& outer) noexcept
        : values_(values), inner_(inner), outer_(outer) {}
    Tensor& Values() const noexcept { return values_; }
    Tensor& Inner() const noexcept { return inner_; }
    Tensor& Outer() const noexcept { return outer_; }

   private:
    std::reference_wrapper<Tensor> values_;
    std::reference_wrapper<Tensor> inner_;
    std::reference_wrapper<Tensor> outer_;
  };

  /// <summary>
  /// Allocates memory for values and index and returns mutator so
  /// data can be populated.
  /// </summary>
  /// <param name="values_count">Use 0 for fully sparse tensors.</param>
  /// <param name="inner_index_count">Use 0 for fully sparse tensors.</param>
  /// <param name="outer_index_count">Use 0 for fully sparse tensors.</param>
  /// <returns></returns>
  CsrMutator MakeCsrData(size_t values_count, size_t inner_index_count, size_t outer_index_count);

  /// <summary>
  /// Read only access to BlockSparse index
  /// </summary>
  class BlockSparseView {
   public:
    explicit BlockSparseView(const Tensor& indices) noexcept
        : indices_(indices) {}
    const Tensor& Indices() const noexcept { return indices_; }

   private:
    std::reference_wrapper<const Tensor> indices_;
  };

  /// <summary>
  /// Return BlockSparseIndex view
  /// </summary>
  /// <returns>an instance of BlockSparseView</returns>
  BlockSparseView AsBlockSparse() const;

  /// <summary>
  /// Use blocksparse indices contained in the user allocated buffer. The shape of the index
  /// must be 2-D and must contain one tuple per each of the value blocks that
  /// were supplied to the constructor. The supplied buffer lifespan must eclipse the life
  /// of sparse tensor instance.
  /// </summary>
  /// <param name="indices_shape">Use {0} for fully sparse tensors.</param>
  /// <param name="indices_data">Ptr to user allocated buffer. Use nullptr for fully spare tensors.</param>
  /// <returns></returns>
  Status UseBlockSparseIndices(const TensorShape& indices_shape, int32_t* indices_data);

  /// <summary>
  /// The function allocates a single contiguous buffer and copies values and index
  /// into it. The shape of the values is expected to be at least 3-D but may contain more
  /// dimensions. At the very minimum it should be (num_blocks, block_size, block_size).
  ///
  // The shape of the index is must be at least 2-D and must contain one tuple per each of
  // the value blocks that  were supplied to the constructor. Each index tuple is a
  // (row, col) coordinates of the values block in a dense matrix.
  /// </summary>
  /// <param name="data_transfer"></param>
  /// <param name="data_location"></param>
  /// <param name="values_shape">The shape is expected to be at least 3-D. However, use {0} for fully sparse tensors.</param>
  /// <param name="values_data">Pointer to a data to be copied. Use nullptr for fully sparse tensors.</param>
  /// <param name="indices_shape">The shape is expected to be 2-D. However, you can use {0} for fully sparse tensors.</param>
  /// <param name="indices_data">Pointer to index data to be copied. Use nullptr for fully sparse tensors.</param>
  /// <returns></returns>
  Status MakeBlockSparseData(const IDataTransfer& data_transfer,
                             const OrtMemoryInfo& data_location,
                             const TensorShape& values_shape, const void* values_data,
                             const TensorShape& indices_shape, const int32_t* indices_data);

  /// <summary>
  /// The method allocates a single contiguous buffer and creates instances of std::strings in it, with
  /// copies of the supplied zero-terminated strings followed by COO indices.
  /// All data is assumed to be on CPU and the allocator supplied must be
  /// a CPU based allocator.
  /// </summary>
  /// <param name="values_shape">Use {0} shape for fully sparse tensors</param>
  /// <param name="strings">array of char* ptrs, use nullptr for fully sparse tensor</param>
  /// <param name="indices_shape">Use {0} for fully sparse tensors</param>
  /// <param name="indices_data">use nullptr for fully sparse tensors</param>
  /// <returns></returns>
  Status MakeBlockSparseStrings(const TensorShape& values_shape, const char* const* strings,
                                const TensorShape& indices_shape, const int32_t* indices_data);

  /// <summary>
  /// Mutable data access
  /// </summary>
  class BlockSparseMutator {
   public:
    BlockSparseMutator(Tensor& values, Tensor& indices) noexcept
        : values_(values), indices_(indices) {}
    Tensor& Values() noexcept { return values_; }
    Tensor& Indices() noexcept { return indices_; }

   private:
    std::reference_wrapper<Tensor> values_;
    std::reference_wrapper<Tensor> indices_;
  };

  /// <summary>
  /// Allocates memory for values and index and returns mutator so
  /// data can be populated
  /// </summary>
  /// <param name="values_shape">Shape is expected to be 3-D, use {0} for fully sparse tensors</param>
  /// <param name="indices_shape">Shape is expected to be 2-D, use {0} for fully sparse tensors </param>
  /// <returns></returns>
  BlockSparseMutator MakeBlockSparseData(const TensorShape& values_shape, const TensorShape& indices_shape);

  /// <summary>
  /// X-device copy. Destination tensor must have allocator set.
  /// </summary>
  /// <param name="data_transfer_manager"></param>
  /// <param name="exec_q_id"></param>
  /// <param name="dst_tensor"></param>
  /// <returns></returns>
  Status Copy(const DataTransferManager& data_transfer_manager, SparseTensor& dst_tensor) const;

  /// <summary>
  /// X-device copy. Destination tensor must have allocator set.
  /// </summary>
  /// <param name="dst_tensor"></param>
  /// <returns></returns>
  Status Copy(const IDataTransfer& data_transfer, SparseTensor& dst_tensor) const;

 private:
  Status AllocateBuffer(int64_t buffer_size, size_t num_values);
  void ReleaseBuffer();
  void* IndicesStart(int64_t values_bytes);
  const void* IndicesStart(int64_t values_bytes) const;
  Status ValidateBlockSparseShapes(const TensorShape& values_shape, const TensorShape& index_shape) const;

  std::vector<int64_t> GetCooIndexDims(size_t values_count, size_t index_size) const;
  void InitCooIndex(const TensorShape& index_shape, int64_t* index_data);

  Status ValidateCsrIndices(size_t values_count, size_t inner_size, size_t outer_size) const;
  void InitCsrIndices(size_t inner_size, const int64_t* inner, size_t outer_size, const int64_t* outer);
  void InitBlockSparseIndices(const TensorShape& indices_shape, int32_t* indices_data);

  SparseFormat format_;                        // sparse format enum value
  TensorShape dense_shape_;                    // a shape of a corresponding dense tensor
  const PrimitiveDataTypeBase* ml_data_type_;  // MLDataType for contained values
  AllocatorPtr allocator_;                     // Allocator or nullptr when using user supplied buffers
  OrtMemoryInfo location_;                     // Memory info where data resides. When allocator is supplied,
                                               // location_ is obtained from the allocator.
  void* p_data_;                               // Allocated buffer ptr, or nullptr when using user supplied buffers
  int64_t buffer_size_;                        // Allocated buffer size or zero when using user supplied buffers.
  Tensor values_;                              // Tensor instance that holds a values buffer information either user supplied or
                                               // to a beginning of p_data_, before format specific indices.
  std::vector<Tensor> format_data_;            // A collection of format specific indices. They contain pointers to either a
                                               // user supplied buffers or to portions of contiguous buffer p_data_.
};

}  // namespace onnxruntime

#endif