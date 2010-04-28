--TEST--
RAR stream stat
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
$stream = fopen("rar://" .
	dirname(__FILE__) . '/latest_winrar.rar' .
	"#1.txt", "r");
var_dump(fstat($stream));

echo "Done.\n";
--EXPECTF--
array(26) {
  [0]=>
  int(0)
  [1]=>
  int(0)
  [2]=>
  int(32)
  [3]=>
  int(1)
  [4]=>
  int(0)
  [5]=>
  int(0)
  [6]=>
  int(0)
  [7]=>
  int(5)
  [8]=>
  int(0)
  [9]=>
  int(1086948438)
  [10]=>
  int(0)
  [11]=>
  int(-1)
  [12]=>
  int(-1)
  ["dev"]=>
  int(0)
  ["ino"]=>
  int(0)
  ["mode"]=>
  int(32)
  ["nlink"]=>
  int(1)
  ["uid"]=>
  int(0)
  ["gid"]=>
  int(0)
  ["rdev"]=>
  int(0)
  ["size"]=>
  int(5)
  ["atime"]=>
  int(0)
  ["mtime"]=>
  int(1086948438)
  ["ctime"]=>
  int(0)
  ["blksize"]=>
  int(-1)
  ["blocks"]=>
  int(-1)
}
Done.
