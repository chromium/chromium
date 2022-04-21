// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::conversion::{
    analysis::fun::function_wrapper::{CppConversionType, TypeConversionPolicy},
    ConvertError,
};

use super::type_to_cpp::{type_to_cpp, CppNameMap};

impl TypeConversionPolicy {
    pub(super) fn unconverted_type(
        &self,
        cpp_name_map: &CppNameMap,
    ) -> Result<String, ConvertError> {
        match self.cpp_conversion {
            CppConversionType::FromUniquePtrToValue => self.unique_ptr_wrapped_type(cpp_name_map),
            CppConversionType::FromPtrToValue => {
                Ok(format!("{}*", self.unwrapped_type_as_string(cpp_name_map)?))
            }
            _ => self.unwrapped_type_as_string(cpp_name_map),
        }
    }

    pub(super) fn converted_type(&self, cpp_name_map: &CppNameMap) -> Result<String, ConvertError> {
        match self.cpp_conversion {
            CppConversionType::FromValueToUniquePtr => self.unique_ptr_wrapped_type(cpp_name_map),
            _ => self.unwrapped_type_as_string(cpp_name_map),
        }
    }

    fn unwrapped_type_as_string(&self, cpp_name_map: &CppNameMap) -> Result<String, ConvertError> {
        type_to_cpp(&self.unwrapped_type, cpp_name_map)
    }

    fn unique_ptr_wrapped_type(
        &self,
        original_name_map: &CppNameMap,
    ) -> Result<String, ConvertError> {
        Ok(format!(
            "std::unique_ptr<{}>",
            self.unwrapped_type_as_string(original_name_map)?
        ))
    }

    pub(super) fn cpp_conversion(
        &self,
        var_name: &str,
        cpp_name_map: &CppNameMap,
        is_return: bool,
    ) -> Result<Option<String>, ConvertError> {
        // If is_return we want to avoid unnecessary std::moves because they
        // make RVO less effective
        Ok(match self.cpp_conversion {
            CppConversionType::None | CppConversionType::FromReturnValueToPlacementPtr => {
                Some(var_name.to_string())
            }
            CppConversionType::Move => Some(format!("std::move({})", var_name)),
            CppConversionType::FromUniquePtrToValue | CppConversionType::FromPtrToMove => {
                Some(format!("std::move(*{})", var_name))
            }
            CppConversionType::FromValueToUniquePtr => Some(format!(
                "std::make_unique<{}>({})",
                self.unconverted_type(cpp_name_map)?,
                var_name
            )),
            CppConversionType::FromPtrToValue => {
                let dereference = format!("*{}", var_name);
                Some(if is_return {
                    dereference
                } else {
                    format!("std::move({})", dereference)
                })
            }
            CppConversionType::IgnoredPlacementPtrParameter => None,
        })
    }
}
