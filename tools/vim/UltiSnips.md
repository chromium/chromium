# UltiSnips

[UltiSnips](https://vimawesome.com/plugin/ultisnips) is one of snippet systems
for vim.
Below are some UltiSnips snippets that other Chromium developers will hopefully
find useful.

## Demos

### Copyright, include guard and namespace

The screencast below showcases how:

- `copyright` + `<tab>` will fill-in current year
- `#ifndef` + `<tab>` will calculate the macro name based on the path
  of the current file
- `namespace` + `<tab>` + `namespace_name` + `tab` will copy the namespace
  name into a comment at the end of the namespace

[![screencast](https://drive.google.com/uc?id=1aDrBQ9G3NG2lO74GXq5J_x3lukzWoZZj)]()


### Ad-hoc logging

The screenscast below showcases how:
- `<< some->expression.logme` + `<tab>` includes the expression
  text in the output
- `<< stack` + `<tab>` logs the callstack

[![screencast](https://drive.google.com/uc?id=1skLOswLaXQ97HEEwxhvZO_wO-xQ5YoxW)]()


## C++ Snippets

### Copyright

```UltiSnips
snippet copyright
// Copyright `date +%Y` The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

$0
endsnippet
```


### Include guard

This relies on current directory being the Chromium root directory.

```Ultisnips
snippet ifndef "include guard" w
ifndef ${1:`!p snip.rv=str(path).replace('/','_').replace('\\','_').replace('.','_').upper()+'_'`}
#define $1

$0

#endif  // $1
endsnippet
```


### Namespace

```Ultisnips
snippet namespace
namespace $1 {
$0
}  // namespace $1
endsnippet
```


### Ad-hoc logging

```Ultisnips
snippet LOG(ERROR)
LOG(ERROR) << __func__$0;
endsnippet

snippet "<<.(.+)\.logme" "Logging expansion" r
<< "; `!p snip.rv = match.group(1).replace('"', '\\"')` = " << `!p snip.rv = match.group(1)`$0
endsnippet

snippet "<<.(.+)\.logint" "Logging expansion" r
<< "; `!p snip.rv = match.group(1).replace('"', '\\"')` = " << static_cast<int>(`!p snip.rv = match.group(1)`)$0
endsnippet

snippet "<<.stack" "Logging expansion2" r
<< "; stack = " << base::debug::StackTrace().ToString()
endsnippet

snippet "<<.jstack" "Logging expansion2" r
<< "; js stack = "
           << [](){
              char* buf = nullptr; size_t size = 0;
              FILE* f = open_memstream(&buf, &size);
              if (!f) return std::string("<mem error>");
              v8::Message::PrintCurrentStackTrace(
                  v8::Isolate::GetCurrent(), f);
              fflush(f); fclose(f);
              std::string result(buf, size);
              free(buf);
              return result; }();
endsnippet
```
