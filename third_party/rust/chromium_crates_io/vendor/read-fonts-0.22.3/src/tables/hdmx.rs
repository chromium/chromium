//! The [Horizontal Device Metrics](https://learn.microsoft.com/en-us/typography/opentype/spec/hdmx) table.

include!("../../generated/generated_hdmx.rs");

use std::cmp::Ordering;

impl<'a> Hdmx<'a> {
    /// Returns for the device record that exactly matches the given
    /// size (as ppem).
    pub fn record_for_size(&self, size: u8) -> Option<DeviceRecord<'a>> {
        let records = self.records();
        // Need a custom binary search because we're working with
        // ComputedArray
        let mut lo = 0;
        let mut hi = records.len();
        while lo < hi {
            let mid = (lo + hi) / 2;
            let record = records.get(mid).ok()?;
            match record.pixel_size.cmp(&size) {
                Ordering::Less => lo = mid + 1,
                Ordering::Greater => hi = mid,
                Ordering::Equal => return Some(record),
            }
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{be_buffer, be_buffer_add, test_helpers::BeBuffer};

    #[test]
    fn read_hdmx() {
        let buf = make_hdmx();
        let hdmx = Hdmx::read(buf.font_data(), 2).unwrap();
        assert_eq!(hdmx.version(), 0);
        assert_eq!(hdmx.num_records(), 3);
        assert_eq!(hdmx.size_device_record(), 4);
        let records = hdmx
            .records()
            .iter()
            .map(|rec| rec.unwrap())
            .collect::<Vec<_>>();
        assert_eq!(records.len(), 3);
        let expected_records = [
            DeviceRecord {
                pixel_size: 8,
                max_width: 12,
                widths: &[10, 12],
            },
            DeviceRecord {
                pixel_size: 16,
                max_width: 20,
                widths: &[18, 20],
            },
            DeviceRecord {
                pixel_size: 32,
                max_width: 40,
                widths: &[38, 40],
            },
        ];
        assert_eq!(records, expected_records);
    }

    #[test]
    fn find_by_size() {
        let buf = make_hdmx();
        let hdmx = Hdmx::read(buf.font_data(), 2).unwrap();
        assert_eq!(hdmx.record_for_size(8).unwrap().pixel_size, 8);
        assert_eq!(hdmx.record_for_size(16).unwrap().pixel_size, 16);
        assert_eq!(hdmx.record_for_size(32).unwrap().pixel_size, 32);
        assert!(hdmx.record_for_size(7).is_none());
        assert!(hdmx.record_for_size(20).is_none());
        assert!(hdmx.record_for_size(72).is_none());
    }

    fn make_hdmx() -> BeBuffer {
        be_buffer! {
            0u16,           // version
            3u16,           // num_records
            4u32,           // size_device_record
            // 3 records [pixel_size, max_width, width0, width1]
            [8u8, 12, 10, 12],
            [16u8, 20, 18, 20],
            [32u8, 40, 38, 40]
        }
    }
}
