--TEST--
fopen modes 'r' and 'rb' are the only allowed
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$file = 'rar://' . rawurlencode(dirname(__FILE__) . '/linux_rar.rar')
		. '#/plain.txt';

echo "Testing 'r'\n";
$fd = fopen($file, 'r');
if ($fd) echo "opened\n\n";

echo "Testing 'rb'\n";
$fd = fopen($file, 'rb');
if ($fd) echo "opened\n\n";

echo "Testing 'r+'\n";
$fd = fopen($file, 'r+');
if ($fd) echo "opened\n\n";

echo "\n";
echo "Done.\n";
?>
--EXPECTF--
Testing 'r'
opened

Testing 'rb'
opened

Testing 'r+'

Warning: fopen(%s): %cailed to open stream: Only the "r" and "rb" open modes are permitted, given r+ in %s on line %d

Done.
