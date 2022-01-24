#![feature(test)]

extern crate test;

use quote::quote;
use test::Bencher;

#[bench]
fn bench_impl(b: &mut Bencher) {
    b.iter(|| {
        quote! {
            impl<'de> _serde::Deserialize<'de> for Response {
                fn deserialize<__D>(__deserializer: __D) -> _serde::export::Result<Self, __D::Error>
                where
                    __D: _serde::Deserializer<'de>,
                {
                    #[allow(non_camel_case_types)]
                    enum __Field {
                        __field0,
                        __field1,
                        __ignore,
                    }
                    struct __FieldVisitor;
                    impl<'de> _serde::de::Visitor<'de> for __FieldVisitor {
                        type Value = __Field;
                        fn expecting(
                            &self,
                            __formatter: &mut _serde::export::Formatter,
                        ) -> _serde::export::fmt::Result {
                            _serde::export::Formatter::write_str(__formatter, "field identifier")
                        }
                        fn visit_u64<__E>(self, __value: u64) -> _serde::export::Result<Self::Value, __E>
                        where
                            __E: _serde::de::Error,
                        {
                            match __value {
                                0u64 => _serde::export::Ok(__Field::__field0),
                                1u64 => _serde::export::Ok(__Field::__field1),
                                _ => _serde::export::Err(_serde::de::Error::invalid_value(
                                    _serde::de::Unexpected::Unsigned(__value),
                                    &"field index 0 <= i < 2",
                                )),
                            }
                        }
                        fn visit_str<__E>(self, __value: &str) -> _serde::export::Result<Self::Value, __E>
                        where
                            __E: _serde::de::Error,
                        {
                            match __value {
                                "id" => _serde::export::Ok(__Field::__field0),
                                "s" => _serde::export::Ok(__Field::__field1),
                                _ => _serde::export::Ok(__Field::__ignore),
                            }
                        }
                        fn visit_bytes<__E>(
                            self,
                            __value: &[u8],
                        ) -> _serde::export::Result<Self::Value, __E>
                        where
                            __E: _serde::de::Error,
                        {
                            match __value {
                                b"id" => _serde::export::Ok(__Field::__field0),
                                b"s" => _serde::export::Ok(__Field::__field1),
                                _ => _serde::export::Ok(__Field::__ignore),
                            }
                        }
                    }
                    impl<'de> _serde::Deserialize<'de> for __Field {
                        #[inline]
                        fn deserialize<__D>(__deserializer: __D) -> _serde::export::Result<Self, __D::Error>
                        where
                            __D: _serde::Deserializer<'de>,
                        {
                            _serde::Deserializer::deserialize_identifier(__deserializer, __FieldVisitor)
                        }
                    }
                    struct __Visitor<'de> {
                        marker: _serde::export::PhantomData<Response>,
                        lifetime: _serde::export::PhantomData<&'de ()>,
                    }
                    impl<'de> _serde::de::Visitor<'de> for __Visitor<'de> {
                        type Value = Response;
                        fn expecting(
                            &self,
                            __formatter: &mut _serde::export::Formatter,
                        ) -> _serde::export::fmt::Result {
                            _serde::export::Formatter::write_str(__formatter, "struct Response")
                        }
                        #[inline]
                        fn visit_seq<__A>(
                            self,
                            mut __seq: __A,
                        ) -> _serde::export::Result<Self::Value, __A::Error>
                        where
                            __A: _serde::de::SeqAccess<'de>,
                        {
                            let __field0 =
                                match try!(_serde::de::SeqAccess::next_element::<u64>(&mut __seq)) {
                                    _serde::export::Some(__value) => __value,
                                    _serde::export::None => {
                                        return _serde::export::Err(_serde::de::Error::invalid_length(
                                            0usize,
                                            &"struct Response with 2 elements",
                                        ));
                                    }
                                };
                            let __field1 =
                                match try!(_serde::de::SeqAccess::next_element::<String>(&mut __seq)) {
                                    _serde::export::Some(__value) => __value,
                                    _serde::export::None => {
                                        return _serde::export::Err(_serde::de::Error::invalid_length(
                                            1usize,
                                            &"struct Response with 2 elements",
                                        ));
                                    }
                                };
                            _serde::export::Ok(Response {
                                id: __field0,
                                s: __field1,
                            })
                        }
                        #[inline]
                        fn visit_map<__A>(
                            self,
                            mut __map: __A,
                        ) -> _serde::export::Result<Self::Value, __A::Error>
                        where
                            __A: _serde::de::MapAccess<'de>,
                        {
                            let mut __field0: _serde::export::Option<u64> = _serde::export::None;
                            let mut __field1: _serde::export::Option<String> = _serde::export::None;
                            while let _serde::export::Some(__key) =
                                try!(_serde::de::MapAccess::next_key::<__Field>(&mut __map))
                            {
                                match __key {
                                    __Field::__field0 => {
                                        if _serde::export::Option::is_some(&__field0) {
                                            return _serde::export::Err(
                                                <__A::Error as _serde::de::Error>::duplicate_field("id"),
                                            );
                                        }
                                        __field0 = _serde::export::Some(
                                            try!(_serde::de::MapAccess::next_value::<u64>(&mut __map)),
                                        );
                                    }
                                    __Field::__field1 => {
                                        if _serde::export::Option::is_some(&__field1) {
                                            return _serde::export::Err(
                                                <__A::Error as _serde::de::Error>::duplicate_field("s"),
                                            );
                                        }
                                        __field1 = _serde::export::Some(
                                            try!(_serde::de::MapAccess::next_value::<String>(&mut __map)),
                                        );
                                    }
                                    _ => {
                                        let _ = try!(_serde::de::MapAccess::next_value::<
                                            _serde::de::IgnoredAny,
                                        >(&mut __map));
                                    }
                                }
                            }
                            let __field0 = match __field0 {
                                _serde::export::Some(__field0) => __field0,
                                _serde::export::None => try!(_serde::private::de::missing_field("id")),
                            };
                            let __field1 = match __field1 {
                                _serde::export::Some(__field1) => __field1,
                                _serde::export::None => try!(_serde::private::de::missing_field("s")),
                            };
                            _serde::export::Ok(Response {
                                id: __field0,
                                s: __field1,
                            })
                        }
                    }
                    const FIELDS: &'static [&'static str] = &["id", "s"];
                    _serde::Deserializer::deserialize_struct(
                        __deserializer,
                        "Response",
                        FIELDS,
                        __Visitor {
                            marker: _serde::export::PhantomData::<Response>,
                            lifetime: _serde::export::PhantomData,
                        },
                    )
                }
            }
        }
    });
}
