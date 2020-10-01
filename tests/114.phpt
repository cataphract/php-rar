--TEST--
Bug 76592: solid files are partially extracted
--SKIPIF--
<?php
if (!extension_loaded("rar")) die("skip");
if (PHP_OS != 'Linux') die('skip for linux');
if (PHP_VERSION_ID < 50400) die("skip for PHP 5.4+");
--FILE--
<?php

$before = hex2bin('526172211a07010030f8db480d01050900080101c5808085800093f4589f2e02030b80808085800004808080858000b483020000000080380108746573742e6461740a031389bf735fa302a31c');
$after = hex2bin('a6e7161b0e0306be0000be000080000102514fe6dcc40b3900b38080053393f4589f2e02030b80808085800004808080858000b483020000000080380108746573742e6461740a031389bf735fa302a31c1d77565103050400');

$middle = file_get_contents('/dev/urandom', false, null, 0, 10 * 1024 * 1024) or die('failed file_get_contents');
$crc32hexbe = crc32($middle);
$crc32le = pack('V', $crc32hexbe);

$before = substr($before, 0, 50) . $crc32le . substr($before, 54);
$after = substr($after, 0, 56) . $crc32le . substr($after, 60);

$data = $before . $middle . $after;
$file = tempnam('/tmp', 'rar');
file_put_contents($file, $data) or die('failed file_put contents');

$rar = \RarArchive::open($file) or die('Unable to open archive');
$rar->setAllowBroken(true); // we don't fixup the headers checksum, only the contents. Ignore the error
$entry = $rar->getEntry('test.dat') or die('Unable to get entry');

$contents = stream_get_contents($entry->getStream(), $entry->getUnpackedSize());
$crc32_rar = $entry->getCrc();
$crc32_cont = dechex(crc32($contents));
$crc32_orig_content = dechex(crc32($contents));

unlink($file);

echo 'orig content size: ', strlen($middle), "\n";
echo 'read content size: ', strlen($contents), "\n";

if ($crc32_rar !== $crc32_cont) {
	die("CRC values do not match");
}
if ($crc32_rar !== $crc32_orig_content) {
	die("CRC values do not match (2)");
}
?>
==DONE==
--EXPECT--
orig content size: 10485760
read content size: 10485760
==DONE==
