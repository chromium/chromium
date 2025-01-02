// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <map>

#include "core/common/common.h"
#include "core/framework/allocator_stats.h"
// some enums are defined in session/onnxruntime_c_api.h but used in ortdevice.h/ortmemory.h
#include "core/session/onnxruntime_c_api.h"
#include "core/framework/ortdevice.h"
#include "core/framework/ortmemoryinfo.h"

// This configures the arena based allocator used by ORT
// See docs/C_API.md for details on what these mean and how to choose these values
struct OrtArenaCfg {
  OrtArenaCfg() : max_mem(0),
                  arena_extend_strategy(-1),
                  initial_chunk_size_bytes(-1),
                  max_dead_bytes_per_chunk(-1),
                  initial_growth_chunk_size_bytes(-1),
                  max_power_of_two_extend_bytes(-1) {}
  OrtArenaCfg(size_t max_mem, int arena_extend_strategy, int initial_chunk_size_bytes,
              int max_dead_bytes_per_chunk, int initial_growth_chunk_size_bytes,
              int64_t max_power_of_two_extend_bytes)
      : max_mem(max_mem),
        arena_extend_strategy(arena_extend_strategy),
        initial_chunk_size_bytes(initial_chunk_size_bytes),
        max_dead_bytes_per_chunk(max_dead_bytes_per_chunk),
        initial_growth_chunk_size_bytes(initial_growth_chunk_size_bytes),
        max_power_of_two_extend_bytes(max_power_of_two_extend_bytes) {}

  size_t max_mem;                         // use 0 to allow ORT to choose the default
  int arena_extend_strategy;              // use -1 to allow ORT to choose the default, 0 = kNextPowerOfTwo, 1 = kSameAsRequested
  int initial_chunk_size_bytes;           // use -1 to allow ORT to choose the default
  int max_dead_bytes_per_chunk;           // use -1 to allow ORT to choose the default
  int initial_growth_chunk_size_bytes;    // use -1 to allow ORT to choose the default
  int64_t max_power_of_two_extend_bytes;  // use -1 to allow ORT to choose the default
};

namespace onnxruntime {
constexpr const char* CPU = "Cpu";
constexpr const char* CUDA = "Cuda";
constexpr const char* CUDA_PINNED = "CudaPinned";
constexpr const char* CANN = "Cann";
constexpr const char* CANN_PINNED = "CannPinned";
constexpr const char* DML = "DML";
constexpr const char* HIP = "Hip";
constexpr const char* HIP_PINNED = "HipPinned";
constexpr const char* OpenVINO_CPU = "OpenVINO_CPU";
constexpr const char* OpenVINO_GPU = "OpenVINO_GPU";
constexpr const char* OpenVINO_RT = "OpenVINO_RT";
constexpr const char* OpenVINO_RT_NPU = "OpenVINO_RT_NPU";
constexpr const char* WEBGPU_BUFFER = "WebGPU_Buffer";
constexpr const char* WEBNN_TENSOR = "WebNN_Tensor";

constexpr size_t kAllocAlignment = 256;

class IAllocator;
class Stream;
namespace synchronize {
class Notification;
}
using WaitNotificationFn = std::function<void(Stream&, synchronize::Notification&)>;
void* AllocateBufferWithOptions(IAllocator& allocator, size_t size, bool use_reserve, Stream* stream, WaitNotificationFn wait_fn);

template <typename T>
using IAllocatorUniquePtr = std::unique_ptr<T, std::function<void(T*)>>;

class IAllocator {
 public:
  IAllocator(const OrtMemoryInfo& info) : memory_info_(info) {}
  virtual ~IAllocator() = default;
  /**
   * Allocate memory of the specified size.
   * If size is 0, nullptr is returned.
   * If allocation fails, an exception is thrown.
   *
   * @remarks Use SafeInt when calculating the size of memory to allocate using Alloc.
   */
  virtual void* Alloc(size_t size) = 0;

  virtual void Free(void* p) = 0;

  // Reserve() is an interface exposed for an implementation of IAllocator
  // to optionally implement some allocation logic that by-passes any arena-based
  // logic that may be housed in the Alloc() implementation.
  // There are SessionOptions config(s) that allow users to allocate some memory
  // by-passing arena-based logic.
  // By default, the base implementation  just calls Alloc().
  virtual void* Reserve(size_t size) { return Alloc(size); }

  const OrtMemoryInfo& Info() const { return memory_info_; };

  // Each implementation of IAllocator can override and provide their own implementation
  virtual void GetStats(AllocatorStats* /*stats*/) { return; }

  static bool CalcMemSizeForArray(size_t nmemb, size_t size, size_t* out) noexcept {
    return CalcMemSizeForArrayWithAlignment(nmemb, size, 0, out);
  }

  /**
   * Calculate the memory size for an array. The size is bounds checked using SafeInt.
   * \tparam alignment must be power of 2
   * \param nmemb Number of members or elements in the array
   * \param size Size of each element
   * \param out Total size required after any alignment is applied
   * \return true, successful. false, overflow
   */
  [[nodiscard]] static bool CalcMemSizeForArrayWithAlignment(size_t nmemb, size_t size, size_t alignment,
                                                             size_t* out) noexcept;

  /**
   * https://cwe.mitre.org/data/definitions/190.html
   * \param alignment must be power of 2
   * \param nmemb Number of members or elements in the array
   * \param size Size of each element
   * \param out Total size required after any alignment is applied
   * \return true, successful. false, overflow
   * \remarks This was the original API and was implemented in the header. Replaced with the above version
   *          implemented in the .cc file so that the SafeInt dependency is internal.
   */
  template <size_t alignment>
  [[nodiscard]] static bool CalcMemSizeForArrayWithAlignment(size_t nmemb, size_t size, size_t* out) noexcept;

  /**
   * allocate memory for an array which has nmemb items of data, each size bytes long
   */
  void* AllocArray(size_t nmemb, size_t size) {
    size_t len;
    if (!CalcMemSizeForArray(nmemb, size, &len)) {
      ORT_THROW("Invalid size requested for allocation: ", nmemb, " * ", size);
    }

    return Alloc(len);
  }

  /**
   * allocate memory for an array which has nmemb items of data, each size bytes long
   */
  template <size_t alignment>
  void* AllocArrayWithAlignment(size_t nmemb, size_t size) {
    size_t len;
    if (!CalcMemSizeForArrayWithAlignment(nmemb, size, alignment, &len)) {
      ORT_THROW("Invalid size requested for allocation: ", nmemb, " * ", size, " with alignment ", alignment);
    }

    return Alloc(len);
  }

  /**
     Create a std::unique_ptr that is allocated and freed by the provided IAllocator.
     @param allocator The allocator.
     @param count_or_bytes The exact bytes to allocate if T is void, otherwise the number of elements to allocate.
     @param use_reserve If true, call Reserve() instead of Alloc() to allocate memory.
     @param stream Which stream instance allocated chunk will be used with.
     @param wait_fn If the allocator want to dynamic reuse a chunk from another stream, use this wait_fn to sync on
                    the target stream to make the reuse safe.
     @returns std::unique_ptr with allocated memory and deleter. Throws if it cannot allocate memory.
  */
  template <typename T>
  static IAllocatorUniquePtr<T> MakeUniquePtr(std::shared_ptr<IAllocator> allocator, size_t count_or_bytes,
                                              bool use_reserve = false,
                                              Stream* stream = nullptr, WaitNotificationFn wait_fn = nullptr) {
    ValidateAllocator(allocator);

    // for now limit to fundamental types. we could support others, but to do so either we or the caller
    // needs to call the dtor for the objects, for buffers allocated on device we don't have destructor
    // static_assert(std::is_fundamental<T>::value, "Fundamental type required as no destructors are called.");

    size_t alloc_size = count_or_bytes;

    // if T is not void, 'count_or_bytes' == number of items so allow for that
    if constexpr (!std::is_void<T>::value) {
      // sizeof(void) isn't valid, but the compiler isn't smart enough to ignore that this line isn't
      // reachable if T is void. use std::conditional to 'use' void* in the sizeof call
      constexpr auto size = sizeof(typename std::conditional<std::is_void<T>::value, void*, T>::type);
      alloc_size = ValidatedCalcMemSizeForArray(count_or_bytes, size);
    }

    // allocate
    T* p = static_cast<T*>(AllocateBufferWithOptions(*allocator, alloc_size, use_reserve, stream, std::move(wait_fn)));
    ValidateAllocation(p, alloc_size);

    return IAllocatorUniquePtr<T>{p,
                                  [allocator = std::move(allocator)](T* p) {
                                    allocator->Free(p);
                                  }};
  }

  /**
     Create a std::unique_ptr that is allocated and freed by the provided OrtAllocator.
     @param ort_allocator The allocator.
     @param count_or_bytes The exact bytes to allocate if T is void, otherwise the number of elements to allocate.
     @returns std::unique_ptr with allocated memory and deleter. Throws if it cannot allocate memory.
  */
  template <typename T>
  static IAllocatorUniquePtr<T> MakeUniquePtrFromOrtAllocator(OrtAllocator* ort_allocator, size_t count_or_bytes) {
    ValidateAllocator(ort_allocator);

    size_t alloc_size = count_or_bytes;
    // if T is not void, 'count_or_bytes' == number of items so allow for that
    if constexpr (!std::is_void<T>::value) {
      // sizeof(void) isn't valid, but the compiler isn't smart enough to ignore that this line isn't
      // reachable if T is void. use std::conditional to 'use' void* in the sizeof call
      constexpr auto size = sizeof(typename std::conditional<std::is_void<T>::value, void*, T>::type);
      alloc_size = ValidatedCalcMemSizeForArray(count_or_bytes, size);
    }

    T* p = static_cast<T*>(ort_allocator->Alloc(ort_allocator, alloc_size));
    ValidateAllocation(p, alloc_size);

    return IAllocatorUniquePtr<T>{p,
                                  [ort_allocator](T* p) {
                                    ort_allocator->Free(ort_allocator, p);
                                  }};
  }

 private:
  //
  // validation functions. split out from methods that are templatized on the data type to minimize binary size.
  //

  template <typename T>
  static void ValidateAllocator(const T& allocator) {
    ORT_ENFORCE(allocator != nullptr);
  }

  static size_t ValidatedCalcMemSizeForArray(size_t count, size_t size) {
    size_t alloc_size = 0;
    if (!CalcMemSizeForArray(count, size, &alloc_size)) {
      ORT_THROW("Invalid size requested for allocation: ", count, " * ", size);
    }

    return alloc_size;
  }

  static void ValidateAllocation(void* p, size_t size) {
    // allocator should throw directly but in case it didn't ensure we do here so that calling code doesn't
    // need to check for nullptr when an actual allocation was expected.
    ORT_ENFORCE(p != nullptr || size == 0, "Memory allocation failed. Size=", size);
  };

  OrtMemoryInfo memory_info_;
};

template <size_t alignment>
bool IAllocator::CalcMemSizeForArrayWithAlignment(size_t nmemb, size_t size, size_t* out) noexcept {
  return CalcMemSizeForArrayWithAlignment(nmemb, size, alignment, out);
}

class CPUAllocator : public IAllocator {
 public:
  explicit CPUAllocator(const OrtMemoryInfo& memory_info) : IAllocator(memory_info) {}

  CPUAllocator() : IAllocator(OrtMemoryInfo(CPU, OrtAllocatorType::OrtDeviceAllocator)) {}

  void* Alloc(size_t size) override;
  void Free(void* p) override;
};

using AllocatorPtr = std::shared_ptr<IAllocator>;
using AllocatorMap = std::map<OrtDevice, AllocatorPtr>;

void* AllocatorDefaultAlloc(size_t size);
void AllocatorDefaultFree(void* p);
}  // namespace onnxruntime
