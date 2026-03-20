#![allow(unused_imports, unused_mut)]
// NOTE: this is a vendored and intentionally pruned subset of upstream
// self_cell, kept to the API surface MiniJinja currently needs.

//! Minimal, vendored subset of `self_cell` used by MiniJinja.
//!
//! We only keep the functionality that is currently exercised in this crate:
//! `try_new`, `borrow_dependent` (covariant cells), and `with_dependent_mut`.

#[doc(hidden)]
pub(crate) extern crate alloc;

#[doc(hidden)]
pub(crate) mod unsafe_self_cell;

/// Declares a self-referential struct with the subset MiniJinja uses.
///
/// Generated API:
///
/// ```ignore
/// fn try_new<Err>(
///     owner: $Owner,
///     dependent_builder: impl for<'a> FnOnce(&'a $Owner) -> Result<$Dependent<'a>, Err>
/// ) -> Result<Self, Err>
///
/// fn borrow_dependent<'a>(&'a self) -> &'a $Dependent<'a> // covariant only
///
/// fn with_dependent_mut<'outer_fn, Ret>(
///     &'outer_fn mut self,
///     func: impl for<'a> FnOnce(&'a $Owner, &'outer_fn mut $Dependent<'a>) -> Ret
/// ) -> Ret
/// ```
///
macro_rules! self_cell {
(
    $(#[$StructMeta:meta])*
    $Vis:vis struct $StructName:ident $(<$OwnerLifetime:lifetime>)? {
        owner: $Owner:ty,


        #[covariant]
        dependent: $Dependent:ident,
    }
) => {
    #[repr(transparent)]
    $(#[$StructMeta])*
    $Vis struct $StructName $(<$OwnerLifetime>)? {
        unsafe_self_cell: $crate::vendor::self_cell::unsafe_self_cell::UnsafeSelfCell<
            $StructName$(<$OwnerLifetime>)?,
            $Owner,
            $Dependent<'static>
        >,

        $(owner_marker: ::core::marker::PhantomData<&$OwnerLifetime ()> ,)?
    }

    #[allow(dead_code)]
    impl <$($OwnerLifetime)?> $StructName <$($OwnerLifetime)?> {
        $crate::vendor::self_cell::_self_cell_try_new!($Vis, $Owner $(=> $OwnerLifetime)?, $Dependent);

        /// Calls given closure `func` with an unique reference to dependent.
        $Vis fn with_dependent_mut<'outer_fn, Ret>(
            &'outer_fn mut self,
            func: impl for<'_q> ::core::ops::FnOnce(&'_q $Owner, &'outer_fn mut $Dependent<'_q>) -> Ret
        ) -> Ret {
            let (owner, dependent) = unsafe {
                    self.unsafe_self_cell.borrow_mut()
            };

            func(owner, dependent)
        }

        /// Borrows dependent.
        $Vis fn borrow_dependent<'_q>(&'_q self) -> &'_q $Dependent<'_q> {
            fn _assert_covariance<'x: 'y, 'y>(x: &'y $Dependent<'x>) -> &'y $Dependent<'y> {
                x
            }

            unsafe { self.unsafe_self_cell.borrow_dependent() }
        }
    }

    impl $(<$OwnerLifetime>)? Drop for $StructName $(<$OwnerLifetime>)? {
        fn drop(&mut self) {
            unsafe {
                self.unsafe_self_cell.drop_joined::<$Dependent>();
            }
        }
    }

};
}

#[doc(hidden)]
macro_rules! _self_cell_try_new {
    ($Vis:vis, $Owner:ty $(=> $OwnerLifetime:lifetime)?, $Dependent:ident) => {
        /// Constructs a new self-referential struct or returns an error.
        ///
        /// Consumes owner on error.
        $Vis fn try_new<Err>(
            owner: $Owner,
            dependent_builder:
                impl for<'_q> ::core::ops::FnOnce(&'_q $Owner) -> ::core::result::Result<$Dependent<'_q>, Err>
        ) -> ::core::result::Result<Self, Err> {
            type JoinedCell<'_q $(, $OwnerLifetime)?> =
                    $crate::vendor::self_cell::unsafe_self_cell::JoinedCell<$Owner, $Dependent<'_q>>;

            // unsafe placed here to make sure the body macro can't be abused.
            unsafe {
                $crate::vendor::self_cell::_self_cell_try_new_body!(JoinedCell, owner $(=> $OwnerLifetime)?, dependent_builder)
            }
        }
    };
}

#[doc(hidden)]
macro_rules! _self_cell_try_new_body {
    ($JoinedCell:ty, $owner:expr $(=> $OwnerLifetime:lifetime)?, $dependent_builder:expr) => {{
        // Allocate and initialize the owner/dependent pair in one cell.

        let layout = $crate::vendor::self_cell::alloc::alloc::Layout::new::<$JoinedCell>();
        assert!(layout.size() != 0);

        let joined_void_ptr = ::core::ptr::NonNull::new($crate::vendor::self_cell::alloc::alloc::alloc(layout)).unwrap();

        let joined_ptr = joined_void_ptr.cast::<$JoinedCell>();

        let (owner_ptr, dependent_ptr) = <$JoinedCell>::_field_pointers(joined_ptr.as_ptr());

        // Move owner into newly allocated space.
        owner_ptr.write($owner);

        // Drop guard that cleans up should building the dependent panic.
        let drop_guard =
            $crate::vendor::self_cell::unsafe_self_cell::OwnerAndCellDropGuard::new(joined_ptr);

        match $dependent_builder(&*owner_ptr) {
            ::core::result::Result::Ok(dependent) => {
                dependent_ptr.write(dependent);
                ::core::mem::forget(drop_guard);

                ::core::result::Result::Ok(Self {
                    unsafe_self_cell: $crate::vendor::self_cell::unsafe_self_cell::UnsafeSelfCell::new(
                        joined_void_ptr,
                    ),
                    $(owner_marker: ::core::marker::PhantomData::<&$OwnerLifetime ()> ,)?
                })
            }
            ::core::result::Result::Err(err) => ::core::result::Result::Err(err)
        }
    }}
}

pub(crate) use _self_cell_try_new;
pub(crate) use _self_cell_try_new_body;
pub(crate) use self_cell;
