// Copyright (c) 2016 Stu Small
//
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT
// license <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. All files in the project carrying such notice may not be copied,
// modified, or distributed except according to those terms.

extern crate byteorder;

use std::io::Read;
use byteorder::{LittleEndian, ByteOrder};

pub mod murmur3_32;

pub fn murmur3_32<T :Read>(source: &mut T, seed: u32) -> u32 {
    const C1: u32 = 0x85ebca6b;
    const C2: u32 = 0xc2b2ae35;
    const R1: u32 = 16;
    const R2: u32 = 13;
    const M: u32 = 5;
    const N: u32 = 0xe6546b64;
    let mut hash = seed;
    let mut buf = [0; 4];
    let mut processed: u32 = 0;
    loop {
        match source.read(&mut buf[..]) {
            Ok(size) => {
                match size {
                    4 => {
                        let k = LittleEndian::read_u32(&buf);
                        hash ^= calc_k(k);
                        hash = hash.rotate_left(R2);
                        hash = (hash.wrapping_mul(M)).wrapping_add(N);
                    }
                    3 => {
                        let k: u32 = ((buf[2] as u32) << 16) | ((buf[1] as u32) << 8) |
                                     (buf[0] as u32);
                        hash ^= calc_k(k);
                    }
                    2 => {
                        let k: u32 = ((buf[1] as u32) << 8) | (buf[0] as u32);
                        hash ^= calc_k(k);
                    }
                    1 => {
                        let k: u32 = buf[0] as u32;
                        hash ^= calc_k(k);
                    }
                    0 => {
                        hash ^= (processed) as u32;
                        hash ^= hash.wrapping_shr(R1);
                        hash = hash.wrapping_mul(C1);
                        hash ^= hash.wrapping_shr(R2);
                        hash = hash.wrapping_mul(C2);
                        hash ^= hash.wrapping_shr(R1);
                        return hash;
                    }
                    _ => panic!("Invalid read size!"),
                };
                processed += size as u32;
            }
            Err(e) => panic!(e),
        }
    }
}

fn calc_k(k: u32) -> u32 {
    const C1: u32 = 0xcc9e2d51;
    const C2: u32 = 0x1b873593;
    const R1: u32 = 15;
    k.wrapping_mul(C1).rotate_left(R1).wrapping_mul(C2)
}

pub fn murmur3_x86_128<T :Read>(source: &mut T, seed: u32, out: &mut [u8]) {
    const C1: u32 = 0x239b961b;
    const C2: u32 = 0xab0e9789;
    const C3: u32 = 0x38b34ae5;
    const C4: u32 = 0xa1e38b93;
    const C5: u32 = 0x561ccd1b;
    const C6: u32 = 0x0bcaa747;
    const C7: u32 = 0x96cd1c35;
    const C8: u32 = 0x32ac3b17;
    const M: u32 = 5;

    let mut h1: u32 = seed;
    let mut h2: u32 = seed;
    let mut h3: u32 = seed;
    let mut h4: u32 = seed;

    let mut buf = [0; 16];
    let mut processed: u32 = 0;

    if out.len() < 16 {
        panic!("Invalid out buffer size");
    }

    loop {
        match source.read(&mut buf[..]) {
            Ok(size) => {
                match size {
                    16 => {
                        let k1: u32 = LittleEndian::read_u32(&buf[0..4]);
                        let k2: u32 = LittleEndian::read_u32(&buf[4..8]);
                        let k3: u32 = LittleEndian::read_u32(&buf[8..12]);
                        let k4: u32 = LittleEndian::read_u32(&buf[12..16]);
                        h1 ^= k1.wrapping_mul(C1).rotate_left(15).wrapping_mul(C2);
                        h1 = h1.rotate_left(19).wrapping_add(h2).wrapping_mul(M).wrapping_add(C5);
                        h2 ^= k2.wrapping_mul(C2).rotate_left(16).wrapping_mul(C3);
                        h2 = h2.rotate_left(17).wrapping_add(h3).wrapping_mul(M).wrapping_add(C6);
                        h3 ^= k3.wrapping_mul(C3).rotate_left(17).wrapping_mul(C4);
                        h3 = h3.rotate_left(15).wrapping_add(h4).wrapping_mul(M).wrapping_add(C7);
                        h4 ^= k4.wrapping_mul(C4).rotate_left(18).wrapping_mul(C1);
                        h4 = h4.rotate_left(13).wrapping_add(h1).wrapping_mul(M).wrapping_add(C8);
                    }
                    13...15 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_u32(&buf[0..4]));
                        h2 ^= process_h2_k_x86(LittleEndian::read_u32(&buf[4..8]));
                        h3 ^= process_h3_k_x86(LittleEndian::read_u32(&buf[8..12]));
                        h4 ^= process_h4_k_x86(LittleEndian::read_uint(&buf[12..size], size -12) as u32);
                    }
                    12 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_u32(&buf[0..4]));
                        h2 ^= process_h2_k_x86(LittleEndian::read_u32(&buf[4..8]));
                        h3 ^= process_h3_k_x86(LittleEndian::read_u32(&buf[8..12]));
                    }
                    9...11 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_u32(&buf[0..4]));
                        h2 ^= process_h2_k_x86(LittleEndian::read_u32(&buf[4..8]));
                        h3 ^= process_h3_k_x86(LittleEndian::read_uint(&buf[8..size], size -8) as u32);
                    }
                    8 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_u32(&buf[0..4]));
                        h2 ^= process_h2_k_x86(LittleEndian::read_u32(&buf[4..8]));
                    }
                    5...7 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_u32(&buf[0..4]));
                        h2 ^= process_h2_k_x86(LittleEndian::read_uint(&buf[4..size], size-4) as u32);
                    }
                    4 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_u32(&buf));
                    }
                    1...3 => {
                        h1 ^= process_h1_k_x86(LittleEndian::read_uint(&buf, size) as u32);
                    }
                    0 => {
                        h1 ^= processed;
                        h2 ^= processed;
                        h3 ^= processed;
                        h4 ^= processed;
                        h1 = h1.wrapping_add(h2);
                        h1 = h1.wrapping_add(h3);
                        h1 = h1.wrapping_add(h4);
                        h2 = h2.wrapping_add(h1);
                        h3 = h3.wrapping_add(h1);
                        h4 = h4.wrapping_add(h1);
                        h1 = fmix32(h1);
                        h2 = fmix32(h2);
                        h3 = fmix32(h3);
                        h4 = fmix32(h4);
                        h1 = h1.wrapping_add(h2);
                        h1 = h1.wrapping_add(h3);
                        h1 = h1.wrapping_add(h4);
                        h2 = h2.wrapping_add(h1);
                        h3 = h3.wrapping_add(h1);
                        h4 = h4.wrapping_add(h1);
                        LittleEndian::write_u32(&mut out[0..], h1);
                        LittleEndian::write_u32(&mut out[4..], h2);
                        LittleEndian::write_u32(&mut out[8..], h3);
                        LittleEndian::write_u32(&mut out[12..], h4);
                        return;
                    }
                    _ => {
                        panic!("Invalid read!");
                    }
                }
                processed += size as u32;
            }
            Err(e) => panic!(e),
        }
    }
}

fn process_h1_k_x86(k: u32) -> u32 {
    const C1: u32 = 0x239b961b;
    const C2: u32 = 0xab0e9789;
    k.wrapping_mul(C1).rotate_left(15).wrapping_mul(C2)
}

fn process_h2_k_x86(k: u32) -> u32 {
    const C2: u32 = 0xab0e9789;
    const C3: u32 = 0x38b34ae5;
    k.wrapping_mul(C2).rotate_left(16).wrapping_mul(C3)
}

fn process_h3_k_x86(k: u32) -> u32 {
    const C3: u32 = 0x38b34ae5;
    const C4: u32 = 0xa1e38b93;
    k.wrapping_mul(C3).rotate_left(17).wrapping_mul(C4)
}

fn process_h4_k_x86(k: u32) -> u32 {
    const C1: u32 = 0x239b961b;
    const C4: u32 = 0xa1e38b93;
    k.wrapping_mul(C4).rotate_left(18).wrapping_mul(C1)
}

fn fmix32(k: u32) -> u32 {
    const C1: u32 = 0x85ebca6b;
    const C2: u32 = 0xc2b2ae35;
    const R1: u32 = 16;
    const R2: u32 = 13;
    let mut tmp = k;
    tmp ^= tmp >> R1;
    tmp = tmp.wrapping_mul(C1);
    tmp ^= tmp >> R2;
    tmp = tmp.wrapping_mul(C2);
    tmp ^= tmp >> R1;
    tmp
}


pub fn murmur3_x64_128<T:Read>(source: &mut T, seed: u32, out: &mut [u8]) {
    const C1: u64 = 0x52dce729;
    const C2: u64 = 0x38495ab5;
    const R1: u32 = 27;
    const R2: u32 = 31;
    const M: u64 = 5;
    let mut h1: u64 = seed as u64;
    let mut h2: u64 = seed as u64;
    let mut buf = [0; 16];
    let mut processed: u32 = 0;
    if out.len() < 16 {
        panic!("Invalid out buffer size");
    }
    loop {
        match source.read(&mut buf[..]) {
            Ok(size) => {
                match size {
                    16 => {
                        let k1 = LittleEndian::read_u64(&buf[0..8]);
                        let k2 = LittleEndian::read_u64(&buf[8..]);
                        h1 ^= process_h1_k_x64(k1);
                        h1 = h1.rotate_left(R1).wrapping_add(h2).wrapping_mul(M).wrapping_add(C1);
                        h2 ^= process_h2_k_x64(k2);
                        h2 = h2.rotate_left(R2).wrapping_add(h1).wrapping_mul(M).wrapping_add(C2);
                    }
                    9...15 => {
                        h1 ^= process_h1_k_x64(LittleEndian::read_u64(&buf[0..8]));
                        h2 ^= process_h2_k_x64(LittleEndian::read_uint(&buf[8..], size - 8));
                    }
                    8 => {
                        h1 ^= process_h1_k_x64(LittleEndian::read_u64(&buf));
                    }
                    1...7 => {
                        h1 ^= process_h1_k_x64(LittleEndian::read_uint(&buf, size));
                    }
                    0 => {
                        h1 ^= processed as u64;
                        h2 ^= processed as u64;
                        h1 = h1.wrapping_add(h2);
                        h2 = h2.wrapping_add(h1);
                        h1 = fmix64(h1);
                        h2 = fmix64(h2);
                        h1 = h1.wrapping_add(h2);
                        h2 = h2.wrapping_add(h1);
                        LittleEndian::write_u64(&mut out[0..], h1);
                        LittleEndian::write_u64(&mut out[8..], h2);
                        return;
                    }
                    _ => {
                        panic!("Invalid read");
                    }
                }
                processed += size as u32;
            }
            Err(e) => panic!(e),
        }
    }
}

fn process_h1_k_x64(k: u64) -> u64 {
    const C1: u64 = 0x87c37b91114253d5;
    const C2: u64 = 0x4cf5ad432745937f;
    const R: u32 = 31;
    k.wrapping_mul(C1).rotate_left(R).wrapping_mul(C2)
}

fn process_h2_k_x64(k: u64) -> u64 {
    const C1: u64 = 0x87c37b91114253d5;
    const C2: u64 = 0x4cf5ad432745937f;
    const R: u32 = 33;
    k.wrapping_mul(C2).rotate_left(R).wrapping_mul(C1)
}

fn fmix64(k: u64) -> u64 {
    const C1: u64 = 0xff51afd7ed558ccd;
    const C2: u64 = 0xc4ceb9fe1a85ec53;
    const R: u32 = 33;
    let mut tmp = k;
    tmp ^= tmp >> R;
    tmp = tmp.wrapping_mul(C1);
    tmp ^= tmp >> R;
    tmp = tmp.wrapping_mul(C2);
    tmp ^= tmp >> R;
    tmp
}
