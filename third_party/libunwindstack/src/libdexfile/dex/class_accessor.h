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

#ifndef ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_H_
#define ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_H_

#include "code_item_accessors.h"
#include "dex/dex_file.h"
#include "dex_file_types.h"
#include "invoke_type.h"
#include "modifiers.h"

namespace art {

namespace dex {
struct ClassDef;
struct CodeItem;
class DexFileVerifier;
}  // namespace dex

class ClassIteratorData;
class DexFile;
template <typename Iter> class IterationRange;
class MethodReference;

// Classes to access Dex data.
class ClassAccessor {
 public:
  class BaseItem {
   public:
    explicit BaseItem(const DexFile& dex_file,
                      const uint8_t* ptr_pos,
                      const uint8_t* hiddenapi_ptr_pos)
        : dex_file_(dex_file), ptr_pos_(ptr_pos), hiddenapi_ptr_pos_(hiddenapi_ptr_pos) {}

    uint32_t GetIndex() const {
      return index_;
    }

    uint32_t GetAccessFlags() const {
      return access_flags_;
    }

    uint32_t GetHiddenapiFlags() const {
      return hiddenapi_flags_;
    }

    bool IsFinal() const {
      return (GetAccessFlags() & kAccFinal) != 0;
    }

    const DexFile& GetDexFile() const {
      return dex_file_;
    }

    const uint8_t* GetDataPointer() const {
      return ptr_pos_;
    }

    bool MemberIsNative() const {
      return GetAccessFlags() & kAccNative;
    }

    bool MemberIsFinal() const {
      return GetAccessFlags() & kAccFinal;
    }

   protected:
    // Internal data pointer for reading.
    const DexFile& dex_file_;
    const uint8_t* ptr_pos_ = nullptr;
    const uint8_t* hiddenapi_ptr_pos_ = nullptr;
    uint32_t index_ = 0u;
    uint32_t access_flags_ = 0u;
    uint32_t hiddenapi_flags_ = 0u;
  };

  // A decoded version of the method of a class_data_item.
  class Method : public BaseItem {
   public:
    uint32_t GetCodeItemOffset() const {
      return code_off_;
    }

    InvokeType GetInvokeType(uint32_t class_access_flags) const {
      return is_static_or_direct_
          ? GetDirectMethodInvokeType()
          : GetVirtualMethodInvokeType(class_access_flags);
    }

    MethodReference GetReference() const;

    CodeItemInstructionAccessor GetInstructions() const;
    CodeItemDataAccessor GetInstructionsAndData() const;

    const dex::CodeItem* GetCodeItem() const;

    bool IsStaticOrDirect() const {
      return is_static_or_direct_;
    }

   private:
    Method(const DexFile& dex_file,
           const uint8_t* ptr_pos,
           const uint8_t* hiddenapi_ptr_pos = nullptr,
           bool is_static_or_direct = true)
        : BaseItem(dex_file, ptr_pos, hiddenapi_ptr_pos),
          is_static_or_direct_(is_static_or_direct) {}

    void Read();

    InvokeType GetDirectMethodInvokeType() const {
      return (GetAccessFlags() & kAccStatic) != 0 ? kStatic : kDirect;
    }

    InvokeType GetVirtualMethodInvokeType(uint32_t class_access_flags) const {
      DCHECK_EQ(GetAccessFlags() & kAccStatic, 0U);
      if ((class_access_flags & kAccInterface) != 0) {
        return kInterface;
      } else if ((GetAccessFlags() & kAccConstructor) != 0) {
        return kSuper;
      } else {
        return kVirtual;
      }
    }

    // Move to virtual method section.
    void NextSection() {
      DCHECK(is_static_or_direct_) << "Already in the virtual methods section";
      is_static_or_direct_ = false;
      index_ = 0u;
    }

    bool is_static_or_direct_ = true;
    uint32_t code_off_ = 0u;

    friend class ClassAccessor;
    friend class dex::DexFileVerifier;
  };

  // A decoded version of the field of a class_data_item.
  class Field : public BaseItem {
   public:
    Field(const DexFile& dex_file,
          const uint8_t* ptr_pos,
          const uint8_t* hiddenapi_ptr_pos = nullptr)
        : BaseItem(dex_file, ptr_pos, hiddenapi_ptr_pos) {}

    bool IsStatic() const {
     return is_static_;
    }

   private:
    void Read();

    // Move to instance fields section.
    void NextSection() {
      index_ = 0u;
      is_static_ = false;
    }

    bool is_static_ = true;
    friend class ClassAccessor;
    friend class dex::DexFileVerifier;
  };

  template <typename DataType>
  class DataIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = DataType;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    DataIterator(const DexFile& dex_file,
                 uint32_t position,
                 uint32_t partition_pos,
                 uint32_t iterator_end,
                 const uint8_t* ptr_pos,
                 const uint8_t* hiddenapi_ptr_pos)
        : data_(dex_file, ptr_pos, hiddenapi_ptr_pos),
          position_(position),
          partition_pos_(partition_pos),
          iterator_end_(iterator_end) {
      ReadData();
    }

    bool IsValid() const {
      return position_ < iterator_end_;
    }

    // Value after modification.
    DataIterator& operator++() {
      ++position_;
      ReadData();
      return *this;
    }

    const value_type& operator*() const {
      return data_;
    }

    const value_type* operator->() const {
      return &data_;
    }

    bool operator==(const DataIterator& rhs) const {
      DCHECK_EQ(&data_.dex_file_, &rhs.data_.dex_file_) << "Comparing different dex files.";
      return position_ == rhs.position_;
    }

    bool operator!=(const DataIterator& rhs) const {
      return !(*this == rhs);
    }

    bool operator<(const DataIterator& rhs) const {
      DCHECK_EQ(&data_.dex_file_, &rhs.data_.dex_file_) << "Comparing different dex files.";
      return position_ < rhs.position_;
    }

    bool operator>(const DataIterator& rhs) const {
      return rhs < *this;
    }

    bool operator<=(const DataIterator& rhs) const {
      return !(rhs < *this);
    }

    bool operator>=(const DataIterator& rhs) const {
      return !(*this < rhs);
    }

    const uint8_t* GetDataPointer() const {
      return data_.ptr_pos_;
    }

   private:
    // Read data at current position.
    void ReadData() {
      if (IsValid()) {
        // At the end of the first section, go to the next section.
        if (position_ == partition_pos_) {
          data_.NextSection();
        }
        data_.Read();
      }
    }

    DataType data_;
    // Iterator position.
    uint32_t position_;
    // At partition_pos_, we go to the next section.
    const uint32_t partition_pos_;
    // At iterator_end_, the iterator is no longer valid.
    const uint32_t iterator_end_;

    friend class dex::DexFileVerifier;
  };

  // Not explicit specifically for range-based loops.
  ALWAYS_INLINE ClassAccessor(const ClassIteratorData& data);  // NOLINT [runtime/explicit] [5]

  ALWAYS_INLINE ClassAccessor(const DexFile& dex_file,
                              const dex::ClassDef& class_def,
                              bool parse_hiddenapi_class_data = false);

  ALWAYS_INLINE ClassAccessor(const DexFile& dex_file, uint32_t class_def_index);

  ClassAccessor(const DexFile& dex_file,
                const uint8_t* class_data,
                uint32_t class_def_index = DexFile::kDexNoIndex32,
                bool parse_hiddenapi_class_data = false);

  // Return the code item for a method.
  const dex::CodeItem* GetCodeItem(const Method& method) const;

  // Iterator data is not very iterator friendly, use visitors to get around this.
  template <typename StaticFieldVisitor,
            typename InstanceFieldVisitor,
            typename DirectMethodVisitor,
            typename VirtualMethodVisitor>
  void VisitFieldsAndMethods(const StaticFieldVisitor& static_field_visitor,
                             const InstanceFieldVisitor& instance_field_visitor,
                             const DirectMethodVisitor& direct_method_visitor,
                             const VirtualMethodVisitor& virtual_method_visitor) const;

  template <typename DirectMethodVisitor,
            typename VirtualMethodVisitor>
  void VisitMethods(const DirectMethodVisitor& direct_method_visitor,
                    const VirtualMethodVisitor& virtual_method_visitor) const;

  template <typename StaticFieldVisitor,
            typename InstanceFieldVisitor>
  void VisitFields(const StaticFieldVisitor& static_field_visitor,
                   const InstanceFieldVisitor& instance_field_visitor) const;

  // Return the iteration range for all the fields.
  IterationRange<DataIterator<Field>> GetFields() const;

  // Return the iteration range for all the static fields.
  IterationRange<DataIterator<Field>> GetStaticFields() const;

  // Return the iteration range for all the instance fields.
  IterationRange<DataIterator<Field>> GetInstanceFields() const;

  // Return the iteration range for all the methods.
  IterationRange<DataIterator<Method>> GetMethods() const;

  // Return the iteration range for the direct methods.
  IterationRange<DataIterator<Method>> GetDirectMethods() const;

  // Return the iteration range for the virtual methods.
  IterationRange<DataIterator<Method>> GetVirtualMethods() const;

  uint32_t NumStaticFields() const {
    return num_static_fields_;
  }

  uint32_t NumInstanceFields() const {
    return num_instance_fields_;
  }

  uint32_t NumFields() const {
    return NumStaticFields() + NumInstanceFields();
  }

  uint32_t NumDirectMethods() const {
    return num_direct_methods_;
  }

  uint32_t NumVirtualMethods() const {
    return num_virtual_methods_;
  }

  uint32_t NumMethods() const {
    return NumDirectMethods() + NumVirtualMethods();
  }

  const char* GetDescriptor() const;

  dex::TypeIndex GetClassIdx() const;

  const DexFile& GetDexFile() const {
    return dex_file_;
  }

  bool HasClassData() const {
    return ptr_pos_ != nullptr;
  }

  bool HasHiddenapiClassData() const {
    return hiddenapi_ptr_pos_ != nullptr;
  }

  uint32_t GetClassDefIndex() const {
    return class_def_index_;
  }

  const dex::ClassDef& GetClassDef() const;

 protected:
  // Template visitor to reduce copy paste for visiting elements.
  // No thread safety analysis since the visitor may require capabilities.
  template <typename DataType, typename Visitor>
  void VisitMembers(size_t count, const Visitor& visitor, DataType* data) const
      NO_THREAD_SAFETY_ANALYSIS;

  // Return an iteration range for the first <count> fields.
  IterationRange<DataIterator<Field>> GetFieldsInternal(size_t count) const;

  // Return an iteration range for the first <count> methods.
  IterationRange<DataIterator<Method>> GetMethodsInternal(size_t count) const;

  const DexFile& dex_file_;
  const uint32_t class_def_index_;
  const uint8_t* ptr_pos_ = nullptr;  // Pointer into stream of class_data_item.
  const uint8_t* hiddenapi_ptr_pos_ = nullptr;  // Pointer into stream of hiddenapi_metadata.
  const uint32_t num_static_fields_ = 0u;
  const uint32_t num_instance_fields_ = 0u;
  const uint32_t num_direct_methods_ = 0u;
  const uint32_t num_virtual_methods_ = 0u;

  friend class dex::DexFileVerifier;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_H_
