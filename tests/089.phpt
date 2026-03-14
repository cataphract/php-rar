--TEST--
RarArchive unset_property gives a fatal error
--FILE--
<?php

$f1 = dirname(__FILE__) . "/latest_winrar.rar";
$a = RarArchive::open($f1);

unset($a[0]);

echo "\n";
echo "Done.\n";
--EXPECTF--
Fatal error: main(): A RarArchive object is not writable in %s on line %d%A
