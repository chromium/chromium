" Copyright 2015 The Chromium Authors
" Use of this source code is governed by a BSD-style license that can be
" found in the LICENSE file.

" Vim syntax file
" Language: Mojom
" To get syntax highlighting for .mojom files, add the following to your .vimrc
" file:
"     set runtimepath+=/path/to/src/tools/vim/mojom

if exists("b:current_syntax")
  finish
endif

syn case match

syntax region mojomFold start="{" end="}" transparent fold

" Keywords
syntax keyword mojomType        bool string int8 int16 int32 int64 uint8 uint16
syntax keyword mojomType        uint32 uint64 float double array
syntax match mojomImport        "^\(import\)\s"
syntax keyword mojomKeyword     const module interface enum struct union associated
syntax match mojomOperator      /=>/
syntax match mojomOperator      /?/

" Comments
syntax keyword mojomTodo           contained TODO FIXME XXX
syntax region  mojomDocLink        contained start="\[" end="\]"
syntax region  mojomComment        start="/\*"  end="\*/" contains=mojomTodo,mojomDocLink,@Spell
syntax match   mojomLineComment    "//.*" contains=mojomTodo,mojomDocLink,@Spell

" Literals
syntax match mojomBoolean       /true\|false/
" Negative lookahead for "." so floats are not partly highlighted as integers.
syntax match mojomInteger       /-\=[0-9]\(\.\)\@!/
syntax match mojomFloat         /[0-9]\+\.[0-9]*\|[0-9]*\.[0-9]\+/
syntax region mojomString       start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=@Spell

" Attributes
syntax match mojomAttribute  /\[[^\]]*\]/

" The default highlighting.
highlight default link mojomTodo            Todo
highlight default link mojomComment         Comment
highlight default link mojomLineComment     Comment
highlight default link mojomDocLink         SpecialComment
highlight default link mojomType            Type
highlight default link mojomImport          Include
highlight default link mojomKeyword         Keyword
highlight default link mojomOperator        Operator
highlight default link mojomString          String
highlight default link mojomInteger         Number
highlight default link mojomBoolean         Boolean
highlight default link mojomFloat           Float
highlight default link mojomAttribute       Label

let b:current_syntax = "mojom"
let b:spell_options = "contained"

syn sync minlines=500
