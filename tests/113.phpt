--TEST--
RarEntry::isEncrypted()
--FILE--
<?php
$a = rar_open(dirname(__FILE__) . '/rar5-links.rar');
$e = $a->getEntry('file1.txt');
var_dump($e->isEncrypted());

$a = rar_open(dirname(__FILE__) . '/encrypted_only_files.rar');
$e = $a->getEntry('encfile1.txt');
var_dump($e->isEncrypted());
echo "Done.\n";
--EXPECTF--
bool(false)
bool(true)
Done.
