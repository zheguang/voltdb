#!/bin/bash 
set -e

ssh_handle=/devel/spark-study/bin/ssh-intel.sh
patch=/tmp/voltdb_ginchi.diff
voltdb_root=/home/sam/workspace/zheguang/voltdb
(cd $voltdb_root && git add -u && git diff --cached --no-prefix > $patch)
if [[ ! -s $patch ]]; then
  echo "[error] git diff --cached is empty."
  exit 1
fi
bash $ssh_handle --scp $patch $patch
bash $ssh_handle --ssh "cd /home/user/sam/devel/voltdb && git reset --hard HEAD && git clean -f && patch -p0 < $patch"

echo "[info] done"

