--TEST--
Stream wrapper archive/file not found
--FILE--
<?php

echo "Archive not found :\n";
$stream = fopen("rar://" .
	dirname(__FILE__) . "/not_found.rar" .
	"#1.txt", "r");

echo "\nFile not found :\n";
$stream = fopen("rar://" .
	dirname(__FILE__) . "/latest_winrar.rar" .
	"#not_found.txt", "r");

echo "Done.\n";
--EXPECTF--
Archive not found :

Warning: fopen(rar://%snot_found.rar#1.txt): %cailed to open stream: Error opening RAR archive %snot_found.rar: ERAR_EOPEN (file open error) in %s on line %d

File not found :

Warning: fopen(rar://%slatest_winrar.rar#not_found.txt): %cailed to open stream: Can't file not_found.txt in RAR archive %s on line %d
Done.
