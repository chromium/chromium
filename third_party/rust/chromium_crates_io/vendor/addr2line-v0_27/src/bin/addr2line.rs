use std::borrow::Cow;
use std::ffi::OsStr;
use std::io::{BufRead, Lines, StdinLock, Write};
use std::path::{Path, PathBuf};

use addr2line::{Loader, Location};
use clap::{Arg, ArgAction, Command};
use fallible_iterator::FallibleIterator;

fn parse_uint_from_hex_string(string: &str) -> Option<u64> {
    if string.len() > 2 && string.starts_with("0x") {
        u64::from_str_radix(&string[2..], 16).ok()
    } else {
        u64::from_str_radix(string, 16).ok()
    }
}

enum Addrs<'a, R: gimli::Reader> {
    Args(clap::parser::ValuesRef<'a, String>),
    Stdin(Lines<StdinLock<'a>>),
    All {
        iter: addr2line::LocationRangeIter<'a, R>,
        max: u64,
    },
}

impl<'a, R: gimli::Reader> Iterator for Addrs<'a, R> {
    type Item = Option<u64>;

    fn next(&mut self) -> Option<Option<u64>> {
        let text = match self {
            Addrs::Args(vals) => vals.next().map(Cow::from)?,
            // Treat all stdin errors as EOF.
            Addrs::Stdin(lines) => lines.next()?.ok().map(Cow::from)?,
            Addrs::All { iter, max } => {
                for (addr, _len, _loc) in iter {
                    if addr >= *max {
                        *max = addr + 1;
                        return Some(Some(addr));
                    }
                }
                return None;
            }
        };
        Some(parse_uint_from_hex_string(&text))
    }
}

fn print_loc(loc: Option<&Location<'_>>, basenames: bool, llvm: bool) {
    if let Some(loc) = loc {
        if let Some(ref file) = loc.file.as_ref() {
            let path = if basenames {
                Path::new(Path::new(file).file_name().unwrap())
            } else {
                Path::new(file)
            };
            print!("{}:", path.display());
        } else {
            print!("??:");
        }
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
        println!("??:0");
    }
}

fn print_function(name: Option<&str>, language: Option<gimli::DwLang>, demangle: bool) {
    if let Some(name) = name {
        if demangle {
            print!("{}", addr2line::demangle_auto(Cow::from(name), language));
        } else {
            print!("{}", name);
        }
    } else {
        print!("??");
    }
}

struct Options<'a> {
    do_functions: bool,
    do_inlines: bool,
    pretty: bool,
    print_addrs: bool,
    basenames: bool,
    demangle: bool,
    llvm: bool,
    exe: &'a PathBuf,
    sup: Option<&'a PathBuf>,
    section: Option<&'a String>,
}

fn main() {
    let name = std::env::args_os()
        .next()
        .as_deref()
        .map(Path::new)
        .and_then(Path::file_name)
        .and_then(OsStr::to_str)
        .unwrap_or("addr2line")
        .to_owned();

    let matches = Command::new("addr2line")
        .version(env!("CARGO_PKG_VERSION"))
        .about("A fast addr2line Rust port")
        .max_term_width(100)
        .args(&[
            Arg::new("exe")
                .short('e')
                .long("exe")
                .value_name("filename")
                .value_parser(clap::value_parser!(PathBuf))
                .help(
                    "Specify the name of the executable for which addresses should be translated.",
                )
                .required(true),
            Arg::new("sup")
                .long("sup")
                .value_name("filename")
                .value_parser(clap::value_parser!(PathBuf))
                .help("Path to supplementary object file."),
            Arg::new("section")
                .short('j')
                .long("section")
                .value_name("name")
                .help(
                    "Read offsets relative to the specified section instead of absolute addresses.",
                ),
            Arg::new("all")
                .long("all")
                .action(ArgAction::SetTrue)
                .conflicts_with("addrs")
                .help("Display all addresses that have line number information."),
            Arg::new("functions")
                .short('f')
                .long("functions")
                .action(ArgAction::SetTrue)
                .help("Display function names as well as file and line number information."),
            Arg::new("pretty").short('p').long("pretty-print")
                .action(ArgAction::SetTrue)
                .help(
                "Make the output more human friendly: each location are printed on one line.",
            ),
            Arg::new("inlines").short('i').long("inlines")
                .action(ArgAction::SetTrue)
                .help(
                "If the address belongs to a function that was inlined, the source information for \
                all enclosing scopes back to the first non-inlined function will also be printed.",
            ),
            Arg::new("addresses").short('a').long("addresses")
                .action(ArgAction::SetTrue)
                .help(
                "Display the address before the function name, file and line number information.",
            ),
            Arg::new("basenames")
                .short('s')
                .long("basenames")
                .action(ArgAction::SetTrue)
                .help("Display only the base of each file name."),
            Arg::new("demangle").short('C').long("demangle")
                .action(ArgAction::SetTrue)
                .help(
                "Demangle function names. \
                Specifying a specific demangling style (like GNU addr2line) is not supported. \
                (TODO)"
            ),
            Arg::new("llvm")
                .long("llvm")
                .action(ArgAction::SetTrue)
                .help("Display output in the same format as llvm-symbolizer."),
            Arg::new("addrs")
                .action(ArgAction::Append)
                .help("Addresses to use instead of reading from stdin."),
        ])
        .get_matches();

    let opts = Options {
        do_functions: matches.get_flag("functions"),
        do_inlines: matches.get_flag("inlines"),
        pretty: matches.get_flag("pretty"),
        print_addrs: matches.get_flag("addresses"),
        basenames: matches.get_flag("basenames"),
        demangle: matches.get_flag("demangle"),
        llvm: matches.get_flag("llvm"),
        exe: matches.get_one::<PathBuf>("exe").unwrap(),
        sup: matches.get_one::<PathBuf>("sup"),
        section: matches.get_one::<String>("section"),
    };

    let ctx = match Loader::new_with_sup(opts.exe, opts.sup) {
        Ok(ctx) => ctx,
        Err(e) => {
            eprintln!("{name}: failed to load debug info: {e}");
            std::process::exit(1);
        }
    };

    let section_range = opts.section.map(|section_name| {
        ctx.get_section_range(section_name.as_bytes())
            .unwrap_or_else(|| {
                eprintln!("{name}: cannot find section {section_name}");
                std::process::exit(1);
            })
    });

    let stdin = std::io::stdin();
    let addrs = if matches.get_flag("all") {
        let iter = match ctx.find_location_range(0, !0) {
            Ok(iter) => iter,
            Err(e) => {
                eprintln!("{name}: failed to find all addresses: {e}");
                std::process::exit(1);
            }
        };
        Addrs::All { iter, max: 0 }
    } else {
        matches
            .get_many::<String>("addrs")
            .map(Addrs::Args)
            .unwrap_or_else(|| Addrs::Stdin(stdin.lock().lines()))
    };

    for probe in addrs {
        if opts.print_addrs {
            let addr = probe.unwrap_or(0);
            if opts.llvm {
                print!("0x{:x}", addr);
            } else {
                print!("0x{:016x}", addr);
            }
            if opts.pretty {
                print!(": ");
            } else {
                println!();
            }
        }

        // If --section is given, add the section address to probe.
        let probe = probe.and_then(|probe| {
            if let Some(section_range) = section_range {
                if probe < (section_range.end - section_range.begin) {
                    Some(probe + section_range.begin)
                } else {
                    // If addr >= section size, treat it as if no line number information was found.
                    None
                }
            } else {
                Some(probe)
            }
        });

        if opts.do_functions || opts.do_inlines {
            let print_frames = |probe| {
                let Some(probe) = probe else {
                    return false;
                };
                let frames = match ctx.find_frames(probe) {
                    Ok(frames) => frames,
                    Err(e) => {
                        eprintln!("{name}: failed to find frames for 0x{probe:x}: {e}");
                        // Fallback to symbol lookup to ensure we print something.
                        return false;
                    }
                };

                let mut printed_anything = false;
                let mut frames = frames.peekable();
                let mut first = true;
                while let Some(frame_result) = frames.next().transpose() {
                    let frame = match frame_result {
                        Ok(frame) => frame,
                        Err(e) => {
                            eprintln!("{name}: failed to get next frame for 0x{probe:x}: {e}");
                            // Fallback to symbol lookup if needed to ensure we print something.
                            return printed_anything;
                        }
                    };

                    if opts.pretty && !first {
                        print!(" (inlined by) ");
                    }
                    first = false;

                    if opts.do_functions {
                        // Only use the symbol table if this isn't an inlined function.
                        let symbol = if matches!(frames.peek(), Ok(None)) {
                            ctx.find_symbol(probe)
                        } else {
                            None
                        };
                        if symbol.is_some() {
                            // Prefer the symbol table over the DWARF name because:
                            // - the symbol can include a clone suffix
                            // - llvm may omit the linkage name in the DWARF with -g1
                            print_function(symbol, None, opts.demangle);
                        } else if let Some(func) = frame.function {
                            print_function(
                                func.raw_name().ok().as_deref(),
                                func.language,
                                opts.demangle,
                            );
                        } else {
                            print_function(None, None, opts.demangle);
                        }

                        if opts.pretty {
                            print!(" at ");
                        } else {
                            println!();
                        }
                    }

                    print_loc(frame.location.as_ref(), opts.basenames, opts.llvm);

                    printed_anything = true;

                    if !opts.do_inlines {
                        break;
                    }
                }
                printed_anything
            };

            if !print_frames(probe) {
                if opts.do_functions {
                    let symbol = probe.and_then(|probe| ctx.find_symbol(probe));
                    print_function(symbol, None, opts.demangle);

                    if opts.pretty {
                        print!(" at ");
                    } else {
                        println!();
                    }
                }

                print_loc(None, opts.basenames, opts.llvm);
            }
        } else {
            let loc = probe.and_then(|probe| match ctx.find_location(probe) {
                Ok(loc) => loc,
                Err(e) => {
                    eprintln!("{name}: failed to find location for 0x{probe:x}: {e}");
                    None
                }
            });
            print_loc(loc.as_ref(), opts.basenames, opts.llvm);
        }

        if opts.llvm {
            println!();
        }

        // Flush required because caller may wait for response before
        // writing another address.
        if let Err(e) = std::io::stdout().flush() {
            if e.kind() == std::io::ErrorKind::BrokenPipe {
                std::process::exit(0);
            }
            eprintln!("{name}: error flushing stdout: {e}");
            std::process::exit(1);
        }
    }
}
