/*
*	Name		:	listfiles.c
*	Author		:	Rory Pinkney
*	Last Updated:	13 Dec 2018
*	Description	:	Recursivley lists all the files and directories, descending from the current working
*					directory
*/

#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <grp.h>

static int display_info(const char *fpath, const struct stat *sb,
								int tflag, struct FTW *ftwbuf){	
	mode_t perm = sb->st_mode;
	char permissions[11] = {											//creating permission string in 'ls'
		(perm & S_IFREG) ? '-' : (perm & S_IFDIR) ? 'd' : '?',			//format ->  '-rwxrwxrwx'
		(perm & S_IRUSR) ? 'r' : '-', (perm & S_IWUSR) ? 'w' : '-',
		(perm & S_IXUSR) ? 'x' : '-', (perm & S_IRGRP) ? 'r' : '-',
		(perm & S_IWGRP) ? 'w' : '-', (perm & S_IXGRP) ? 'x' : '-',
       	(perm & S_IROTH) ? 'r' : '-', (perm & S_IWOTH) ? 'w' : '-',
       	(perm & S_IXOTH) ? 'x' : '-', '\0'
	};

	struct passwd *pw;													//passwd struct to get owner name
	if (pw = getpwuid(sb->st_uid), pw == NULL){
		perror("getpwuid");
		exit(EXIT_FAILURE);
	}

	struct group *gr;													//group struct to get group name
	if (gr = getgrgid(sb->st_gid), gr == NULL){
		perror("getpwuid");
		exit(EXIT_FAILURE);
	}

	char lastModified[20];
	strftime(lastModified, 20, "%b %d %H:%M", localtime(&sb->st_mtime));	//date string of last modified
																			//date
	printf("  %s  %2d  %s  %10s  %6jd  %s  %-16s\n", permissions, (int)sb->st_nlink, 
			pw->pw_name, gr->gr_name, (intmax_t)sb->st_size, lastModified, fpath + ftwbuf->base);

	return 0;
}

int main(int argc, char *argv[]){
	int flags = 0;
	flags |= FTW_PHYS;												//do not follow symbolic links
	
	if (nftw(".", display_info, 20, 0) == -1){						//start file walk from current directory
		perror("ntfw");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
