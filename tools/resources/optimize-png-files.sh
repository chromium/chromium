#!/bin/bash
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The optimization code is based on pngslim (http://goo.gl/a0XHg)
# and executes a similar pipleline to optimize the png file size.
# The steps that require pngoptimizercl/pngrewrite/deflopt are omitted,
# but this runs all other processes, including:
# 1) various color-dependent optimizations using optipng.
# 2) optimize the number of huffman blocks.
# 3) randomize the huffman table.
# 4) Further optimize using optipng and advdef (zlib stream).
# Due to the step 3), each run may produce slightly different results.
#
# Note(oshima): In my experiment, advdef didn't reduce much. I'm keeping it
# for now as it does not take much time to run.

readonly ALL_DIRS="
ash/resources
chrome/android/java/res
chrome/app/theme
chrome/browser/resources
chrome/renderer/resources
content/public/android/java/res
content/shell/resources
remoting/resources
ui/android/java/res
ui/resources
ui/chromeos/resources
ui/webui/resources/images
"

# Files larger than this file size (in bytes) will
# use the optimization parameters tailored for large files.
LARGE_FILE_THRESHOLD=3000

# Constants used for optimization
readonly DEFAULT_MIN_BLOCK_SIZE=128
readonly DEFAULT_LIMIT_BLOCKS=256
readonly DEFAULT_RANDOM_TRIALS=100
# Taken from the recommendation in the pngslim's readme.txt.
readonly LARGE_MIN_BLOCK_SIZE=1
readonly LARGE_LIMIT_BLOCKS=2
readonly LARGE_RANDOM_TRIALS=1

# Global variables for stats
TOTAL_OLD_BYTES=0
TOTAL_NEW_BYTES=0
TOTAL_FILE=0
CORRUPTED_FILE=0
PROCESSED_FILE=0

declare -a THROBBER_STR=('-' '\\' '|' '/')
THROBBER_COUNT=0

VERBOSE=false

# Echo only if verbose option is set.
function info {
  if $VERBOSE ; then
    echo $@
  fi
}

# Show throbber character at current cursor position.
function throbber {
  info -ne "${THROBBER_STR[$THROBBER_COUNT]}\b"
  let THROBBER_COUNT=$THROBBER_COUNT+1
  let THROBBER_COUNT=$THROBBER_COUNT%4
}

# Usage: pngout_loop <file> <png_out_options> ...
# Optimize the png file using pngout with the given options
# using various block split thresholds and filter types.
function pngout_loop {
  local file=$1
  shift
  local opts=$*
  if [ $OPTIMIZE_LEVEL == 1 ]; then
    for j in $(eval echo {0..5}); do
      throbber
      pngout -q -k1 -s1 -f$j $opts $file
    done
  else
    for i in 0 128 256 512; do
      for j in $(eval echo {0..5}); do
        throbber
        pngout -q -k1 -s1 -b$i -f$j $opts $file
      done
    done
  fi
}

# Usage: get_color_depth_list
# Returns the list of color depth options for current optimization level.
function get_color_depth_list {
  if [ $OPTIMIZE_LEVEL == 1 ]; then
    echo "-d0"
  else
    echo "-d1 -d2 -d4 -d8"
  fi
}

# Usage: process_grayscale <file>
# Optimize grayscale images for all color bit depths.
#
# TODO(oshima): Experiment with -d0 w/o -c0.
function process_grayscale {
  info -ne "\b\b\b\b\b\b\b\bgray...."
  for opt in $(get_color_depth_list); do
    pngout_loop $file -c0 $opt
  done
}

# Usage: process_grayscale_alpha <file>
# Optimize grayscale images with alpha for all color bit depths.
function process_grayscale_alpha {
  info -ne "\b\b\b\b\b\b\b\bgray-a.."
  pngout_loop $file -c4
  for opt in $(get_color_depth_list); do
    pngout_loop $file -c3 $opt
  done
}

# Usage: process_rgb <file>
# Optimize rgb images with or without alpha for all color bit depths.
function process_rgb {
  info -ne "\b\b\b\b\b\b\b\brgb....."
  for opt in $(get_color_depth_list); do
    pngout_loop $file -c3 $opt
  done
  pngout_loop $file -c2
  pngout_loop $file -c6
}

# Usage: huffman_blocks <file>
# Optimize the huffman blocks.
function huffman_blocks {
  info -ne "\b\b\b\b\b\b\b\bhuffman."
  local file=$1
  local size=$(stat -c%s $file)
  local min_block_size=$DEFAULT_MIN_BLOCK_SIZE
  local limit_blocks=$DEFAULT_LIMIT_BLOCKS

  if [ $size -gt $LARGE_FILE_THRESHOLD ]; then
    min_block_size=$LARGE_MIN_BLOCK_SIZE
    limit_blocks=$LARGE_LIMIT_BLOCKS
  fi
  let max_blocks=$size/$min_block_size
  if [ $max_blocks -gt $limit_blocks ]; then
    max_blocks=$limit_blocks
  fi

  for i in $(eval echo {2..$max_blocks}); do
    throbber
    pngout -q -k1 -ks -s1 -n$i $file
  done
}

# Usage: random_huffman_table_trial <file>
# Try compressing by randomizing the initial huffman table.
#
# TODO(oshima): Try adjusting different parameters for large files to
# reduce runtime.
function random_huffman_table_trial {
  info -ne "\b\b\b\b\b\b\b\brandom.."
  local file=$1
  local old_size=$(stat -c%s $file)
  local trials_count=$DEFAULT_RANDOM_TRIALS

  if [ $old_size -gt $LARGE_FILE_THRESHOLD ]; then
    trials_count=$LARGE_RANDOM_TRIALS
  fi
  for i in $(eval echo {1..$trials_count}); do
    throbber
    pngout -q -k1 -ks -s0 -r $file
  done
  local new_size=$(stat -c%s $file)
  if [ $new_size -lt $old_size ]; then
    random_huffman_table_trial $file
  fi
}

# Usage: final_comprssion <file>
# Further compress using optipng and advdef.
# TODO(oshima): Experiment with 256.
function final_compression {
  info -ne "\b\b\b\b\b\b\b\bfinal..."
  local file=$1
  if [ $OPTIMIZE_LEVEL == 2 ]; then
    for i in 32k 16k 8k 4k 2k 1k 512; do
      throbber
      optipng -q -nb -nc -zw$i -zc1-9 -zm1-9 -zs0-3 -f0-5 $file
    done
  fi
  for i in $(eval echo {1..4}); do
    throbber
    advdef -q -z -$i $file
  done

  # Clear the current line.
  if $VERBOSE ; then
    printf "\033[0G\033[K"
  fi
}

# Usage: get_color_type <file>
# Returns the color type name of the png file. Here is the list of names
# for each color type codes.
# 0: grayscale
# 2: RGB
# 3: colormap
# 4: gray+alpha
# 6: RGBA
# See http://en.wikipedia.org/wiki/Portable_Network_Graphics#Color_depth
# for details about the color type code.
function get_color_type {
  local file=$1
  echo $(file $file | awk -F, '{print $3}' | awk '{print $2}')
}

# Usage: optimize_size <file>
# Performs png file optimization.
function optimize_size {
  # Print filename, trimmed to ensure it + status don't take more than 1 line
  local filename_length=${#file}
  local -i allowed_length=$COLUMNS-11
  local -i trimmed_length=$filename_length-$COLUMNS+14
  if [ "$filename_length" -lt "$allowed_length" ]; then
    info -n "$file|........"
  else
    info -n "...${file:$trimmed_length}|........"
  fi

  local file=$1

  advdef -q -z -4 $file

  pngout -q -s4 -c0 -force $file $file.tmp.png
  if [ -f $file.tmp.png ]; then
    rm $file.tmp.png
    process_grayscale $file
    process_grayscale_alpha $file
  else
    pngout -q -s4 -c4 -force $file $file.tmp.png
    if [ -f $file.tmp.png ]; then
      rm $file.tmp.png
      process_grayscale_alpha $file
    else
      process_rgb $file
    fi
  fi

  info -ne "\b\b\b\b\b\b\b\bfilter.."
  local old_color_type=$(get_color_type $file)
  optipng -q -zc9 -zm8 -zs0-3 -f0-5 -out $file.tmp.png $file
  local new_color_type=$(get_color_type $file.tmp.png)
  # optipng may corrupt a png file when reducing the color type
  # to grayscale/grayscale+alpha. Just skip such cases until
  # the bug is fixed. See crbug.com/174505, crbug.com/174084.
  # The issue is reported in
  # https://sourceforge.net/tracker/?func=detail&aid=3603630&group_id=151404&atid=780913
  if [[ $old_color_type == "RGBA" && $new_color_type == gray* ]] ; then
    rm $file.tmp.png
  else
    mv $file.tmp.png $file
  fi
  pngout -q -k1 -s1 $file

  huffman_blocks $file

  # TODO(oshima): Experiment with strategy 1.
  info -ne "\b\b\b\b\b\b\b\bstrategy"
  if [ $OPTIMIZE_LEVEL == 2 ]; then
    for i in 3 2 0; do
      pngout -q -k1 -ks -s$i $file
    done
  else
    pngout -q -k1 -ks -s1 $file
  fi

  if [ $OPTIMIZE_LEVEL == 2 ]; then
    random_huffman_table_trial $file
  fi

  final_compression $file
}

# Usage: process_file <file>
function process_file {
  local file=$1
  local name=$(basename $file)
  # -rem alla removes all ancillary chunks except for tRNS
  pngcrush -d $TMP_DIR -brute -reduce -rem alla $file > /dev/null 2>&1

  if [ -f $TMP_DIR/$name -a $OPTIMIZE_LEVEL != 0 ]; then
    optimize_size $TMP_DIR/$name
  fi
}

# Usage: optimize_file <file>
function optimize_file {
  local file=$1
  if $using_cygwin ; then
    file=$(cygpath -w $file)
  fi

  local name=$(basename $file)
  local old=$(stat -c%s $file)
  local tmp_file=$TMP_DIR/$name
  let TOTAL_FILE+=1

  process_file $file

  if [ ! -e $tmp_file ] ; then
    let CORRUPTED_FILE+=1
    echo "$file may be corrupted; skipping\n"
    return
  fi

  local new=$(stat -c%s $tmp_file)
  let diff=$old-$new
  let percent=$diff*100
  let percent=$percent/$old

  if [ $new -lt $old ]; then
    info "$file: $old => $new ($diff bytes: $percent%)"
    cp "$tmp_file" "$file"
    let TOTAL_OLD_BYTES+=$old
    let TOTAL_NEW_BYTES+=$new
    let PROCESSED_FILE+=1
  else
    if [ $OPTIMIZE_LEVEL == 0 ]; then
      info "$file: Skipped"
    else
      info "$file: Unable to reduce size"
    fi
    rm $tmp_file
  fi
}

function optimize_dir {
  local dir=$1
  if $using_cygwin ; then
    dir=$(cygpath -w $dir)
  fi

  for f in $(find $dir -name "*.png"); do
    optimize_file $f
  done
}

function install_if_not_installed {
  local program=$1
  local package=$2
  which $program > /dev/null 2>&1
  if [ "$?" != "0" ]; then
    if $using_cygwin ; then
      echo "Couldn't find $program. " \
           "Please run cygwin's setup.exe and install the $package package."
      exit 1
    else
      read -p "Couldn't find $program. Do you want to install? (y/n)"
      [ "$REPLY" == "y" ] && sudo apt-get install $package
      [ "$REPLY" == "y" ] || exit
    fi
  fi
}

function fail_if_not_installed {
  local program=$1
  local url=$2
  which $program > /dev/null 2>&1
  if [ $? != 0 ]; then
    echo "Couldn't find $program. Please download and install it from $url ."
    exit 1
  fi
}

# Check pngcrush version and exit if the version is in bad range.
# See crbug.com/404893.
function exit_if_bad_pngcrush_version {
  local version=$(pngcrush -v 2>&1 | awk "/pngcrush 1.7./ {print \$3}")
  local version_num=$(echo $version | sed "s/\.//g")
  if [[ (1748 -lt $version_num && $version_num -lt 1773) ]] ; then
    echo "Your pngcrush ($version) has a bug that exists from " \
         "1.7.49 to 1.7.72  (see crbug.com/404893 for details)."
    echo "Please upgrade pngcrush and try again"
    exit 1;
  fi
}

function show_help {
  local program=$(basename $0)
  echo \
"Usage: $program [options] <dir> ...

$program is a utility to reduce the size of png files by removing
unnecessary chunks and compressing the image.

Options:
  -o<optimize_level>  Specify optimization level: (default is 1)
      0  Just run pngcrush. It removes unnecessary chunks and perform basic
         optimization on the encoded data.
      1  Optimize png files using pngout/optipng and advdef. This can further
         reduce addtional 5~30%. This is the default level.
      2  Aggressively optimize the size of png files. This may produce
         addtional 1%~5% reduction.  Warning: this is *VERY*
         slow and can take hours to process all files.
  -c<commit>   Same as -r but referencing a git commit. Only files changed
               between this commit and HEAD will be processed.
  -v  Shows optimization process for each file.
  -h  Print this help text."
  exit 1
}

if [ "$(expr substr $(uname -s) 1 6)" == "CYGWIN" ]; then
  using_cygwin=true
else
  using_cygwin=false
fi

# The -i in the shebang line should result in $COLUMNS being set on newer
# versions of bash.  If it's not set yet, attempt to set it.
if [ -z $COLUMNS ]; then
  which tput > /dev/null 2>&1
  if [ "$?" == "0" ]; then
    COLUMNS=$(tput cols)
  else
    # No tput either... give up and just guess 80 columns.
    COLUMNS=80
  fi
  export COLUMNS
fi

OPTIMIZE_LEVEL=1
# Parse options
while getopts o:c:h:v opts
do
  case $opts in
    c)
      COMMIT=$OPTARG
      ;;
    o)
      if [[ "$OPTARG" != 0 && "$OPTARG" != 1 && "$OPTARG" != 2 ]] ; then
        show_help
      fi
      OPTIMIZE_LEVEL=$OPTARG
      ;;
    v)
      VERBOSE=true
      ;;
    [h?])
      show_help;;
  esac
done

# Remove options from argument list.
shift $(($OPTIND -1))

# Make sure we have all necessary commands installed.
install_if_not_installed pngcrush pngcrush
exit_if_bad_pngcrush_version

if [ $OPTIMIZE_LEVEL -ge 1 ]; then
  install_if_not_installed optipng optipng

  if $using_cygwin ; then
    fail_if_not_installed advdef "http://advancemame.sourceforge.net/comp-readme.html"
  else
    install_if_not_installed advdef advancecomp
  fi

  if $using_cygwin ; then
    pngout_url="http://www.advsys.net/ken/utils.htm"
  else
    pngout_url="http://www.jonof.id.au/kenutils"
  fi
  fail_if_not_installed pngout $pngout_url
fi

# Create tmp directory for crushed png file.
TMP_DIR=$(mktemp -d)
if $using_cygwin ; then
  TMP_DIR=$(cygpath -w $TMP_DIR)
fi

# Make sure we cleanup temp dir
#trap "rm -rf $TMP_DIR" EXIT

# If no directories are specified, optimize all directories.
DIRS=$@
set ${DIRS:=$ALL_DIRS}

info "Optimize level=$OPTIMIZE_LEVEL"

if [ -n "$COMMIT" ] ; then
  # To keep git logic below sane, require it be run from the top dir.
  if [ ! -e ../.gclient ]; then
    echo "$0 must be run in src directory"
    exit 1
  fi

 ALL_FILES=$(git diff --name-only $COMMIT HEAD $DIRS | grep "png$")
 ALL_FILES_LIST=( $ALL_FILES )
 echo "Processing ${#ALL_FILES_LIST[*]} files"
 for f in $ALL_FILES; do
   if [ -f $f ] ; then
     optimize_file $f
   else
     echo "Skipping deleted file: $f";
   fi
 done
else
  for d in $DIRS; do
    if [ -d $d ] ; then
      info "Optimizing png files in $d"
      optimize_dir $d
      info ""
    elif [ -f $d ] ; then
      optimize_file $d
    else
      echo "Not a file or directory: $d";
    fi
  done
fi

# Print the results.
echo "Optimized $PROCESSED_FILE/$TOTAL_FILE files in" \
     "$(date -d "0 + $SECONDS sec" +%Ts)"
if [ $PROCESSED_FILE != 0 ]; then
  let diff=$TOTAL_OLD_BYTES-$TOTAL_NEW_BYTES
  let percent=$diff*100/$TOTAL_OLD_BYTES
  echo "Result: $TOTAL_OLD_BYTES => $TOTAL_NEW_BYTES bytes" \
       "($diff bytes: $percent%)"
fi
if [ $CORRUPTED_FILE != 0 ]; then
  echo "Warning: corrupted files found: $CORRUPTED_FILE"
  echo "Please contact the author of the CL that landed corrupted png files"
fi
