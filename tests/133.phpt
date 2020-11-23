--TEST--
RarArchive has interface Traversable
--SKIPIF--
<?php if(!extension_loaded("rar")) die("skip");
--FILE--
<?php

$rar = rar_open(dirname(__FILE__) . "/linux_rar.rar");
if ($rar instanceof Traversable) {
    echo "\nOK, instanceof Traversable\n";
}

echo "Done\n";
--EXPECTF--
OK, instanceof Traversable
Done
