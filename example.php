<?

$archive_name = '/full/path/to/file.rar'
$entry_name = 'path/to/archive/entry.txt'; //notice: no slash at the beginning
$dir_to_extract_to = '/path/to/extract/dir';
$new_entry_name = 'some.txt';


$rar = rar_open($archive_name) OR die('failed to open ' . $archive_name);
$entry = rar_entry_get($rar, $entry_name) OR die('failed to find ' . $entry_name . ' in ' . $archive_name);

// this will create all necessary subdirs under $dir_to_extract_to
$entry->extract($dir_to_extract_to); 
/* OR */

// this will create only one new file $new_entry_name in $dir_to_extract_to
$entry->extract('', $dir_to_extract_to.'/'.$new_entry_name); 

// this line is really not necessary
rar_close($rar);

?>
