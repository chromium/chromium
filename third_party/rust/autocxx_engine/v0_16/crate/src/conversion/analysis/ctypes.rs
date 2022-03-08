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

use std::collections::HashMap;

use syn::Ident;

use crate::conversion::api::ApiName;
use crate::types::Namespace;
use crate::{conversion::api::Api, known_types::known_types, types::QualifiedName};

use super::fun::FnPhase;

/// Spot any variable-length C types (e.g. unsigned long)
/// used in the [Api]s and append those as extra APIs.
pub(crate) fn append_ctype_information(apis: &mut Vec<Api<FnPhase>>) {
    let ctypes: HashMap<Ident, QualifiedName> = apis
        .iter()
        .flat_map(|api| api.deps())
        .filter(|ty| known_types().is_ctype(ty))
        .map(|ty| (ty.get_final_ident(), ty))
        .collect();
    for (id, typename) in ctypes {
        apis.push(Api::CType {
            name: ApiName::new(&Namespace::new(), id),
            typename,
        });
    }
}
