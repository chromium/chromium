// Mangled symbol arrangements:
//
//   (a) One-off internal symbol.
//          pattern:  {CXXBRIDGE} $ {NAME}
//          examples:
//             - cxxbridge1$exception
//          defining characteristics:
//             - 2 segments, none an integer
//
//   (b) Behavior on a builtin binding without generic parameter.
//          pattern:  {CXXBRIDGE} $ {TYPE} $ {NAME}
//          examples:
//             - cxxbridge1$string$len
//          defining characteristics:
//             - 3 segments, none an integer
//
//   (c) Behavior on a builtin binding with generic parameter.
//          pattern:  {CXXBRIDGE} $ {TYPE} $ {PARAM...} $ {NAME}
//          examples:
//             - cxxbridge1$box$org$rust$Struct$alloc
//             - cxxbridge1$unique_ptr$std$vector$u8$drop
//          defining characteristics:
//             - 4+ segments, none an integer
//
//   (d) User-defined extern function.
//          pattern:  {NAMESPACE...} $ {CXXBRIDGE} $ {CXXVERSION} $ {NAME}
//          examples:
//             - cxxbridge1$189$new_client
//             - org$rust$cxxbridge1$189$new_client
//          defining characteristics:
//             - second segment from end is an integer
//
//   (e) User-defined extern member function.
//          pattern:  {NAMESPACE...} $ {CXXBRIDGE} $ {CXXVERSION} $ {TYPE} $ {NAME}
//          examples:
//             - org$cxxbridge1$189$Struct$get
//          defining characteristics:
//             - third segment from end is an integer
//
//   (f) Operator overload.
//          pattern:  {NAMESPACE...} $ {CXXBRIDGE} $ {CXXVERSION} $ {TYPE} $ operator $ {NAME}
//          examples:
//             - org$rust$cxxbridge1$189$Struct$operator$eq
//          defining characteristics:
//             - second segment from end is `operator` (not possible in type or namespace names)
//
//   (g) Closure trampoline.
//          pattern:  {NAMESPACE...} $ {CXXBRIDGE} $ {CXXVERSION} $ {TYPE?} $ {NAME} $ {ARGUMENT} $ {DIRECTION}
//          examples:
//             - org$rust$cxxbridge1$Struct$invoke$f$0
//          defining characteristics:
//             - last symbol is `0` (C half) or `1` (Rust half) which are not legal identifiers on their own
//
//
// Mangled preprocessor variable arrangements:
//
//   (A) One-off internal variable.
//          pattern:  {CXXBRIDGE} _ {NAME}
//          examples:
//             - CXXBRIDGE1_PANIC
//             - CXXBRIDGE1_RUST_STRING
//          defining characteristics:
//             - NAME does not begin with STRUCT or ENUM
//
//   (B) Guard around user-defined type.
//          pattern:  {CXXBRIDGE} _ {STRUCT or ENUM} _ {NAMESPACE...} $ {TYPE}
//          examples:
//             - CXXBRIDGE1_STRUCT_org$rust$Struct
//             - CXXBRIDGE1_ENUM_Enabled

use crate::syntax::map::UnorderedMap;
use crate::syntax::resolve::Resolution;
use crate::syntax::symbol::{self, Symbol};
use crate::syntax::{ExternFn, Pair, Type, Types};
use proc_macro2::Ident;

const CXXBRIDGE: &str = "cxxbridge1";
const CXXVERSION: &str = env!("CARGO_PKG_VERSION_PATCH");

macro_rules! join {
    ($($segment:expr),+ $(,)?) => {
        symbol::join(&[$(&$segment),+])
    };
}

pub(crate) fn extern_fn(efn: &ExternFn, types: &Types) -> Symbol {
    match efn.self_type() {
        Some(self_type) => {
            let self_type_ident = types.resolve(self_type);
            join!(
                efn.name.namespace,
                CXXBRIDGE,
                CXXVERSION,
                self_type_ident.name.cxx,
                efn.name.rust,
            )
        }
        None => join!(efn.name.namespace, CXXBRIDGE, CXXVERSION, efn.name.rust),
    }
}

pub(crate) fn operator(receiver: &Pair, operator: &'static str) -> Symbol {
    join!(
        receiver.namespace,
        CXXBRIDGE,
        CXXVERSION,
        receiver.cxx,
        "operator",
        operator,
    )
}

// The C half of a function pointer trampoline.
pub(crate) fn c_trampoline(efn: &ExternFn, var: &Pair, types: &Types) -> Symbol {
    join!(extern_fn(efn, types), var.rust, 0)
}

// The Rust half of a function pointer trampoline.
pub(crate) fn r_trampoline(efn: &ExternFn, var: &Pair, types: &Types) -> Symbol {
    join!(extern_fn(efn, types), var.rust, 1)
}

/// Mangles the given type (e.g. `Box<org::rust::Struct>`) into a symbol
/// fragment (`box$org$rust$Struct`) to be used in the name of generic
/// instantiations (`cxxbridge1$box$org$rust$Struct$alloc`) pertaining to that
/// type.
///
/// Generic instantiation is not supported for all types in full generality.
/// This function must handle unsupported types gracefully by returning `None`
/// because it is used early during construction of the data structures that are
/// the input to 'syntax/check.rs', and unsupported generic instantiations are
/// only reported as an error later.
pub(crate) fn typename(t: &Type, res: &UnorderedMap<&Ident, Resolution>) -> Option<Symbol> {
    match t {
        Type::Ident(named_type) => res.get(&named_type.rust).map(|res| res.name.to_symbol()),
        Type::CxxVector(ty1) => typename(&ty1.inner, res).map(|s| join!("std", "vector", s)),
        _ => None,
    }
}
