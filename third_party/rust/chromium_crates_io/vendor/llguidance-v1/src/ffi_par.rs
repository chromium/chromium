use std::ffi::c_void;

use crate::ffi::{LlgCallback, LlgConstraintStep};

fn par_compute_mask_inner(constraints: Vec<LlgConstraintStep>) {
    use rayon::prelude::*;
    constraints.into_par_iter().for_each(|step| {
        assert!(step.mask_byte_len % 4 == 0);
        assert!(!step.mask_dest.is_null());
        let mask_elts = step.mask_byte_len / 4;

        let cc = unsafe { &mut *step.constraint };
        if let Some(constraint) = &mut cc.constraint {
            let mut num_copied = 0;
            let mut add_eos = false;
            let eos = constraint.tok_trie().eos_token() as usize;
            match constraint.compute_mask() {
                Ok(r) => {
                    if let Some(m) = r.sample_mask.as_ref() {
                        num_copied = std::cmp::min(m.len(), mask_elts);
                        unsafe {
                            std::ptr::copy_nonoverlapping(m.as_ptr(), step.mask_dest, num_copied);
                        }
                    }
                    add_eos = r.is_stop();
                }
                Err(e) => cc.set_error(&e.to_string()),
            }

            let left = mask_elts - num_copied;
            if left > 0 {
                unsafe {
                    std::ptr::write_bytes(step.mask_dest.add(num_copied), 0, left);
                }
            }
            if add_eos && eos / 32 < mask_elts {
                unsafe {
                    *step.mask_dest.add(eos / 32) |= 1 << (eos % 32);
                }
            }
        }
    });
}

pub(crate) fn par_compute_mask(
    constraints: Vec<LlgConstraintStep>,
    user_data: *const c_void,
    done_cb: LlgCallback,
) {
    struct CbData {
        user_data: *const c_void,
    }
    unsafe impl Send for CbData {}

    if let Some(cb) = done_cb {
        let ptr = CbData { user_data };
        rayon::spawn(move || {
            par_compute_mask_inner(constraints);
            cb(ptr.user_data);
            #[allow(clippy::drop_non_drop)]
            drop(ptr);
        });
    } else {
        par_compute_mask_inner(constraints);
    }
}
