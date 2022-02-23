#!/bin/bash
#
# Removes ./wpt/ directory containing the reduced web-platform-tests tree and
# starts a new checkout. Only files in WPTIncludeList are retained. The
# revisions getting checked out are defined in WPTHeads.
# TODO(weizhong): consider updating WPTIncludeList if there is new file added at
# upstream

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $DIR

TARGET_DIR=$DIR/wpt
REMOTE_REPO="https://github.com/web-platform-tests/wpt.git"
if [ "$1" == "" ]
then
  echo "Remote head is empty!"
  exit 1
fi
WPT_HEAD=$1

function clone {
  # Remove existing repo if already exists.
  [ -d "$TARGET_DIR" ] && rm -rf $TARGET_DIR

  # Clone the main repository.
  git clone $REMOTE_REPO $TARGET_DIR
  cd $TARGET_DIR && git checkout $WPT_HEAD
}

function reduce {
  cd $TARGET_DIR
  # Some directories contain filenames with ' (single quote), and it confuses
  # xargs on some platforms, so we remove those directories first.
  rm -fr html css
  # Remove all except white-listed.
  comm -23 <(find . -type f -o -type l | sort) <(cat ../WPTIncludeList | sort) | xargs -d '\n' -n 1 rm
  find . -empty -type d -delete
}

actions="clone reduce"
[ "$2" != "" ] && actions="$@"

for action in $actions; do
  type -t $action >/dev/null || (echo "Unknown action: $action" 1>&2 && exit 1)
  $action
done

# TODO(burnik): Handle the SSL certs and other configuration.
