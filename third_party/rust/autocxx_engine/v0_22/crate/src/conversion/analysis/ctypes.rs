// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::map::IndexMap as HashMap;

use syn::Ident;

use crate::conversion::api::ApiName;
use crate::conversion::apivec::ApiVec;
use crate::types::Namespace;
use crate::{conversion::api::Api, known_types::known_types, types::QualifiedName};

use super::deps::HasDependencies;
use super::fun::FnPhase;

/// Spot any variable-length C types (e.g. unsigned long)
/// used in the [Api]s and append those as extra APIs.
pub(crate) fn append_ctype_information(apis: &mut ApiVec<FnPhase>) {
    let ctypes: HashMap<Ident, QualifiedName> = apis
        .iter()
        .flat_map(|api| api.deps())
        .filter(|ty| known_types().is_ctype(ty))
        .map(|ty| (ty.get_final_ident(), ty.clone()))
        .collect();
    for (id, typename) in ctypes {
        apis.push(Api::CType {
            name: ApiName::new(&Namespace::new(), id),
            typename,
        });
    }
}
