--TEST--
RarEntry::isDirectory() basic test
--FILE--
<?php
$rar_file1 = rar_open(dirname(__FILE__).'/directories.rar');
$entries = rar_list($rar_file1);

foreach ($entries as $e) {
	echo "{$e->getName()} is ". ($e->isDirectory()?"":"not ") . "a directory.\n";
}

echo "Done\n";
--EXPECTF--
dirwithsth%cfileindir.txt is not a directory.
dirwithsth is a directory.
emptydir is a directory.
Done
