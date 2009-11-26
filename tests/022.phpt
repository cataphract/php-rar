--TEST--
RarEntry::extract() with directory
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
$rar_file1 = rar_open(dirname(__FILE__).'/directories.rar');
$e = rar_entry_get($rar_file1, "emptydir");

$dir = "." . DIRECTORY_SEPARATOR . "emptydir";
$exists = is_dir($dir);
var_dump($exists);
$extrres = $e->extract(".");
var_dump($extrres);
$exists = is_dir($dir);
var_dump($exists);
@rmdir($dir);

echo "\n\n";

$dir = "." . DIRECTORY_SEPARATOR . "emptydircust";
$exists = is_dir($dir);
var_dump($exists);
$extrres = $e->extract(false, $dir);
var_dump($extrres);
$exists = is_dir($dir);
var_dump($exists);
@rmdir($dir);

echo "Done\n";
--EXPECTF--
bool(false)
bool(true)
bool(true)


bool(false)
bool(true)
bool(true)
Done
