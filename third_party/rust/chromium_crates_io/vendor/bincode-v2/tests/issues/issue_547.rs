#![cfg(all(feature = "serde", feature = "std"))]

use glam::vec3;

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, PartialEq)]
struct Instance {
    position: glam::Vec3,
}

#[test]
fn test() {
    let instance = Instance {
        position: vec3(2.0, 2.0, 2.0),
    };

    let m = bincode::serde::encode_to_vec(&instance, bincode::config::standard()).unwrap();
    let instance2: Instance = bincode::serde::decode_from_slice(&m, bincode::config::standard())
        .unwrap()
        .0;

    assert_eq!(instance, instance2);
}
