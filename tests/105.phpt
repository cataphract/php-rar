--TEST--
Clone of RarArchive is forbidden (PHP 5.x)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip";
if (!defined('PHP_VERSION_ID') || PHP_VERSION_ID >= 70000) die("skip for PHP 5.x");
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
Fatal error: Trying to clone an uncloneable object of class RarArchive in %s on line %d
