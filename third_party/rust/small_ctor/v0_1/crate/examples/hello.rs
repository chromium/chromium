extern crate small_ctor;

/// This is a small example function that is executed as ctor
#[small_ctor::ctor]
unsafe fn hello() {
    println!("life before main");
}

fn main() {
    println!("main");
}
