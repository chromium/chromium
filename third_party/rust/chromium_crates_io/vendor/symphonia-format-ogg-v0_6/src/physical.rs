// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
use std::collections::{BTreeMap, BTreeSet};
use std::io::{Seek, SeekFrom};

use symphonia_core::errors::Result;
use symphonia_core::io::{MediaSourceStream, ReadBytes, ScopedStream, SeekBuffered};

use super::logical::{InspectState, LogicalStream};
use super::page::*;

use log::debug;

pub fn probe_stream_start(
    reader: &mut MediaSourceStream<'_>,
    pages: &mut PageReader,
    streams: &mut BTreeMap<u32, LogicalStream>,
    byte_range_start: u64,
) -> Result<()> {
    // Save the original reader position. This position is immediately after the end of the current
    // page. If the current page is not consumed and no additional pages are read, then the reader
    // needs to be repositioned here.
    let original_pos = reader.pos();

    // However, if the current page is consumed while probing and a new page is read, then we need
    // to be able to return to the start of the current page so that it may be re-read. This
    // position is indicated by `byte_range_start`. To be able to do this using the seekback buffer,
    // no more than OGG_PAGE_MAX_SIZE bytes should be read from `byte_range_start`. Do not probe if
    // that's not possible.
    if original_pos < byte_range_start || original_pos - byte_range_start > OGG_PAGE_MAX_SIZE as u64
    {
        return Ok(());
    }

    // Scope the reader the prevent overruning the seekback region.
    let mut scoped_reader =
        ScopedStream::new(reader, OGG_PAGE_MAX_SIZE as u64 - (original_pos - byte_range_start));

    let mut probed = BTreeSet::<u32>::new();
    let mut page_changed = false;

    // Examine the first bitstream page of each logical stream within the physical stream to
    // determine the number of leading samples, and start time. This function is called assuming
    // the page reader is on the first bitstream page within the physical stream.
    loop {
        let page = pages.page();

        // If the page does not belong to the current physical stream, break out.
        let stream = if let Some(stream) = streams.get_mut(&page.header.serial) {
            stream
        }
        else {
            break;
        };

        // If the stream hasn't been marked as probed.
        if !probed.contains(&page.header.serial) {
            // Probe the first page of the logical stream.
            stream.inspect_start_page(&page);
            // Mark the logical stream as probed.
            probed.insert(page.header.serial);
        }

        // If all logical streams were probed, break out immediately.
        if probed.len() >= streams.len() {
            break;
        }

        // Read the next page.
        match pages.try_next_page(&mut scoped_reader) {
            Ok(_) => {
                page_changed = true;
            }
            _ => break,
        };
    }

    let reader = scoped_reader.into_inner();

    if page_changed {
        // Restore the original page that was read when entering this probe function.
        reader.seek_buffered(byte_range_start);
        pages.next_page(reader)
    }
    else {
        // Just restore the original reader position, the page has not changed.
        reader.seek_buffered(original_pos);
        Ok(())
    }
}

pub fn probe_stream_end(
    reader: &mut MediaSourceStream<'_>,
    pages: &mut PageReader,
    streams: &mut BTreeMap<u32, LogicalStream>,
    byte_range_start: u64,
    byte_range_end: u64,
) -> Result<Option<u64>> {
    // Number of bytes to linearly scan. We assume the OGG maximum page size for each logical
    // stream.
    let linear_scan_len = (streams.len() * OGG_PAGE_MAX_SIZE) as u64;

    // Optimization: Try a linear scan of the last few pages first. This will cover all
    // non-chained physical streams, which is the majority of cases.
    if byte_range_end >= linear_scan_len && byte_range_start <= byte_range_end - linear_scan_len {
        reader.seek(SeekFrom::Start(byte_range_end - linear_scan_len))?;
    }
    else {
        reader.seek(SeekFrom::Start(byte_range_start))?;
    }

    pages.next_page(reader)?;

    let result = scan_stream_end(reader, pages, streams, byte_range_end);

    // If there are no pages belonging to the current physical stream at the end of the media
    // source stream, then one or more physical streams are chained. Use a bisection method to find
    // the end of the current physical stream.
    let result = if result.is_none() {
        debug!("media source stream is chained, bisecting end of physical stream");

        let mut start = byte_range_start;
        let mut end = byte_range_end;

        loop {
            let mid = (end + start) / 2;
            reader.seek(SeekFrom::Start(mid))?;

            match pages.next_page(reader) {
                Ok(_) => (),
                _ => break,
            }

            let header = pages.header();

            if streams.contains_key(&header.serial) {
                start = mid;
            }
            else {
                end = mid;
            }

            if end - start < linear_scan_len {
                break;
            }
        }

        // Scan the last few pages of the physical stream.
        reader.seek(SeekFrom::Start(start))?;

        pages.next_page(reader)?;

        scan_stream_end(reader, pages, streams, end)
    }
    else {
        result
    };

    // Restore the original page
    reader.seek(SeekFrom::Start(byte_range_start))?;
    pages.next_page(reader)?;

    Ok(result)
}

fn scan_stream_end(
    reader: &mut MediaSourceStream<'_>,
    pages: &mut PageReader,
    streams: &mut BTreeMap<u32, LogicalStream>,
    byte_range_end: u64,
) -> Option<u64> {
    let scoped_len = byte_range_end - reader.pos();

    let mut scoped_reader = ScopedStream::new(reader, scoped_len);

    let mut upper_pos = None;

    // One inspection state is required per logical stream.
    let mut states: BTreeMap<u32, InspectState> = Default::default();

    // Read pages until the provided end position or a new physical stream starts.
    loop {
        let page = pages.page();

        // If the page does not belong to the current physical stream, then exit. The extent of the
        // physical stream has been found.
        let Some(stream) = streams.get_mut(&page.header.serial)
        else {
            break;
        };

        // Inspect the end page.
        stream.inspect_end_page(states.entry(page.header.serial).or_default(), &page);

        // The new end of the physical stream is the position after this page.
        upper_pos = Some(scoped_reader.pos());

        // Read to the next page. Exit on error.
        if pages.next_page(&mut scoped_reader).is_err() {
            break;
        }
    }

    upper_pos
}
