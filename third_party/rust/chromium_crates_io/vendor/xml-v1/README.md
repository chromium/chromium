xml-rs, an XML library for Rust
===============================

[![CI](https://github.com/kornelski/xml-rs/actions/workflows/main.yml/badge.svg)](https://github.com/kornelski/xml-rs/actions/workflows/main.yml)
[![crates.io][crates-io-img]](https://lib.rs/crates/xml)
[![docs][docs-img]](https://docs.rs/xml/)

[Documentation](https://docs.rs/xml/)

  [crates-io-img]: https://img.shields.io/crates/v/xml.svg
  [docs-img]: https://img.shields.io/badge/docs-latest%20release-6495ed.svg

xml-rs is an XML library for the [Rust](https://www.rust-lang.org/) programming language.
It supports reading and writing of XML documents in a streaming fashion (without DOM).

### Features

* XML spec conformance better than other pure-Rust libraries.

* Easy to use API based on `Iterator`s and regular `String`s without tricky lifetimes.

* Support for UTF-16, UTF-8, ISO-8859-1, and ASCII encodings.

* Written entirely in the safe Rust subset. Designed to safely handle untrusted input.


The API is heavily inspired by Java Streaming API for XML ([StAX][stax]). It contains a pull parser much like StAX event reader. It provides an iterator API, so you can leverage Rust's existing iterators library features.

  [stax]: https://en.wikipedia.org/wiki/StAX

It also provides a streaming document writer much like StAX event writer.
This writer consumes its own set of events, but reader events can be converted to
writer events easily, and so it is possible to write XML transformation chains in a pretty
clean manner.

This parser is mostly full-featured, however, there are limitations:
* Legacy code pages and non-Unicode encodings are not supported;
* DTD validation is not supported (but entities defined in the internal subset are supported);
* attribute value normalization is not performed, and end-of-line characters are not normalized either.

Other than that the parser tries to be mostly XML-1.1-compliant.

Writer is also mostly full-featured with the following limitations:
* no support for encodings other than UTF-8,
* no support for emitting `<!DOCTYPE>` declarations;
* more validations of input are needed, for example, checking that namespace prefixes are bounded
  or comments are well-formed.

Building and using
------------------

xml-rs uses [Cargo](https://crates.io), so add it with `cargo add xml` or modify `Cargo.toml`:

```toml
[dependencies]
xml = "1.0"
```

The package exposes a single crate called `xml`.

Reading XML documents
---------------------

[`xml::reader::EventReader`][EventReader] requires a [`Read`][stdread] instance to read from. It can be a `File` wrapped in `BufReader`, or a `Vec<u8>`, or a `&[u8]` slice.

[EventReader]: https://docs.rs/xml/latest/xml/reader/struct.EventReader.html
[stdread]: https://doc.rust-lang.org/stable/std/io/trait.Read.html

`EventReader` implements `IntoIterator` trait, so you can use it in a `for` loop directly:

```rust,no_run
use std::fs::File;
use std::io::BufReader;

use xml::reader::{EventReader, XmlEvent};

fn main() -> std::io::Result<()> {
    let file = File::open("file.xml")?;
    let file = BufReader::new(file); // Buffering is important for performance

    let parser = EventReader::new(file);
    let mut depth = 0;
    for e in parser {
        match e {
            Ok(XmlEvent::StartElement { name, .. }) => {
                println!("{:spaces$}+{name}", "", spaces = depth * 2);
                depth += 1;
            }
            Ok(XmlEvent::EndElement { name }) => {
                depth -= 1;
                println!("{:spaces$}-{name}", "", spaces = depth * 2);
            }
            Err(e) => {
                eprintln!("Error: {e}");
                break;
            }
            // There's more: https://docs.rs/xml/latest/xml/reader/enum.XmlEvent.html
            _ => {}
        }
    }

    Ok(())
}
```

Document parsing can end normally or with an error. Regardless of exact cause, the parsing
process will be stopped, and the iterator will terminate normally.

You can also have finer control over when to pull the next event from the parser using its own
`next()` method:

```rust,ignore
match parser.next() {
    ...
}
```

Upon the end of the document or an error, the parser will remember the last event and will always
return it in the result of `next()` call afterwards. If iterator is used, then it will yield
error or end-of-document event once and will produce `None` afterwards.

It is also possible to tweak parsing process a little using [`xml::reader::ParserConfig`][ParserConfig] structure.
See its documentation for more information and examples.

[ParserConfig]: https://docs.rs/xml/latest/xml/reader/struct.ParserConfig.html

You can find a more extensive example of using `EventReader` in `src/analyze.rs`, which is a
small program (BTW, it is built with `cargo build` and can be run after that) which shows various
statistics about specified XML document. It can also be used to check for well-formedness of
XML documents - if a document is not well-formed, this program will exit with an error.


## Parsing untrusted inputs

The parser is written in safe Rust subset, so by Rust's guarantees the worst that it can do is to cause a panic.
You can use `ParserConfig` to set limits on maximum lenghts of names, attributes, text, entities, etc.
You should also set a maximum document size via `io::Read`'s [`take(max)`](https://doc.rust-lang.org/stable/std/io/trait.Read.html#method.take) method.

Writing XML documents
---------------------

xml-rs also provides a streaming writer much like StAX event writer. With it you can write an
XML document to any `Write` implementor.

```rust,no_run
use std::io;
use xml::writer::{EmitterConfig, XmlEvent};

/// A simple demo syntax where "+foo" makes `<foo>`, "-foo" makes `</foo>`
fn make_event_from_line(line: &str) -> XmlEvent {
    let line = line.trim();
    if let Some(name) = line.strip_prefix("+") {
        XmlEvent::start_element(name).into()
    } else if line.starts_with("-") {
        XmlEvent::end_element().into()
    } else {
        XmlEvent::characters(line).into()
    }
}

fn main() -> io::Result<()> {
    let input = io::stdin();
    let output = io::stdout();
    let mut writer = EmitterConfig::new()
        .perform_indent(true)
        .create_writer(output);

    let mut line = String::new();
    loop {
        line.clear();
        let bytes_read = input.read_line(&mut line)?;
        if bytes_read == 0 {
            break; // EOF
        }

        let event = make_event_from_line(&line);
        if let Err(e) = writer.write(event) {
            panic!("Write error: {e}")
        }
    }
    Ok(())
}
```

The code example above also demonstrates how to create a writer out of its configuration.
Similar thing also works with `EventReader`.

The library provides an XML event building DSL which helps to construct complex events,
e.g. ones having namespace definitions. Some examples:

```rust,ignore
// <a:hello a:param="value" xmlns:a="urn:some:document">
XmlEvent::start_element("a:hello").attr("a:param", "value").ns("a", "urn:some:document")

// <hello b:config="name" xmlns="urn:default:uri">
XmlEvent::start_element("hello").attr("b:config", "value").default_ns("urn:defaul:uri")

// <![CDATA[some unescaped text]]>
XmlEvent::cdata("some unescaped text")
```

Of course, one can create `XmlEvent` enum variants directly instead of using the builder DSL.
There are more examples in [`xml::writer::XmlEvent`][XmlEvent] documentation.

[XmlEvent]: https://docs.rs/xml/latest/xml/reader/enum.XmlEvent.html

The writer has multiple configuration options; see `EmitterConfig` documentation for more
information.

[EmitterConfig]: https://docs.rs/xml/latest/xml/writer/struct.EmitterConfig.html

Bug reports
------------

Please report issues at: <https://github.com/kornelski/xml-rs/issues>.

Before reporting issues with XML conformance, please find the relevant section in the XML spec first.

## [Upgrading from 0.8 to 1.0](https://github.com/kornelski/xml-rs/blob/main/Changelog.md)

It should be pretty painless:

* Change `xml-rs = "0.8"` to `xml = "1.0"` in `Cargo.toml`
* Add `_ => {}` to `match` statements where the compiler complains. A new `Doctype` event has been added, and error enums are non-exhaustive.
* If you were creating `ParserConfig` using a struct literal, please use `ParserConfig::new()` and the setters.
