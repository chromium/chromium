#[cfg(feature = "jvm-callback-support")]
use alloc::boxed::Box;
use core::ffi::c_void;
#[cfg(feature = "jvm-callback-support")]
use jni::{
    objects::{GlobalRef, JObject},
    sys::jlong,
    JNIEnv,
};

// struct representing a callback from Rust into a foreign language
// TODO restrict the return type?
#[repr(C)]
pub struct DiplomatCallback<ReturnType> {
    // any data required to run the callback; e.g. a pointer to the
    // callback wrapper object in the foreign runtime + the runtime itself
    pub data: *mut c_void,
    // function to actually run the callback. Note the first param is mutable, but depending
    // on if this is passed to a Fn or FnMut may not actually need to be. FFI-Callers
    // of said functions should cast to mutable.
    pub run_callback: unsafe extern "C" fn(*mut c_void, ...) -> ReturnType,
    // function to destroy this callback struct
    pub destructor: Option<unsafe extern "C" fn(*mut c_void)>,
}

impl<ReturnType> Drop for DiplomatCallback<ReturnType> {
    fn drop(&mut self) {
        if let Some(destructor) = self.destructor {
            unsafe {
                (destructor)(self.data);
            }
        }
    }
}

// return a pointer to a JNI GlobalRef, which is a JVM GC root to the object provided.
// this can then be stored as a field in a struct, so that the struct
// is not deallocated until the JVM calls a destructor that unwraps
// the GlobalRef so it can be dropped.
#[cfg(feature = "jvm-callback-support")]
#[no_mangle]
extern "system" fn create_rust_jvm_cookie<'local>(
    env: JNIEnv<'local>,
    obj_to_ref: JObject<'local>,
) -> jlong {
    let global_ref = env.new_global_ref(obj_to_ref).unwrap();
    Box::into_raw(Box::new(global_ref)) as jlong
}

#[cfg(feature = "jvm-callback-support")]
#[no_mangle]
extern "system" fn destroy_rust_jvm_cookie(global_ref_boxed: jlong) {
    unsafe {
        drop(Box::from_raw(global_ref_boxed as *mut GlobalRef));
    }
}
