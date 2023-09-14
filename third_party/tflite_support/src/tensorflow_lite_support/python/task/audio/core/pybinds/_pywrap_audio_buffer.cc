/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "tensorflow_lite_support/cc/task/audio/core/audio_buffer.h"
#include "tensorflow_lite_support/cc/task/audio/utils/audio_utils.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace audio {

namespace {
namespace py = ::pybind11;

}  //  namespace

PYBIND11_MODULE(_pywrap_audio_buffer, m) {
  // python wrapper for AudioBuffer class which shouldn't be directly used by
  // the users.

  py::class_<AudioBuffer::AudioFormat>(m, "AudioFormat")
      .def(py::init([](const int channels, const int sample_rate) {
        return AudioBuffer::AudioFormat{channels, sample_rate};
      }))
      .def_readonly("channels", &AudioBuffer::AudioFormat::channels)
      .def_readonly("sample_rate", &AudioBuffer::AudioFormat::sample_rate);

  py::class_<AudioBuffer>(m, "AudioBuffer", py::buffer_protocol())
      .def(py::init([](
            py::buffer buffer, const int sample_count,
            const AudioBuffer::AudioFormat& audio_format)
            -> std::unique_ptr<AudioBuffer> {
              py::buffer_info info = buffer.request();

              auto audio_buffer = AudioBuffer::Create(
                  static_cast<float*>(info.ptr), sample_count, audio_format);
              return core::get_value(audio_buffer);
          }))
      .def_property_readonly("audio_format", &AudioBuffer::GetAudioFormat)
      .def_property_readonly("buffer_size", &AudioBuffer::GetBufferSize)
      .def_property_readonly("float_buffer", [](AudioBuffer& self) {
        py::object py_object =
            py::cast(self, py::return_value_policy::reference);

        return py::array_t<float, py::array::c_style>(
            {self.GetBufferSize(), self.GetAudioFormat().channels},
            reinterpret_cast<const float*>(self.GetFloatBuffer()), py_object);
      });

  m.def("LoadAudioBufferFromFile",
        [](const std::string& wav_file, uint32_t* buffer_size, uint32_t* offset,
           py::buffer buffer) -> AudioBuffer {
          py::buffer_info info = buffer.request();

          auto audio_buffer = LoadAudioBufferFromFile(
              wav_file, buffer_size, offset,
              static_cast<std::vector<float>*>(info.ptr));
          return core::get_value(audio_buffer);
        });
}

}  // namespace audio
}  // namespace task
}  // namespace tflite
