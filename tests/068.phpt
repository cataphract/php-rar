--TEST--
RarArchive direct instantiation does not crash
--SKIPIF--
<?php if(!extension_loaded("rar")) die("skip");
--FILE--
<?php

new RarArchive();

echo "Done\n";
--EXPECTF--
Fatal error: Uncaught Error: Call to private RarArchive::__construct() from invalid context in %s
Stack trace:
#0 {main}
%sthrown in %s on line %d
