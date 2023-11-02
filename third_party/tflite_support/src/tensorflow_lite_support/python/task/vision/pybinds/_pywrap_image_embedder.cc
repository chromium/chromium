/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include <stdexcept>

#include "pybind11/pybind11.h"
#include "pybind11_protobuf/native_proto_caster.h"  // from @pybind11_protobuf
#include "tensorflow_lite_support/cc/task/processor/proto/bounding_box.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"
#include "tensorflow_lite_support/cc/task/vision/image_embedder.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {
namespace py = ::pybind11;
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
}  // namespace

PYBIND11_MODULE(_pywrap_image_embedder, m) {
  // python wrapper for C++ ImageEmbeder class which shouldn't be directly used
  // by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  py::class_<ImageEmbedder>(m, "ImageEmbedder")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options,
             const processor::EmbeddingOptions& embedding_options) {
            ImageEmbedderOptions options;
            if (base_options.has_file_content()) {
              options.mutable_model_file_with_metadata()->set_file_content(
                  base_options.file_content());
            }
            if (base_options.has_file_name()) {
              options.mutable_model_file_with_metadata()->set_file_name(
                  base_options.file_name());
            }

            options.set_num_threads(base_options.num_threads());
            if (base_options.use_coral()) {
              options.mutable_compute_settings()
                  ->mutable_tflite_settings()
                  ->set_delegate(tflite::proto::Delegate::EDGETPU_CORAL);
            }

            if (embedding_options.has_l2_normalize()) {
              options.set_l2_normalize(embedding_options.l2_normalize());
            }
            if (embedding_options.has_quantize()) {
              options.set_quantize(embedding_options.quantize());
            }
            auto embedder = ImageEmbedder::CreateFromOptions(options);
            return get_value(embedder);
          })
      .def("embed",
           [](ImageEmbedder& self,
              const ImageData& image_data) -> processor::EmbeddingResult {
             auto frame_buffer = CreateFrameBufferFromImageData(image_data);
             auto vision_embedding_result =
                 self.Embed(*core::get_value(frame_buffer));
             // Convert from vision::EmbeddingResult to
             // processor::EmbeddingResult
             processor::EmbeddingResult embedding_result;
             embedding_result.ParseFromString(
                 core::get_value(vision_embedding_result).SerializeAsString());
             return embedding_result;
           })
      .def("embed",
           [](ImageEmbedder& self, const ImageData& image_data,
              const processor::BoundingBox& bounding_box)
               -> processor::EmbeddingResult {
             // Convert from processor::BoundingBox to vision::BoundingBox as
             // the later is used in the C++ layer.
             BoundingBox vision_bounding_box;
             vision_bounding_box.ParseFromString(
                 bounding_box.SerializeAsString());

             auto frame_buffer = CreateFrameBufferFromImageData(image_data);
             auto vision_embedding_result = self.Embed(
                 *core::get_value(frame_buffer), vision_bounding_box);
             // Convert from vision::EmbeddingResult to
             // processor::EmbeddingResult as required by the Python layer.
             processor::EmbeddingResult embedding_result;
             embedding_result.ParseFromString(
                 core::get_value(vision_embedding_result).SerializeAsString());
             return embedding_result;
           })
      .def("get_embedding_by_index",
           [](ImageEmbedder& self,
              const processor::EmbeddingResult& embedding_result,
              const int index) -> processor::Embedding {
             // Convert from processor::EmbeddingResult to
             // vision::EmbeddingResult as the latter is used in the C++ API.
             EmbeddingResult vision_embedding_result;
             vision_embedding_result.ParseFromString(
                 embedding_result.SerializeAsString());

             Embedding vision_embedding{
                 self.GetEmbeddingByIndex(vision_embedding_result, index)};
             // Convert from vision::Embedding to processor::Embedding
             // as required by the Python layer.
             processor::Embedding embedding;
             embedding.ParseFromString(vision_embedding.SerializeAsString());
             return embedding;
           })
      .def("get_number_of_output_layers",
           &ImageEmbedder::GetNumberOfOutputLayers)
      .def("get_embedding_dimension", &ImageEmbedder::GetEmbeddingDimension)
      .def_static(
          "cosine_similarity",
          [](const processor::FeatureVector& u,
             const processor::FeatureVector& v) -> double {
            // Convert from processor::FeatureVector to
            // vision::FeatureVector as the latter is used in the C++
            // layer.
            FeatureVector vision_feature_vector_u;
            vision_feature_vector_u.ParseFromString(u.SerializeAsString());
            FeatureVector vision_feature_vector_v;
            vision_feature_vector_v.ParseFromString(v.SerializeAsString());
            auto similarity = ImageEmbedder::CosineSimilarity(
                vision_feature_vector_u, vision_feature_vector_v);
            return core::get_value(similarity);
          });
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
