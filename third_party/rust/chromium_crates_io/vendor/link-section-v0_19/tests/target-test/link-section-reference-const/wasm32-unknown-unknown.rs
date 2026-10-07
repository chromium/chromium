use link_section::declarative::{in_section, section};
use link_section::TypedReferenceSection;



#[allow(non_camel_case_types)]
struct FOO;
impl FOO {
    /// Get a `const` reference to the underlying section. In
    /// non-const contexts, `deref` is sufficient.
    pub const fn const_deref(&self) -> &'static TypedReferenceSection<u32> {
        static SECTION: TypedReferenceSection<u32> =
            {
                let section =
                    {
                        static __LINK_SECTION_NAME: &'static str =
                            ".data.link_section.FOO";
                        #[export_name = ".data.link_section.FOO.bounds"]
                        #[used]
                        #[used]
                        static __LINK_SECTION_INFO:
                            ::link_section::__support::wasm::LinkSectionInfoLock<::link_section::__support::wasm::LinkSectionInfo>
                            =
                            ::link_section::__support::wasm::LinkSectionInfoLock::new(::link_section::__support::wasm::LinkSectionInfo::new::<(u32)>(__LINK_SECTION_NAME,
                                    true));
                        #[link_section = ".init_array.1"]
                        #[used]
                        #[allow(non_snake_case)]
                        static __LINK_SECTION_FLATTEN_FN_REF: extern "C" fn() =
                            {
                                extern "C" fn __LINK_SECTION_FLATTEN_FN() {
                                    unsafe {
                                        ::link_section::__support::wasm::flatten(&raw const __LINK_SECTION_INFO);
                                    }
                                }
                                __LINK_SECTION_FLATTEN_FN
                            };
                        unsafe {
                            <::link_section::__support::Bounds>::new(&raw const __LINK_SECTION_INFO)
                        }
                    };
                let name = ".data.link_section.FOO";
                ::link_section::__support::validate_section_name(name);
                unsafe { <TypedReferenceSection<u32>>::new(name, section) }
            };
        &SECTION
    }
}
impl ::core::fmt::Debug for FOO {
    fn fmt(&self, f: &mut ::core::fmt::Formatter<'_>) -> ::core::fmt::Result {
        ::core::ops::Deref::deref(self).fmt(f)
    }
}
impl ::core::ops::Deref for FOO {
    type Target = TypedReferenceSection<u32>;
    fn deref(&self) -> &Self::Target { self.const_deref() }
}
impl ::link_section::__support::SectionItemType for FOO {
    type Item = (u32);
}
impl ::link_section::__support::SectionItemTyped<(u32)> for FOO {
    type Item = (u32);
}
impl FOO {
    /// Get the section as a slice.
    pub fn as_slice(&self) -> &[(u32)] { self.const_deref().as_slice() }
}
impl ::core::iter::IntoIterator for FOO {
    type Item = &'static (u32);
    type IntoIter = ::core::slice::Iter<'static, (u32)>;
    fn into_iter(self) -> Self::IntoIter {
        self.const_deref().as_slice().iter()
    }
}
const ITEM: u32 =
    const {
            type __InSecStoredTy =
                <::link_section::TypedSection<u32> as
                ::link_section::__support::SectionItemType>::Item;
            const __LINK_SECTION_CONST_ITEM_VALUE: __InSecStoredTy = 42;
            #[used]
            static __LINK_SECTION_CELL:
                ::link_section::__support::wasm::LinkCell<__InSecStoredTy,
                ::link_section::__support::wasm::LinkMetaSlot> =
                <::link_section::__support::wasm::LinkCell<__InSecStoredTy,
                        ::link_section::__support::wasm::LinkMetaSlot>>::new(__LINK_SECTION_CONST_ITEM_VALUE,
                    ::core::ptr::null());
            #[allow(missing_unsafe_on_extern)]
            extern "C" {
                #[link_name = ".data.link_section.FOO.bounds"]
                static __LINK_SECTION_INFO:
                    ::link_section::__support::wasm::LinkSectionInfoLock<::link_section::__support::wasm::LinkSectionInfo>;
            }
            #[link_section = ".init_array.0"]
            #[used]
            #[allow(non_snake_case)]
            static __LINK_SECTION_ITEM_FN_REF: extern "C" fn() =
                {
                    extern "C" fn __LINK_SECTION_ITEM_FN() {
                        unsafe {
                            ::link_section::__support::wasm::register(&raw const __LINK_SECTION_INFO,
                                __LINK_SECTION_CELL.as_cell_ptr());
                        }
                    }
                    __LINK_SECTION_ITEM_FN
                };
            __LINK_SECTION_CONST_ITEM_VALUE
        };
fn main() {}
