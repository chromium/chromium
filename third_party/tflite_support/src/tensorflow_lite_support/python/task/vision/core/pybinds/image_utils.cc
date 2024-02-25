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
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

#include "pybind11/pybind11.h"
#include "pybind11_abseil/status_casters.h"  // from @pybind11_abseil
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {
namespace py = ::pybind11;

}  //  namespace

PYBIND11_MODULE(image_utils, m) {
  // python wrapper for ImageData class which shouldn't be directly used by
  // the users.
  pybind11::google::ImportStatusModule();

  py::class_<ImageData>(m, "ImageData", py::buffer_protocol())
      .def(py::init([](py::buffer buffer) {
        py::buffer_info info = buffer.request();

        if (info.ndim != 2 && info.ndim != 3) {
          throw py::value_error("Incompatible buffer dimension!");
        }

        int height = info.shape[0];
        int width = info.shape[1];
        int channels = info.ndim == 3 ? info.shape[2] : 1;

        return ImageData{static_cast<uint8 *>(info.ptr), width, height,
                         channels};
      }))
      .def_readonly("width", &ImageData::width)
      .def_readonly("height", &ImageData::height)
      .def_readonly("channels", &ImageData::channels)
      .def_buffer([](ImageData &data) -> py::buffer_info {
        return py::buffer_info(
            data.pixel_data, sizeof(uint8),
            py::format_descriptor<uint8>::format(), 3,
            {data.height, data.width, data.channels},
            {sizeof(uint8) * size_t(data.width) * size_t(data.channels),
             sizeof(uint8) * size_t(data.channels), sizeof(uint8)});
      });

  m.def("decode_image_from_file",
        [](const std::string &file_name) {
          auto image_data = DecodeImageFromFile(file_name);
          return core::get_value(image_data);
        })
      .def("decode_image_from_buffer",
           [](const char *buffer, int len) {
             auto image_data = DecodeImageFromBuffer(
                 reinterpret_cast<unsigned char const *>(buffer), len);
             return core::get_value(image_data);
           })
      .def("image_data_free", &ImageDataFree);
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
