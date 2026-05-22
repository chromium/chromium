use serde::de::{Deserializer, EnumAccess, VariantAccess, Visitor};
use std::fmt;

struct EnumVisitor;

impl<'de> Visitor<'de> for EnumVisitor {
    type Value = (Vec<bool>, ());

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("enum")
    }

    fn visit_enum<A>(self, data: A) -> Result<Self::Value, A::Error>
    where
        A: EnumAccess<'de>,
    {
        let (key, variant) = data.variant()?;
        variant.newtype_variant::<()>()?;
        Ok((key, ()))
    }
}

#[test]
fn test() {
    let mut de = serde_json::Deserializer::from_str("{[true]: null}");
    let err = de.deserialize_enum("name", &[], EnumVisitor).unwrap_err();

    assert!(err.to_string().contains("key must be a string"));
}
