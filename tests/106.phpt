--TEST--
Stat times don't depend on timezone (cf. 056.phpt)
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--ENV--
TZ=UTC
--FILE--
<?php
umask(0);
$stream = fopen("rar://" .
	dirname(__FILE__) . '/latest_winrar.rar' .
	"#1.txt", "r");
$fs = fstat($stream);
echo $fs['mtime'], "\n";
echo "Done.\n";
--EXPECTF--
1086948439
Done.
