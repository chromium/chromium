use crate::ComInterface;

// A streamlined version of the IActivationFactory interface used by WinRT class factories used internally by the windows crate
// to simplify code generation. Components should implement the `IActivationFactory` interface published by the windows crate.
#[repr(transparent)]
#[derive(Clone, PartialEq, Eq)]
pub struct IGenericFactory(crate::IUnknown);

impl IGenericFactory {
    pub fn ActivateInstance<I: crate::ComInterface>(&self) -> crate::Result<I> {
        unsafe {
            let mut result__ = std::mem::zeroed();
            (crate::Interface::vtable(self).ActivateInstance)(std::mem::transmute_copy(self), &mut result__ as *mut _ as *mut _).from_abi::<crate::IInspectable>(result__)?.cast()
        }
    }
}

#[repr(C)]
pub struct IGenericFactory_Vtbl {
    pub base__: crate::IInspectable_Vtbl,
    pub ActivateInstance: unsafe extern "system" fn(this: *mut std::ffi::c_void, instance: *mut *mut std::ffi::c_void) -> crate::HRESULT,
}

unsafe impl crate::Interface for IGenericFactory {
    type Vtable = IGenericFactory_Vtbl;
}

unsafe impl crate::ComInterface for IGenericFactory {
    const IID: crate::GUID = crate::GUID::from_u128(0x00000035_0000_0000_c000_000000000046);
}
