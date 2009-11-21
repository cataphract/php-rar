--TEST--
rar_close() liberates resource (PECL bug #9649)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
exec('pause');
copy(dirname(__FILE__).'/latest_winrar.rar', dirname(__FILE__).'/temp.rar');
$rar_file1 = rar_open(dirname(__FILE__).'/temp.rar');
var_dump($rar_file1);
$entries = rar_list($rar_file1);
$entry1 = reset($entries);
unset($entries);
var_dump($entry1);
echo "\n";

rar_close($rar_file1);
var_dump($rar_file1);
unlink(dirname(__FILE__).'/temp.rar');
	
echo "Done\n";
?>
--EXPECTF--
resource(8) of type (Rar file)
object(RarEntry)#%d (%d) {
  ["rarfile"]=>
  resource(%d) of type (Rar file)
  ["name"]=>
  string(5) "1.txt"
  ["unpacked_size"]=>
  int(5)
  ["packed_size"]=>
  int(17)
  ["host_os"]=>
  int(2)
  ["file_time"]=>
  string(19) "2004-06-11 10:07:18"
  ["crc"]=>
  string(8) "a0de71c0"
  ["attr"]=>
  int(32)
  ["version"]=>
  int(29)
  ["method"]=>
  int(53)
}

resource(%d) of type (Unknown)
Done
