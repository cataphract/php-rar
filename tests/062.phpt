--TEST--
RAR directory stream stat
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php

echo "Root:\n";

$u = "rar://" .
	dirname(__FILE__) . '/dirs_and_extra_headers.rar';

var_dump(fstat(opendir($u)));

echo "\nSub-root directory:\n";

$u = "rar://" .
	dirname(__FILE__) . '/dirs_and_extra_headers.rar#%EF%AC%B0';

var_dump(fstat(opendir($u)));

echo "Done.\n";
--EXPECTF--
Root:
array(26) {
  [0]=>
  int(0)
  [1]=>
  int(0)
  [2]=>
  int(16895)
  [3]=>
  int(1)
  [4]=>
  int(0)
  [5]=>
  int(0)
  [6]=>
  int(0)
  [7]=>
  int(0)
  [8]=>
  int(0)
  [9]=>
  int(312768000)
  [10]=>
  int(0)
  [11]=>
  int(%s)
  [12]=>
  int(%s)
  ["dev"]=>
  int(0)
  ["ino"]=>
  int(0)
  ["mode"]=>
  int(16895)
  ["nlink"]=>
  int(1)
  ["uid"]=>
  int(0)
  ["gid"]=>
  int(0)
  ["rdev"]=>
  int(0)
  ["size"]=>
  int(0)
  ["atime"]=>
  int(0)
  ["mtime"]=>
  int(312768000)
  ["ctime"]=>
  int(0)
  ["blksize"]=>
  int(%s)
  ["blocks"]=>
  int(%s)
}

Sub-root directory:
array(26) {
  [0]=>
  int(0)
  [1]=>
  int(0)
  [2]=>
  int(16895)
  [3]=>
  int(1)
  [4]=>
  int(0)
  [5]=>
  int(0)
  [6]=>
  int(0)
  [7]=>
  int(0)
  [8]=>
  int(0)
  [9]=>
  int(1272938642)
  [10]=>
  int(0)
  [11]=>
  int(%s)
  [12]=>
  int(%s)
  ["dev"]=>
  int(0)
  ["ino"]=>
  int(0)
  ["mode"]=>
  int(16895)
  ["nlink"]=>
  int(1)
  ["uid"]=>
  int(0)
  ["gid"]=>
  int(0)
  ["rdev"]=>
  int(0)
  ["size"]=>
  int(0)
  ["atime"]=>
  int(0)
  ["mtime"]=>
  int(1272938642)
  ["ctime"]=>
  int(0)
  ["blksize"]=>
  int(%s)
  ["blocks"]=>
  int(%s)
}
Done.
