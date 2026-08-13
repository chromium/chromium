use std::ffi::c_void;
use std::panic::AssertUnwindSafe;

use crate::ffi::{LlgCallback, LlgConstraintStep};
use crate::panic_utils;

/// A `*const c_void` wrapper that is `Send`.
///
/// The C caller guarantees that the pointee remains valid and is safe for
/// cross-thread access until the callback is invoked.
#[derive(Clone, Copy)]
struct SendPtr(*const c_void);

// SAFETY: The C API contract for `llg_par_compute_mask` requires that
// `user_data` remains valid and is safe for cross-thread access until
// `done_cb` is invoked. The caller upholds this invariant.
unsafe impl Send for SendPtr {}

impl SendPtr {
    fn as_ptr(self) -> *const c_void {
        self.0
    }
}

fn par_compute_mask_inner(constraints: Vec<LlgConstraintStep>) {
    use rayon::prelude::*;
    constraints.into_par_iter().for_each(|step| {
        // A null constraint pointer is a caller bug — skip silently since
        // there is no constraint handle to record an error on.
        if step.constraint.is_null() {
            return;
        }

        // Wrap each step in catch_unwind to prevent panics from aborting the
        // process via rayon's spawn (which has no scope to propagate to).
        let result = panic_utils::catch_unwind(AssertUnwindSafe(|| {
            // SAFETY: `step.constraint` is non-null (checked above). The caller
            // of `llg_par_compute_mask` guarantees that each step's constraint
            // pointer is valid and unaliased for the duration of the parallel
            // computation (documented in the `# Safety` section of that function).
            let cc = unsafe { &mut *step.constraint };

            // Validate step parameters — set per-constraint error instead of panicking.
            if step.mask_byte_len % 4 != 0 {
                cc.set_error("llg_par_compute_mask: mask_byte_len is not a multiple of 4");
                return Ok(());
            }
            if step.mask_dest.is_null() {
                cc.set_error("llg_par_compute_mask: mask_dest is null");
                return Ok(());
            }
            let mask_elts = step.mask_byte_len / 4;

            if let Some(constraint) = &mut cc.constraint {
                let mut num_copied = 0;
                let mut add_eos = false;
                let eos = constraint.tok_trie().eos_token() as usize;
                match constraint.compute_mask() {
                    Ok(r) => {
                        if let Some(m) = r.sample_mask.as_ref() {
                            num_copied = std::cmp::min(m.len(), mask_elts);
                            // SAFETY: mask_dest is non-null (checked above), and
                            // mask_byte_len guarantees sufficient space.
                            unsafe {
                                std::ptr::copy_nonoverlapping(
                                    m.as_ptr(),
                                    step.mask_dest,
                                    num_copied,
                                );
                            }
                        }
                        add_eos = r.is_stop();
                    }
                    Err(e) => cc.set_error(&e.to_string()),
                }

                let left = mask_elts - num_copied;
                if left > 0 {
                    // SAFETY: mask_dest + num_copied is within the buffer.
                    unsafe {
                        std::ptr::write_bytes(step.mask_dest.add(num_copied), 0, left);
                    }
                }
                if add_eos && eos / 32 < mask_elts {
                    // SAFETY: eos / 32 < mask_elts, so this is within bounds.
                    unsafe {
                        *step.mask_dest.add(eos / 32) |= 1 << (eos % 32);
                    }
                }
            }
            Ok(())
        }));

        if let Err(e) = result {
            // A panic escaped from compute_mask despite inner catch_unwind —
            // record it on the constraint handle so the caller can observe it.
            // SAFETY: `step.constraint` is non-null (checked at the top of this
            // closure, before catch_unwind). The aliasing guarantee still holds.
            let cc = unsafe { &mut *step.constraint };
            cc.set_error(&e.to_string());
        }
    });
}

pub(crate) fn par_compute_mask(
    constraints: Vec<LlgConstraintStep>,
    user_data: *const c_void,
    done_cb: LlgCallback,
) {
    // Wrap `user_data` in a `Send`-capable newtype immediately so the raw
    // `*const c_void` is never captured by the closure sent to rayon.
    let user_data = SendPtr(user_data);

    if let Some(cb) = done_cb {
        // `cb` is a plain function pointer (Copy), so moving it into the
        // spawned task still leaves a usable copy here for the failure path.
        // Guard the spawn itself: if `rayon::spawn` panics (e.g. the global
        // thread pool fails to build), the task never runs and would never
        // invoke `cb`, deadlocking the caller. Catch that and invoke `cb`
        // synchronously instead — guaranteeing it fires exactly once.
        let spawned = panic_utils::catch_unwind(AssertUnwindSafe(|| {
            rayon::spawn(move || {
                par_compute_mask_inner(constraints);
                cb(user_data.as_ptr());
            });
            Ok(())
        }));
        if spawned.is_err() {
            cb(user_data.as_ptr());
        }
    } else {
        par_compute_mask_inner(constraints);
    }
}
