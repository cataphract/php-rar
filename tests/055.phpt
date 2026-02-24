--TEST--
Stream wrapper with volume find callback
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
function resolve($vol) {
	if (preg_match('/_broken/', $vol))
		return str_replace('_broken', '', $vol);
	else
		return null;
}
$stream = fopen("rar://" .
	dirname(__FILE__) . '/multi_broken.part1.rar' .
	"#file2.txt", "r", false,
	stream_context_create(array('rar'=>array('volume_callback'=>'resolve'))));
$a = stream_get_contents($stream);
echo strlen($a)." bytes, CRC ";
echo strtoupper(hash("crc32b", $a))."\n\n"; //you can confirm they're equal to those given by $e->getCrc()
echo "Done.\n";
--EXPECTF--
17704 bytes, CRC F2C79881

Done.

