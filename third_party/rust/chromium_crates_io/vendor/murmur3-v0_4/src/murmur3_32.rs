
use std::hash::Hasher;
use byteorder::{LittleEndian, ByteOrder};


const C1: u32 = 0x85ebca6b;
const C2: u32 = 0xc2b2ae35;
const R1: u32 = 16;
const R2: u32 = 13;
const M: u32 = 5;
const N: u32 = 0xe6546b64;


pub struct MurmurHasher{
    state: u32,
    buf: [u8; 4],
    index: usize,
    processed: u32
}

impl Hasher for MurmurHasher{
    fn write(&mut self, bytes: &[u8]){
        self.processed += bytes.len() as u32;

        let to_split = if self.index == 0 {
            bytes
        }else{ 
            if bytes.len() + self.index >= 4 {
                let t = bytes.split_at(4 - self.index);
                for i in 0 .. (4 - self.index) {
                    self.buf[self.index + i] = t.0[i];
                }
                let x = self.buf.clone();//TODO: we can remove this clone
                self.state = process_4_bytes(self.state, &x);
                self.index = 0;
                t.1
            }else{
                bytes
            }
        };

        for chunk in to_split.chunks(4) {
            if chunk.len() == 4 {
                self.state = process_4_bytes(self.state, chunk);
            }else{
                self.push_odd_bytes(chunk);
            }
        }
    }

    fn finish(&self) -> u64 {
        let state = if self.index != 0 {
             process_odd_bytes(self.state, self.index, &self.buf)
        }else{
            self.state
        };
        finish(state, self.processed) as u64
    }
}


impl Default for MurmurHasher {
    fn default() -> Self{
        MurmurHasher{
            state: 0,
            buf: [0; 4],
            index: 0,
            processed: 0
        }
    }
}

impl MurmurHasher {
    fn push_odd_bytes(&mut self, to_push: &[u8]){
        for x in to_push {
            self.buf[self.index] = *x;
            self.index += 1;
        }
    }
}

fn process_4_bytes(state: u32, chunk:&[u8]) -> u32{
    let mut state = state;
    let k = LittleEndian::read_u32(&chunk);
    state ^= calc_k(k);
    state = state.rotate_left(R2);
    state = (state.wrapping_mul(M)).wrapping_add(N);
    state
}

fn process_odd_bytes(state: u32, index: usize, buf:&[u8]) -> u32{
    let mut state = state;
    match index {
        3 => {
            let k: u32 = ((buf[2] as u32) << 16) | ((buf[1] as u32) << 8) |
                (buf[0] as u32);
            state ^= calc_k(k);
            state
        }
        2 => {
            let k: u32 = ((buf[1] as u32) << 8) | (buf[0] as u32);
            state ^= calc_k(k);
            state
        }
        1 => {
            let k: u32 = buf[0] as u32;
            state ^= calc_k(k);
            state
        }
        _ => {
            panic!("");
        }
    }
}

fn calc_k(k: u32) -> u32 {
    const C1: u32 = 0xcc9e2d51;
    const C2: u32 = 0x1b873593;
    const R1: u32 = 15;
    k.wrapping_mul(C1).rotate_left(R1).wrapping_mul(C2)
}

fn finish(state: u32, processed: u32) -> u32 {
    let mut hash = state;
    hash ^= processed as u32;
    hash ^= hash.wrapping_shr(R1);
    hash = hash.wrapping_mul(C1);
    hash ^= hash.wrapping_shr(R2);
    hash = hash.wrapping_mul(C2);
    hash ^= hash.wrapping_shr(R1);
    hash
}