// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::{Endianness, JxlColorType, JxlDataFormat, JxlOutputBuffer},
    error::Result,
    headers::Orientation,
    image::{DataTypeTag, Image, ImageDataType, Rect},
    render::{SimpleRenderPipeline, buffer_splitter::BufferSplitter},
    util::{
        ShiftRightCeil,
        test::check_equal_images,
        tracing_wrappers::{instrument, trace},
    },
};
use rand::SeedableRng;

use super::{
    RenderPipeline, RenderPipelineBuilder, RenderPipelineInOutStage, RenderPipelineInPlaceStage,
    internal::Stage, stages::ExtendToImageDimensionsStage,
};

pub(super) trait RenderPipelineTestableStage<V> {
    type InputT: ImageDataType;
    type OutputT: ImageDataType;
    fn into_stage(self) -> Stage<Image<f64>>;
}

impl RenderPipelineTestableStage<()> for ExtendToImageDimensionsStage {
    type InputT = f32;
    type OutputT = f32;
    fn into_stage(self) -> Stage<Image<f64>> {
        Stage::Extend(self)
    }
}

impl<T: RenderPipelineInOutStage> RenderPipelineTestableStage<()> for T {
    type InputT = T::InputT;
    type OutputT = T::OutputT;
    fn into_stage(self) -> Stage<Image<f64>> {
        Stage::InOut(Box::new(self))
    }
}

pub(super) struct Empty {}

impl<T: RenderPipelineInPlaceStage> RenderPipelineTestableStage<Empty> for T {
    type InputT = T::Type;
    type OutputT = T::Type;
    fn into_stage(self) -> Stage<Image<f64>> {
        Stage::InPlace(Box::new(self))
    }
}

fn extract_group_rect<T: ImageDataType>(
    image: &Image<T>,
    group_id: usize,
    log_group_size: (usize, usize),
) -> Result<Image<T>> {
    let xgroups = image.size().0.shrc(log_group_size.0);
    let group = (group_id % xgroups, group_id / xgroups);
    let origin = (group.0 << log_group_size.0, group.1 << log_group_size.1);
    let size = (
        (image.size().0 - origin.0).min(1 << log_group_size.0),
        (image.size().1 - origin.1).min(1 << log_group_size.1),
    );
    trace!(
        "making rect {}x{}+{}+{} for group {group_id} in image of size {:?}, log group sizes {:?}",
        size.0,
        size.1,
        origin.0,
        origin.1,
        image.size(),
        log_group_size
    );
    let rect = image.get_rect(Rect { origin, size });
    let mut out = Image::new(rect.size()).unwrap();
    for y in 0..out.size().1 {
        out.row_mut(y).copy_from_slice(rect.row(y));
    }
    Ok(out)
}

fn make_and_run_simple_pipeline_impl<InputT: ImageDataType, OutputT: ImageDataType>(
    stage: Stage<Image<f64>>,
    input_images: &[Image<InputT>],
    image_size: (usize, usize),
    downsampling_shift: usize,
    chunk_size: usize,
) -> Result<Vec<Image<OutputT>>> {
    let shift = stage.shift();
    let final_size = stage.new_size(image_size);
    const LOG_GROUP_SIZE: usize = 8;
    let all_channels = (0..input_images.len()).collect::<Vec<_>>();
    let uses_channel: Vec<_> = all_channels
        .iter()
        .map(|x| stage.uses_channel(*x))
        .collect();
    let mut pipeline = RenderPipelineBuilder::<SimpleRenderPipeline>::new_with_chunk_size(
        input_images.len(),
        image_size,
        downsampling_shift,
        LOG_GROUP_SIZE,
        1,
        chunk_size,
    )
    .add_stage_internal(stage)?;

    let jxl_data_type = match OutputT::DATA_TYPE_ID {
        DataTypeTag::U8 | DataTypeTag::I8 => JxlDataFormat::U8 { bit_depth: 8 },
        DataTypeTag::U16 | DataTypeTag::I16 => JxlDataFormat::U16 {
            bit_depth: 16,
            endianness: Endianness::native(),
        },
        DataTypeTag::F32 => JxlDataFormat::f32(),
        DataTypeTag::F16 => JxlDataFormat::F16 {
            endianness: Endianness::native(),
        },
        _ => unimplemented!("unsupported data type"),
    };

    for i in 0..input_images.len() {
        pipeline = pipeline.add_save_stage(
            &[i],
            Orientation::Identity,
            i,
            JxlColorType::Grayscale,
            jxl_data_type,
            false,
        )?;
    }
    let mut pipeline = pipeline.build()?;

    let num_groups = image_size.0.shrc(LOG_GROUP_SIZE) * image_size.1.shrc(LOG_GROUP_SIZE);

    let mut outputs = (0..input_images.len())
        .map(|_| Image::<OutputT>::new(final_size))
        .collect::<Result<Vec<_>, _>>()?;

    let mut buf_ptrs: Vec<_> = outputs
        .iter_mut()
        .map(|x| {
            let size = x.size();
            Some(JxlOutputBuffer::from_image_rect_mut(
                x.get_rect_mut(Rect {
                    size,
                    origin: (0, 0),
                })
                .into_raw(),
            ))
        })
        .collect();

    let mut buffer_splitter = BufferSplitter::new(&mut buf_ptrs);

    for g in 0..num_groups {
        for &c in all_channels.iter() {
            let log_group_size = if uses_channel[c] {
                (
                    LOG_GROUP_SIZE - shift.0 as usize,
                    LOG_GROUP_SIZE - shift.1 as usize,
                )
            } else {
                (LOG_GROUP_SIZE, LOG_GROUP_SIZE)
            };
            pipeline.set_buffer_for_group(
                c,
                g,
                1,
                extract_group_rect(&input_images[c], g, log_group_size)?,
                &mut buffer_splitter,
            )?;
        }
    }

    Ok(outputs)
}

pub(super) fn make_and_run_simple_pipeline<S: RenderPipelineTestableStage<V>, V>(
    stage: S,
    input_images: &[Image<S::InputT>],
    image_size: (usize, usize),
    downsampling_shift: usize,
    chunk_size: usize,
) -> Result<Vec<Image<S::OutputT>>> {
    make_and_run_simple_pipeline_impl(
        stage.into_stage(),
        input_images,
        image_size,
        downsampling_shift,
        chunk_size,
    )
}

#[instrument(skip(make_stage), err)]
pub(super) fn test_stage_consistency<S: RenderPipelineTestableStage<V>, V>(
    make_stage: impl Fn() -> S,
    image_size: (usize, usize),
    num_image_channels: usize,
) -> Result<()> {
    let mut rng = rand_xorshift::XorShiftRng::seed_from_u64(0);
    let stage = make_stage().into_stage();
    let images: Result<Vec<_>> = (0..num_image_channels)
        .map(|c| {
            let size = if stage.uses_channel(c) {
                (
                    image_size.0.shrc(stage.shift().0),
                    image_size.1.shrc(stage.shift().1),
                )
            } else {
                image_size
            };
            Image::new_random(size, &mut rng)
        })
        .collect();
    let images = images?;

    let base_output = make_and_run_simple_pipeline_impl::<S::InputT, S::OutputT>(
        stage, &images, image_size, 0, 256,
    )?;

    arbtest::arbtest(move |p| {
        let chunk_size = p.arbitrary::<u16>()?.saturating_add(1) as usize;
        let output = make_and_run_simple_pipeline_impl::<S::InputT, S::OutputT>(
            make_stage().into_stage(),
            &images,
            image_size,
            0,
            chunk_size,
        )
        .unwrap_or_else(|_| panic!("error running pipeline with chunk size {chunk_size}"));

        for (o, bo) in output.iter().zip(base_output.iter()) {
            check_equal_images(bo, o);
        }

        Ok(())
    });
    Ok(())
}
