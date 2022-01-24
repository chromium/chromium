#[cxx::bridge]
mod ffi {
    extern "Rust" {
        async fn f();
    }
}

async fn f() {}

fn main() {}
