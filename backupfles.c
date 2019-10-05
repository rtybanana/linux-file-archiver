/*
*	Name		:	backupfles.c
*	Author		:	Rory Pinkney
*	Last Updated:	15 Dec 2018
*	Description	:	Displays the details of files and directories newer than a specifiec date either from
*					a last modified time of a file or a specified date as a string
*/

#define _XOPEN_SOURCE 500		//implementing POSIX 1995
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <grp.h>
#include <unistd.h>

/* GLOBAL VARIABLES
* gl_backup		time_t		cutoff time for files to be added to the archive
*/
time_t gl_backup;

static int to_backup(const char *fpath, const struct stat *sb,
								int tflag, struct FTW *ftwbuf){	
	if (difftime(sb->st_mtime, gl_backup) > 0){
		mode_t perm = sb->st_mode;
		char permissions[11] = {									//creating permissions string in 'ls' form
		(perm & S_IFREG) ? '-' : (perm & S_IFDIR) ? 'd' : '?',		//ie. -rwxrwxrwx
		(perm & S_IRUSR) ? 'r' : '-', (perm & S_IWUSR) ? 'w' : '-',
		(perm & S_IXUSR) ? 'x' : '-', (perm & S_IRGRP) ? 'r' : '-',
		(perm & S_IWGRP) ? 'w' : '-', (perm & S_IXGRP) ? 'x' : '-',
		(perm & S_IROTH) ? 'r' : '-', (perm & S_IWOTH) ? 'w' : '-',
		(perm & S_IXOTH) ? 'x' : '-', '\0'
		};

		struct passwd *pw;	
		if (pw = getpwuid(sb->st_uid), pw  == NULL){				//passwd struct to get owner name
			perror("getpwuid");
			exit(EXIT_FAILURE);
		}

		struct group *gr;											//group struct to get group name
		if (gr = getgrgid(sb->st_gid), gr == NULL){
			perror("getgrgid");
			exit(EXIT_FAILURE);
		}

		char lastModified[20];
		strftime(lastModified, 20, "%b %d %H:%M", localtime(&sb->st_mtime));	//date string of last modified
																				//date of current file
		printf("  %s  %2d  %8s  %10s  %6jd  %s  %-16s\n", permissions, (int)sb->st_nlink, 
			pw->pw_name, gr->gr_name, (intmax_t)sb->st_size, lastModified, fpath + ftwbuf->base);
	}
	return 0;				//continue to next file
}

int main(int argc, char *argv[]){
	char *path;
	int flags = 0 | FTW_PHYS ;
	int option;
	int tflag;
	int hflag;
	char *targ;

	while ((option = getopt(argc, argv, "ht:")) != -1){
		switch (option){
			case 't':
				tflag = 1;				//set tflag
				targ = optarg;			
				break;
			case 'h':
				hflag = 1;				//set hflag
				break;
			case '?':
				if (optopt == 't'){
					printf("    -t {<filename>, <date>}\n");	//-t usage reminder
					exit(EXIT_FAILURE);
				}
				break;
			default:
				printf("fatal error\n");		//shouldnt ever reach here just a catch
				exit(EXIT_FAILURE);
		}
	}
	
	if (hflag == 1){							//printf help
		printf("\nUse of ./backupfles:\n" 
			"    Use of backupfles requires two or three arguments, either '-t' and a\n"
		    "    {path} to start the recursive function (in which case the provided date\n" 
		  	"    will default to the epoch (1/1/1970)), or with an additional argument\n"
			"    {<filename> or <date>} to select only files which were last modified after\n"
			"    the provided file, or after the date if a date is provided.\n\n"
			"    -t {<filename>, <date>} {path}\n"
			"        filename: Relative path to a file\n"
			"        date    : A date in the format 'YYYY-MM-DD hh:mm:ss'\n"
			"        path    : A path to the directory where the function will start\n\n");
		exit(EXIT_SUCCESS);
	}
		
	if (tflag == 1){							//checking for string in date format
		if (((targ[4] & targ[7]) == '-') && ((targ[13] & targ[16])) == ':'){			
			struct tm tm;
			if (strptime(targ, "%Y-%m-%d %H:%M:%S", &tm) == NULL){
				printf("strptime: Date format not recognised\n");
				exit(EXIT_FAILURE);
			}
			gl_backup = mktime(&tm);			//set global variable
		}
		else {									//if not in date format, consider it a file
			struct stat sb;
			if (stat(targ, &sb) == -1){			//catches if no file exists, therefore also catches if a date
				perror("stat");					//is input slightly wrong
				exit(EXIT_FAILURE);
			}
			gl_backup = sb.st_mtime;			//set global variable
		}	
	}		
	
	if (argv[optind] != NULL){
		path = argv[optind];					//last argument is where to start recursion
	}	
	else {
		printf("error: No path given to start the function in, use -h for help\n");
		exit(EXIT_FAILURE);
	}
		
	if (nftw(path, to_backup, 20, 0) == -1){
		perror("ntfw");
		exit(EXIT_FAILURE);
	}
	
	exit(EXIT_SUCCESS);
}
