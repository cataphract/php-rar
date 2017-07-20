--TEST--
RarEntry direct instantiation does not crash (PHP 5.x)
--SKIPIF--
<?php if(!extension_loaded("rar")) die("skip");
if (!defined('PHP_VERSION_ID') || PHP_VERSION_ID >= 70000) die("skip for PHP 5.x");
--FILE--
<?php

new RarEntry();

echo "Done\n";
--EXPECTF--
Fatal error: Call to private RarEntry::__construct() from invalid context in %s on line %d
