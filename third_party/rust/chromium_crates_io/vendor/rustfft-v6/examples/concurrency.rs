//! Show how to use an `FFT` object from multiple threads

use std::sync::Arc;
use std::thread;

use rustfft::num_complex::Complex32;
use rustfft::FftPlanner;

fn main() {
    // Verify that the planner is sync + send
    fn test_sync<T: Sync + Send>(val: T) -> T {
        val
    }
    let mut planner = test_sync(FftPlanner::new());
    let fft = planner.plan_fft_forward(100);

    let threads: Vec<thread::JoinHandle<_>> = (0..2)
        .map(|_| {
            let fft_copy = Arc::clone(&fft);
            thread::spawn(move || {
                let mut buffer = vec![Complex32::new(0.0, 0.0); 100];
                fft_copy.process(&mut buffer);
            })
        })
        .collect();

    for thread in threads {
        thread.join().unwrap();
    }
}
