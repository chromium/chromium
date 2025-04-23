pub struct Rng {
    state: usize,
}

impl Rng {
    pub fn new(seed: usize) -> Self {
        Self {
            state: if seed == 0 { 13 } else { seed },
        }
    }

    pub fn gen(&mut self) -> usize {
        // xor-shift algorithm
        #[cfg(target_pointer_width = "32")]
        {
            let mut x = self.state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            self.state = x;
            x
        }
        #[cfg(target_pointer_width = "64")]
        {
            let mut x = self.state;
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            self.state = x;
            x
        }
    }

    pub fn gen_up_to(&mut self, mx: usize) -> usize {
        let mut mask = 1;
        while mask < mx {
            mask = (mask << 1) | 1;
        }
        loop {
            let r = self.gen() & mask;
            if r <= mx {
                return r;
            }
        }
    }
}
