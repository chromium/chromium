#!/bin/bash
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# Clones the dom-distiller repo, compiles and extracts its JavaScript. The
# artifact would be uploaded as a CL in dom-distiller/dist repo, and then
# generates a rolling commit to be uploaded.
# This script requires that ant is installed. It takes an optional parameter
# for which SHA1 in dom-distiller repo to roll to. If left unspecified the
# script rolls to HEAD. The second optional parameter is the Gerrit URL of
# the CL in dom-distiller/dist repo to be validated.

(
  set -e

  dom_distiller_js_path=$(dirname "${BASH_SOURCE[0]}")
  src_path=$dom_distiller_js_path/../..
  readme_chromium=$dom_distiller_js_path/README.chromium
  [ ! -f $readme_chromium ] && echo "$readme_chromium is not found" && exit 1
  tmpdir=/tmp/domdistiller-$$
  changes=$tmpdir/domdistiller.changes
  bugs=$tmpdir/domdistiller.bugs
  curr_gitsha=$(grep 'Version:' $readme_chromium | awk '{print $2}')
  repo_host=https://chromium.googlesource.com/chromium

  rm -rf $tmpdir
  mkdir $tmpdir
  pushd $tmpdir

  function finish {
    rm -rf $tmpdir
  }
  trap finish EXIT

  git clone $repo_host/dom-distiller
  pushd dom-distiller

  # The new git SHA1 is HEAD or the first command line parameter.
  [[ -z "$1" ]] && gitsha_target="HEAD" || gitsha_target="$1"
  gerrit_url="$2"
  new_gitsha=$(git rev-parse --short=10 ${gitsha_target})
  git reset --hard ${new_gitsha}
  git log --oneline ${curr_gitsha}..${new_gitsha} > $changes

  echo -n 'Bug: ' > $bugs

  # This extracts BUG= lines from the log, extracts the numbers part, removes
  # whitespace and deletes empty lines. Then, split on ',', sort, uniquify and
  # rejoin. Finally, remove the trailing ',' and concat to $bugs.
  git log ${curr_gitsha}..${new_gitsha} \
    | grep -E 'BUG=|Bug:' \
    | sed -e 's/.*\(BUG=\|Bug:\)\(.*\)/\2/' -e 's/\s*//g' -e '/^$/d' -e '/None/d' \
    | tr ',' '\n' \
    | sort \
    | uniq \
    | tr '\n' ',' \
    | sed -e 's/,/, /g' \
    | head --bytes=-2 \
    >> $bugs

  echo >> $bugs  # add a newline

  ant package
  popd # dom-distiller

  git clone $repo_host/dom-distiller/dist $tmpdir/dom-distiller-dist
  pushd dom-distiller-dist
  if [[ -n "$gerrit_url" ]]; then
    echo "Validating $gerrit_url"
    git cl patch --force $gerrit_url
  fi
  rm -rf $tmpdir/dom-distiller-dist/*

  cp -r $tmpdir/dom-distiller/out/package/* .

  git add .
  if [[ $(git status --short | wc -l) -ne 0 ]]; then
    if [[ -n "$gerrit_url" ]]; then
      echo "FAIL. The output is different from $gerrit_url."
      exit 1
    fi
    # For Change-Id footer.
    curl -Lo $(git rev-parse --git-dir)/hooks/commit-msg https://gerrit-review.googlesource.com/tools/hooks/commit-msg
    chmod +x $(git rev-parse --git-dir)/hooks/commit-msg

    gen_message () {
      echo "Package for ${new_gitsha}"
      echo
      echo "This is generated from:"
      echo "${repo_host}/dom-distiller/+/${new_gitsha}."
      echo
      echo "To validate, run the following command in chromium/src:"
      echo "third_party/dom_distiller_js/update_domdistiller_js.sh ${new_gitsha} <Gerrit-URL>"
    }

    message=$tmpdir/message
    gen_message > $message

    git commit -a -F $message
    git push origin main:refs/for/main
  else
    # No changes to external repo, but need to check if DEPS refers to same SHA1.
    if [[ -n "$gerrit_url" ]]; then
      echo "PASS. The output is the same as $gerrit_url."
      exit 0
    fi
    echo "WARNING: There were no changes to the distribution package."
  fi
  new_dist_gitsha=$(git rev-parse HEAD)
  popd # dom-distiller-dist

  popd # tmpdir
  curr_dist_gitsha=$(grep -e "/chromium\/dom-distiller\/dist.git" $src_path/DEPS | sed -e "s/.*'\([A-Za-z0-9]\{40\}\)'.*/\1/g")
  if [[ "${new_dist_gitsha}" == "${curr_dist_gitsha}" ]]; then
    echo "The roll does not include any changes to the dist package. Exiting."
    exit 1
  fi

  cp $tmpdir/dom-distiller/LICENSE $dom_distiller_js_path/
  sed -i "s/Version: [0-9a-f]*/Version: ${new_gitsha}/" $readme_chromium
  sed -i -e "s/\('\/chromium\/dom-distiller\/dist.git' + '@' + '\)\([0-9a-f]\+\)'/\1${new_dist_gitsha}'/" $src_path/DEPS

  gen_message () {
    echo "Roll DOM Distiller JavaScript distribution package"
    echo
    echo "Diff since last roll:"
    echo "https://chromium.googlesource.com/chromium/dom-distiller/+/${curr_gitsha}..${new_gitsha}"
    echo
    echo "Picked up changes:"
    echo "https://chromium.googlesource.com/chromium/dom-distiller/+log/${curr_gitsha}..${new_gitsha}"
    cat $changes
    echo
    cat $bugs
  }

  message=$tmpdir/message
  gen_message > $message

  # Run checklicenses.py on the pulled files, but only print the output on
  # failures.
  $src_path/tools/checklicenses/checklicenses.py third_party/dom_distiller_js > $tmpdir/checklicenses.out || cat $tmpdir/checklicenses.out

  git commit -a -F $message
)
