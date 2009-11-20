--TEST--
RarEntry::getStream() function (bad RAR file)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/corrupted.rar');
$entries = rar_list($rar_file1);
echo count($entries)." files (will test only the first 4):\n\n";
//var_dump($entries);
$i = 0;
foreach ($entries as $e) {
	if ($i++ >= 4)
		break;
	$stream = $e->getStream();
	echo $e->getName().": ";
	if ($stream === false) {
		echo "Could not get stream.\n\n";
		continue;
	}
	while (!feof($stream)) {
		echo fread($stream, 8192);
	}
	fclose($stream);
	echo "\n";

}

echo "Done\n";
?>
--EXPECTF--
51 files (will test only the first 4):


Warning: RarEntry::getStream(): ERAR_BAD_DATA in D:\Users\Cataphract\Documents\php_rar\trunk\tests\012.php on line 11
test\%s': Could not get stream.


Warning: RarEntry::getStream(): ERAR_BAD_DATA in D:\Users\Cataphract\Documents\php_rar\trunk\tests\012.php on line 11
test\%s: Could not get stream.


Warning: RarEntry::getStream(): ERAR_BAD_DATA in D:\Users\Cataphract\Documents\php_rar\trunk\tests\012.php on line 11
test\%s: Could not get stream.


Warning: RarEntry::getStream(): ERAR_BAD_DATA in D:\Users\Cataphract\Documents\php_rar\trunk\tests\012.php on line 11
test\a\a: Could not get stream.

Done
