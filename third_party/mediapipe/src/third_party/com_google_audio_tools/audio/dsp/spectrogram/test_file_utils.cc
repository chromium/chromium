/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/spectrogram/test_file_utils.h"

#include <math.h>
#include <stddef.h>

#include <complex>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "glog/logging.h"

namespace audio_dsp {

bool ReadRawDoubleFileToComplexVector(
    const std::string& file_name, int row_length,
    std::vector<std::vector<std::complex<double> > >* data) {
  data->clear();
  std::ifstream file(file_name, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  double real_out;
  double imag_out;
  const int kBytesPerValue = 8;
  ABSL_CHECK_EQ(sizeof(real_out), kBytesPerValue);
  std::vector<std::complex<double> > data_row;
  int row_counter = 0;
  while (file.read(reinterpret_cast<char*>(&real_out), kBytesPerValue) &&
         file.read(reinterpret_cast<char*>(&imag_out), kBytesPerValue)) {
    if (row_counter >= row_length) {
      data->push_back(data_row);
      data_row.clear();
      row_counter = 0;
    }
    data_row.push_back(std::complex<double>(real_out, imag_out));
    ++row_counter;
  }
  if (row_counter >= row_length) {
    data->push_back(data_row);
  }
  return true;
}

// Helper: parse a double from a string, returning true on success.
static bool SafeStrtod(const std::string& s, double* result) {
  if (s.empty()) return false;
  char* end;
  *result = std::strtod(s.c_str(), &end);
  return end != s.c_str();
}

// Helper: parse a float from a string, returning true on success.
static bool SafeStrtof(const std::string& s, float* result) {
  if (s.empty()) return false;
  char* end;
  *result = std::strtof(s.c_str(), &end);
  return end != s.c_str();
}

void ReadCSVFileToComplexVectorOrDie(
    const std::string& file_name,
    std::vector<std::vector<std::complex<double> > >* data) {
  data->clear();
  std::ifstream file(file_name);
  ABSL_CHECK(file.is_open()) << "Failed to open CSV file: " << file_name;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) continue;
    std::vector<std::complex<double> > data_line;
    std::vector<std::string> values =
        absl::StrSplit(line, ',', absl::SkipEmpty());
    for (std::vector<std::string>::const_iterator i = values.begin();
         i != values.end(); ++i) {
      // each element of values may be in the form:
      // 0.001+0.002i, 0.001, 0.001i, -1.2i, -1.2-3.2i, 1.5, 1.5e-03+21.0i
      std::vector<std::string> parts;
      // Find the first instance of + or - after the second character
      // in the string, that does not immediately follow an 'e'.
      size_t operator_index = i->find_first_of("+-", 2);
      if (operator_index < i->size()
          && i->substr(operator_index - 1, 1) == "e") {
        operator_index = i->find_first_of("+-", operator_index + 1);
      }
      parts.push_back(i->substr(0, operator_index));
      if (operator_index < i->size()) {
        parts.push_back(i->substr(operator_index, std::string::npos));
      }

      double real_part = 0.0;
      double imaginary_part = 0.0;
      for (std::vector<std::string>::const_iterator j = parts.begin();
           j != parts.end(); ++j) {
        if (j->find_first_of("ij") != std::string::npos) {
          SafeStrtod(*j, &imaginary_part);
        } else {
          SafeStrtod(*j, &real_part);
        }
      }
      data_line.push_back(std::complex<double>(real_part, imaginary_part));
    }
    data->push_back(data_line);
  }
}

void ReadCSVFileToArrayOrDie(const std::string& filename,
                             std::vector<std::vector<float> >* array) {
  std::ifstream file(filename);
  ABSL_CHECK(file.is_open()) << "Failed to open CSV file: " << filename;
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  std::vector<std::string> lines =
      absl::StrSplit(contents, '\n', absl::SkipEmpty());
  contents.clear();

  array->clear();
  for (int l = 0; l < lines.size(); ++l) {
    std::vector<std::string> tokens =
        absl::StrSplit(lines[l], ',', absl::SkipEmpty());
    std::vector<float> values;
    for (const auto& token : tokens) {
      float val;
      ABSL_CHECK(SafeStrtof(token, &val)) << "Failed to parse: " << token;
      values.push_back(val);
    }
    array->push_back(values);
  }
}

bool WriteDoubleVectorToFile(const std::string& file_name,
                             absl::Span<const double> data) {
  std::ofstream file(file_name, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  for (size_t i = 0; i < data.size(); ++i) {
    file.write(reinterpret_cast<const char*>(&data[i]), sizeof(data[i]));
  }
  return file.good();
}

bool WriteFloatVectorToFile(const std::string& file_name,
                            absl::Span<const float> data) {
  std::ofstream file(file_name, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  for (size_t i = 0; i < data.size(); ++i) {
    file.write(reinterpret_cast<const char*>(&data[i]), sizeof(data[i]));
  }
  return file.good();
}

bool WriteDoubleArrayToFile(const std::string& file_name, int size,
                            const double* data) {
  std::ofstream file(file_name, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  for (int i = 0; i < size; ++i) {
    file.write(reinterpret_cast<const char*>(&data[i]), sizeof(data[i]));
  }
  return file.good();
}

bool WriteFloatArrayToFile(const std::string& file_name, int size,
                           const float* data) {
  std::ofstream file(file_name, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  for (int i = 0; i < size; ++i) {
    file.write(reinterpret_cast<const char*>(&data[i]), sizeof(data[i]));
  }
  return file.good();
}

bool WriteComplexVectorToRawDoubleFile(
    const std::string& file_name,
    absl::Span<const std::vector<std::complex<double>>> data) {
  std::ofstream file(file_name, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  for (size_t i = 0; i < data.size(); ++i) {
    for (size_t j = 0; j < data[i].size(); ++j) {
      const double real_part = std::real(data[i][j]);
      file.write(reinterpret_cast<const char*>(&real_part), sizeof(real_part));
      const double imag_part = std::imag(data[i][j]);
      file.write(reinterpret_cast<const char*>(&imag_part), sizeof(imag_part));
    }
  }
  return file.good();
}

void SineWave(int sample_rate,
              float frequency,
              float duration_seconds,
              std::vector<double>* data) {
  data->clear();
  for (int i = 0; i < static_cast<int>(sample_rate * duration_seconds); ++i) {
    data->push_back(sin(2.0 * M_PI * i * frequency /
                        static_cast<double>(sample_rate)));
  }
}

std::string JoinPath(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  if (a.back() == '/') return a + b;
  return a + "/" + b;
}

std::string TestSrcDir() {
  const char* srcdir = std::getenv("TEST_SRCDIR");
  const char* workspace = std::getenv("TEST_WORKSPACE");
  if (srcdir == nullptr) return "";
  std::string result(srcdir);
  if (workspace != nullptr && std::strlen(workspace) > 0) {
    result = JoinPath(result, workspace);
  }
  return result;
}

}  // namespace audio_dsp
