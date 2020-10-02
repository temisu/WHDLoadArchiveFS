/* Copyright (C) Teemu Suutari */

#include "container_api.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int read_func(void *dest,uint32_t length,uint32_t offset,void *context)
{
	int fd=(int)context;
	int ret=lseek(fd,offset,SEEK_SET);
	if (ret==-1)
	{
#if 0
		printf("system read 0x%x bytes at offset 0x%x, return 0x%x\n",length,offset,ret);
#endif
		return -123;
	}
 	ret=read(fd,dest,length);
#if 0
	printf("system read 0x%x bytes at offset 0x%x, return 0x%x\n",length,offset,ret);
#endif
	if (ret==-1) return -123;
 	return ret;
}

#define readMax 10	// max files to read
#define skipMax 20	// max files to skip, them terminate
#define fibMax 30	// max entries to process

int fileCount = 0;
void files[readMax];	// files read
int fibCount = 0;

/*
	read some files
	skip some files
	then terminate
*/
static int whd_allocFile(const char *name, uint32_t length) {
	void *mem;
	if (fileCount < readMax) {
		mem = malloc(length);
		// check error...
		files[fileCount] = mem;
	} else if (fileCount < skipMax) {
		mem = -1;
	} else {
		mem = NULL;
	}
	printf("whd_allocFile: num=%d name=%s len=%d rc=%x\n");
	fileCount++;
}

/*
	retrieve some fibs then terminate
*/
static int whd_registerEntry(const char *path, const FIB *fib) {
	if (fibCount < fibMax) {
		printFIB(fib);
		fibCount++;
		return -1;
	} else {
		return 0;
	}
}

struct FIB
{
	uint32_t				diskKey;
	uint32_t				dirEntryType;
	uint8_t					filename[108];
	uint32_t				protection;
	uint32_t				entryType;
	uint32_t				size;
	uint32_t				blocks;
	uint32_t				mtimeDays;
	uint32_t				mtimeMinutes;
	uint32_t				mtimeTicks;
	uint8_t					comment[80];
	uint16_t				uid;
	uint16_t				gid;
};

/*
   24 name
   8 size or 'Dir'
   space
   protection 'hsparwed'
   space
   Timestamp DD-MMM-YY HH:MM::SS
   next line
   : comment
*/

static const char protectionLetters[8]="dewrapsh";
static const char *monthNames[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

void printFIB(struct FIB *fib)
{
	int i,len=strlen((char*)fib->filename);
	printf("%s",fib->filename);
	while (len++<24) printf(" ");
	if (fib->dirEntryType==2) printf("    Dir ");
		else printf("%7u ",fib->size);
	for (i=7;i>=0;i--)
		printf("%c",((fib->protection^0xf)&(1<<i))?protectionLetters[i]:'-');

	time_t time=(fib->mtimeDays+2922)*86400+fib->mtimeMinutes*60+fib->mtimeTicks/50;
	struct tm *tm=gmtime(&time);
	printf(" %02u-%s-%02u %02u:%02u:%02u\n",tm->tm_mday,monthNames[tm->tm_mon],tm->tm_year%100,tm->tm_hour,tm->tm_min,tm->tm_sec);
	if (*fib->comment)
		printf(": %s\n",fib->comment);
}

int processDirectory(void *container,const char *name)
{
	struct FIB fib;
	int ret,pos,subDirPos;
	/* quick and dirty hack*/
	char subDirs[4096];

	subDirPos=0;
	ret=container_examine(&fib,container,name);
	if (ret)
	{
		printf("Failure to open directory '%s'\n",name);
		return ret;
	}
	if (fib.dirEntryType==2)
	{
		printf("Directory \"%s\" (\"%s\")\n",name,fib.filename);
		while (!ret)
		{
			ret=container_exnext(&fib,container);
			if (!ret)
			{
				printFIB(&fib);
				if (fib.dirEntryType==2)
				{
					if (*name) sprintf(subDirs+subDirPos,"%s/%s",name,fib.filename);
						else sprintf(subDirs+subDirPos,"%s",fib.filename);
					subDirPos+=strlen(subDirs+subDirPos)+1;
				}
			}
		}
		if (ret==CONTAINER_ERROR_END_OF_ENTRIES) ret=0;
	} else {
		printf("'%s' is not a directory\n",name);
		return -1;
	}
	if (!ret)
	{
		pos=0;
		while (pos!=subDirPos&&!ret)
		{
			ret=processDirectory(container,subDirs+pos);
			pos+=strlen(subDirs+pos)+1;
		}
	}
	return ret;
}



int main(int argc,char **argv)
{
	if (argc!=2)
	{
		printf("no (single) archive file defined as a command line parameter\n");
		return 0;
	}

	int fd=open(argv[1],O_RDONLY);
	if (fd<0)
	{
		printf("unable to open archive %s\n",argv[1]);
	}
	struct stat st;
	int ret=fstat(fd,&st);
	if (ret<0)
	{
		printf("unable to stat archive %s\n",argv[1]);
	}

	void *container;
	ret=container_initialize(&container,argv[1]);
	if (ret) printf("container_initialize failed with code %d\n",ret);
	if (!ret)
	{
		printf("\ncontainer '%s' opened...\n\n",argv[1]);

		printf("-------------------------------------------------------------------------------\n");

		printf("container_fileCache:\n");
		ret = container_fileCache(container,&whd_allocFile);
		printf("container_fileCache retunred error code %d\n",ret);
		while (fileCount) {
			free(files[fileCount--]);
		}

		printf("-------------------------------------------------------------------------------\n");

		printf("container_examine:\n");
		ret = container_examine(container,&whd_registerEntry);
		printf("container_examine retunred error code %d\n",ret);

		printf("-------------------------------------------------------------------------------\n");

		/*
		would be useful the have a test for container_getFileSize and container_fileRead too
		maybe one case with an invalid filename and second with a filename saved from the container_examine call
		*/

		container_uninitialize(container);
	}
	close(fd);
	return 0;
}
