use read_fonts::collections::IntSet;

use rand::Rng;

pub fn random_set(size: u32, max_value: u32) -> IntSet<u32> {
    let mut rng = rand::thread_rng();
    let mut set = IntSet::<u32>::empty();
    for _ in 0..size {
        loop {
            let candidate: u32 = rng.gen::<u32>() % max_value;
            if set.insert(candidate) {
                break;
            }
        }
    }
    set
}
