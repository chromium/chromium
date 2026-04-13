use serde_::de::{self, DeserializeSeed, IgnoredAny, SeqAccess, Visitor};

use crate::const_marker::{CheckExpected, ValidateEquals};


impl<'de, T> Visitor<'de> for ValidateEquals<&'static &'static [T]>
where
    T: core::fmt::Debug,
    ValidateEquals<&'static T>: DeserializeSeed<'de>,
{
    type Value = ();

    fn expecting(&self, fmt: &mut core::fmt::Formatter) -> core::fmt::Result {
        write!(fmt, "the value `{:?}`", self.equals_to)
    }

    fn visit_seq<S>(self, mut seq: S) -> Result<(), S::Error>
    where
        S: SeqAccess<'de>,
    {
        let mut this = *self.equals_to;
        loop {
            match this.split_first() {
                Some((expected, rest)) => {
                    this = rest;

                    let seed = ValidateEquals { equals_to: expected };

                    let Some(_) = seq.next_element_seed(seed)? else {
                        return Err(de::Error::custom(format_args!(
                            "sequence is {} elements too short",
                            this.len(),
                        )))
                    };
                }
                None => {
                    let mut excess = 0;
                    while let Some(_) = seq.next_element::<IgnoredAny>()? {
                        excess += 1;
                    }
                    if excess != 0 {
                        return Err(de::Error::custom(format_args!(
                            "sequence has {excess} too many elements",
                        )))
                    } else {
                        return Ok(())
                    }
                }
            }
        }
    }
}

//////////////////////////////////


impl<'de> Visitor<'de> for ValidateEquals<&'static &'static str> {
    type Value = ();

    fn expecting(&self, fmt: &mut core::fmt::Formatter) -> core::fmt::Result {
        write!(fmt, "the value `{:?}`", self.equals_to)
    }

    fn visit_str<E>(self, found: &str) -> Result<(), E>
    where
        E: serde_::de::Error,
    {
        CheckExpected { expected: *self.equals_to, found }.call()
    }
}

