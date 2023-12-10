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

pub mod offsets_arrays {
    include!("../generated/generated_test_offsets_arrays.rs");

    #[test]
    fn array_offsets() {
        let builder = crate::test_helpers::BeBuffer::new()
            .push(MajorMinor::VERSION_1_0)
            .push(12_u16) // offset to 0xdead
            .push(0u16) // nullable
            .push(2u16) // array len
            .push(12u16) // array offset
            .extend([0xdead_u16, 0xbeef]);

        let table = KindsOfOffsets::read(builder.font_data()).unwrap();
        assert_eq!(table.nonnullable().unwrap().value(), 0xdead);

        let array = table.array().unwrap();
        assert_eq!(array, &[0xdead, 0xbeef]);
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
