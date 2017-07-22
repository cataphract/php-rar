--TEST--
RarEntry::getMethod()
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
$a = rar_open(dirname(__FILE__) . '/rar5-links.rar');
$e = $a->getEntry('file1.txt');
var_dump($e->getMethod());
echo "Done.\n";
--EXPECTF--
int(48)
Done.
