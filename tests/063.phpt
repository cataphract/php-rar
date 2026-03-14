--TEST--
RAR directory stream stat consistency with url stat
--FILE--
<?php

echo "Root:\n";

$u = "rar://" .
	dirname(__FILE__) . '/dirs_and_extra_headers.rar';

var_dump(fstat(opendir($u)) == stat($u));

echo "\nSub-root directory:\n";

$u = "rar://" .
	dirname(__FILE__) . '/dirs_and_extra_headers.rar#%EF%AC%B0';

var_dump(fstat(opendir($u)) == stat($u));

echo "Done.\n";
--EXPECTF--
Root:
bool(true)

Sub-root directory:
bool(true)
Done.
