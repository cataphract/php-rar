--TEST--
Bug #75557 (Error opening archive w/non-english chars in path)
--SKIPIF--
<?php
if (!extension_loaded("rar")) die("skip rar extension not available");
?>
--FILE--
<?php
$rar = RarArchive::open(__DIR__ . '/75557тест.rar');
var_dump(count($rar->getEntries()));
?>
--EXPECT--
int(1)
