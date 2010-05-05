--TEST--
url stat test
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php

$inex_rar = "rar://" .
	dirname(__FILE__) . '/not_found.rar' .
	"#emptydir";

echo "RAR not found:\n";
var_dump(stat($inex_rar));

$inex_entry = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar' .
	"#inexistent entry";

echo "\nRAR entry not found:\n";
var_dump(stat($inex_entry));
	
$root1 = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar';

echo "\nRAR root:\n";
$statr1 = stat($root1);
var_dump($statr1);
echo "\nRAR root is dir:\n";
var_dump(is_dir($root1));
	
$root2 = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar#';

echo "\nRAR root variant 2 matches:\n";
var_dump(stat($root2) == $statr1);
	
$root3 = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar#/';
echo "\nRAR root variant 3 matches:\n";
var_dump(stat($root3) == $statr1);

$file = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar' .
	"#file";

echo "\nRegular file:\n";
var_dump(stat($file));
	
$dir = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar' .
	"#emptydir";
echo "\nRegular file:\n";
var_dump(stat($dir));

echo "Done.\n";
--EXPECTF--
RAR not found:

Warning: stat(): Failed to open %snot_found.rar: ERAR_EOPEN (file open error) in %s on line %d

Warning: stat(): stat failed for rar://%s/not_found.rar#emptydir in %s on line %d
bool(false)

RAR entry not found:

Warning: stat(): Found no entry inexistent entry in archive %sdirlink_unix.rar in %s on line %d

Warning: stat(): stat failed for rar://%s/dirlink_unix.rar#inexistent entry in %s on line %d
bool(false)

RAR root:
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

RAR root is dir:
bool(true)

RAR root variant 2 matches:
bool(true)

RAR root variant 3 matches:
bool(true)

Regular file:
array(26) {
  [0]=>
  int(0)
  [1]=>
  int(0)
  [2]=>
  int(33188)
  [3]=>
  int(1)
  [4]=>
  int(0)
  [5]=>
  int(0)
  [6]=>
  int(0)
  [7]=>
  int(8)
  [8]=>
  int(0)
  [9]=>
  int(1259625512)
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
  int(33188)
  ["nlink"]=>
  int(1)
  ["uid"]=>
  int(0)
  ["gid"]=>
  int(0)
  ["rdev"]=>
  int(0)
  ["size"]=>
  int(8)
  ["atime"]=>
  int(0)
  ["mtime"]=>
  int(1259625512)
  ["ctime"]=>
  int(0)
  ["blksize"]=>
  int(%s)
  ["blocks"]=>
  int(%s)
}

Regular file:
array(26) {
  [0]=>
  int(0)
  [1]=>
  int(0)
  [2]=>
  int(16877)
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
  int(1259625807)
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
  int(16877)
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
  int(1259625807)
  ["ctime"]=>
  int(0)
  ["blksize"]=>
  int(%s)
  ["blocks"]=>
  int(%s)
}
Done.
