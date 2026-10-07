# Nautilus 2.0 `LibAFL` Mutator

Nautilus is a coverage guided, grammar-based mutator. You can use it to improve your test coverage and find more bugs. By specifying the grammar of semi-valid inputs, Nautilus is able to perform complex mutation and to uncover more interesting test cases. Many of the ideas behind the original fuzzer are documented in a paper published at NDSS 2019.

<p>
<a href="https://www.ndss-symposium.org/wp-content/uploads/2019/02/ndss2019_04A-3_Aschermann_paper.pdf"> <img align="right" width="200"  src="https://github.com/RUB-SysSec/nautilus/raw/master/paper.png"> </a>
</p>

Version 2.0 has added many improvements to this early prototype.
Features from version 2.0 we support in `LibAFL`:

* Support for grammars specified in python
* Support for non-context free grammars using python scripts to generate inputs from the structure
* Support for specifying binary protocols/formats
* Support for specifying regex based terminals that aren't part of the directed mutations
* Better ability to avoid generating the same very short inputs over and over
* Helpful error output on invalid grammars

## How Does Nautilus Work?

You specify a grammar using rules such as `EXPR -> EXPR + EXPR` or `EXPR -> NUM` and `NUM -> 1`. From these rules, the fuzzer constructs a tree. This internal representation allows to apply much more complex mutations than raw bytes. This tree is then turned into a real input for the target application. In normal Context Free Grammars, this process is straightforward: all leaves are concatenated. The left tree in the example below would unparse to the input `a=1+2` and the right one to `a=1+1+1+2`. To increase the expressiveness of your grammars, using Nautilus you are able to provide python functions for the unparsing process to allow much more complex specifications.

<p align="center">
<img width="400" align="center" src="https://github.com/RUB-SysSec/nautilus/raw/master/tree.png">
</p>

## Examples

Here, we use python to generate a grammar for valid XML-like inputs. Notice the use of a script rule to ensure the opening
and closing tags match.

```python
#ctx.rule(NONTERM: string, RHS: string|bytes) adds a rule NONTERM->RHS. We can use {NONTERM} in the RHS to request a recursion. 
ctx.rule("START","<document>{XML_CONTENT}</document>")
ctx.rule("XML_CONTENT","{XML}{XML_CONTENT}")
ctx.rule("XML_CONTENT","")

#ctx.script(NONTERM:string, RHS: [string]], func) adds a rule NONTERM->func(*RHS). 
# In contrast to normal `rule`, RHS is an array of nonterminals. 
# It's up to the function to combine the values returned for the NONTERMINALS with any fixed content used.
ctx.script("XML",["TAG","ATTR","XML_CONTENT"], lambda tag,attr,body: b"<%s %s>%s</%s>"%(tag,attr,body,tag) )
ctx.rule("ATTR","foo=bar")
ctx.rule("TAG","some_tag")
ctx.rule("TAG","other_tag")

#sometimes we don't want to explore the set of possible inputs in more detail. For example, if we fuzz a script
#interpreter, we don't want to spend time on fuzzing all different variable names. In such cases we can use Regex
#terminals. Regex terminals are only mutated during generation, but not during normal mutation stages, saving a lot of time. 
#The fuzzer still explores different values for the regex, but it won't be able to learn interesting values incrementally. 
#Use this when incremantal exploration would most likely waste time.

ctx.regex("TAG","[a-z]+")
```

To test your [grammars](https://github.com/nautilus-fuzz/nautilus/tree/mit-main/grammars) you can use the generator:

```sh
$ cargo run --bin generator -- -g grammars/grammar_py_exmaple.py -t 100 
<document><some_tag foo=bar><other_tag foo=bar><other_tag foo=bar><some_tag foo=bar></some_tag></other_tag><some_tag foo=bar><other_tag foo=bar></other_tag></some_tag><other_tag foo=bar></other_tag><some_tag foo=bar></some_tag></other_tag><other_tag foo=bar></other_tag><some_tag foo=bar></some_tag></some_tag></document>
```

## Trophies

* <https://github.com/Microsoft/ChakraCore/issues/5503>
* <https://github.com/mruby/mruby/issues/3995>  (**CVE-2018-10191**)
* <https://github.com/mruby/mruby/issues/4001>  (**CVE-2018-10199**)
* <https://github.com/mruby/mruby/issues/4038>  (**CVE-2018-12248**)
* <https://github.com/mruby/mruby/issues/4027>  (**CVE-2018-11743**)
* <https://github.com/mruby/mruby/issues/4036>  (**CVE-2018-12247**)
* <https://github.com/mruby/mruby/issues/4037>  (**CVE-2018-12249**)
* <https://bugs.php.net/bug.php?id=76410>
* <https://bugs.php.net/bug.php?id=76244>
