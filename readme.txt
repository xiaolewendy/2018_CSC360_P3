CSC 360: Operating Systems (FALL 2018)
Assignment 3: A Simple File System (SFS)

PART I: diskinfo
Invoke program by: ./diskinfo <file system name>
Example: ./diskinfo disk.IMA
NOTE: file system name is case sensitive


PART II: disklist
Invoke program by: ./disklist <file system name>
Example: ./disklist disk.IMA
NOTE: file system name is case sensitive
      creation date is in DD/MM/YYYY format


PART III: diskget
Invoke program by: ./diskget <file system name> <file exits in file system>
Example: ./diskget disk.IMA reminder.txt
NOTE: file system name is case sensitive
      get file must exist in file system, name enter on command line is case insensitive

PART IV: diskput
Invoke program by: ./diskput <file system name> <filename / path and filename>
Example: ./diskput disk.IMA foo.txt
NOTE: file system name is case sensitive
      put file must exist in current directory (case sensitive)
      if enter path, subdirectory must exist in file system (subdirectory name enter is case insensitive)
