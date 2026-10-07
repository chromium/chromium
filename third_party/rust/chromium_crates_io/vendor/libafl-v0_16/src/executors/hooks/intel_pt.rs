use core::fmt::Debug;

pub use libafl_intelpt::{CoverageEntry, IntelPT, PAGE_SIZE, PtImage};
use serde::Serialize;
use typed_builder::TypedBuilder;

use crate::executors::hooks::ExecutorHook;

/// Hook to enable Intel Processor Trace (PT) tracing
#[derive(Debug, TypedBuilder)]
pub struct IntelPTHook<'a, T> {
    intel_pt: IntelPT<'a>,
    map_ptr: *mut T,
    map_len: usize,
}

impl<I, S, T> ExecutorHook<I, S> for IntelPTHook<'_, T>
where
    S: Serialize,
    T: CoverageEntry,
{
    fn init(&mut self, _state: &mut S) {}

    fn pre_exec(&mut self, _state: &mut S, _input: &I) {
        self.intel_pt.enable_tracing().unwrap();
    }

    fn post_exec(&mut self, _state: &mut S, _input: &I) {
        let pt = &mut self.intel_pt;
        pt.disable_tracing().unwrap();

        let _ = pt
            .decode_traces_into_map(self.map_ptr, self.map_len)
            .inspect_err(|e| log::warn!("Intel PT trace decoding failed: {e}"));
        #[cfg(feature = "intel_pt_export_raw")]
        {
            let _ = pt
                .dump_last_trace_to_file()
                .inspect_err(|e| log::warn!("Intel PT trace save to file failed: {e}"));
        }
    }
}
