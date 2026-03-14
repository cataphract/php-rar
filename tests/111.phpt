--TEST--
RarEntry::getVersion()
--FILE--
<?php
$a = rar_open(dirname(__FILE__) . '/rar5-links.rar');
$e = $a->getEntry('file1.txt');
var_dump($e->getVersion());
echo "Done.\n";
--EXPECTF--
int(50)
Done.
