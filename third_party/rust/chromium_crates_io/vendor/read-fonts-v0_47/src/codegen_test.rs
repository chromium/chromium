//! A module used to test codegen.
//!
//! This imports a single codegen output; while modifying the codegen crate,
//! this file can be regenerated to check that changes compile, without needing
//! to rebuild everything.
//!
//! To rebuild this input and test it, run:
//!
//! $ cargo run --bin=codegen resources/test_plan.toml && cargo test

pub mod records {
    include!("../generated/generated_test_records.rs");
}

pub mod formats {
    include!("../generated/generated_test_formats.rs");
}

pub mod read_args {
    include!("../generated/generated_test_read_args.rs");
}

pub mod embedded_records {

    include!("../generated/generated_test_embedded_records.rs");

    #[cfg(test)]
    mod tests {
        use super::*;
        use font_test_data::bebuffer::BeBuffer;

        const REC: usize = 4; // i16 + Offset16

        /// A table whose embedded records point at device tables laid out after
        /// the fixed part, so we can tell a resolved offset from a stray read.
        fn embedded_table() -> BeBuffer {
            // version, 2 records, middle, 1 record, a pair of records, trailer
            let fixed = 2 + REC * 2 + 2 + REC + REC * 2 + 2;
            let mut buf = BeBuffer::new().push(0x0100u16); // version
            let mut device_at = fixed;
            // first, second
            for value in [11i16, 22] {
                buf = buf.push(value).push(device_at as u16);
                device_at += 2;
            }
            buf = buf.push(0xABCDu16); // middle
                                       // third
            buf = buf.push(33i16).push(device_at as u16);
            device_at += 2;
            // pair.first, pair.second
            for value in [44i16, 55] {
                buf = buf.push(value).push(device_at as u16);
                device_at += 2;
            }
            buf = buf.push(0xEF01u16); // trailer
                                       // the device tables, one u16 marker each
            for marker in 0xD0u16..0xD5 {
                buf = buf.push(marker);
            }
            buf
        }

        #[test]
        fn records_are_read_in_place() {
            let buf = embedded_table();
            let table = EmbeddedRecords::read(FontData::new(buf.data())).unwrap();

            // the scalars around the records still land correctly, which is the
            // real test that each record occupies exactly its own bytes
            assert_eq!(table.version(), 0x0100);
            assert_eq!(table.middle(), 0xABCD);
            assert_eq!(table.trailer(), 0xEF01);

            assert_eq!(table.first().value(), 11);
            assert_eq!(table.second().value(), 22);
            assert_eq!(table.third().value(), 33);
            // a record embedded in a record, reached through one embedded in a table
            assert_eq!(table.pair().first().value(), 44);
            assert_eq!(table.pair().second().value(), 55);
        }

        /// The offsets inside an embedded record are measured from the table, so
        /// resolving one has to be given the table's data.
        #[test]
        fn embedded_offsets_resolve_against_the_table() {
            let buf = embedded_table();
            let data = FontData::new(buf.data());
            let table = EmbeddedRecords::read(data).unwrap();
            let offset_data = table.offset_data();

            let markers = [
                table.first().device(offset_data),
                table.second().device(offset_data),
                table.third().device(offset_data),
                table.pair().first().device(offset_data),
                table.pair().second().device(offset_data),
            ];
            for (i, device) in markers.into_iter().enumerate() {
                let device = device.expect("offset is not null").unwrap();
                assert_eq!(device.marker(), 0xD0 + i as u16, "device {i}");
            }
        }

        /// Each record must be located where the byte ranges say, so that a
        /// table full of them stays in step.
        #[test]
        fn byte_ranges_are_contiguous() {
            let buf = embedded_table();
            let table = EmbeddedRecords::read(FontData::new(buf.data())).unwrap();

            assert_eq!(table.version_byte_range(), 0..2);
            assert_eq!(table.first_byte_range(), 2..2 + REC);
            assert_eq!(table.second_byte_range(), 2 + REC..2 + REC * 2);
            assert_eq!(table.middle_byte_range().start, 2 + REC * 2);
            // the pair is two records wide
            assert_eq!(table.pair_byte_range().len(), REC * 2);
            assert_eq!(table.trailer_byte_range().end, EmbeddedRecords::MIN_SIZE);
        }

        /// An embedded record counts toward the table's minimum size, so a
        /// table too short to hold it is rejected rather than read.
        #[test]
        fn short_table_is_rejected() {
            let buf = embedded_table();
            let full = buf.data();
            assert!(EmbeddedRecords::read(FontData::new(full)).is_ok());
            for len in 0..EmbeddedRecords::MIN_SIZE {
                assert!(
                    EmbeddedRecords::read(FontData::new(&full[..len])).is_err(),
                    "a table of {len} bytes should not read"
                );
            }
        }

        /// A record may be followed by variable-length data; it is still within
        /// the minimum size.
        #[test]
        fn record_followed_by_array() {
            let buf = BeBuffer::new()
                .push(7i16) // metrics.value
                .push(0u16) // metrics.device_offset, null
                .push(3u16) // value_count
                .extend([10u16, 20, 30]);
            let table = RecordThenArray::read(FontData::new(buf.data())).unwrap();
            assert_eq!(table.metrics().value(), 7);
            assert!(table.metrics().device(table.offset_data()).is_none());
            let values: Vec<u16> = table.values().iter().map(|v| v.get()).collect();
            assert_eq!(values, vec![10u16, 20, 30]);
        }
    }
}

pub mod positioned {

    include!("../generated/generated_test_positioned.rs");

    /// A record read at an offset within the enclosing table's data.
    ///
    /// Its second field is an offset measured from the start of that table, so
    /// the record can only resolve it by holding the table's data rather than a
    /// slice of its own bytes. That is the whole reason positioned records
    /// exist; the assertions below check the base survives.
    ///
    /// On disk: a `u16` value, then a `u16` offset to another `u16`. The record
    /// is declared with an explicit size so that a record wider than the fields
    /// it defines still strides correctly.
    #[derive(Copy, Clone, Default, Debug)]
    pub struct Positioned<'a> {
        /// The enclosing table's data, not a slice of this record.
        data: FontData<'a>,
        offset: u32,
        size: u16,
    }

    impl<'a> Positioned<'a> {
        pub fn new(data: FontData<'a>, offset: usize, size: u16) -> Self {
            Self {
                data,
                offset: u32::try_from(offset).unwrap_or(u32::MAX),
                size,
            }
        }

        /// Where this record starts within [`offset_data`](Self::offset_data).
        pub fn offset(&self) -> usize {
            self.offset as usize
        }

        /// The enclosing table's data.
        pub fn offset_data(&self) -> FontData<'a> {
            self.data
        }

        /// How many bytes this record occupies, which comes from the read args
        /// rather than the fields it defines.
        pub fn size(&self) -> u16 {
            self.size
        }

        pub fn value(&self) -> Option<u16> {
            self.data.read_at(self.offset as usize).ok()
        }

        /// The raw offset, measured from the start of the enclosing table.
        pub fn target_offset(&self) -> Option<u16> {
            self.data.read_at(self.offset as usize + 2).ok()
        }

        /// The value the offset points at, resolved against the enclosing
        /// table. This is what would be unreachable if the record only had its
        /// own bytes.
        pub fn target(&self) -> Option<u16> {
            self.data.read_at(self.target_offset()? as usize).ok()
        }
    }

    impl ReadArgs for Positioned<'_> {
        type Args = u16;
    }

    impl ComputeSize for Positioned<'_> {
        fn compute_size(args: u16) -> Result<usize, ReadError> {
            Ok(args as usize)
        }
    }

    impl<'a> FontReadAt<'a> for Positioned<'a> {
        fn read_at(data: FontData<'a>, offset: usize, args: u16) -> Result<Self, ReadError> {
            Ok(Self::new(data, offset, args))
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use font_test_data::bebuffer::BeBuffer;

        const SIZE: u16 = 4;
        /// tag + two records
        const PAIR_LEN: usize = 2 + (SIZE as usize * 2);

        /// A table with one solo record and two pairs, whose five records point
        /// at five values laid out after them.
        ///
        /// The targets deliberately sit past every record, so a record that
        /// resolved against its own bytes could not reach them.
        fn positioned_table() -> BeBuffer {
            BeBuffer::new()
                .push(SIZE) // positioned_size
                .push(0x1111u16) // solo.value
                .push(28u16) // solo.target_offset
                .push(2u16) // pair_count
                .push(0xAAAAu16) // pairs[0].tag
                .push(0x2222u16)
                .push(30u16)
                .push(0x3333u16)
                .push(32u16)
                .push(0xBBBBu16) // pairs[1].tag
                .push(0x4444u16)
                .push(34u16)
                .push(0x5555u16)
                .push(36u16)
                // the targets, at 28..38
                .push(0xDEADu16)
                .push(0xBEEFu16)
                .push(0xCAFEu16)
                .push(0xF00Du16)
                .push(0x1234u16)
        }

        /// A field of positioned type is handed the table's data plus where it
        /// starts, so an offset inside it still resolves.
        #[test]
        fn table_field_keeps_table_data() {
            let buf = positioned_table();
            let table = PositionedTable::read(FontData::new(buf.data())).unwrap();
            let solo = table.solo();

            assert_eq!(solo.offset(), table.solo_byte_range().start);
            assert_eq!(solo.offset(), 2);
            assert_eq!(solo.value(), Some(0x1111));
            assert_eq!(solo.target_offset(), Some(28));
            // resolved against the table, not against the record
            assert_eq!(solo.target(), Some(0xDEAD));
            assert_eq!(solo.offset_data().len(), buf.data().len());
        }

        /// Array items are located by stride from the array's start, and each
        /// one keeps the table's data.
        #[test]
        fn array_items_are_located_not_sliced() {
            let buf = positioned_table();
            let table = PositionedTable::read(FontData::new(buf.data())).unwrap();
            let pairs = table.pairs();
            assert_eq!(pairs.len(), 2);

            let start = table.pairs_byte_range().start;
            let expected = [
                (
                    0xAAAAu16,
                    [(0x2222u16, 30u16, 0xBEEFu16), (0x3333, 32, 0xCAFE)],
                ),
                (0xBBBB, [(0x4444, 34, 0xF00D), (0x5555, 36, 0x1234)]),
            ];

            for (i, (tag, records)) in expected.into_iter().enumerate() {
                let pair = pairs.get(i).unwrap();
                assert_eq!(pair.tag(), tag);
                let got = [pair.first(), pair.second()];
                for (j, (value, target_offset, target)) in records.into_iter().enumerate() {
                    let rec = got[j];
                    assert_eq!(
                        rec.offset(),
                        start + i * PAIR_LEN + 2 + j * SIZE as usize,
                        "pair {i} record {j}"
                    );
                    assert_eq!(rec.value(), Some(value));
                    assert_eq!(rec.target_offset(), Some(target_offset));
                    assert_eq!(rec.target(), Some(target), "pair {i} record {j}");
                }
            }

            // iterating agrees with indexing, and the iterator walks offsets
            // statefully, so check it is repeatable and cloneable
            let offsets = |it: &mut dyn Iterator<Item = Result<PositionedPair, ReadError>>| {
                it.map(|p| {
                    let p = p.unwrap();
                    (p.tag(), p.first().offset(), p.second().offset())
                })
                .collect::<Vec<_>>()
            };
            let expected_offsets =
                offsets(&mut (0..pairs.len()).map(|i| Ok(pairs.get(i).unwrap())));
            assert_eq!(offsets(&mut pairs.iter()), expected_offsets);
            // a second call starts over rather than resuming
            assert_eq!(offsets(&mut pairs.iter()), expected_offsets);
            // and a clone resumes from where it was taken
            let mut it = pairs.iter();
            it.next().unwrap().unwrap();
            assert_eq!(offsets(&mut it.clone()), expected_offsets[1..].to_vec());
        }

        /// A record that is positioned only because it holds an array of
        /// positioned records must still pass the table's data down.
        #[test]
        fn nested_arrays_keep_table_data() {
            const PAIRS_PER_GROUP: u16 = 2;
            const GROUPS: u16 = 2;
            // header, then GROUPS * PAIRS_PER_GROUP pairs
            let header = 6;
            let body = GROUPS as usize * PAIRS_PER_GROUP as usize * PAIR_LEN;
            let targets_at = header + body;

            let mut buf = BeBuffer::new()
                .push(SIZE)
                .push(PAIRS_PER_GROUP)
                .push(GROUPS);
            // every record points at a distinct target after the body
            let mut next_target = targets_at;
            for i in 0..(GROUPS as usize * PAIRS_PER_GROUP as usize) {
                buf = buf.push(0xA000u16 + i as u16);
                for _ in 0..2 {
                    buf = buf.push(0u16).push(next_target as u16);
                    next_target += 2;
                }
            }
            for i in 0..(GROUPS as usize * PAIRS_PER_GROUP as usize * 2) {
                buf = buf.push(0xD000u16 + i as u16);
            }

            let data = FontData::new(buf.data());
            let table = NestedPositionedTable::read(data).unwrap();
            let groups = table.groups();
            assert_eq!(groups.len(), GROUPS as usize);

            let mut seen = 0;
            for g in 0..groups.len() {
                let group = groups.get(g).unwrap();
                let pairs = group.pairs();
                assert_eq!(pairs.len(), PAIRS_PER_GROUP as usize);
                for p in 0..pairs.len() {
                    let pair = pairs.get(p).unwrap();
                    for rec in [pair.first(), pair.second()] {
                        // two levels down, still resolving against the table
                        assert_eq!(
                            rec.target(),
                            Some(0xD000 + seen as u16),
                            "group {g} pair {p}"
                        );
                        assert_eq!(rec.offset_data().len(), buf.data().len());
                        seen += 1;
                    }
                }
            }
            assert_eq!(seen, GROUPS as usize * PAIRS_PER_GROUP as usize * 2);
        }

        /// The stride comes from the runtime size, so a record declared wider
        /// than its fields still lands in the right place.
        #[test]
        fn stride_follows_computed_size() {
            const WIDE: u16 = 6; // two bytes of padding per record
            let buf = BeBuffer::new()
                .push(WIDE) // positioned_size
                .push(0u16) // solo.value
                .push(0u16) // solo.target_offset
                .push(0u16) // solo padding
                .push(1u16) // pair_count
                .push(0xAAAAu16) // pairs[0].tag
                .push(0x2222u16)
                .push(0u16)
                .push(0u16) // first, padded
                .push(0x3333u16)
                .push(0u16)
                .push(0u16); // second, padded

            let table = PositionedTable::read(FontData::new(buf.data())).unwrap();
            let pairs = table.pairs();
            assert_eq!(pairs.len(), 1);
            let pair = pairs.get(0).unwrap();
            assert_eq!(pair.first().value(), Some(0x2222));
            assert_eq!(pair.second().value(), Some(0x3333));
            assert_eq!(pair.first().size(), WIDE);
            // the second record starts a full WIDE bytes after the first
            assert_eq!(
                pair.second().offset() - pair.first().offset(),
                WIDE as usize
            );
        }

        #[test]
        fn out_of_range_and_truncated() {
            let buf = positioned_table();
            let table = PositionedTable::read(FontData::new(buf.data())).unwrap();
            let pairs = table.pairs();
            assert!(pairs.get(pairs.len()).is_err());

            // a declared array that doesn't fit yields nothing rather than
            // handing out records pointing past the end
            let truncated = BeBuffer::new()
                .push(SIZE)
                .push(0u16)
                .push(0u16) // solo
                .push(9u16); // pair_count, with no pairs following
            let table = PositionedTable::read(FontData::new(truncated.data())).unwrap();
            assert!(table.pairs().is_empty());
        }
    }
}

pub mod offsets_arrays {

    include!("../generated/generated_test_offsets_arrays.rs");

    #[cfg(test)]
    use font_test_data::bebuffer::BeBuffer;

    pub struct VarSizeDummy<'a> {
        #[allow(dead_code)]
        count: u16,
        pub bytes: &'a [u8],
    }

    impl VarSize for VarSizeDummy<'_> {
        type Size = u16;
    }

    impl ReadArgs for VarSizeDummy<'_> {
        type Args = ();
    }

    impl<'a> FontRead<'a> for VarSizeDummy<'a> {
        fn read_with_args(data: FontData<'a>, _: ()) -> Result<Self, ReadError> {
            let count: u16 = data.read_at(0)?;
            let bytes = data
                .as_bytes()
                .get(2..2 + (count as usize))
                .ok_or(ReadError::OutOfBounds)?;
            Ok(Self { count, bytes })
        }
    }

    #[test]
    fn array_offsets() {
        let builder = BeBuffer::new()
            .push(MajorMinor::VERSION_1_0)
            .push(12_u16) // offset to 0xdead
            .push(0u16) // nullable
            .push(2u16) // array len
            .push(12u16) // array offset
            .extend([0xdead_u16, 0xbeef]);

        let table = KindsOfOffsets::read(builder.data().into()).unwrap();
        assert_eq!(table.nonnullable().unwrap().value(), 0xdead);

        let array = table.array().unwrap();
        assert_eq!(array, &[0xdead, 0xbeef]);
    }

    #[test]
    fn offset_after_array_truncated() {
        let builder = BeBuffer::new().push(5u16).extend([0u8; 9]);
        let table = OffsetAfterArray::read(builder.data().into()).unwrap();
        assert!(table.item_offset().is_null());
        assert!(matches!(table.item(), Err(ReadError::NullOffset)));
        assert!(table.short_offset().is_null());
        assert!(table.medium_offset().is_null());
    }

    #[test]
    fn var_len_array_empty() {
        let builder = BeBuffer::new().push(0u16).push(0xdeadbeef_u32);

        let table = VarLenHaver::read(builder.data().into()).unwrap();
        assert_eq!(table.other_field(), 0xdeadbeef);
    }

    #[test]
    fn var_len_array_some() {
        let builder = BeBuffer::new()
            .push(3u16)
            .push(0u16) // first item in array is empty
            .push(2u16)
            .extend([1u8, 1])
            .push(5u16)
            .extend([7u8, 7, 7, 7, 7])
            .push(0xdeadbeef_u32);

        let table = VarLenHaver::read(builder.data().into()).unwrap();
        let kids = table
            .var_len()
            .iter()
            .map(|x| x.unwrap())
            .collect::<Vec<_>>();
        assert!(kids[0].bytes.is_empty());
        assert_eq!(kids[1].bytes, &[1, 1]);
        assert_eq!(kids[2].bytes, &[7, 7, 7, 7, 7]);
        assert_eq!(table.other_field(), 0xdeadbeef)
    }

    #[test]
    fn versioned_array_bad_data() {
        let buf = BeBuffer::new()
            .push(1u16) // version
            .push(1u16) // count
            .push(2u16) // scalar array
            .push(3u16)
            .push(4u32); // shmecord array
        let table = KindsOfArrays::read(buf.data().into()).unwrap();
        assert!(table.versioned_scalars().is_none()); // should be there but isn't
    }
}

pub mod flags {
    include!("../generated/generated_test_flags.rs");

    #[test]
    fn basics() {
        let all = ValueFormat::all();
        let none = ValueFormat::empty();
        assert!(all.contains(ValueFormat::X_PLACEMENT));
        assert!(all.contains(ValueFormat::Y_PLACEMENT));
        assert!(!none.contains(ValueFormat::X_PLACEMENT));
        assert!(!none.contains(ValueFormat::Y_PLACEMENT));
        assert_eq!(none, ValueFormat::default());
    }

    #[test]
    fn formatting() {
        let all = ValueFormat::all();
        assert_eq!(format!("{all:?}"), "X_PLACEMENT | Y_PLACEMENT");
        let none = ValueFormat::empty();
        assert_eq!(format!("{none:?}"), "(empty)");
        let xplace = ValueFormat::X_PLACEMENT;
        assert_eq!(format!("{xplace:?}"), "X_PLACEMENT");
    }

    // not exactly a test, but this will fail to compile if these are missing
    #[test]
    fn impl_traits() {
        fn impl_check<T: Copy + std::hash::Hash + Eq + Ord>() {}
        impl_check::<ValueFormat>();
    }
}

pub mod enums {
    include!("../generated/generated_test_enum.rs");
}

pub mod count_all {
    use crate::FontData;

    include!("../generated/generated_test_count_all.rs");

    /// Test for count(..) with element sizes > 1
    #[test]
    fn element_size_greater_than_one_with_padding() {
        // Size of 13 ensures we have an extra padding byte
        let bytes = [0u8; 13];
        // Generated table has a 2 byte field above the array
        let remainder_len = bytes.len() - 2;
        let data = FontData::new(&bytes);
        // Trailing array with 16-bit elements
        assert!(remainder_len % 2 != 0);
        let count16 = CountAll16::read(data).unwrap();
        assert_eq!(count16.remainder().len(), remainder_len / 2);
        // Trailing array with 32-bit elements
        assert!(remainder_len % 4 != 0);
        let count32 = CountAll32::read(data).unwrap();
        assert_eq!(count32.remainder().len(), remainder_len / 4);
    }
}

pub mod conditions {
    #[cfg(test)]
    use font_test_data::bebuffer::BeBuffer;
    use font_types::MajorMinor;

    include!("../generated/generated_test_conditions.rs");

    #[test]
    fn majorminor_1() {
        let bytes = BeBuffer::new().push(MajorMinor::VERSION_1_0).push(0u16);
        let table = MajorMinorVersion::read(bytes.data().into()).unwrap();
        assert_eq!(table.always_present(), 0);
    }

    #[test]
    fn majorminor_1_1() {
        let bytes = BeBuffer::new().push(MajorMinor::VERSION_1_1).push(0u16);
        let too_small = MajorMinorVersion::read(bytes.data().into()).unwrap();
        // this is expected to be present but the data is malformed; we will
        // still parse the table but checked read of the field will fail
        assert!(too_small.if_11().is_none());

        let bytes = BeBuffer::new()
            .push(MajorMinor::VERSION_1_1)
            .push(0u16)
            .push(1u16);
        let table = MajorMinorVersion::read(bytes.data().into()).unwrap();
        assert_eq!(table.if_11(), Some(1));
    }

    #[test]
    fn major_minor_2() {
        let bytes = BeBuffer::new().push(MajorMinor::VERSION_2_0).push(0u16);
        let too_small = MajorMinorVersion::read(bytes.data().into()).unwrap();
        assert!(too_small.if_11().is_none());
        assert!(too_small.if_20().is_none());

        let bytes = BeBuffer::new()
            .push(MajorMinor::VERSION_2_0)
            .push(0u16)
            .push(2u32);
        let table = MajorMinorVersion::read(bytes.data().into()).unwrap();
        assert_eq!(table.if_11(), None);
        assert_eq!(table.if_20(), Some(2));
    }

    #[cfg(test)]
    fn make_flag_data(flags: GotFlags) -> BeBuffer {
        let mut buf = BeBuffer::new().push(42u16).push(flags);
        if flags.contains(GotFlags::FOO) {
            buf = buf.push(0xf00_u16);
        }
        if flags.contains(GotFlags::BAR) {
            buf = buf.push(0xba4_u16);
        }
        if flags.contains(GotFlags::FOO) || flags.contains(GotFlags::BAZ) {
            buf = buf.push(0xba2_u16);
        }
        buf
    }

    #[test]
    fn flags_none() {
        let data = make_flag_data(GotFlags::empty());
        let table = FlagDay::read(data.data().into()).unwrap();
        assert!(table.foo().is_none());
        assert!(table.bar().is_none());
    }

    #[test]
    fn flags_foo() {
        let data = make_flag_data(GotFlags::FOO);
        let table = FlagDay::read(data.data().into()).unwrap();
        assert_eq!(table.foo(), Some(0xf00));
        assert!(table.bar().is_none());
    }

    #[test]
    fn flags_bar() {
        let data = make_flag_data(GotFlags::BAR);
        let table = FlagDay::read(data.data().into()).unwrap();
        assert!(table.foo().is_none());
        assert_eq!(table.bar(), Some(0xba4));
    }

    #[test]
    fn flags_foobar() {
        let data = make_flag_data(GotFlags::BAR | GotFlags::FOO);
        let table = FlagDay::read(data.data().into()).unwrap();
        assert_eq!(table.foo(), Some(0xf00));
        assert_eq!(table.bar(), Some(0xba4));
    }
}

pub mod generic_group {
    include!("../generated/generated_test_generic_group.rs");

    #[cfg(test)]
    use font_test_data::bebuffer::BeBuffer;

    /// Build bytes for a MyLookup with one subtable offset pointing at data
    /// immediately after the header.
    /// Layout: [lookup_type: u16, sub_table_count: u16, offset0: Offset16, ...subtable data]
    #[cfg(test)]
    fn make_lookup_with_format1(lookup_type: u16) -> BeBuffer {
        BeBuffer::new()
            .push(lookup_type) // lookup_type
            .push(1u16) // sub_table_count
            .push(6u16) // offset to subtable (6 bytes from start)
            // subtable data (MySubtableFormat1): format=1, value=42
            .push(1u16)
            .push(42u16)
    }

    #[test]
    fn parse_lookup_group_type_one() {
        let buf = make_lookup_with_format1(1);
        let group = MyLookupGroup::read(buf.data().into()).unwrap();
        assert!(matches!(group, MyLookupGroup::TypeOne(_)));
        let lookup = group.of_unit_type();
        assert_eq!(lookup.lookup_type(), 1);
        assert_eq!(lookup.sub_table_count(), 1);
    }

    #[test]
    fn parse_lookup_group_type_two() {
        let buf = make_lookup_with_format1(2);
        let group = MyLookupGroup::read(buf.data().into()).unwrap();
        assert!(matches!(group, MyLookupGroup::TypeTwo(_)));
    }

    #[test]
    fn parse_lookup_group_invalid_type() {
        let buf = make_lookup_with_format1(99);
        let result = MyLookupGroup::read(buf.data().into());
        assert!(matches!(result, Err(ReadError::InvalidFormat(99))))
    }

    #[test]
    fn parse_subtable_format_dispatch() {
        // Format 1
        let buf = BeBuffer::new().push(1u16).push(42u16);
        let sub = MySubtable::read(buf.data().into()).unwrap();
        assert!(matches!(sub, MySubtable::Format1(_)));
        if let MySubtable::Format1(f1) = sub {
            assert_eq!(f1.value(), 42);
        }

        // Format 2
        let buf = BeBuffer::new()
            .push(2u16) // format
            .push(2u16) // count
            .extend([10u16, 20]);
        let sub = MySubtable::read(buf.data().into()).unwrap();
        assert!(matches!(sub, MySubtable::Format2(_)));
        if let MySubtable::Format2(f2) = sub {
            assert_eq!(f2.count(), 2);
        }
    }
}
