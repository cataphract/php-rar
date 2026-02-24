--TEST--
RarEntry::extract() function (broken set fixed with volume callback)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
function resolve($vol) {
	if (preg_match('/_broken/', $vol))
		return str_replace('_broken', '', $vol);
	else
		return null;
}
echo "Fail:\n";
$rar_file1 = rar_open(dirname(__FILE__).'/multi_broken.part1.rar');
$entry = $rar_file1->getEntry('file2.txt');

echo "\nSuccess:\n";
$rar_file1 = rar_open(dirname(__FILE__).'/multi_broken.part1.rar', null, 'resolve');
$entry = $rar_file1->getEntry('file2.txt');
$entry->extract(null, dirname(__FILE__) . "/temp_file2.txt");
echo strtoupper(hash("crc32b", file_get_contents(dirname(__FILE__) . "/temp_file2.txt")));
echo "\n";
echo "Done\n";
?>
--CLEAN--
<?php
@unlink(dirname(__FILE__) . "/temp_file2.txt");
--EXPECTF--
Fail:

Warning: RarArchive::getEntry(): Volume %smulti_broken.part2.rar was not found in %s on line %d

Warning: RarArchive::getEntry(): ERAR_EOPEN (file open error) in %s on line %d

Success:
F2C79881
Done
