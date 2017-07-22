--TEST--
RarEntry::getHostOs()
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
$a = rar_open(dirname(__FILE__) . '/4mb.rar');
$e = $a->getEntry('4mb.txt');
var_dump($e->getHostOs());
var_dump(RarEntry::HOST_WIN32);
echo "Done.\n";
--EXPECTF--
int(2)
int(2)
Done.
