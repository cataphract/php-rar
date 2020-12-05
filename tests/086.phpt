--TEST--
RarArchive write_property gives a fatal error
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
if (key_exists('USE_ZEND_ALLOC', $_ENV) && PHP_VERSION_ID < 70000) die('skip do not use with valgrind in PHP <7');
?>
--FILE--
<?php

$f1 = dirname(__FILE__) . "/latest_winrar.rar";
$a = RarArchive::open($f1);

$a[0] = "jjj";

echo "\n";
echo "Done.\n";
--EXPECTF--
Fatal error: main(): A RarArchive object is not writable in %s on line %d
