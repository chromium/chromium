// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#[cfg(not(feature = "shuttle"))]
use std::sync::Mutex;
#[cfg(not(feature = "shuttle"))]
use std::sync::atomic::{AtomicUsize, Ordering};
#[cfg(not(feature = "shuttle"))]
use std::thread;

#[cfg(feature = "shuttle")]
use shuttle::sync::Mutex;
#[cfg(feature = "shuttle")]
use shuttle::sync::atomic::{AtomicUsize, Ordering};
#[cfg(feature = "shuttle")]
use shuttle::thread;

use crate::api::JxlParallelRunner;
use crate::error::Error;

pub struct TestParallelRunner {
    pub max_threads: usize,
}

impl JxlParallelRunner for TestParallelRunner {
    fn run(&mut self, num: usize, fun: &crate::api::JxlParallelRunnerFun<'_>) -> Result<(), Error> {
        if num <= 1 || self.max_threads <= 1 {
            for i in 0..num {
                fun(i)?;
            }
            return Ok(());
        }
        let num_threads = self.max_threads.min(num);
        let next_task = AtomicUsize::new(0);
        let error = Mutex::new(None);

        thread::scope(|s| {
            let mut handles = Vec::with_capacity(num_threads);
            for _ in 0..num_threads {
                handles.push(s.spawn(|| {
                    loop {
                        if error.lock().unwrap().is_some() {
                            break;
                        }
                        let task = next_task.fetch_add(1, Ordering::Relaxed);
                        if task >= num {
                            break;
                        }
                        if let Err(e) = fun(task) {
                            let mut err = error.lock().unwrap();
                            if err.is_none() {
                                *err = Some(e);
                            }
                            break;
                        }
                    }
                }));
            }
            for handle in handles {
                if let Err(e) = handle.join() {
                    std::panic::resume_unwind(e);
                }
            }
        });

        if let Some(err) = error.into_inner().unwrap() {
            Err(err)
        } else {
            Ok(())
        }
    }

    fn num_threads(&self) -> usize {
        self.max_threads
    }
}
