--TEST--
RarArchive::getEntry() basic test
--FILE--
<?php
$rar_arch = RarArchive::open(dirname(__FILE__) . '/solid.rar');
$rar_entry = $rar_arch->getEntry('tese.txt');
echo $rar_entry;
echo "\n";
echo "Done\n";
--EXPECTF--
RarEntry for file "tese.txt" (23b93a7a)
Done
