//! A module used to test codegen.
//!
//! This imports a single codegen output; while modifying the codegen crate,
//! this file can be regenerated to check that changes compile, without needing
//! to rebuild everything.
//!
//! To rebuild this input and test it, run:
//!
//! $ cargo run --bin=codegen resources/test_plan.toml && cargo test

mod records {
    include!("../generated/generated_test_records.rs");

    impl BasicTable {
        fn compute_arrays_inner_count(&self) -> u16 {
            self.array_records
                .first()
                .map(|x| x.scalars.len().try_into().unwrap())
                .unwrap_or_default()
        }

        fn my_custom_validate(&self, _: &mut ValidationCtx) {
            // Wowee, I can validate the entire table!
        }
    }

    impl SimpleRecord {
        // the [compile_with] attribute specifies this method, which returns
        // a different type than the one declared in the table.
        fn compile_va2(&self) -> [u8; 4] {
            [0xde, 0xad, 0xbe, 0xef]
        }
    }

    #[test]
    fn compile_with() {
        let record = SimpleRecord::new(69, 16);
        let bytes = crate::dump_table(&record).unwrap();
        assert_eq!(bytes, [0, 69, 0xde, 0xad, 0xbe, 0xef])
    }

    #[test]
    fn constructors() {
        let simple = vec![SimpleRecord::new(6, 32)];
        let contains_arrays = ContainsArrays::new(vec![1, 2, 3], simple.clone());
        let basic = BasicTable::new(simple.clone(), vec![contains_arrays]);
        let contains_offsets = ContainsOffsets::new(simple, basic);
        assert_eq!(contains_offsets.other.simple_records.len(), 1);
    }
}

mod formats {

    include!("../generated/generated_test_formats.rs");

    #[test]
    fn construct_formats() {
        let one = MyTable::format_1(404, 12);
        let two = MyTable::my_format_22(vec![5, 6, 7]);

        assert!(matches!(
            one,
            MyTable::Format1(Table1 {
                heft: 404,
                flex: 12
            })
        ));
        assert!(matches!(two, MyTable::MyFormat22(Table2 { .. })));
    }
}

pub mod read_args {
    include!("../generated/generated_test_read_args.rs");
}

mod offsets_arrays {
    include!("../generated/generated_test_offsets_arrays.rs");
    #[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
    #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
    pub struct VarSizeDummy {
        bytes: Vec<u8>,
    }

    impl Validate for VarSizeDummy {
        fn validate_impl(&self, _ctx: &mut ValidationCtx) {}
    }

    impl FontWrite for VarSizeDummy {
        fn write_into(&self, writer: &mut TableWriter) {
            (self.bytes.len() as u16).write_into(writer);
            self.bytes.write_into(writer);
        }
    }

    impl FromObjRef<read_fonts::codegen_test::offsets_arrays::VarSizeDummy<'_>> for VarSizeDummy {
        fn from_obj_ref(
            from: &read_fonts::codegen_test::offsets_arrays::VarSizeDummy,
            _: FontData,
        ) -> Self {
            Self {
                bytes: from.bytes.to_owned(),
            }
        }
    }
}

mod enums {
    include!("../generated/generated_test_enum.rs");

    #[test]
    fn default_works() {
        let rec = MyRecord::new(Default::default(), Default::default());
        assert_eq!(MyEnum1::ItsAZero, rec.my_enum1);
        assert_eq!(MyEnum2::ItsAThree, rec.my_enum2);
    }
}

pub mod conditions {
    #[cfg(test)]

    include!("../generated/generated_test_conditions.rs");

    #[test]
    #[should_panic(expected = "'foo' is present but FOO not set")]
    fn field_present_flag_missing() {
        let mut flags_are_wrong = FlagDay::new(42, GotFlags::empty());
        flags_are_wrong.foo = Some(0xf00);
        crate::dump_table(&flags_are_wrong).unwrap();
    }

    #[test]
    #[should_panic(expected = "FOO is set but 'foo' is None")]
    fn flag_present_field_missing() {
        let flags_are_wrong = FlagDay::new(42, GotFlags::FOO);
        crate::dump_table(&flags_are_wrong).unwrap();
    }
}

mod generic_group {
    include!("../generated/generated_test_generic_group.rs");

    #[test]
    fn construct_subtable_format1() {
        let st = MySubtableFormat1 { value: 42 };
        let bytes = crate::dump_table(&st).unwrap();
        // format=1 (2 bytes) + value=42 (2 bytes)
        assert_eq!(bytes, [0, 1, 0, 42]);
    }

    #[test]
    fn construct_subtable_enum() {
        let st = MySubtable::from(MySubtableFormat1 { value: 7 });
        assert!(matches!(st, MySubtable::Format1(_)));

        let st2 = MySubtable::from(MySubtableFormat2 {
            values: vec![1, 2, 3],
        });
        assert!(matches!(st2, MySubtable::Format2(_)));
    }

    #[test]
    fn construct_lookup_group() {
        let lookup: MyLookup<MySubtableFormat1> = MyLookup {
            lookup_type: 2,
            subtables: vec![OffsetMarker::new(MySubtableFormat1 { value: 99 })],
        };
        let group = MyLookupGroup::from(lookup);
        assert!(matches!(group, MyLookupGroup::TypeTwo(_)));
    }

    #[test]
    fn round_trip_subtable_format1() {
        let owned = MySubtableFormat1 { value: 42 };
        let bytes = crate::dump_table(&owned).unwrap();
        let parsed = read_fonts::codegen_test::generic_group::MySubtableFormat1::read(
            bytes.as_slice().into(),
        )
        .unwrap();
        assert_eq!(parsed.value(), 42);
    }
}
