// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::collections::HashMap;

use crate::error::Result;
use crate::image::{Image, ImageDataType, OwnedRawImage};
use crate::util::sync::Mutex;

struct BufferBucket {
    images: Mutex<Vec<OwnedRawImage>>,
}

pub struct BufferRecycler {
    buckets: HashMap<(usize, usize), BufferBucket>,
}

impl BufferRecycler {
    pub fn new(group_dim: usize) -> Self {
        let ty_sizes = [2, 4];
        let group_shifts = [0, 1, 2, 3];
        let modular_small_buffer_side = 4;
        // actually scaled by 4.
        let small_pipeline_buffer_sides = [1, 2, 3, 4, 5, 6, 7, 8, 9];
        let mut buckets = HashMap::new();
        let mut add = |x, y| {
            buckets.entry((x, y)).or_insert(BufferBucket {
                images: Mutex::new(vec![]),
            });
        };
        for ts in ty_sizes {
            for gs in group_shifts {
                let gsz = group_dim >> gs;
                for ogs in group_shifts {
                    add(gsz * ts, group_dim >> ogs);
                }
                add(gsz * ts, modular_small_buffer_side);
                add(modular_small_buffer_side * ts, gsz);
                for s in small_pipeline_buffer_sides {
                    add(gsz * ts, s * 4);
                    add(s * 4 * ts, gsz);
                }
            }
        }
        Self { buckets }
    }

    pub fn get_buffer<T: ImageDataType>(&self, size: (usize, usize)) -> Result<Image<T>> {
        self.get_raw_buffer((std::mem::size_of::<T>() * size.0, size.1))
            .map(Image::from_raw)
    }

    pub fn get_raw_buffer(&self, byte_size: (usize, usize)) -> Result<OwnedRawImage> {
        self.buckets
            .get(&byte_size)
            .and_then(|x| x.images.lock().unwrap().pop())
            .map(Ok)
            .unwrap_or_else(|| OwnedRawImage::new(byte_size))
    }

    pub fn recycle_buffer<T: ImageDataType>(&self, buffer: Image<T>) {
        let buffer = buffer.into_raw();
        self.recycle_raw_buffer(buffer);
    }

    pub fn recycle_raw_buffer(&self, buffer: OwnedRawImage) {
        let byte_size = buffer.byte_size();
        if let Some(b) = self.buckets.get(&byte_size) {
            b.images.lock().unwrap().push(buffer);
        }
    }
}

impl std::fmt::Debug for BufferRecycler {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("BufferRecycler").finish()
    }
}
