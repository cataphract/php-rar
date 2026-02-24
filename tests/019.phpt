--TEST--
RarEntry::getStream() function (store method)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/store_method.rar');
$entries = rar_list($rar_file1);
echo count($entries)." files:\n\n";
//var_dump($entries);
foreach ($entries as $e) {
	$stream = $e->getStream();
	echo $e->getName().": ";
	$a = "";
	while (!feof($stream)) {
		$a .= fread($stream, 512);
	}
	echo strlen($a)." bytes, CRC ";
	echo strtoupper(hash("crc32b", $a))."\n\n"; //you can confirm they're equal to those given by $e->getCrc()
}

echo "Done\n";
?>
--EXPECTF--
1 files:

tese.txt: 787 bytes, CRC 23B93A7A

Done
