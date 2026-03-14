--TEST--
RarArchive::open() basic test
--FILE--
<?php
$arch = RarArchive::open(dirname(__FILE__) . "/dirlink_unix.rar");
var_dump(get_class($arch));


echo "Done\n";
--EXPECTF--
string(10) "RarArchive"
Done
