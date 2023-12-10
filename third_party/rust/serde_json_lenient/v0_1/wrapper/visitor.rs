// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::{ContextPointer, Functions};
use serde::de::{DeserializeSeed, Deserializer, Error, MapAccess, SeqAccess, Visitor};
use std::convert::TryFrom;
use std::fmt;
use std::pin::Pin;

/// Watches to ensure recursion does not go too deep during deserialization.
struct RecursionDepthCheck(usize);

impl RecursionDepthCheck {
    /// Recurse a level and return an error if we've recursed too far.
    fn recurse<E: Error>(&self) -> Result<RecursionDepthCheck, E> {
        match self.0.checked_sub(1) {
            Some(recursion_limit) => Ok(RecursionDepthCheck(recursion_limit)),
            None => Err(Error::custom("recursion limit exceeded")),
        }
    }
}

/// What type of aggregate JSON type is being deserialized.
pub enum DeserializationTarget<'c> {
    /// Deserialize by appending to a list.
    List { ctx: Pin<&'c mut ContextPointer> },
    /// Deserialize by setting a dictionary key.
    Dict { ctx: Pin<&'c mut ContextPointer>, key: String },
}

/// A deserializer and visitor type that is used to visit each value in the JSON
/// input when it is deserialized.
///
/// Normally serde deserialization instantiates a new object, but this visitor
/// is designed to call back into C++ for creating the deserialized objects. To
/// achieve this we use a feature of serde called "stateful deserialization" (https://docs.serde.rs/serde/de/trait.DeserializeSeed.html).
pub struct ValueVisitor<'c> {
    fns: &'static Functions,
    aggregate: DeserializationTarget<'c>,
    recursion_depth_check: RecursionDepthCheck,
}

impl<'c> ValueVisitor<'c> {
    pub fn new(
        fns: &'static Functions,
        target: DeserializationTarget<'c>,
        max_depth: usize,
    ) -> Self {
        Self {
            fns,
            aggregate: target,
            // The `max_depth` includes the top level of the JSON input, which is where parsing
            // starts. We subtract 1 to count the top level now.
            recursion_depth_check: RecursionDepthCheck(max_depth - 1),
        }
    }
}

impl<'de, 'c> Visitor<'de> for ValueVisitor<'c> {
    // We call out to C++ to construct the deserialized type, so no output from the
    // visitor.
    type Value = ();

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("any valid JSON")
    }

    fn visit_i32<E: serde::de::Error>(self, value: i32) -> Result<Self::Value, E> {
        match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_i32(ctx, value),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_i32(ctx, &key, value),
        };
        Ok(())
    }

    fn visit_i8<E: serde::de::Error>(self, value: i8) -> Result<Self::Value, E> {
        self.visit_i32(value as i32)
    }

    fn visit_bool<E: serde::de::Error>(self, value: bool) -> Result<Self::Value, E> {
        match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_bool(ctx, value),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_bool(ctx, &key, value),
        };
        Ok(())
    }

    fn visit_i64<E: serde::de::Error>(self, value: i64) -> Result<Self::Value, E> {
        // Integer values that are > 32 bits large are returned as doubles instead. See
        // JSONReaderTest.LargerIntIsLossy for a related test.
        match i32::try_from(value) {
            Ok(value) => self.visit_i32(value),
            Err(_) => self.visit_f64(value as f64),
        }
    }

    fn visit_u64<E: serde::de::Error>(self, value: u64) -> Result<Self::Value, E> {
        // See visit_i64 comment.
        match i32::try_from(value) {
            Ok(value) => self.visit_i32(value),
            Err(_) => self.visit_f64(value as f64),
        }
    }

    fn visit_f64<E: serde::de::Error>(self, value: f64) -> Result<Self::Value, E> {
        match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_f64(ctx, value),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_f64(ctx, &key, value),
        };
        Ok(())
    }

    fn visit_str<E: serde::de::Error>(self, value: &str) -> Result<Self::Value, E> {
        match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_str(ctx, value),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_str(ctx, &key, value),
        };
        Ok(())
    }

    fn visit_borrowed_str<E: serde::de::Error>(self, value: &'de str) -> Result<Self::Value, E> {
        match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_str(ctx, value),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_str(ctx, &key, value),
        };
        Ok(())
    }

    fn visit_string<E: serde::de::Error>(self, value: String) -> Result<Self::Value, E> {
        self.visit_str(&value)
    }

    fn visit_none<E: serde::de::Error>(self) -> Result<Self::Value, E> {
        match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_none(ctx),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_none(ctx, &key),
        };
        Ok(())
    }

    fn visit_unit<E: serde::de::Error>(self) -> Result<Self::Value, E> {
        self.visit_none()
    }

    fn visit_map<M>(self, mut access: M) -> Result<Self::Value, M::Error>
    where
        M: MapAccess<'de>,
    {
        // TODO(danakj): base::Value::Dict doesn't expose a way to reserve space, so we
        // don't bother using `access.size_hint()` here, unlike when creating a
        // list.
        let mut inner_ctx = match self.aggregate {
            DeserializationTarget::List { ctx } => self.fns.list_append_dict(ctx),
            DeserializationTarget::Dict { ctx, key } => self.fns.dict_set_dict(ctx, &key),
        };
        while let Some(key) = access.next_key::<String>()? {
            access.next_value_seed(ValueVisitor {
                fns: self.fns,
                aggregate: DeserializationTarget::Dict { ctx: inner_ctx.as_mut(), key },
                recursion_depth_check: self.recursion_depth_check.recurse()?,
            })?;
        }
        Ok(())
    }

    fn visit_seq<S>(self, mut access: S) -> Result<Self::Value, S::Error>
    where
        S: SeqAccess<'de>,
    {
        let mut inner_ctx = match self.aggregate {
            DeserializationTarget::List { ctx } => {
                self.fns.list_append_list(ctx, access.size_hint().unwrap_or(0))
            }
            DeserializationTarget::Dict { ctx, key } => {
                self.fns.dict_set_list(ctx, &key, access.size_hint().unwrap_or(0))
            }
        };
        while let Some(_) = access.next_element_seed(ValueVisitor {
            fns: self.fns,
            aggregate: DeserializationTarget::List { ctx: inner_ctx.as_mut() },
            recursion_depth_check: self.recursion_depth_check.recurse()?,
        })? {}
        Ok(())
    }
}

impl<'de, 'c> DeserializeSeed<'de> for ValueVisitor<'c> {
    // We call out to C++ to construct the deserialized type, so no output from
    // here.
    type Value = ();

    fn deserialize<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(self)
    }
}
