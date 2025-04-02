mod bindings;
mod com_bindings;
mod delay_load;
mod factory_cache;
mod generic_factory;
mod heap;
mod ref_count;
mod sha1;
mod waiter;
mod weak_ref_count;

pub use bindings::*;
pub use com_bindings::*;
pub use delay_load::*;
pub use factory_cache::*;
pub use generic_factory::*;
pub use heap::*;
pub use ref_count::*;
pub use sha1::*;
pub use waiter::*;
pub use weak_ref_count::*;

// This is a workaround since 1.56 does not include `bool::then_some`.
pub fn then_some<T>(value: bool, t: T) -> Option<T> {
    if value {
        Some(t)
    } else {
        None
    }
}

pub fn wide_trim_end(mut wide: &[u16]) -> &[u16] {
    while let Some(last) = wide.last() {
        match last {
            32 | 9..=13 => wide = &wide[..wide.len() - 1],
            _ => break,
        }
    }
    wide
}

#[doc(hidden)]
#[macro_export]
macro_rules! interface_hierarchy {
    ($child:ty, $parent:ty) => {
        impl ::windows_core::CanInto<$parent> for $child {}
    };
    ($child:ty, $first:ty, $($rest:ty),+) => {
        $crate::imp::interface_hierarchy!($child, $first);
        $crate::imp::interface_hierarchy!($child, $($rest),+);
    };
}

#[doc(hidden)]
pub use interface_hierarchy;
