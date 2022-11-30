# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(CMAKE_CROSSCOMPILING)
  iree_declare_host_excutable(generate_cc_embed_data)
else()
  add_executable(generate_cc_embed_data)
  target_sources(generate_cc_embed_data PRIVATE generate_cc_embed_data.cc)
  set_target_properties(generate_cc_embed_data PROPERTIES OUTPUT_NAME generate_cc_embed_data)

  target_link_libraries(generate_cc_embed_data
    absl::flags
    absl::flags_parse
    absl::strings
    absl::time
  )
  install(TARGETS generate_cc_embed_data
          COMPONENT generate_cc_embed_data
          RUNTIME DESTINATION bin)
endif()
