--TEST--
rar_close() liberates resource (PECL bug #9649)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
copy(dirname(__FILE__).'/latest_winrar.rar', dirname(__FILE__).'/temp.rar');
$rar_file1 = rar_open(dirname(__FILE__).'/temp.rar');
var_dump($rar_file1);
$entries = rar_list($rar_file1);
$entry1 = reset($entries);
unset($entries);
echo $entry1."\n";
echo "\n";

rar_close($rar_file1);
var_dump($rar_file1);
unlink(dirname(__FILE__).'/temp.rar');
	
echo "Done\n";
?>
--EXPECTF--
resource(%d) of type (Rar file)
RarEntry for file "1.txt" (a0de71c0)

resource(%d) of type (Unknown)
Done
