// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::errors::ffi::DataError;
    #[cfg(feature = "buffer_provider")]
    use crate::provider::ffi::DataProvider;

    #[diplomat::opaque]
    /// An ICU4X Units Converter Factory object, capable of creating converters a [`UnitsConverter`]
    /// for converting between two [`MeasureUnit`]s.
    ///
    /// Also, it can parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`MeasureUnit`].
    #[diplomat::rust_link(icu::experimental::units::converter_factory::ConverterFactory, Struct)]
    pub struct UnitsConverterFactory(
        pub icu_experimental::units::converter_factory::ConverterFactory,
    );

    impl UnitsConverterFactory {
        /// Construct a new [`UnitsConverterFactory`] instance using compiled data.
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::new,
            FnInStruct
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<UnitsConverterFactory> {
            Box::new(UnitsConverterFactory(
                icu_experimental::units::converter_factory::ConverterFactory::new(),
            ))
        }
        /// Construct a new [`UnitsConverterFactory`] instance using a particular data source.
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::new,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<UnitsConverterFactory>, DataError> {
            Ok(Box::new(UnitsConverterFactory(icu_experimental::units::converter_factory::ConverterFactory::try_new_with_buffer_provider(provider.get()?)?)))
        }
        /// Creates a new [`UnitsConverter`] from the input and output [`MeasureUnit`]s.
        /// Returns nothing if the conversion between the two units is not possible.
        /// For example, conversion between `meter` and `second` is not possible.
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::converter,
            FnInStruct
        )]
        pub fn converter(
            &self,
            from: &MeasureUnit,
            to: &MeasureUnit,
        ) -> Option<Box<UnitsConverter>> {
            self.0
                .converter(&from.0, &to.0)
                .map(UnitsConverter)
                .map(Box::new)
        }

        /// Creates a parser to parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`MeasureUnit`].
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::parser,
            FnInStruct
        )]
        pub fn parser<'a>(&'a self) -> Box<MeasureUnitParser<'a>> {
            MeasureUnitParser(self.0.parser()).into()
        }
    }

    #[diplomat::opaque]
    /// An ICU4X Measurement Unit parser object which is capable of parsing the CLDR unit identifier
    /// (e.g. `meter-per-square-second`) and get the [`MeasureUnit`].
    #[diplomat::rust_link(icu::experimental::measure::parser::MeasureUnitParser, Struct)]
    pub struct MeasureUnitParser<'a>(pub icu_experimental::measure::parser::MeasureUnitParser<'a>);

    impl<'a> MeasureUnitParser<'a> {
        /// Parses the CLDR unit identifier (e.g. `meter-per-square-second`) and returns the corresponding [`MeasureUnit`],
        /// if the identifier is valid.
        #[diplomat::rust_link(
            icu::experimental::measure::parser::MeasureUnitParser::parse,
            FnInStruct
        )]
        pub fn parse(&self, unit_id: &DiplomatStr) -> Option<Box<MeasureUnit>> {
            self.0
                .try_from_utf8(unit_id)
                .ok()
                .map(MeasureUnit)
                .map(Box::new)
        }
    }

    #[diplomat::opaque]
    /// An ICU4X Measurement Unit object which represents a single unit of measurement
    /// such as `meter`, `second`, `kilometer-per-hour`, `square-meter`, etc.
    ///
    /// You can create an instance of this object using [`MeasureUnitParser`] by calling the `parse_measure_unit` method.
    #[diplomat::rust_link(icu::experimental::measure::measureunit::MeasureUnit, Struct)]
    pub struct MeasureUnit(pub icu_experimental::measure::measureunit::MeasureUnit);

    #[diplomat::opaque]
    /// An ICU4X Units Converter object, capable of converting between two [`MeasureUnit`]s.
    ///
    /// You can create an instance of this object using [`UnitsConverterFactory`] by calling the `converter` method.
    #[diplomat::rust_link(icu::experimental::units::converter::UnitsConverter, Struct)]
    pub struct UnitsConverter(pub icu_experimental::units::converter::UnitsConverter<f64>);
    impl UnitsConverter {
        /// Converts the input value from the input unit to the output unit (that have been used to create this converter).
        /// NOTE:
        ///   The conversion using floating-point operations is not as accurate as the conversion using ratios.
        #[diplomat::rust_link(
            icu::experimental::units::converter::UnitsConverter::convert,
            FnInStruct
        )]
        #[diplomat::attr(supports = method_overloading, rename = "convert")]
        #[diplomat::attr(js, rename = "convert_number")]
        pub fn convert_double(&self, value: f64) -> f64 {
            self.0.convert(&value)
        }

        /// Clones the current [`UnitsConverter`] object.
        #[diplomat::rust_link(
            icu::experimental::units::converter::UnitsConverter::clone,
            FnInStruct
        )]
        pub fn clone(&self) -> Box<Self> {
            Box::new(UnitsConverter(self.0.clone()))
        }
    }
}
