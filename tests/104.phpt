--TEST--
Clone of RarArchive is forbidden (PHP 7)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip";
if (!defined('PHP_VERSION_ID') || PHP_VERSION_ID < 70000) die("skip for PHP >= 7");
--FILE--
<?php
RarException::setUsingExceptions(true);
$file = dirname(__FILE__) . '/rar5_multi.part1.rar';
$rar = RarArchive::open($file);
$rar2 = clone $rar;
$rar2->getEntries();
echo "Never reached.\n";
?>
--EXPECTF--
Fatal error: Uncaught Error: Trying to clone an uncloneable object of class RarArchive in %s:5
Stack trace:
#0 {main}
  thrown in %s on line %d
