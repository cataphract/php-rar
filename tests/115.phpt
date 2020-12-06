--TEST--
getIterator() (PHP 8+)
--SKIPIF--
<?php
if (!extension_loaded("rar")) die("skip");
if (PHP_VERSION_ID < 80000) print "skip for PHP 8";
?>
--FILE--
<?php

$a = rar_open(dirname(__FILE__).'/linux_rar.rar'); 
$it = $a->getIterator();
var_dump($it);
foreach ($it as $e) {
    echo $e->getName(), "\n";
}

echo "Done\n";
?>
--EXPECT--
object(InternalIterator)#3 (0) {
}
plain.txt
test file with whitespaces.txt
Done
