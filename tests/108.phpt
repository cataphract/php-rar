--TEST--
RarEntry::getPackedSize()
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
$a = rar_open(dirname(__FILE__) . '/4mb.rar');
$e = $a->getEntry('4mb.txt');
var_dump($e->getPackedSize());
echo "Done.\n";
--EXPECTF--
int(2444)
Done.
