--TEST--
RarEntry::getStream() function (solid archive)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/solid.rar');
$entries = rar_list($rar_file1);
echo count($entries)." files:\n\n";
//var_dump($entries);
foreach ($entries as $e) {
	$stream = $e->getStream();
	echo $e->getName().": ";
	$a = "";
	while (!feof($stream)) {
		$a .= fread($stream, 8192);
	}
	echo strlen($a)." bytes, CRC ";
	echo strtoupper(hash("crc32b", $a))."\n\n"; //you can confirm they're equal to those given by $e->getCrc()
}

echo "Done\n";
?>
--EXPECTF--
2 files:

tese.txt: 787 bytes, CRC 23B93A7A

unrardll.txt: 19192 bytes, CRC 2ED64B6E

Done
