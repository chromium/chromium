/// Utility macro that creates a type erased version of a trait.
///
/// This is used in the engine to create a `DynObject` for an `Object`.
/// For the exact use of this look at where the macro is invoked.
macro_rules! type_erase {
    ($v:vis trait $t_name:ident => $erased_t_name:ident {
        $(fn $f:ident(&self $(, $p:ident: $t:ty $(,)?)*) $(-> $r:ty)?;)*
        $(
            impl $impl_name:path {
                $(
                    fn $f_impl:ident(
                        &self $(, $p_impl:ident: $t_impl:ty $(,)?)*
                    ) $(-> $r_impl:ty)?;
                )*
            }
        )*
    }) => {
        #[doc = concat!("Type-erased version of [`", stringify!($t_name), "`]")]
        $v struct $erased_t_name {
            ptr: *const (),
            vtable: *const (),
        }

        const _: () = {
            struct VTable {
                $($f: fn(*const (), $($p: $t),*) $(-> $r)?,)*
                $($($f_impl: fn(*const (), $($p_impl: $t_impl),*) $(-> $r_impl)?,)*)*
                __type_id: fn() -> std::any::TypeId,
                __type_name: fn() -> &'static str,
                __incref: fn(*const ()),
                __decref: fn(*const ()),
            }

            #[inline(always)]
            fn vt(e: &$erased_t_name) -> &VTable {
                unsafe { &*(e.vtable as *const VTable) }
            }

            impl $erased_t_name {
                #[doc = concat!("Returns a new boxed, type-erased [`", stringify!($t_name), "`].")]
                $v fn new<T: $t_name + 'static>(v: std::sync::Arc<T>) -> Self {
                    let ptr = std::sync::Arc::into_raw(v) as *const T as *const ();
                    let vtable = &VTable {
                        $(
                            $f: |ptr, $($p),*| unsafe {
                                let arc = std::mem::ManuallyDrop::new(std::sync::Arc::<T>::from_raw(ptr as *const T));
                                <T as $t_name>::$f(&arc, $($p),*)
                            },
                        )*
                        $($(
                            $f_impl: |ptr, $($p_impl),*| unsafe {
                                let arc = std::mem::ManuallyDrop::new(std::sync::Arc::<T>::from_raw(ptr as *const T));
                                <T as $impl_name>::$f_impl(&*arc, $($p_impl),*)
                            },
                        )*)*
                        __type_id: || std::any::TypeId::of::<T>(),
                        __type_name: || std::any::type_name::<T>(),
                        __incref: |ptr| unsafe {
                            std::sync::Arc::<T>::increment_strong_count(ptr as *const T);
                        },
                        __decref: |ptr| unsafe {
                            std::sync::Arc::from_raw(ptr as *const T);
                        },
                    };

                    Self { ptr, vtable: vtable as *const VTable as *const () }
                }

                $(
                    #[doc = concat!(
                        "Calls [`", stringify!($t_name), "::", stringify!($f),
                        "`] of the underlying boxed value."
                    )]
                    $v fn $f(&self, $($p: $t),*) $(-> $r)? {
                        (vt(self).$f)(self.ptr, $($p),*)
                    }
                )*

                /// Returns the type name of the concrete underlying type.
                $v fn type_name(&self) -> &'static str {
                    (vt(self).__type_name)()
                }

                /// Downcast to `T` if the boxed value holds a `T`.
                ///
                /// This works like [`Any::downcast_ref`](std::any::Any#method.downcast_ref).
                $v fn downcast_ref<T: 'static>(&self) -> Option<&T> {
                    if (vt(self).__type_id)() == std::any::TypeId::of::<T>() {
                        unsafe {
                            return Some(&*(self.ptr as *const T));
                        }
                    }

                    None
                }

                /// Downcast to `T` if the boxed value holds a `T`.
                ///
                /// This is similar to [`downcast_ref`](Self::downcast_ref) but returns the [`Arc`].
                $v fn downcast<T: 'static>(&self) -> Option<Arc<T>> {
                    if (vt(self).__type_id)() == std::any::TypeId::of::<T>() {
                        unsafe {
                            std::sync::Arc::<T>::increment_strong_count(self.ptr as *const T);
                            return Some(std::sync::Arc::<T>::from_raw(self.ptr as *const T));
                        }
                    }

                    None
                }

                /// Checks if the boxed value is a `T`.
                ///
                /// This works like [`Any::is`](std::any::Any#method.is).
                $v fn is<T: 'static>(&self) -> bool {
                    self.downcast::<T>().is_some()
                }
            }

            impl Clone for $erased_t_name {
                fn clone(&self) -> Self {
                    (vt(self).__incref)(self.ptr);
                    Self {
                        ptr: self.ptr,
                        vtable: self.vtable,
                    }
                }
            }

            impl Drop for $erased_t_name {
                fn drop(&mut self) {
                    (vt(self).__decref)(self.ptr);
                }
            }

            impl<T: $t_name + 'static> From<Arc<T>> for $erased_t_name {
                fn from(value: Arc<T>) -> Self {
                    $erased_t_name::new(value)
                }
            }

            $(
                impl $impl_name for $erased_t_name {
                    $(
                        fn $f_impl(&self, $($p_impl: $t_impl),*) $(-> $r_impl)? {
                            (vt(self).$f_impl)(self.ptr, $($p_impl),*)
                        }
                    )*
                }
            )*
        };
    };
}
