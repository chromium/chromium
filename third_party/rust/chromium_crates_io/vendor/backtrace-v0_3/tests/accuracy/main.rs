mod auxiliary;

macro_rules! pos {
    () => {
        (file!(), line!())
    };
}

#[collapse_debuginfo(yes)]
macro_rules! check {
    ($($pos:expr),*) => ({
        verify(&[$($pos,)* pos!()]);
    })
}

type Pos = (&'static str, u32);

#[test]
fn doit() {
    if
    // Skip musl which is by default statically linked and doesn't support
    // dynamic libraries.
    !cfg!(target_env = "musl")
    // Skip Miri, since it doesn't support dynamic libraries.
    && !cfg!(miri)
    {
        // TODO(#238) this shouldn't have to happen first in this function, but
        // currently it does.
        let path = dylib_dep_path();
        unsafe {
            let lib = libloading::Library::new(&path).unwrap();
            let api = lib.get::<extern "C" fn(Pos, fn(Pos, Pos))>(b"foo").unwrap();
            api(pos!(), |a, b| {
                check!(a, b);
            });
        }
    }

    outer(pos!());
}

fn dylib_dep_path() -> std::path::PathBuf {
    let name = format!(
        "{}dylib_dep{}",
        std::env::consts::DLL_PREFIX,
        std::env::consts::DLL_SUFFIX
    );
    let exe = std::env::current_exe().unwrap();

    // Old layout: test and dylib are both in `<profile>/deps/`
    let path = exe.ancestors().nth(1).unwrap().join(&name);
    if path.exists() {
        return path;
    }

    // New layout:
    // test is in `<profile>/build/backtrace/<hash1>/out/`
    // dylib is in `<profile>/build/dylib-dep/<hash2>/out/`
    let dir = exe
        .ancestors()
        .nth(4)
        .unwrap_or_else(|| panic!("unexpected exe path {}", exe.display()))
        .join("dylib-dep");
    std::fs::read_dir(&dir)
        .unwrap_or_else(|err| panic!("read_dir({}) failed: {err}", dir.display()))
        .flatten()
        .map(|entry| entry.path().join("out").join(&name))
        .find(|p| p.exists())
        .unwrap_or_else(|| panic!("missing {} in {}/*/out/", name, dir.display()))
}

#[inline(never)]
fn outer(main_pos: Pos) {
    inner(main_pos, pos!());
    inner_inlined(main_pos, pos!());
}

#[inline(never)]
#[rustfmt::skip]
fn inner(main_pos: Pos, outer_pos: Pos) {
    check!(main_pos, outer_pos);
    check!(main_pos, outer_pos);
    let inner_pos = pos!(); auxiliary::callback(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });
    let inner_pos = pos!(); auxiliary::callback_inlined(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });
}

#[inline(always)]
#[rustfmt::skip]
fn inner_inlined(main_pos: Pos, outer_pos: Pos) {
    check!(main_pos, outer_pos);
    check!(main_pos, outer_pos);

    #[inline(always)]
    fn inner_further_inlined(main_pos: Pos, outer_pos: Pos, inner_pos: Pos) {
        check!(main_pos, outer_pos, inner_pos);
    }
    inner_further_inlined(main_pos, outer_pos, pos!());

    let inner_pos = pos!(); auxiliary::callback(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });
    let inner_pos = pos!(); auxiliary::callback_inlined(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });

    // this tests a distinction between two independent calls to the inlined function.
    // (un)fortunately, LLVM somehow merges two consecutive such calls into one node.
    inner_further_inlined(main_pos, outer_pos, pos!());
}

fn verify(filelines: &[Pos]) {
    let trace = backtrace::Backtrace::new();
    println!("-----------------------------------");
    println!("looking for:");
    for (file, line) in filelines.iter().rev() {
        println!("\t{file}:{line}");
    }
    println!("found:\n{trace:?}");
    let mut symbols = trace.frames().iter().flat_map(|frame| frame.symbols());
    let mut iter = filelines.iter().rev();
    while let Some((file, line)) = iter.next() {
        loop {
            let sym = match symbols.next() {
                Some(sym) => sym,
                None => panic!("failed to find {file}:{line}"),
            };
            if let Some(filename) = sym.filename() {
                if let Some(lineno) = sym.lineno() {
                    if filename.ends_with(file) && lineno == *line {
                        break;
                    }
                }
            }
        }
    }
}
