#!/bin/bash

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Source this file into your shell to gain the cr function and tab completion
# for it

# Make sure we're being sourced (possibly by another script). Check for bash
# since zsh sets $0 when sourcing.
if [[ -n "$BASH_VERSION" && "${BASH_SOURCE:-$0}" == "$0" ]]; then
  echo "ERROR: cr-bash-helpers.sh must be sourced."
  exit 1
fi

READLINK_e=("readlink" "-e")
if [[ -x `which greadlink` ]]; then
  READLINK_e=("greadlink" "-e")
fi

if [[ $(uname) == "Darwin" ]]; then
  cr_base_dir=$(dirname "${BASH_SOURCE:-$0}")
else
  cr_base_dir=$(dirname $(${READLINK_e[@]} "${BASH_SOURCE:-$0}"))
fi

cr_main="${cr_base_dir}/main.py"
cr_exec=("PYTHONDONTWRITEBYTECODE=1" "python3" "${cr_main}")

# The main entry point to the cr tool.
# Invokes the python script with pyc files turned off.
function cr() {
  env ${cr_exec[@]} "$@"
}

# Attempts to cd to the root/src of the current client.
function crcd() {
  cd $(cr info -s CR_SRC)
}

# Add to your PS1 to have the current selected output directory in your prompt
function _cr_ps1() {
  cr info -s CR_OUT_FULL
}

# The tab completion handler, delegates into the python script.
function _cr_complete() {
  COMPREPLY=()
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local main="python -B "${cr_main}")"
  local completions="$(env COMP_CWORD=${COMP_CWORD} \
      COMP_WORD=${cur} \
      ${cr_exec[@]})"
  COMPREPLY=( $(compgen -W "${completions}" -- ${cur}) )
}

# Setup the bash auto complete
complete -F _cr_complete cr
