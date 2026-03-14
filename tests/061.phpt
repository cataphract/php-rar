--TEST--
RAR directory stream attempt on file
--FILE--
<?php
$u = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar' .
	"#file";
var_dump(opendir($u));

echo "Done.\n";
--EXPECTF--
Warning: opendir(rar://%sdirlink_unix.rar#file): %cailed to open dir%S: Archive %sdirlink_unix.rar has an entry named file, but it is not a directory in %s on line %d
bool(false)
Done.
