use super::stream::{DecodingError, FormatErrorInner};
use super::zlib::UnfilterBuf;
use crate::common::BytesPerPixel;
use crate::filter::{unfilter, RowFilter};
use crate::Info;

// Buffer for temporarily holding decompressed, not-yet-`unfilter`-ed rows.
pub(crate) struct UnfilteringBuffer {
    /// Vec containing the uncompressed image data currently being processed.
    data_stream: Vec<u8>,
    prev_row: PrevRow,
    /// Index in `data_stream` where the current row starts.
    /// This points at the filter type byte of the current row (i.e. the actual pixel data starts at `current_start + 1`)
    /// The pixel data is not-yet-`unfilter`-ed.
    ///
    /// `current_start` can wrap around the length.
    current_start: usize,
    /// Logical length of data that must be preserved.
    filled: usize,
    /// Length of data that can be modified.
    available: usize,
    /// The number of bytes before we shift the buffer back.
    shift_back_limit: usize,
    /// How many bytes are left to decompress into this buffer for the current frame.
    remaining_bytes: u64,
    /// To avoid always allocating a new vector in `fn unfilter_curr_row_using_scratch_buffer`.
    scratch_buffer: Vec<u8>,
}

impl UnfilteringBuffer {
    pub const GROWTH_BYTES: usize = 8 * 1024;

    /// Asserts in debug builds that all the invariants hold.  No-op in release
    /// builds.  Intended to be called after creating or mutating `self` to
    /// ensure that the final state preserves the invariants.
    #[cfg(not(debug_assertions))]
    fn debug_assert_invariants(&self) {}
    #[cfg(debug_assertions)]
    fn debug_assert_invariants(&self) {
        if let PrevRow::InPlace(prev_start) = &self.prev_row {
            debug_assert!(*prev_start <= self.current_start);
        }
        debug_assert!(self.current_start <= self.filled);
        debug_assert!(self.available <= self.filled);
        debug_assert!(self.filled <= self.data_stream.len());
    }

    /// Create a buffer tuned for filtering rows of the image type.
    pub fn new(info: &Info<'_>) -> Self {
        // We don't need all of `info` here so if that becomes a structural problem then these
        // derived constants can be extracted into a parameter struct. For instance they may be
        // adjusted according to platform hardware such as cache sizes.
        let data_stream_capacity = {
            let max_data = info
                .checked_raw_row_length()
                // In the current state this is really dependent on IDAT sizes and the compression
                // settings. We aim to avoid overallocation here, but that occurs in part due to
                // the algorithm for draining the buffer, which at the time of writing is at each
                // individual IDAT chunk boundary. So this is set for a quadratic image roughly
                // fitting into a single 4k chunk at compression.. A very arbitrary choice made
                // from (probably overfitting) a benchmark of that image size. With a different
                // algorithm we may come to different buffer uses and have to re-evaluate.
                .and_then(|v| v.checked_mul(info.height.min(128) as usize))
                // In the worst case this is additional room for use of unmasked SIMD moves. But
                // the other idea here is that the allocator generally aligns the buffer.
                .and_then(|v| checked_next_multiple_of(v, 256))
                .unwrap_or(usize::MAX);
            // We do not want to pre-allocate too much in case of a faulty image (no DOS by
            // pretending to be very very large) and also we want to avoid allocating more data
            // than we need for the image itself.
            max_data.min(128 * 1024)
        };

        let shift_back_limit = {
            // Prefer shifting by powers of two and only after having done some number of
            // lines that then become free at the end of the buffer.
            let rowlen_pot = info
                .checked_raw_row_length()
                // Ensure some number of rows are actually present before shifting back, i.e. next
                // time around we want to be able to decode them without reallocating the buffer.
                .and_then(|v| v.checked_mul(4))
                // And also, we should be able to use aligned memcopy on the whole thing. Well at
                // least that is the idea but the parameter is just benchmarking. Higher numbers
                // did not result in performance gains but lowers also, so this is fickle. Maybe
                // our shift back behavior can not be tuned very well.
                .and_then(|v| checked_next_multiple_of(v, 64))
                .unwrap_or(isize::MAX as usize);
            // But never shift back before we have a number of pages freed.
            rowlen_pot.max(128 * 1024)
        };

        let result = Self {
            data_stream: Vec::with_capacity(data_stream_capacity),
            prev_row: PrevRow::None,
            current_start: 0,
            filled: 0,
            available: 0,
            shift_back_limit,
            remaining_bytes: u64::MAX,
            scratch_buffer: Vec::new(),
        };

        result.debug_assert_invariants();
        result
    }

    /// Called to indicate that there is no previous row (e.g. when the current
    /// row is the first scanline of a given Adam7 pass).
    pub fn reset_prev_row(&mut self) {
        // Stash a previously allocated buffer (for potential reuse later)
        // rather than throwing it away when resetting `self.prev_row`.
        if let PrevRow::Scratch(buf) = &mut self.prev_row {
            self.scratch_buffer = std::mem::take(buf);
        }

        self.prev_row = PrevRow::None;
        self.debug_assert_invariants();
    }

    pub fn start_frame(&mut self, frame_bytes: u64) {
        self.data_stream.clear();
        self.prev_row = PrevRow::None;
        self.current_start = 0;
        self.filled = 0;
        self.available = 0;
        self.remaining_bytes = frame_bytes;
    }

    pub fn remaining_bytes(&self) -> u64 {
        self.remaining_bytes
    }

    /// Returns the previous (already `unfilter`-ed) row.
    pub fn prev_row(&self) -> &[u8] {
        self.prev_row
            .as_slice(&self.data_stream[..self.current_start])
    }

    /// Returns how many bytes of the current row are present in the mutable
    /// part of the buffer (32kB most recently decompressed bytes are read-only
    /// to retain the "lookback" window as needed for inflate algorithm).  If a
    /// full row is mutable, then it may be unfiltered using
    /// `unfilter_curr_row_in_place`.
    ///
    /// See also `readable_len_of_curr_row`.
    pub fn mutable_len_of_curr_row(&self) -> usize {
        self.available.saturating_sub(self.current_start)
    }

    /// Returns how many bytes of the current row have been already
    /// decompressed.  If a full row is available, then it may be unfiltered
    /// using `unfilter_curr_row_using_scratch_buffer`.
    ///
    /// See also `mutable_len_of_curr_row`.
    pub fn readable_len_of_curr_row(&self) -> usize {
        self.filled - self.current_start
    }

    /// Runs `f` on the underlying buffer.
    ///
    /// Invariants of `self` depend on the assumption that the caller will only
    /// append new bytes to the returned vector (which is indeed the behavior of
    /// `ReadDecoder` and `StreamingDecoder`).  TODO: Consider protecting the
    /// invariants by returning an append-only view of the vector
    /// (`FnMut(&[u8])`??? or maybe `std::io::Write`???).
    pub fn with_unfilled_buffer<F, T>(&mut self, f: F) -> T
    where
        F: FnOnce(&mut UnfilterBuf<'_>) -> T,
    {
        // Potentially shift the buffer left to avoid unbounded growth.
        let discard_size = self.available.min(match &self.prev_row {
            PrevRow::None | PrevRow::Scratch(_) => self.current_start,
            PrevRow::InPlace(prev_start) => *prev_start,
        });
        if discard_size >= self.shift_back_limit
            // Avoid the shift back if the buffer is still very empty. Consider how we got here: a
            // previous decompression filled the buffer, then we unfiltered, we're now refilling
            // the buffer again. The condition implies, the previous decompression filled at most
            // half the buffer. Likely the same will happen again so the following decompression
            // attempt will not yet be limited by the buffer length.
            && self.filled >= self.data_stream.len() / 2
        {
            self.shift_buffer_left(discard_size);
        }

        if self.filled + Self::GROWTH_BYTES > self.data_stream.len() {
            self.data_stream.resize(self.filled + Self::GROWTH_BYTES, 0);
        }

        if self.remaining_bytes < usize::MAX as u64
            && self.filled.saturating_add(self.remaining_bytes as usize) < self.data_stream.len()
        {
            self.data_stream
                .resize(self.filled + self.remaining_bytes as usize, 0);
        }

        let old_filled = self.filled;
        let ret = f(&mut UnfilterBuf {
            buffer: &mut self.data_stream,
            filled: &mut self.filled,
            available: &mut self.available,
        });
        assert!(self.filled >= old_filled);
        self.remaining_bytes -= (self.filled - old_filled) as u64;

        if self.remaining_bytes == 0 {
            self.available = self.filled;
        }

        self.debug_assert_invariants();
        ret
    }

    /// Shifts the contents of `self.data_stream` left,
    /// discarding the first `discard_size` bytes.
    fn shift_buffer_left(&mut self, discard_size: usize) {
        // Violating this assertion will clobber the immutable "lookback"
        // window that needs to be maintained for decompressor.
        assert!(discard_size <= self.available);

        // We have to relocate the data to the start of the buffer. Benchmarking suggests that
        // the codegen for an unbounded range is better / different than the one for a bounded
        // range. We prefer the former if the data overhead is not too high. `16` was
        // determined experimentally and might be system (memory) dependent. There's also the
        // question if we could be a little smarter and avoid crossing page boundaries when
        // that is not required. Alas, microbenchmarking TBD.
        if let Some(16..) = self.data_stream.len().checked_sub(self.filled) {
            self.data_stream.copy_within(discard_size..self.filled, 0);
        } else {
            self.data_stream.copy_within(discard_size.., 0);
        }

        // The data kept its relative position to `filled` which now lands exactly at
        // the distance between prev_start and filled.
        self.current_start -= discard_size;
        self.available -= discard_size;
        self.filled -= discard_size;
        match &mut self.prev_row {
            PrevRow::None | PrevRow::Scratch(_) => (),
            PrevRow::InPlace(prev_start) => *prev_start -= discard_size,
        }
    }

    fn curr_row_filter(&self) -> Result<RowFilter, DecodingError> {
        let filter = self.data_stream[self.current_start];
        RowFilter::from_u8(filter).ok_or(DecodingError::Format(
            FormatErrorInner::UnknownFilterMethod(filter).into(),
        ))
    }

    /// Runs `unfilter` on the current row, and then shifts rows so that the
    /// current row becomes the previous row.
    ///
    /// `unfilter` will mutate the current row in-place, and therefore the
    /// caller should first consult `mutable_len_of_curr_row` to check if all
    /// bytes of the current row are indeed mutable.
    pub fn unfilter_curr_row_in_place(
        &mut self,
        rowlen: usize,
        bpp: BytesPerPixel,
    ) -> Result<(), DecodingError> {
        debug_assert!(rowlen >= 2); // 1 byte for `RowFilter` and at least 1 byte of pixel data.

        // Violating the assertion below would clobber the bytes in the
        // "lookback" window.
        debug_assert!(self.mutable_len_of_curr_row() >= rowlen);

        let filter = self.curr_row_filter()?;
        let (prev, row) = self.data_stream.split_at_mut(self.current_start);
        let prev: &[u8] = self.prev_row.as_slice(prev);
        let row = &mut row[1..rowlen]; // Skip the `RowFilter` byte.
        debug_assert!(prev.is_empty() || prev.len() == row.len());

        unfilter(filter, bpp, prev, row);

        self.reset_prev_row();
        self.prev_row = PrevRow::InPlace(self.current_start + 1);
        self.current_start += rowlen;
        self.debug_assert_invariants();

        Ok(())
    }

    /// Runs `unfilter` on the current row, and then shifts rows so that the
    /// current row becomes the previous row.
    ///
    /// Before running `unfilter`, the contents of the current row will be
    /// copied into a scratch buffer.  This allows unfiltering to happen even
    /// if `mutable_len_of_curr_row < rowlen` (e.g. when handling partial
    /// or not-yet-complete input streams).
    pub fn unfilter_curr_row_using_scratch_buffer(
        &mut self,
        rowlen: usize,
        bpp: BytesPerPixel,
    ) -> Result<(), DecodingError> {
        debug_assert!(rowlen >= 2); // 1 byte for `RowFilter` and at least 1 byte of pixel data.
        debug_assert!(self.readable_len_of_curr_row() >= rowlen);

        // If `mutable_len_of_curr_row >= rowlen`, then `unfilter_curr_row_in_place`
        // should have been called instead (to avoid the cost of `copy_from_slice` below).
        debug_assert!(self.mutable_len_of_curr_row() < rowlen);

        let filter = self.curr_row_filter()?;

        let mut row = std::mem::take(&mut self.scratch_buffer);
        row.resize(rowlen - 1, 0);
        row.as_mut_slice()
            .copy_from_slice(&self.data_stream[self.current_start + 1..][..rowlen - 1]);

        let prev = self.prev_row();
        debug_assert!(prev.is_empty() || prev.len() == (rowlen - 1));

        unfilter(filter, bpp, prev, &mut row);

        self.reset_prev_row();
        self.prev_row = PrevRow::Scratch(row);
        self.current_start += rowlen;

        self.debug_assert_invariants();

        Ok(())
    }
}

/// An already `unfilter`-ed, previous row.
///
/// The data excludes the `RowFilter` byte - it only includes the actual pixel data.
enum PrevRow {
    /// No unfiltered row.
    ///
    /// `None` is the value of `UnfilteringBuffer::prev_row` before any row has
    /// been unfiltered (or at the start of a new interlace pass).
    None,

    /// Offset of `UnfilteringBuffer::data_stream` where the unfiltered row
    /// starts.
    ///
    /// `UnfilteringBuffer::InPlace(_)` is used by `unfilter_curr_row_in_place`
    /// when setting `UnfilteringBuffer::prev_row`.
    InPlace(usize),

    /// Separate scratch buffer containing the unfiltered row data.
    ///
    /// `UnfilteringBuffer::Scratch(_)` is used by
    /// `unfilter_curr_row_using_scratch_buffer`
    /// when setting `UnfilteringBuffer::prev_row`.
    Scratch(Vec<u8>),
}

impl PrevRow {
    /// Returns the previous unfiltered row as a slice of bytes.
    ///
    /// `buf` should refer to the `..current_start` portion of
    /// `UnfilteringBuffer::data_stream`.
    fn as_slice<'a>(&'a self, buf: &'a [u8]) -> &'a [u8] {
        match self {
            PrevRow::None => &[],
            PrevRow::InPlace(prev_start) => &buf[*prev_start..],
            PrevRow::Scratch(scratch) => scratch.as_slice(),
        }
    }
}

fn checked_next_multiple_of(val: usize, factor: usize) -> Option<usize> {
    if factor == 0 {
        return None;
    }

    let remainder = val % factor;

    if remainder > 0 {
        val.checked_add(factor - remainder)
    } else {
        Some(val)
    }
}

#[test]
fn next_multiple_of_backport_testsuite() {
    assert_eq!(checked_next_multiple_of(1, 0), None);
    assert_eq!(checked_next_multiple_of(2, 0), None);
    assert_eq!(checked_next_multiple_of(1, 2), Some(2));
    assert_eq!(checked_next_multiple_of(2, 2), Some(2));
    assert_eq!(checked_next_multiple_of(2, 5), Some(5));
    assert_eq!(checked_next_multiple_of(1, usize::MAX), Some(usize::MAX));
    assert_eq!(checked_next_multiple_of(usize::MAX, 2), None);
}
