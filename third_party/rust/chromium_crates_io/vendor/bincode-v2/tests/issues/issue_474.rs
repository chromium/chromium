#![cfg(all(feature = "serde", feature = "std"))]

extern crate std;

use chrono::{DateTime, Utc};
use serde::de::DeserializeOwned;
use std::collections::HashMap;
use std::prelude::rust_2021::*;
use uuid::Uuid;

#[derive(serde_derive::Serialize, serde_derive::Deserialize, PartialEq, Eq, Debug)]
pub struct MyStruct {
    name: String,
}

#[derive(serde_derive::Serialize, serde_derive::Deserialize, PartialEq, Eq, Debug)]
pub struct CustomerTest {
    pub id: Option<Uuid>,
    pub email_address: Option<String>,
    pub is_active: Option<bool>,
    pub date_stamp: Option<DateTime<Utc>>,
}

#[test]
fn test() {
    let test = MyStruct {
        name: "Test Value".into(),
    };
    let cache_id = Uuid::nil();
    let cache = MemCache::default();

    cache.set_data::<MyStruct>(&cache_id, &test, 5).unwrap();
    let model = cache.get_data::<MyStruct>(&cache_id).unwrap();
    assert_eq!(test, model);

    let test = CustomerTest {
        id: Some(Uuid::nil()),
        email_address: Some("foo@bar".into()),
        is_active: None,
        date_stamp: Some(Utc::now()),
    };

    cache.set_data::<CustomerTest>(&cache_id, &test, 5).unwrap();
    let model = cache.get_data::<CustomerTest>(&cache_id).unwrap();
    assert_eq!(test, model);
}

#[derive(Default)]
struct MemCache {
    cache: std::sync::RwLock<HashMap<Uuid, CacheItem>>,
}

impl MemCache {
    fn set_data<T>(
        &self,
        key: &Uuid,
        cache_data: &T,
        expire_seconds: i64,
    ) -> Result<(), bincode::error::EncodeError>
    where
        T: Send + Sync + serde::Serialize,
    {
        let config = bincode::config::standard();
        let mut guard = self.cache.write().unwrap();

        let encoded = bincode::serde::encode_to_vec(cache_data, config)?;
        let cache_item = CacheItem::new(encoded, expire_seconds);

        guard.insert(*key, cache_item);
        Ok(())
    }

    fn get_data<T>(&self, key: &Uuid) -> Result<T, bincode::error::DecodeError>
    where
        T: Send + Sync + DeserializeOwned,
    {
        let config = bincode::config::standard();
        let guard = self.cache.read().unwrap();
        let cache_item = guard.get(key).unwrap();
        let (decoded, _len): (T, usize) =
            bincode::serde::decode_from_slice(&cache_item.payload[..], config)?;
        Ok(decoded)
    }
}

struct CacheItem {
    payload: Vec<u8>,
}

impl CacheItem {
    fn new(payload: Vec<u8>, _expire_seconds: i64) -> Self {
        Self { payload }
    }
}
