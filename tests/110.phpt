--TEST--
RarEntry::getFileTime()
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
$a = rar_open(dirname(__FILE__) . '/4mb.rar');
$e = $a->getEntry('4mb.txt');
var_dump($e->getFileTime());
echo "Done.\n";
--EXPECTF--
string(19) "2010-05-30 01:22:00"
Done.
