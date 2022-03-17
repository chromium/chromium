// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    conversion::ConvertError,
    types::{make_ident, QualifiedName},
};
use indoc::indoc;
use once_cell::sync::OnceCell;
use std::collections::HashMap;
use syn::{parse_quote, Type, TypePath, TypePtr};

//// The behavior of the type.
#[derive(Debug)]
enum Behavior {
    CxxContainerByValueSafe,
    CxxContainerNotByValueSafe,
    CxxString,
    RustStr,
    RustString,
    RustByValue,
    CByValue,
    CVariableLengthByValue,
    CVoid,
    RustContainerByValueSafe,
}

/// Details about known special types, mostly primitives.
#[derive(Debug)]
struct TypeDetails {
    /// The name used by cxx (in Rust code) for this type.
    rs_name: String,
    /// C++ equivalent name for a Rust type.
    cpp_name: String,
    /// The behavior of the type.
    behavior: Behavior,
    /// Any extra non-canonical names
    extra_non_canonical_name: Option<String>,
    has_const_copy_constructor: bool,
    has_move_constructor: bool,
}

impl TypeDetails {
    fn new(
        rs_name: impl Into<String>,
        cpp_name: impl Into<String>,
        behavior: Behavior,
        extra_non_canonical_name: Option<String>,
        has_const_copy_constructor: bool,
        has_move_constructor: bool,
    ) -> Self {
        TypeDetails {
            rs_name: rs_name.into(),
            cpp_name: cpp_name.into(),
            behavior,
            extra_non_canonical_name,
            has_const_copy_constructor,
            has_move_constructor,
        }
    }

    /// Whether and how to include this in the prelude given to bindgen.
    fn get_prelude_entry(&self) -> Option<String> {
        match self.behavior {
            Behavior::RustString
            | Behavior::RustStr
            | Behavior::CxxString
            | Behavior::CxxContainerByValueSafe
            | Behavior::CxxContainerNotByValueSafe
            | Behavior::RustContainerByValueSafe => {
                let tn = QualifiedName::new_from_cpp_name(&self.rs_name);
                let cxx_name = tn.get_final_item();
                let (templating, payload) = match self.behavior {
                    Behavior::CxxContainerByValueSafe
                    | Behavior::CxxContainerNotByValueSafe
                    | Behavior::RustContainerByValueSafe => ("template<typename T> ", "T* ptr"),
                    _ => ("", "char* ptr"),
                };
                Some(format!(
                    indoc! {"
                    /**
                    * <div rustbindgen=\"true\" replaces=\"{}\">
                    */
                    {}class {} {{
                        {};
                    }};
                    "},
                    self.cpp_name, templating, cxx_name, payload
                ))
            }
            _ => None,
        }
    }

    fn to_type_path(&self) -> TypePath {
        let mut segs = self.rs_name.split("::").peekable();
        if segs.peek().map(|seg| seg.is_empty()).unwrap_or_default() {
            segs.next();
            let segs = segs.into_iter().map(make_ident);
            parse_quote! {
                ::#(#segs)::*
            }
        } else {
            let segs = segs.into_iter().map(make_ident);
            parse_quote! {
                #(#segs)::*
            }
        }
    }

    fn to_typename(&self) -> QualifiedName {
        QualifiedName::new_from_cpp_name(&self.rs_name)
    }

    fn get_generic_behavior(&self) -> CxxGenericType {
        match self.behavior {
            Behavior::CxxContainerByValueSafe | Behavior::CxxContainerNotByValueSafe => {
                CxxGenericType::Cpp
            }
            Behavior::RustContainerByValueSafe => CxxGenericType::Rust,
            _ => CxxGenericType::Not,
        }
    }
}

/// Database of known types.
#[derive(Default)]
pub(crate) struct TypeDatabase {
    by_rs_name: HashMap<QualifiedName, TypeDetails>,
    canonical_names: HashMap<QualifiedName, QualifiedName>,
}

/// Returns a database of known types.
pub(crate) fn known_types() -> &'static TypeDatabase {
    static KNOWN_TYPES: OnceCell<TypeDatabase> = OnceCell::new();
    KNOWN_TYPES.get_or_init(create_type_database)
}

/// The type of payload that a cxx generic can contain.
#[derive(PartialEq, Clone, Copy)]
pub enum CxxGenericType {
    /// Not a generic at all
    Not,
    /// Some generic like cxx::UniquePtr where the contents must be a
    /// complete type.
    Cpp,
    /// Some generic like rust::Box where forward declarations are OK
    Rust,
}

pub struct KnownTypeConstructorDetails {
    pub has_move_constructor: bool,
    pub has_const_copy_constructor: bool,
}

impl TypeDatabase {
    fn get(&self, ty: &QualifiedName) -> Option<&TypeDetails> {
        // The following line is important. It says that
        // when we encounter something like 'std::unique_ptr'
        // in the bindgen-generated bindings, we'll immediately
        // start to refer to that as 'UniquePtr' henceforth.
        let canonical_name = self.canonical_names.get(ty).unwrap_or(ty);
        self.by_rs_name.get(canonical_name)
    }

    /// Prelude of C++ for squirting into bindgen. This configures
    /// bindgen to output simpler types to replace some STL types
    /// that bindgen just can't cope with. Although we then replace
    /// those types with cxx types (e.g. UniquePtr), this intermediate
    /// step is still necessary because bindgen can't otherwise
    /// give us the templated types (e.g. when faced with the STL
    /// unique_ptr, bindgen would normally give us std_unique_ptr
    /// as opposed to std_unique_ptr<T>.)
    pub(crate) fn get_prelude(&self) -> String {
        itertools::join(
            self.by_rs_name
                .values()
                .filter_map(|t| t.get_prelude_entry()),
            "",
        )
    }

    /// Returns all known types.
    pub(crate) fn all_names(&self) -> impl Iterator<Item = &QualifiedName> {
        self.canonical_names.keys().chain(self.by_rs_name.keys())
    }

    /// Types which are known to be safe (or unsafe) to hold and pass by
    /// value in Rust.
    pub(crate) fn get_pod_safe_types(&self) -> impl Iterator<Item = (QualifiedName, bool)> {
        let pod_safety = self
            .all_names()
            .map(|tn| {
                (
                    tn.clone(),
                    match self.get(tn).unwrap().behavior {
                        Behavior::CxxContainerByValueSafe
                        | Behavior::RustStr
                        | Behavior::RustString
                        | Behavior::RustByValue
                        | Behavior::CByValue
                        | Behavior::CVariableLengthByValue
                        | Behavior::RustContainerByValueSafe => true,
                        Behavior::CxxString
                        | Behavior::CxxContainerNotByValueSafe
                        | Behavior::CVoid => false,
                    },
                )
            })
            .collect::<HashMap<_, _>>();
        pod_safety.into_iter()
    }

    pub(crate) fn get_constructor_details(
        &self,
        qn: &QualifiedName,
    ) -> Option<KnownTypeConstructorDetails> {
        self.get(qn).map(|x| KnownTypeConstructorDetails {
            has_move_constructor: x.has_move_constructor,
            has_const_copy_constructor: x.has_const_copy_constructor,
        })
    }

    /// Whether this TypePath should be treated as a value in C++
    /// but a reference in Rust. This only applies to rust::Str
    /// (C++ name) which is &str in Rust.
    pub(crate) fn should_dereference_in_cpp(&self, tn: &QualifiedName) -> bool {
        self.get(tn)
            .map(|td| matches!(td.behavior, Behavior::RustStr))
            .unwrap_or(false)
    }

    /// Whether this can only be passed around using `std::move`
    pub(crate) fn lacks_copy_constructor(&self, tn: &QualifiedName) -> bool {
        self.get(tn)
            .map(|td| {
                matches!(
                    td.behavior,
                    Behavior::CxxContainerByValueSafe | Behavior::CxxContainerNotByValueSafe
                )
            })
            .unwrap_or(false)
    }

    /// Here we substitute any names which we know are Special from
    /// our type database, e.g. std::unique_ptr -> UniquePtr.
    /// We strip off and ignore
    /// any PathArguments within this TypePath - callers should
    /// put them back again if needs be.
    pub(crate) fn consider_substitution(&self, tn: &QualifiedName) -> Option<TypePath> {
        self.get(tn).map(|td| td.to_type_path())
    }

    pub(crate) fn special_cpp_name(&self, rs: &QualifiedName) -> Option<String> {
        self.get(rs).map(|x| x.cpp_name.to_string())
    }

    pub(crate) fn is_known_type(&self, ty: &QualifiedName) -> bool {
        self.get(ty).is_some()
    }

    pub(crate) fn known_type_type_path(&self, ty: &QualifiedName) -> Option<TypePath> {
        self.get(ty).map(|td| td.to_type_path())
    }

    /// Get the list of types to give to bindgen to ask it _not_ to
    /// generate code for.
    pub(crate) fn get_initial_blocklist(&self) -> impl Iterator<Item = &str> + '_ {
        self.by_rs_name
            .iter()
            .filter_map(|(_, td)| td.get_prelude_entry().map(|_| td.cpp_name.as_str()))
    }

    /// Whether this is one of the ctypes (mostly variable length integers)
    /// which we need to wrap.
    pub(crate) fn is_ctype(&self, ty: &QualifiedName) -> bool {
        self.get(ty)
            .map(|td| {
                matches!(
                    td.behavior,
                    Behavior::CVariableLengthByValue | Behavior::CVoid
                )
            })
            .unwrap_or(false)
    }

    /// Whether this is a generic type acceptable to cxx. Otherwise,
    /// if we encounter a generic, we'll replace it with a synthesized concrete
    /// type.
    pub(crate) fn cxx_generic_behavior(&self, ty: &QualifiedName) -> CxxGenericType {
        self.get(ty)
            .map(|x| x.get_generic_behavior())
            .unwrap_or(CxxGenericType::Not)
    }

    pub(crate) fn is_cxx_acceptable_receiver(&self, ty: &QualifiedName) -> bool {
        self.get(ty).is_none() // at present, none of our known types can have
                               // methods attached.
    }

    pub(crate) fn conflicts_with_built_in_type(&self, ty: &QualifiedName) -> bool {
        self.get(ty).is_some()
    }

    pub(crate) fn convertible_from_strs(&self, ty: &QualifiedName) -> bool {
        self.get(ty)
            .map(|x| matches!(x.behavior, Behavior::CxxString))
            .unwrap_or(false)
    }

    fn insert(&mut self, td: TypeDetails) {
        let rs_name = td.to_typename();
        if let Some(extra_non_canonical_name) = &td.extra_non_canonical_name {
            self.canonical_names.insert(
                QualifiedName::new_from_cpp_name(extra_non_canonical_name),
                rs_name.clone(),
            );
        }
        self.canonical_names.insert(
            QualifiedName::new_from_cpp_name(&td.cpp_name),
            rs_name.clone(),
        );
        self.by_rs_name.insert(rs_name, td);
    }
}

fn create_type_database() -> TypeDatabase {
    let mut db = TypeDatabase::default();
    db.insert(TypeDetails::new(
        "cxx::UniquePtr",
        "std::unique_ptr",
        Behavior::CxxContainerByValueSafe,
        None,
        false,
        true,
    ));
    db.insert(TypeDetails::new(
        "cxx::CxxVector",
        "std::vector",
        Behavior::CxxContainerNotByValueSafe,
        None,
        false,
        true,
    ));
    db.insert(TypeDetails::new(
        "cxx::SharedPtr",
        "std::shared_ptr",
        Behavior::CxxContainerByValueSafe,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "cxx::WeakPtr",
        "std::weak_ptr",
        Behavior::CxxContainerByValueSafe,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "cxx::CxxString",
        "std::string",
        Behavior::CxxString,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "str",
        "rust::Str",
        Behavior::RustStr,
        None,
        true,
        false,
    ));
    db.insert(TypeDetails::new(
        "String",
        "rust::String",
        Behavior::RustString,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "std::boxed::Box",
        "rust::Box",
        Behavior::RustContainerByValueSafe,
        None,
        false,
        true,
    ));
    db.insert(TypeDetails::new(
        "i8",
        "int8_t",
        Behavior::CByValue,
        Some("std::os::raw::c_schar".into()),
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "u8",
        "uint8_t",
        Behavior::CByValue,
        Some("std::os::raw::c_uchar".into()),
        true,
        true,
    ));
    for (cpp_type, rust_type) in (4..7).map(|x| 2i32.pow(x)).flat_map(|x| {
        vec![
            (format!("uint{}_t", x), format!("u{}", x)),
            (format!("int{}_t", x), format!("i{}", x)),
        ]
    }) {
        db.insert(TypeDetails::new(
            rust_type,
            cpp_type,
            Behavior::CByValue,
            None,
            true,
            true,
        ));
    }
    db.insert(TypeDetails::new(
        "bool",
        "bool",
        Behavior::CByValue,
        None,
        true,
        true,
    ));

    db.insert(TypeDetails::new(
        "std::pin::Pin",
        "Pin",
        Behavior::RustByValue, // because this is actually Pin<&something>
        None,
        true,
        false,
    ));

    let mut insert_ctype = |cname: &str| {
        let concatenated_name = cname.replace(' ', "");
        db.insert(TypeDetails::new(
            format!("autocxx::c_{}", concatenated_name),
            cname,
            Behavior::CVariableLengthByValue,
            Some(format!("std::os::raw::c_{}", concatenated_name)),
            true,
            true,
        ));
        db.insert(TypeDetails::new(
            format!("autocxx::c_u{}", concatenated_name),
            format!("unsigned {}", cname),
            Behavior::CVariableLengthByValue,
            Some(format!("std::os::raw::c_u{}", concatenated_name)),
            true,
            true,
        ));
    };

    insert_ctype("long");
    insert_ctype("int");
    insert_ctype("short");
    insert_ctype("long long");

    db.insert(TypeDetails::new(
        "f32",
        "float",
        Behavior::CByValue,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "f64",
        "double",
        Behavior::CByValue,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "::std::os::raw::c_char",
        "char",
        Behavior::CByValue,
        None,
        true,
        true,
    ));
    db.insert(TypeDetails::new(
        "autocxx::c_void",
        "void",
        Behavior::CVoid,
        Some("std::os::raw::c_void".into()),
        false,
        false,
    ));
    db
}

pub(crate) fn ensure_pointee_is_valid(ptr: &TypePtr) -> Result<(), ConvertError> {
    match *ptr.elem {
        Type::Path(..) => Ok(()),
        _ => Err(ConvertError::InvalidPointee),
    }
}
