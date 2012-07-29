--TEST--
Bug #59939: Streaming empty file from archive issues a warning
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar = RarArchive::open(dirname(__FILE__) . '/empty_file.rar');
if ($rar === false) die("could not open RAR file");

$rar_file = $rar->getEntry('empty_file');
if ($rar_file === false) die("could not find entry");

$stream = $rar_file->getStream();
if ($stream === false) die("could not open stream");

var_dump(feof($stream),
	fread($stream, 1024*1024),
	feof($stream));

echo "\n";
echo "Done.\n";
?>
--EXPECT--
bool(false)
string(0) ""
bool(true)

Done.
