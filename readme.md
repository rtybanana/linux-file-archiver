# Linux File Archiver
File archiver tool written in C with a command line interface for a 2nd year university coursework.
This repo contains three seperate files which are different C programs for the sections of the coursework I made them for.

**listfiles.c**   
Recursivley lists all the files and directories, descending from the current working directory.

**backupfles.c**   
Displays the details of files and directories newer than a specifiec date either from a last modified time of a file or a specified date as a string.

**backup.c**     
This file contains two different programs, one of which runs if the command is run from a linux symlink called 'restore' rather than 'backup'

*backup*  
Recusively processes every file and directory beneath a given start directory and archives them each into a .tar file format. Additionally,  
you can choose a date from which files will be archived only if they have been modified since.

*restore*  
Processes a given file in the .tar format and recreates the directory and file structure exactly as it was archived - preserving modification dates  
and owner-group permissions.

Completed as the second peice of coursework for my second year module 'Architectures and Operating Systems'. It was written entirely  
in vim on a Raspberry Pi  command line interface. I received 98% for this coursework.
