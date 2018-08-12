--TEST--
RarArchive direct instantiation does not crash (PHP 5.x)
--SKIPIF--
<?php
if (!extension_loaded("rar")) die("skip");
if (!defined('PHP_VERSION_ID') || PHP_VERSION_ID >= 70000) die("skip for PHP 5.x");
--FILE--
<?php

new RarArchive();

echo "Done\n";
--EXPECTF--
Fatal error: Uncaught Error: Call to private RarArchive::__construct() from invalid context in %s
Stack trace:
#0 {main}
%sthrown in %s on line %d
