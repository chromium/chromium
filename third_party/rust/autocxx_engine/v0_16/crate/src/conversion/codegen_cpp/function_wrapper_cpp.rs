// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use crate::conversion::{
    analysis::fun::function_wrapper::{CppConversionType, TypeConversionPolicy},
    ConvertError,
};
use crate::known_types::type_lacks_copy_constructor;

use super::type_to_cpp::{type_to_cpp, CppNameMap};

impl TypeConversionPolicy {
    pub(super) fn unconverted_type(
        &self,
        cpp_name_map: &CppNameMap,
    ) -> Result<String, ConvertError> {
        match self.cpp_conversion {
            CppConversionType::FromUniquePtrToValue => self.wrapped_type(cpp_name_map),
            _ => self.unwrapped_type_as_string(cpp_name_map),
        }
    }

    pub(super) fn converted_type(&self, cpp_name_map: &CppNameMap) -> Result<String, ConvertError> {
        match self.cpp_conversion {
            CppConversionType::FromValueToUniquePtr => self.wrapped_type(cpp_name_map),
            _ => self.unwrapped_type_as_string(cpp_name_map),
        }
    }

    fn unwrapped_type_as_string(&self, cpp_name_map: &CppNameMap) -> Result<String, ConvertError> {
        type_to_cpp(&self.unwrapped_type, cpp_name_map)
    }

    fn wrapped_type(&self, original_name_map: &CppNameMap) -> Result<String, ConvertError> {
        Ok(format!(
            "std::unique_ptr<{}>",
            self.unwrapped_type_as_string(original_name_map)?
        ))
    }

    pub(super) fn cpp_conversion(
        &self,
        var_name: &str,
        cpp_name_map: &CppNameMap,
        use_rvo: bool,
    ) -> Result<String, ConvertError> {
        Ok(match self.cpp_conversion {
            CppConversionType::None => {
                if type_lacks_copy_constructor(&self.unwrapped_type) && !use_rvo {
                    format!("std::move({})", var_name)
                } else {
                    var_name.to_string()
                }
            }
            CppConversionType::FromUniquePtrToValue | CppConversionType::FromPtrToMove => {
                format!("std::move(*{})", var_name)
            }
            CppConversionType::FromValueToUniquePtr => format!(
                "std::make_unique<{}>({})",
                self.unconverted_type(cpp_name_map)?,
                var_name
            ),
        })
    }
}
