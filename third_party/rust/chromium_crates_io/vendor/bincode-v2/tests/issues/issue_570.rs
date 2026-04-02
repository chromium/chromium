#![cfg(feature = "derive")]

#[derive(bincode::Encode, bincode::Decode)]
pub struct Eg<D, E> {
    data: (D, E),
}
