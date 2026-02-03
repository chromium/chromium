<div align="center">
  <img src="https://github.com/mitsuhiko/minijinja/raw/main/artwork/logo.png" alt="" width=320>
  <p><strong>MiniJinja: a powerful template engine with minimal dependencies</strong></p>

[![License](https://img.shields.io/github/license/mitsuhiko/minijinja)](https://github.com/mitsuhiko/minijinja/blob/main/LICENSE)
[![Crates.io](https://img.shields.io/crates/d/minijinja.svg)](https://crates.io/crates/minijinja)
[![rustc 1.63.0](https://img.shields.io/badge/rust-1.63%2B-orange.svg)](https://img.shields.io/badge/rust-1.63%2B-orange.svg)
[![Documentation](https://docs.rs/minijinja/badge.svg)](https://docs.rs/minijinja)

</div>

MiniJinja is a powerful but minimal dependency template engine
which is based on the syntax and behavior of the
[Jinja2](https://jinja.palletsprojects.com/) template engine for Python.

It's implemented in [Rust](https://github.com/mitsuhiko/minijinja/tree/main/minijinja) and [Go](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-go) and is also available via WASM for [JavaScript](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-js)
and as a [Python extension module](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-py) and as a [command line utility](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-cli).

It's supports all `serde` types and only has it as a single required
dependency. It supports [a range of features from Jinja2](https://github.com/mitsuhiko/minijinja/blob/main/COMPATIBILITY.md)
including inheritance, filters and more.  The goal is that it should be possible
to use some templates in Rust programs without the fear of pulling in complex
dependencies for a small problem.  Additionally it tries not to re-invent
something but stay in line with prior art to leverage an already existing
ecosystem of editor integrations.

```
$ cargo tree
minimal v0.1.0 (examples/minimal)
└── minijinja v2.15.1 (minijinja)
    └── serde v1.0.144
```

Additionally minijinja is also available as an (optionally pre-compiled) command line executable
called [`minijinja-cli`](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-cli):

```
$ curl -sSfL https://github.com/mitsuhiko/minijinja/releases/latest/download/minijinja-cli-installer.sh | sh
$ echo "Hello {{ name }}" | minijinja-cli - -Dname=World
Hello World
```

You can play with MiniJinja online [in the browser playground](https://mitsuhiko.github.io/minijinja-playground/)
powered by a WASM build of MiniJinja.

**Goals:**

* [Well documented](https://docs.rs/minijinja), compact API
* Minimal dependencies, reasonable compile times and [decent runtime performance](https://github.com/mitsuhiko/minijinja/tree/main/benchmarks#comparison-results)
* [Stay as close as possible](https://github.com/mitsuhiko/minijinja/blob/main/COMPATIBILITY.md) to Jinja2
* Support for [expression evaluation](https://docs.rs/minijinja/latest/minijinja/struct.Expression.html) which
  allows the use [as a DSL](https://github.com/mitsuhiko/minijinja/tree/main/examples/dsl)
* Support for all [`serde`](https://serde.rs) compatible types
* [Well tested](https://github.com/mitsuhiko/minijinja/tree/main/minijinja/tests)
* Support for [dynamic runtime objects](https://docs.rs/minijinja/latest/minijinja/value/trait.Object.html) with methods and dynamic attributes
* [Descriptive errors](https://github.com/mitsuhiko/minijinja/tree/main/examples/error)
* Bindings for [JavaScript](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-js),
  [Python](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-py), and [C](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-cabi)
* Also available for [Go](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-go)
* Comes with a handy [CLI](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-cli)
* [Compiles to WebAssembly](https://github.com/mitsuhiko/minijinja-playground/blob/main/src/lib.rs)

## Example

**Example Template:**

```jinja
{% extends "layout.html" %}
{% block body %}
  <p>Hello {{ name }}!</p>
{% endblock %}
```

**Invoking from Rust:**

```rust
use minijinja::{Environment, context};

fn main() {
    let mut env = Environment::new();
    env.add_template("hello.txt", "Hello {{ name }}!").unwrap();
    let template = env.get_template("hello.txt").unwrap();
    println!("{}", template.render(context! { name => "World" }).unwrap());
}
```

## Getting Help

If you are stuck with `MiniJinja`, have suggestions or need help, you can use the
[GitHub Discussions](https://github.com/mitsuhiko/minijinja/discussions).

## Related Crates

* [minijinja-autoreload](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-autoreload): provides
  auto reloading functionality of environments
* [minijinja-embed](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-embed): provides
  utilities for embedding templates in a binary
* [minijinja-contrib](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-contrib): provides
  additional utilities too specific for the core
* [minijinja-py](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-py): makes MiniJinja
  available to Python
* [minijinja-js](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-js): makes MiniJinja
  available to JavaScript via WASM (for Node and Browser)
* [minijinja-go](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-go): a native Go
  implementation of MiniJinja
* [minijinja-cli](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-cli): a command line utility.
* [minijinja-cabi](https://github.com/mitsuhiko/minijinja/tree/main/minijinja-cabi): a C binding to MiniJinja.

## Use Cases and Users

Here are some interesting Open Source users and use cases of MiniJinja.  The examples link directly to where
the engine is used so you can see how it's utilized:

* AI Chat Templating:
  * **[HuggingFace](https://huggingface.co/docs/text-generation-inference/index)** uses it to [render LLM chat templates](https://github.com/huggingface/text-generation-inference/blob/0759ec495e15a865d2a59befc2b796b5acc09b50/router/src/infer/mod.rs)
  * **[mistral.rs](https://github.com/EricLBuehler/mistral.rs)** uses it to [render LLM chat templates](https://github.com/EricLBuehler/mistral.rs/blob/c834f59fe0b3b020a56cb6a0279a051370554539/mistralrs-core/src/pipeline/chat_template.rs)
  * **[BoundaryML's BAML](https://docs.boundaryml.com/)** uses it to [render LLM chat templates](https://github.com/BoundaryML/baml/blob/17123de7ea653f51547576169bb0589d39053edc/engine/baml-lib/jinja/src/lib.rs)
  * **[LSP-AI](https://github.com/SilasMarvin/lsp-ai)** uses it to [render LLM chat templates](https://github.com/SilasMarvin/lsp-ai/blob/1f70756c5b48e9098d64a7c5ce63ac803bc5d0ab/crates/lsp-ai/src/template.rs)
  * **[LoRAX](https://loraexchange.ai/)** uses it to [render LLM chat templates](https://github.com/predibase/lorax/blob/6a83954b8c6ffd51eb69e7096ee2730d53b903dd/router/src/infer.rs)
  * **[tensorzero](https://www.tensorzero.com/)** uses it to [render LLM and system templates](https://github.com/tensorzero/tensorzero/blob/26ae697f219c1f0385fe7936b9f04b97ff318f61/tensorzero-internal/src/minijinja_util.rs#L2)

* Data and Processing:
  * **[Cube](https://cube.dev/docs/product/data-modeling/dynamic/jinja)** uses it [for data modelling](https://github.com/cube-js/cube/tree/db11c121c77c663845242366d3d972b9bc30ae54/packages/cubejs-backend-native/src/template/mj_value)
  * **[PRQL](https://prql-lang.org/)** uses it [to handle DBT style pipelines](https://github.com/PRQL/prql/blob/59fb3cc4b9b6c9e195c928b1ba1134e2c5706ea3/prqlc/prqlc/src/cli/jinja.rs#L21)
  * **[qsv](https://qsv.dathere.com)** uses it [to render templates from CSV files](https://github.com/jqnatividad/qsv/blob/master/src/cmd/template.rs#L2), to [construct payloads to post to web services](https://github.com/jqnatividad/qsv/blob/master/src/cmd/fetchpost.rs#L3) and to [infer Data Dictionaries, Descriptions & Tags or Chat with your data](https://github.com/dathere/qsv/blob/master/src/cmd/describegpt.rs#L2).

* HTML Generation:
  * **[Zine](https://github.com/zineland/zine)** uses it to [generate static HTML](https://github.com/zineland/zine/blob/17285efe9f9a63b79a42a738b54d4d730b8cd551/src/engine.rs#L8)
  * **[Oranda](https://github.com/axodotdev/oranda)** uses it to [generate HTML landing pages](https://github.com/axodotdev/oranda/blob/fb97859c99ab81f644ab5b1449f725fc5c3e9721/src/site/templates.rs)

* Code Generation:
  * **[OpenTelemetry's Weaver](https://github.com/open-telemetry/weaver)** uses it to [generate documentation, code and other outputs](https://github.com/open-telemetry/weaver/blob/d49881445e09beb42e1a394bfa5f3068c660daf3/crates/weaver_forge/src/lib.rs#L482-L567) from the OTel specification.
  * **[Maturin](https://github.com/PyO3/maturin)** uses it to [generate project structures](https://github.com/PyO3/maturin/blob/e35097e6cf3b9115736e8ae208972178029a20d0/src/new_project.rs)
  * **[cargo-dist](https://github.com/axodotdev/cargo-dist)** uses it to [generate CI and project configuration](https://github.com/axodotdev/cargo-dist/blob/4cd61134863f54ca5a037400ebec71d039d42742/cargo-dist/src/backend/templates.rs)

## Similar Projects

These are related template engines for Rust:

* [Askama](https://crates.io/crates/askama): Jinja inspired, type-safe, requires template
  precompilation. Has significant divergence from Jinja syntax in parts.
* [Tera](https://crates.io/crates/tera): Jinja inspired, dynamic, has divergences from Jinja.
* [Liquid](https://crates.io/crates/liquid): an implementation of Liquid templates for Rust.
  Liquid was inspired by Django from which Jinja took it's inspiration.
* [TinyTemplate](https://crates.io/crates/tinytemplate): minimal footprint template engine
  with syntax that takes lose inspiration from Jinja and handlebars.

## Sponsor

If you like the project and find it useful you can [become a
sponsor](https://github.com/sponsors/mitsuhiko).

## AI Use Disclaimer

This codebase mostly predates LLM based code generation but some recent features
have been built with support of AI.  For the AI contribution rules see
[AI Disclosure Rules](CONTRIBUTING.md#ai-disclosure).

## License and Links

- [Documentation](https://docs.rs/minijinja/)
- [Discussions](https://github.com/mitsuhiko/minijinja/discussions)
- [Examples](https://github.com/mitsuhiko/minijinja/tree/main/examples)
- [Issue Tracker](https://github.com/mitsuhiko/minijinja/issues)
- [MiniJinja Playground](https://mitsuhiko.github.io/minijinja-playground/)
- [Updating Guide](UPDATING.md)
- License: [Apache-2.0](https://github.com/mitsuhiko/minijinja/blob/main/LICENSE)
