extern crate addr2line;
extern crate clap;
extern crate fallible_iterator;
extern crate gimli;
extern crate memmap;
extern crate object;
extern crate typed_arena;

use std::borrow::Cow;
use std::fs::File;
use std::io::{BufRead, Lines, StdinLock, Write};
use std::path::Path;

use clap::{App, Arg, Values};
use fallible_iterator::FallibleIterator;
use object::{Object, ObjectSection};
use typed_arena::Arena;

use addr2line::{Context, Location};

fn parse_uint_from_hex_string(string: &str) -> u64 {
    if string.len() > 2 && string.starts_with("0x") {
        u64::from_str_radix(&string[2..], 16).expect("Failed to parse address")
    } else {
        u64::from_str_radix(string, 16).expect("Failed to parse address")
    }
}

enum Addrs<'a> {
    Args(Values<'a>),
    Stdin(Lines<StdinLock<'a>>),
}

impl<'a> Iterator for Addrs<'a> {
    type Item = u64;

    fn next(&mut self) -> Option<u64> {
        let text = match *self {
            Addrs::Args(ref mut vals) => vals.next().map(Cow::from),
            Addrs::Stdin(ref mut lines) => lines.next().map(Result::unwrap).map(Cow::from),
        };
        text.as_ref()
            .map(Cow::as_ref)
            .map(parse_uint_from_hex_string)
    }
}

fn print_loc(loc: &Option<Location>, basenames: bool, llvm: bool) {
    if let Some(ref loc) = *loc {
        let file = loc.file.as_ref().unwrap();
        let path = if basenames {
            Path::new(Path::new(file).file_name().unwrap())
        } else {
            Path::new(file)
        };
        print!("{}:", path.display());
        if llvm {
            print!("{}:{}", loc.line.unwrap_or(0), loc.column.unwrap_or(0));
        } else if let Some(line) = loc.line {
            print!("{}", line);
        } else {
            print!("?");
        }
        println!();
    } else if llvm {
        println!("??:0:0");
    } else {
        println!("??:?");
    }
}

fn print_function(name: &str, language: Option<gimli::DwLang>, demangle: bool) {
    if demangle {
        print!("{}", addr2line::demangle_auto(Cow::from(name), language));
    } else {
        print!("{}", name);
    }
}

fn load_file_section<'input, 'arena, Endian: gimli::Endianity>(
    id: gimli::SectionId,
    file: &object::File<'input>,
    endian: Endian,
    arena_data: &'arena Arena<Cow<'input, [u8]>>,
) -> Result<gimli::EndianSlice<'arena, Endian>, ()> {
    // TODO: Unify with dwarfdump.rs in gimli.
    let name = id.name();
    match file.section_by_name(name) {
        Some(section) => match section.uncompressed_data().unwrap() {
            Cow::Borrowed(b) => Ok(gimli::EndianSlice::new(b, endian)),
            Cow::Owned(b) => Ok(gimli::EndianSlice::new(arena_data.alloc(b.into()), endian)),
        },
        None => Ok(gimli::EndianSlice::new(&[][..], endian)),
    }
}

fn main() {
    let matches = App::new("hardliner")
        .version("0.1")
        .about("A fast addr2line clone")
        .arg(
            Arg::with_name("exe")
                .short("e")
                .long("exe")
                .value_name("filename")
                .help(
                    "Specify the name of the executable for which addresses should be translated.",
                )
                .required(true),
        )
        .arg(
            Arg::with_name("sup")
                .long("sup")
                .value_name("filename")
                .help("Path to supplementary object file."),
        )
        .arg(
            Arg::with_name("functions")
                .short("f")
                .long("functions")
                .help("Display function names as well as file and line number information."),
        )
        .arg(
            Arg::with_name("pretty")
                .short("p")
                .long("pretty-print")
                .help(
                    "Make the output more human friendly: each location are printed on \
                     one line.",
                ),
        )
        .arg(Arg::with_name("inlines").short("i").long("inlines").help(
            "If the address belongs to a function that was inlined, the source \
             information for all enclosing scopes back to the first non-inlined \
             function will also be printed.",
        ))
        .arg(
            Arg::with_name("addresses")
                .short("a")
                .long("addresses")
                .help(
                    "Display the address before the function name, file and line \
                     number information.",
                ),
        )
        .arg(
            Arg::with_name("basenames")
                .short("s")
                .long("basenames")
                .help("Display only the base of each file name."),
        )
        .arg(Arg::with_name("demangle").short("C").long("demangle").help(
            "Demangle function names. \
             Specifying a specific demangling style (like GNU addr2line) \
             is not supported. (TODO)",
        ))
        .arg(
            Arg::with_name("llvm")
                .long("llvm")
                .help("Display output in the same format as llvm-symbolizer."),
        )
        .arg(
            Arg::with_name("addrs")
                .takes_value(true)
                .multiple(true)
                .help("Addresses to use instead of reading from stdin."),
        )
        .get_matches();

    let arena_data = Arena::new();

    let do_functions = matches.is_present("functions");
    let do_inlines = matches.is_present("inlines");
    let pretty = matches.is_present("pretty");
    let print_addrs = matches.is_present("addresses");
    let basenames = matches.is_present("basenames");
    let demangle = matches.is_present("demangle");
    let llvm = matches.is_present("llvm");
    let path = matches.value_of("exe").unwrap();

    let file = File::open(path).unwrap();
    let map = unsafe { memmap::Mmap::map(&file).unwrap() };
    let object = &object::File::parse(&*map).unwrap();

    let endian = if object.is_little_endian() {
        gimli::RunTimeEndian::Little
    } else {
        gimli::RunTimeEndian::Big
    };

    let mut load_section = |id: gimli::SectionId| -> Result<_, _> {
        load_file_section(id, object, endian, &arena_data)
    };

    let sup_map;
    let sup_object = if let Some(sup_path) = matches.value_of("sup") {
        let sup_file = File::open(sup_path).unwrap();
        sup_map = unsafe { memmap::Mmap::map(&sup_file).unwrap() };
        Some(object::File::parse(&*sup_map).unwrap())
    } else {
        None
    };

    let symbols = object.symbol_map();
    let mut dwarf = gimli::Dwarf::load(&mut load_section).unwrap();
    if let Some(ref sup_object) = sup_object {
        let mut load_sup_section = |id: gimli::SectionId| -> Result<_, _> {
            load_file_section(id, sup_object, endian, &arena_data)
        };
        dwarf.load_sup(&mut load_sup_section).unwrap();
    }

    let ctx = Context::from_dwarf(dwarf).unwrap();

    let stdin = std::io::stdin();
    let addrs = matches
        .values_of("addrs")
        .map(Addrs::Args)
        .unwrap_or_else(|| Addrs::Stdin(stdin.lock().lines()));

    for probe in addrs {
        if print_addrs {
            if llvm {
                print!("0x{:x}", probe);
            } else {
                print!("0x{:016x}", probe);
            }
            if pretty {
                print!(": ");
            } else {
                println!();
            }
        }

        if do_functions || do_inlines {
            let mut printed_anything = false;
            let mut frames = ctx.find_frames(probe).unwrap().enumerate();
            while let Some((i, frame)) = frames.next().unwrap() {
                if pretty && i != 0 {
                    print!(" (inlined by) ");
                }

                if do_functions {
                    if let Some(func) = frame.function {
                        print_function(&func.raw_name().unwrap(), func.language, demangle);
                    } else if let Some(name) = symbols.get(probe).map(|x| x.name()) {
                        print_function(name, None, demangle);
                    } else {
                        print!("??");
                    }

                    if pretty {
                        print!(" at ");
                    } else {
                        println!();
                    }
                }

                print_loc(&frame.location, basenames, llvm);

                printed_anything = true;

                if !do_inlines {
                    break;
                }
            }

            if !printed_anything {
                if do_functions {
                    if let Some(name) = symbols.get(probe).map(|x| x.name()) {
                        print_function(name, None, demangle);
                    } else {
                        print!("??");
                    }

                    if pretty {
                        print!(" at ");
                    } else {
                        println!();
                    }
                }

                if llvm {
                    println!("??:0:0");
                } else {
                    println!("??:?");
                }
            }
        } else {
            let loc = ctx.find_location(probe).unwrap();
            print_loc(&loc, basenames, llvm);
        }

        if llvm {
            println!();
        }
        std::io::stdout().flush().unwrap();
    }
}
