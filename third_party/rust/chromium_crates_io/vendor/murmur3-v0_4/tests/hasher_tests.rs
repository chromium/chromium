extern crate murmur3;

use std::hash::Hasher;

use murmur3::murmur3_32::MurmurHasher;

#[test]
fn hasher_test_1_32bit(){
    let bytes = b"Lorem ipsum dolor sit amet, consectetur adipisicing elit";
    let hash32 = 0x3bf7e870;
    let mut h = MurmurHasher::default();
    h.write(bytes);
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    h.write( b"Lorem ipsum dolor sit amet,");
    h.write( b" consectetur adipisicing elit");
    assert_eq!(hash32, h.finish());


    let mut h = MurmurHasher::default();
    h.write( b"L");
    h.write( b"orem ipsum dolor sit amet, consectetur adipisicing elit");
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    h.write( b"Lo");
    h.write( b"rem ipsum dolor sit amet, consectetur adipisicing elit");

    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    h.write( b"Lor");
    h.write(b"em ipsum dolor sit amet, consectetur adipisicing elit");
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(1){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(2){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(3){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(4){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());
}


#[test]
fn test_32_hasher_2(){
    let hash32  = 0x13a51193;
    let bytes = b"12345";

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(1){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(2){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(3){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(4){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

    let mut h = MurmurHasher::default();
    for x in bytes.chunks(5){
        h.write(x);
    }
    assert_eq!(hash32, h.finish());

}