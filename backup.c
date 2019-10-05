/*
*	Name:			Backup/Restore
*	Author:			Rory Pinkney
*	Last Updated:	16th Dec 2018
*	Description:	Backup:		Recusively processes every file and directory beneath a given start 
*								directory and archives them each into a .tar file format. Additionally, 
*								you can choose a date from which files will be archived only if they 
*								have been modified since.
*					Restore:	Processes a given file in the .tar format and recreates the directory
*								and file structure exactly as it was archived - preserving modification
*								dates and owner-group permissions.
*/

#define _XOPEN_SOURCE 500			//incorporating POSIX 1995
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <utime.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

/*	Global Variables
* Named gl_<name> to try and prevent name collisions and indicate that I'm using a global variable.
*
* gl_startDate		time_t		used in 'backup' function as date cut off for file selection
* gl_now			time_t		used in 'backup' function to exclude the archive file from file walk
* gl_pathOffset 	int			used in 'backup' function to cut out all the unnecessary parent directory info
* archive			FILE		holds the open file for archiving the selected content			
*/
time_t 	gl_startDate;
time_t 	gl_now;
int 	gl_pathOffset;
FILE    *archive;

/*	Struct TarHeader  -typedef-  Header
* Contains all the information to store about each entry in our tar archive in a neat 512 byte block.
*/
typedef struct TarHeader{
	char name[100];
	char mode[8];
	char owner[8];
	char group[8];
	char size[12];
	char modified[12];
	char checksum[8];
	char type[1];
	char link[100];
	char padding[255];			//pad total size of this struct to 512 bytes
}
Header;

/*	fpad  -  returns void
* Pads the provided 'file' by 'amount' with null chars. Used to keep the start of the archived files and the 
* total file size padded to a multiple of 512
*
* file		FILE		file to pad
* amount	size_t		amount to pad by
*/
void fpad(FILE *file, size_t amount){
	while(amount--){
		fputc(0, file);
	}
}

/*	backup  -  return int
* For every file passed to it, this function will create an appropriate .tar header and add it and the file
* contents to the 'archive' (FILE) with tar formatting (padding to multiples of 512, and 1024 0 bytes to end)
*
* Code is modified from the top answer at:
* https://stackoverflow.com/questions/29641965/creating-my-own-archive-tool-in-c 
*
* fpath		char		path of current file from nftw()
* sb		stat		stat struct of the current file from nftw()
* tflag		int			holds additional info about the current file
* ftwbuf	FTW			holds the offset of file's filename in fpath so that you can print just the name
*/
int backup(const char *fpath, const struct stat *sb,
								int tflag, struct FTW *ftwbuf){
	/*	This first if statement skips the entire process of adding to the archive for a given file and moves
	* onto the next on in the recursion. I've done this for two reasons, the first and more common occurence
	* is that the last modified time of the file falls outside of the date provided by the -t switch (<= 
	* startDate). Secondly, it skips the archive file specified by the -f switch (>= now), so that the archive
	* file isn't included in the backup, inflating the file with data it already contains. The right hand side 
	* of the '&&' is required because when you create the archive file, the directories last modified state
	* updates, so without that check the parent directory of where you create the archive file will be skipped. 
	*/
	if ((difftime(sb->st_mtime, gl_startDate) < 0) || 
		((difftime(sb->st_mtime, gl_now) >= 0) && !S_ISDIR(sb->st_mode))){		
		return 0;										//continue to next file
	}

	FILE *input;									//I put this second because if you can't open the file
	errno = 0;										//then there is no point writing it's header etc.
	if (!(input = fopen(fpath, "rb"))){				//Fortunately - C specifies that fopen won't fail if you
		perror("fopen");							//attempt to open a directory in read mode. So this won't
		printf("file skipped: %s\n", fpath);		//skip any directories.
		return 0;										//continue to next file
	}
	
	Header header;											//Creation of the tar header
	memset(&header, 0, sizeof(Header));						//512 null chars allocated
	snprintf(header.mode, 8, "%06o", sb->st_mode);
	snprintf(header.owner, 8, "%06o", sb->st_uid);
	snprintf(header.group, 8, "%06o", sb->st_gid);
	snprintf(header.size, 12, "%011o", sb->st_size);
	snprintf(header.modified, 12, "%011o", sb->st_mtime);
	if (S_ISDIR(sb->st_mode)){													//If DIR add trailing '/'
		if (snprintf(header.name, 100, "%s/", fpath + gl_pathOffset) >= 100){	//If path truncated
			printf("File path length doesn't fit in tar format, try archiving from a deeper root directory\n");
			printf("%s\n", fpath + gl_pathOffset);
			return 1;														//exits ntfw for post cleanup
		}
		header.type[0] = '5';												//type '5' indicates directory
	}
	else {																	//no trailing '/'
		if (snprintf(header.name, 100, "%s", fpath + gl_pathOffset) >= 100){	
			printf("File path length doesn't fit in tar format, try archiving from a deeper root directory\n");
			printf("%s\n", fpath + gl_pathOffset);
			return 1;														//exits ntfw for post cleanup
		}
		header.type[0] = '0';												//type '0' indicates regular file
	}	
	
	memset(header.checksum, ' ', 8);					//checksum space in header set to ' ' chars because
	size_t checksum = 0;								//this data is included in the checksum calculation
	const unsigned char* bytes = (char*)&header;	
	for (int i = 0; i < sizeof(Header); ++i){			//calculate checksum
		checksum += bytes[i];
	}
	snprintf(header.checksum, 8, "%06o", checksum);

	fwrite(&header, 1, sizeof(Header), archive);		//write the header to the archive

	if (!S_ISDIR(sb->st_mode)){								//if not DIR
		while (!feof(input)){								//read file to buffer and write buffer to archive
			char buffer[2000];
			size_t read = fread(buffer, 1, 2000, input);
			fwrite(buffer, 1, read, archive);
		}

		size_t index = ftell(archive);						
		size_t offset = index % 512;						//calculate how far off a multiple of 512 we are
		if (offset != 0){									
			fpad(archive, 512 - offset);					//pad to a multiple of 512
		}
	}

	fclose(input);											//close input file
	printf("Successfully archived: %s\n", fpath);			//console message
	return 0;											//continue to next file
}

/*	parseHeader  -  returns Header
* parseHeader reads the data from the next 512 bytes after a provided index of the archive file and puts
* the information into a Header stuct and returns that so that all of the data can be accessed seperately.
*
* index		long int		The index of the start of the 512 bytes of header information 
*/
Header parseHeader(long int index){
	fseek(archive, index, SEEK_SET);						//set cursor to given index
	Header header;
	memset(&header, 0, sizeof(Header));
	fscanf(archive, "%100s%8s%8s%8s%12s%12s%8s%1s", &header.name, 		//read data into Header struct
			&header.mode, &header.owner, &header.group, &header.size, 
			&header.modified, &header.checksum, &header.type);
	
	size_t check = strtol(header.checksum, NULL, 8);		//get checksum from tar header
	memset(header.checksum, ' ', 8);						//set checksum data to ' ' chars to check properly
	size_t sum = 0;											
	const unsigned char* bytes = (char*)&header;			//convert header to a stream of bytes
	for (int i = 0; i < sizeof(Header); ++i){				
		sum += bytes[i];									//calculate sum
	}

	if (check != sum){										//check sum
		printf("Checksum incorrect, tar file possibly corrupted\n");
		exit(EXIT_FAILURE);	
	}
	
	fseek(archive, index + 512, SEEK_SET);					//set cursor to end of this files header
	return header;
}

/*	restore  -  returns int
* restore unpacks a file in .tar format into it's original file and directory format into the current
* directory. It runs when this program is run from the symlink 'restore' instead of 'backup'. It does not
* currently work correctly with archiving symbolic links and instead archives them as a copy of the original
* file with all the same data in it.
*/
int restore(){
	Header header;
	FILE *file;
	struct utimbuf times;									//create a utimbuf for the utime() function
	times.actime = time(NULL);								//set access time to now

	fseek(archive, -1024, SEEK_END);						//tar file ends with 1024 null bytes
	long int end = ftell(archive);							//end = cursor position of the end of the tar data
	fseek(archive, 0, SEEK_SET);

	while (!feof(archive)){	
		long int index = ftell(archive);
		if (index >= end){									//check we arent at the end of the file
			return 1;										//end
		}
		
		header = parseHeader(index);
		mode_t mode  = strtol(header.mode, NULL, 8);		//converts octal string into int
		uid_t uid    = strtol(header.owner, NULL, 8);
		gid_t gid    = strtol(header.group, NULL, 8);
		time_t mtime = strtol(header.modified, NULL, 8);
		times.modtime = mtime;								//utimbuf modified time set to mtime from data

		if (header.type[0] == '5'){						//if DIR
			mkdir(header.name, mode);						//make directory with permissions
			chown(header.name, uid, gid);					//change owner 
			utime(header.name, &times);						//if there is anything in this directory the last
		}													//modification time is overwritten to now when they
															//are processed. Requires large workaround to fix.
		else {											//else
			off_t size = strtol(header.size, NULL, 8);		//size of the file
			int blocks = size / 512;						//number of full 512 data blocks to read
			int remainder = size % 512;						//remaining x blocks to read
	
			errno = 0;
			if (!(file = fopen(header.name, "wb"))){		//create and open file with name: header.name
				perror("fopen");
				exit(EXIT_FAILURE);
			}
			char buffer[512];
			for (int i = 0; i < blocks; i++){				
				fread(buffer, 1, 512, archive);				//read 512 bytes of data into buffer
				fwrite(buffer, 1, 512, file);				//write 512 bytes of data from buffer
			}
			fread(buffer, 1, remainder, archive);			//read remaining x bytes to buffer
			fwrite(buffer, 1, remainder, file);				//write remaining x bytes from buffer
			fclose(file);									//close file (needed for chown and chmod to work)

			int offset = 512 - remainder;
			if (offset != 512){
				fseek(archive, offset, SEEK_CUR);			//place cursor at next byte which is a multiple
			}												//of 512

			chown(header.name, uid, gid);					//change owner
			chmod(header.name, mode);						//change mode
			utime(header.name, &times);						//change last modified time
		}
		printf("Successfully restored: %s\n", header.name);
	}
}

int main(int argc, char *argv[]){
	char *path;
	int flags = 0 | FTW_PHYS;
	int option;
	int tflag;
	int hflag;
	int fflag;
	char *targ;
	char *farg;
	gl_now = time(0);		//record time now to differentiate the archive from other files in backup function

	while ((option = getopt(argc, argv, "ht:f:")) != -1){		//parsing command options
		switch (option){
			case 't':
				tflag = 1;					//time flag
				targ = optarg;					//targ = dateString or filename
				break;
			case 'h':
				hflag = 1;					//help flag
				break;
			case 'f':
				fflag = 1;					//file flag
				farg = optarg;					//farg = archive filename
			case '?':
				if (optopt == 't'){
					printf("    -t {<filename>, <date>}\n");	//reminder how to use -t
					exit(EXIT_FAILURE);
				}
				if (optopt == 'f'){
					printf("    -f {filename}\n");				//reminder how to use -f
					exit(EXIT_FAILURE);
				}		
				break;
			default:
				printf("FATAL ERROR\n");				//shouldn't ever reach here just a safety catch
				exit(EXIT_FAILURE);
		}
	}

	/*	hflag active	
	* Prints the help message into the console and then exits, if the hflag is active the program will terminate
	* here regardless of if other flags are also active, so that the help message cannot print as well as the 
	* regular program output.
	* Runs if '-h' is used in the command line.
	*/
	if (hflag == 1){
		printf("\nUse of ./backup and ./restore\n"
			"    Backup requires one argument and has 3 optional switches to modify the way it runs.\n"
			"    The only required argument is the path of directory where you want the recursive file\n"
			"    walk to begin, this should always be the final argument.\n" 
			"    The 3 switches are -t, -f and -h.\n" 
			"    -t {<filename>, <date>}  -  Specify starting time from which files will be archived\n"
			"        filename: Relative path to a file\n"
			"        date    : A date in the format 'YYYY-MM-DD hh:mm:ss'\n"
			"        path    : A path to the directory where the function will start\n"
			"    -f {filename}            -  Specify the file the program will archive to\n"
			"        filename: New or existing file in the current directory(recommended to end with .tar)\n"
			"    -h                       -  Help message\n\n"
			"    Restore only uses the -f switch to select the archive to unpack and -h for help\n\n");
		exit(EXIT_SUCCESS);
	}

	if (strstr(argv[0], "restore") != NULL){				//if run from symlink, argv[0] is the name of the
		if (tflag == 1){									//link file, 'restore' in this case.
			printf("-t switch not required for restore, use -h for help\n");
			exit(EXIT_FAILURE);
		}

		else if (fflag == 1){
			if (!(archive = fopen(farg, "rb"))){
				perror("fopen -f");
				printf("%s\n", farg);
				exit(EXIT_FAILURE);
			}
			printf("Archive opened successfully: %s\n", farg);
			if (restore() == 1){								//if restore runs successfully
				printf("Done\n");
				exit(EXIT_SUCCESS);
			}
		}
		else {
			printf("No archive file specified with -f switch, use -h for help\n");
		}

		exit(EXIT_FAILURE);	
	}

	if (argv[optind] != NULL){					//checks if there is a final argument and treats it as the last
		errno = 0;
		if (path = realpath(argv[optind], path), path == NULL){
			perror("realpath");
			printf("Couldn't resolve relative path, maybe try the absolute path\n");
			free(path);
			exit(EXIT_FAILURE);				//takes a path and resolves it to an absolute path if relative
		}
	
		int pathLength = strlen(path);				//total length of absolute path to file
		char temp[PATH_MAX];
		strcpy(temp, path);
		char *lastToken = NULL;
		char *token = strtok(temp, "/\n");
		while (token){
			lastToken = token;						//lastToken = name of the directory to start from
			token = strtok(NULL, "/\n");
		}
		gl_pathOffset = pathLength - strlen(lastToken); 	//length of path not including the last dir name
	}
	else {									//else prints an error and exits (no path given)
		printf("error: No path given to start the function in, use -h for help\n");
		exit(EXIT_FAILURE);
	}
		
	/*	fflag active
	* Opens or creates and opens a file to archive the selected files in.
	* If '-f' isn't used in the command line a default file is created called backup_{date}
	*/
	if (fflag == 1){
		if (!(archive = fopen(farg, "wb"))){			//if file creation unsuccessful
			perror("fopen -f");
			printf("%s\n", farg);
			exit(EXIT_FAILURE);
		}
		printf("File opened successfully: %s\n", farg);
	}
	else {												//create a default file name and opens it
		struct tm *tmNow = localtime(&gl_now);
		char defName[40];
		strftime(defName, 40, "backup_%Y-%m-%d_%H-%M-%S.tar", tmNow);	//name is in this format with that date
		if (!(archive = fopen(defName, "wb"))){
			perror("fopen");
			printf("An internal error occurred, please specify a filename with -f or retry\n");
			exit(EXIT_FAILURE);
		}
		farg = &defName[0];								//make farg = default archive file name for easier
		printf("Default file used: ./%s\n", defName);	//cleanup ( remove(farg); )
	}

	/*	tflag active
	* Sets cut off date for selection of files to be backed up to last modified date if a file or the provided
	* date if a string in the required format is provided.
	* Runs if '-t' is used correctly in the command line.
	*/
	if (tflag == 1){
		if (((targ[4] & targ[7]) == '-') && (targ[13] & targ[16]) == ':'){	//checking string for date format
			struct tm tm;
			if (strptime(targ, "%Y-%m-%d %H:%M:%S", &tm) == NULL){
				printf("strptime: Date format not recognised\n");
				remove(farg);					//delete corrupted or unfinished archive file silently
				exit(EXIT_FAILURE);
			}
			gl_startDate = mktime(&tm);			//sets global variable gl_startDate
		}
		else {										//if not in date format assume it's a file
			struct stat sb;
			if (stat(targ, &sb) == -1){			//this will fail if the file doesn't exist, this also means
				perror("stat -t");				//that it catches when the date format is slightly wrong
				printf("%s\n", targ);
				remove(farg);					//delete corrupted or unfinished archive file silently
				exit(EXIT_FAILURE);
			}
			gl_startDate = sb.st_mtime;			//sets global variable time_t for use in 'backup' function
		}	
	}

	if (nftw(path, backup, 20, 0) == 0){	//start file tree walk, running the backup function for every file
		printf("Done\n");
	}
	else {
		perror("ntfw");
		printf("%s\n", argv[optind]);
		remove(farg);					//delete corrupted or unfinished archive file silently
		exit(EXIT_FAILURE);
	}

	fpad(archive, 1024);					//pad the archive with two tar headers worth of empty space
	fclose(archive);
	free(path);								//realpath() function allocates memory that needs to be freed
	exit(EXIT_SUCCESS);
}
