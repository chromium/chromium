" Copyright 2015 The Chromium Authors
" Use of this source code is governed by a BSD-style license that can be
" found in the LICENSE file.

" We take care to preserve the user's fileencodings and fileformats,
" because those settings are global (not buffer local), yet we want
" to override them for loading mojom files, which should be UTF-8.

let s:current_fileformats = ''
let s:current_fileencodings = ''

" define fileencodings to open as utf-8 encoding even if it's ascii.
function! s:mojomfiletype_pre()
  let s:current_fileformats = &g:fileformats
  let s:current_fileencodings = &g:fileencodings
  set fileencodings=utf-8 fileformats=unix
  setlocal filetype=mojom
endfunction

" restore fileencodings as others
function! s:mojomfiletype_post()
  let &g:fileformats = s:current_fileformats
  let &g:fileencodings = s:current_fileencodings
endfunction

au BufNewFile *.mojom setlocal filetype=mojom fileencoding=utf-8 fileformat=unix
au BufRead *.mojom call s:mojomfiletype_pre()
au BufReadPost *.mojom call s:mojomfiletype_post()
