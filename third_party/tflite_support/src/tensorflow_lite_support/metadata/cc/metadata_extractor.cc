/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"

#include <functional>

#include "absl/memory/memory.h"       // from @com_google_absl
#include "absl/status/status.h"       // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "lib/zip.h"                  // from @org_libzip
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace metadata {

namespace {
constexpr char kMetadataBufferName[] = "TFLITE_METADATA";

using ::absl::StatusCode;
using ::flatbuffers::Offset;
using ::flatbuffers::Vector;
using ::tflite::TensorMetadata;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::TfLiteSupportStatus;

// Helper class that takes a callback function, and invokes it in its
// destructor.
class SimpleCleanUp {
 public:
  explicit SimpleCleanUp(std::function<void()> callback)
      : callback_(std::move(callback)) {}

  ~SimpleCleanUp() {
    if (callback_ != nullptr)
      callback_();
  }

  // Use `std::move(simple_cleanup).Cancel()` to prevent the callback from ever
  // executing at all. Once a SimpleCleanUp object has been `std::move(...)`-ed,
  // it may not be read from again.
  void Cancel() && { callback_ = nullptr; }

 private:
  std::function<void()> callback_;
};

// Util to get item from src_vector specified by index.
template <typename T>
const T* GetItemFromVector(
    const flatbuffers::Vector<flatbuffers::Offset<T>>* src_vector,
    int index) {
  if (src_vector == nullptr || index < 0 || index >= src_vector->size()) {
    return nullptr;
  }
  return src_vector->Get(index);
}
}  // namespace

/* static */
tflite::support::StatusOr<std::unique_ptr<ModelMetadataExtractor>>
ModelMetadataExtractor::CreateFromModelBuffer(const char* buffer_data,
                                              size_t buffer_size) {
  // Use absl::WrapUnique() to call private constructor:
  // https://abseil.io/tips/126.
  std::unique_ptr<ModelMetadataExtractor> extractor =
      absl::WrapUnique(new ModelMetadataExtractor());
  RETURN_IF_ERROR(extractor->InitFromModelBuffer(buffer_data, buffer_size));
  return extractor;
}

/* static */
tflite::support::StatusOr<const tflite::ProcessUnit*>
ModelMetadataExtractor::FindFirstProcessUnit(
    const tflite::TensorMetadata& tensor_metadata,
    tflite::ProcessUnitOptions type) {
  const tflite::ProcessUnit* result = nullptr;
  if (tensor_metadata.process_units() == nullptr) {
    return result;
  }
  for (const auto process_unit : *tensor_metadata.process_units()) {
    if (process_unit->options_type() == type) {
      if (result != nullptr) {
        return CreateStatusWithPayload(
            StatusCode::kInvalidArgument,
            absl::StrCat("Found multiple ProcessUnits with type=",
                         tflite::EnumNameProcessUnitOptions(type),
                         ", expected at most one."),
            TfLiteSupportStatus::kMetadataInvalidProcessUnitsError);
      }
      result = process_unit;
    }
  }
  return result;
}

/* static */
std::string ModelMetadataExtractor::FindFirstAssociatedFileName(
    const tflite::TensorMetadata& tensor_metadata,
    tflite::AssociatedFileType type,
    absl::string_view locale) {
  if (tensor_metadata.associated_files() == nullptr) {
    return std::string();
  }
  for (const auto associated_file : *tensor_metadata.associated_files()) {
    if (associated_file->type() != type || associated_file->name() == nullptr) {
      continue;
    }
    if (locale.empty() || (associated_file->locale() != nullptr &&
                           locale == associated_file->locale()->str())) {
      return associated_file->name()->str();
    }
  }
  return std::string();
}

absl::Status ModelMetadataExtractor::InitFromModelBuffer(
    const char* buffer_data,
    size_t buffer_size) {
  // Rely on the simplest, base flatbuffers verifier. Here is not the place to
  // e.g. use an OpResolver: we just want to make sure the buffer is valid to
  // access the metadata.
  flatbuffers::Verifier verifier = flatbuffers::Verifier(
      reinterpret_cast<const uint8_t*>(buffer_data), buffer_size);
  if (!tflite::VerifyModelBuffer(verifier)) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "The model is not a valid FlatBuffer buffer.",
        TfLiteSupportStatus::kInvalidFlatBufferError);
  }
  model_ = tflite::GetModel(buffer_data);
  if (model_->metadata() == nullptr) {
    // Not all models have metadata, which is OK. `GetModelMetadata()` then
    // returns nullptr.
    return absl::OkStatus();
  }
  // Look for the "TFLITE_METADATA" field, if any.
  for (int i = 0; i < model_->metadata()->size(); ++i) {
    const auto metadata = model_->metadata()->Get(i);
    if (!metadata->name()) {
      continue;
    }
    if (metadata->name()->str() != kMetadataBufferName) {
      continue;
    }
    const auto buffer_index = metadata->buffer();
    const auto metadata_buffer =
        model_->buffers()->Get(buffer_index)->data()->data();
    if (!tflite::ModelMetadataBufferHasIdentifier(metadata_buffer)) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat(
              "Invalid metadata schema version: expected %s, got %s",
              absl::string_view(tflite::ModelMetadataIdentifier())
                  .substr(
                      0, flatbuffers::FlatBufferBuilder::kFileIdentifierLength),
              // Returned identifier is not null terminated; has to be
              // truncated.
              absl::string_view(
                  flatbuffers::GetBufferIdentifier(metadata_buffer))
                  .substr(
                      0,
                      flatbuffers::FlatBufferBuilder::kFileIdentifierLength)),
          TfLiteSupportStatus::kMetadataInvalidSchemaVersionError);
    }
    model_metadata_ = tflite::GetModelMetadata(metadata_buffer);
    if (model_metadata_ == nullptr) {
      return CreateStatusWithPayload(StatusCode::kInternal,
                                     "Expected Model Metadata not to be null.");
    }
    return ExtractAssociatedFiles(buffer_data, buffer_size);
    break;
  }
  return absl::OkStatus();
}

absl::Status ModelMetadataExtractor::ExtractAssociatedFiles(
    const char* buffer_data,
    size_t buffer_size) {
  // Setup libzip error reporting.
  zip_error_t error;
  zip_error_init(&error);
  auto zip_error_cleanup = SimpleCleanUp([&error] { zip_error_fini(&error); });

  // Initialize zip source.
  zip_source_t* src =
      zip_source_buffer_create(buffer_data, buffer_size, /*freep=*/0, &error);
  if (src == nullptr) {
    return CreateStatusWithPayload(
        StatusCode::kUnknown,
        absl::StrFormat("Can't create zip source from model buffer: %s",
                        zip_error_strerror(&error)),
        TfLiteSupportStatus::kMetadataAssociatedFileZipError);
  }
  auto zip_source_cleanup = SimpleCleanUp([src] { zip_source_free(src); });

  // Try opening zip source.
  zip* zip_archive = zip_open_from_source(src, /*flags=*/0, &error);
  if (zip_archive == nullptr) {
    // It's OK if it fails: this means there are no associated files with this
    // model.
    return absl::OkStatus();
  }
  auto zip_archive_cleanup =
      SimpleCleanUp([zip_archive] { zip_close(zip_archive); });
  // As per the documentation [1] for zip_source_free, it should not be called
  // after a successful call to zip_open_from_source.
  //
  // [1]: https://libzip.org/documentation/zip_source_free.html
  std::move(zip_source_cleanup).Cancel();

  const int num_files = zip_get_num_entries(zip_archive, /*flags=*/0);
  for (int index = 0; index < num_files; ++index) {
    // Get file stats.
    struct zip_stat zip_file_stat;
    zip_stat_init(&zip_file_stat);
    zip_stat_index(zip_archive, index, /*flags=*/0, &zip_file_stat);
    absl::string_view filename = zip_file_stat.name;
    const auto unzip_filesize = zip_file_stat.size;

    // Open file.
    zip_file* zip_file = zip_fopen_index(zip_archive, index, /*flags=*/0);
    if (zip_file == nullptr) {
      return CreateStatusWithPayload(
          StatusCode::kUnknown,
          absl::StrFormat("Unable to open associated file with name: %s",
                          zip_file_stat.name),
          TfLiteSupportStatus::kMetadataAssociatedFileZipError);
    }
    auto zip_file_cleanup = SimpleCleanUp([zip_file] { zip_fclose(zip_file); });

    // Unzip file.
    char* unzip_buffer = new char[unzip_filesize];
    auto unzip_buffer_cleanup =
        SimpleCleanUp([unzip_buffer] { delete[] unzip_buffer; });
    if (zip_fread(zip_file, unzip_buffer, unzip_filesize) != unzip_filesize) {
      return CreateStatusWithPayload(
          StatusCode::kUnknown,
          absl::StrFormat("Unzipping failed for file: %s.", filename),
          TfLiteSupportStatus::kMetadataAssociatedFileZipError);
    }

    // Copy file contents in map.
    associated_files_[filename] = std::string(unzip_buffer, unzip_filesize);
  }
  return absl::OkStatus();
}

tflite::support::StatusOr<absl::string_view>
ModelMetadataExtractor::GetAssociatedFile(const std::string& filename) const {
  auto it = associated_files_.find(filename);
  if (it == associated_files_.end()) {
    return CreateStatusWithPayload(
        StatusCode::kNotFound,
        absl::StrFormat("No associated file with name: %s", filename),
        TfLiteSupportStatus::kMetadataAssociatedFileNotFoundError);
  }
  return it->second;
}

const flatbuffers::Vector<flatbuffers::Offset<tflite::TensorMetadata>>*
ModelMetadataExtractor::GetInputTensorMetadata() const {
  if (model_metadata_ == nullptr ||
      model_metadata_->subgraph_metadata() == nullptr) {
    return nullptr;
  }
  return model_metadata_->subgraph_metadata()
      ->Get(kDefaultSubgraphIndex)
      ->input_tensor_metadata();
}

const tflite::TensorMetadata* ModelMetadataExtractor::GetInputTensorMetadata(
    int index) const {
  return GetItemFromVector<tflite::TensorMetadata>(GetInputTensorMetadata(),
                                                   index);
}

int ModelMetadataExtractor::GetInputTensorCount() const {
  const flatbuffers::Vector<flatbuffers::Offset<tflite::TensorMetadata>>*
      input_tensor_metadata = GetInputTensorMetadata();
  return input_tensor_metadata == nullptr ? 0 : input_tensor_metadata->size();
}

const Vector<Offset<TensorMetadata>>*
ModelMetadataExtractor::GetOutputTensorMetadata() const {
  if (model_metadata_ == nullptr ||
      model_metadata_->subgraph_metadata() == nullptr) {
    return nullptr;
  }
  return model_metadata_->subgraph_metadata()
      ->Get(kDefaultSubgraphIndex)
      ->output_tensor_metadata();
}

const tflite::TensorMetadata* ModelMetadataExtractor::GetOutputTensorMetadata(
    int index) const {
  return GetItemFromVector<tflite::TensorMetadata>(GetOutputTensorMetadata(),
                                                   index);
}

int ModelMetadataExtractor::GetOutputTensorCount() const {
  const flatbuffers::Vector<flatbuffers::Offset<tflite::TensorMetadata>>*
      output_tensor_metadata = GetOutputTensorMetadata();
  return output_tensor_metadata == nullptr ? 0 : output_tensor_metadata->size();
}

const Vector<flatbuffers::Offset<tflite::ProcessUnit>>*
ModelMetadataExtractor::GetInputProcessUnits() const {
  if (model_metadata_ == nullptr ||
      model_metadata_->subgraph_metadata() == nullptr) {
    return nullptr;
  }
  return model_metadata_->subgraph_metadata()
      ->Get(kDefaultSubgraphIndex)
      ->input_process_units();
}

const tflite::ProcessUnit* ModelMetadataExtractor::GetInputProcessUnit(
    int index) const {
  return GetItemFromVector<tflite::ProcessUnit>(GetInputProcessUnits(), index);
}

int ModelMetadataExtractor::GetInputProcessUnitsCount() const {
  const Vector<flatbuffers::Offset<tflite::ProcessUnit>>* input_process_units =
      GetInputProcessUnits();
  return input_process_units == nullptr ? 0 : input_process_units->size();
}

const Vector<flatbuffers::Offset<tflite::ProcessUnit>>*
ModelMetadataExtractor::GetOutputProcessUnits() const {
  if (model_metadata_ == nullptr ||
      model_metadata_->subgraph_metadata() == nullptr) {
    return nullptr;
  }
  return model_metadata_->subgraph_metadata()
      ->Get(kDefaultSubgraphIndex)
      ->output_process_units();
}

const tflite::ProcessUnit* ModelMetadataExtractor::GetOutputProcessUnit(
    int index) const {
  return GetItemFromVector<tflite::ProcessUnit>(GetOutputProcessUnits(), index);
}

int ModelMetadataExtractor::GetOutputProcessUnitsCount() const {
  const Vector<flatbuffers::Offset<tflite::ProcessUnit>>* output_process_units =
      GetOutputProcessUnits();
  return output_process_units == nullptr ? 0 : output_process_units->size();
}

}  // namespace metadata
}  // namespace tflite
