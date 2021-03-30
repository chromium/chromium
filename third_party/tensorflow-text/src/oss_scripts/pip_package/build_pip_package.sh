#!/usr/bin/env bash
# Tool to build the TensorFlow Text pip package.
#
# Usage:
#   bazel build oss_scripts/pip_package:build_pip_package
#   bazel-bin/oss_scripts/build_pip_package
#
# Arguments:
#   output_dir: An output directory. Defaults to `/tmp/tensorflow_text_pkg`.

set -e  # fail and exit on any command erroring

die() {
  echo >&2 "$@"
  exit 1
}

osname="$(uname -s | tr 'A-Z' 'a-z')"
echo $osname

function is_windows() {
  # On windows, the shell script is actually running in msys
  [[ "${osname}" =~ msys_nt*|mingw*|cygwin*|uwin* ]]
}

function is_macos() {
  [[ "${osname}" == "darwin" ]]
}

function is_nightly() {
  [[ "$IS_NIGHTLY" == "nightly" ]]
}

function abspath() {
  cd "$(dirname $1)"
  echo "$PWD/$(basename $1)"
  cd "$OLDPWD"
}

plat_name=""
if is_macos; then
  plat_name="--plat-name macosx-10.9-x86_64"
fi

main() {
  local output_dir="$1"

  if [[ -z "${output_dir}" ]]; then
    output_dir="/tmp/tensorflow_text_pkg"
  fi
  mkdir -p ${output_dir}
  output_dir=$(abspath "${output_dir}")
  echo "=== Destination directory: ${output_dir}"

  if [[ ! -d "bazel-bin/tensorflow_text" ]]; then
    die "Could not find bazel-bin. Did you run from the root of the build tree?"
  fi

  local temp_dir="$(mktemp -d)"
  trap "rm -rf ${temp_dir}" EXIT
  echo "=== Using tmpdir ${temp_dir}"

  if is_windows; then
    runfiles="bazel-bin/oss_scripts/pip_package/build_pip_package.exe.runfiles"
  else
    runfiles="bazel-bin/oss_scripts/pip_package/build_pip_package.runfiles"
  fi
  cp -LR \
      "${runfiles}/org_tensorflow_text/tensorflow_text" \
      "${temp_dir}"
  if is_nightly; then
    cp "${runfiles}/org_tensorflow_text/oss_scripts/pip_package/setup.nightly.py" \
        "${temp_dir}"
  else
    cp "${runfiles}/org_tensorflow_text/oss_scripts/pip_package/setup.py" \
        "${temp_dir}"
  fi
  cp "${runfiles}/org_tensorflow_text/oss_scripts/pip_package/MANIFEST.in" \
      "${temp_dir}"
  cp "${runfiles}/org_tensorflow_text/oss_scripts/pip_package/LICENSE" \
      "${temp_dir}"

  pushd "${temp_dir}" > /dev/null

  # Build pip package
  if is_nightly; then
    python setup.nightly.py bdist_wheel --universal $plat_name
  else
    python setup.py bdist_wheel --universal $plat_name
  fi
  cp dist/*.whl "${output_dir}"
}

main "$@"
