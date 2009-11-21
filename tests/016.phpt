--TEST--
RarEntry::extract() method (corrupt RAR file)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
exec('pause');
$rar_file1 = rar_open(dirname(__FILE__).'/corrupted.rar');
$entries = rar_list($rar_file1);
echo count($entries)." files (will test only the first 4):\n\n";
//var_dump($entries);
$i = 0;
foreach ($entries as $e) {
	if ($i++ >= 4)
		break;
	$e->extract(false, dirname(__FILE__).'/temp.txt');
}

@unlink('temp.txt');

echo "Done\n";
?>
--EXPECTF--
51 files (will test only the first 4):


Warning: RarEntry::extract(): ERAR_BAD_DATA in %s on line %d

Warning: RarEntry::extract(): ERAR_BAD_DATA in %s on line %d

Warning: RarEntry::extract(): ERAR_BAD_DATA in %s on line %d

Warning: RarEntry::extract(): ERAR_BAD_DATA in %s on line %d
Done
