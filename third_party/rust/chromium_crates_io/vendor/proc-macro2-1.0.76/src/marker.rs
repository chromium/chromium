use alloc::rc::Rc;
use core::marker::PhantomData;
use core::panic::{RefUnwindSafe, UnwindSafe};

// Zero sized marker with the correct set of autotrait impls we want all proc
// macro types to have.
pub(crate) type Marker = PhantomData<ProcMacroAutoTraits>;

pub(crate) use self::value::*;

mod value {
    pub(crate) use core::marker::PhantomData as Marker;
}

pub(crate) struct ProcMacroAutoTraits(
    #[allow(dead_code)] // https://github.com/rust-lang/rust/issues/119645
    Rc<()>,
);

impl UnwindSafe for ProcMacroAutoTraits {}
impl RefUnwindSafe for ProcMacroAutoTraits {}
