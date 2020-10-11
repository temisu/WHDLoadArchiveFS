/* Copyright (C) Teemu Suutari */

#include "container_api.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

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

static const char protectionLetters[8]="dewrapsh";
static const char *monthNames[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static void printFIB(const struct FIB *fib)
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

static int test_registerFunc(const char *path,const void *fib)
{
	printf("Directory '%s'\n",path);
	printFIB(fib);
	return -1;
}

/* just a hack */
static int allocatedFileCount=0;
static void *allocatedFiles[1000];
static const char *allocatedFilenames[1000];
static void *container;

static void *test_allocFunc(const char *name,uint32_t length)
{
	void *ret;
	uint32_t verify;

	/* test that different part of the APIs return same info */
	verify=container_getFileSize(container,name);
	if (verify!=length)
	{
		printf("container_getFileSize returned wrong answer %d (should be %d)\n",verify,length);
	}

	printf("File '%s', Length %u\n",name,length);
	ret=malloc(length);
	allocatedFilenames[allocatedFileCount]=name;
	allocatedFiles[allocatedFileCount++]=ret;
	return ret;
}

int main(int argc,char **argv)
{
	int ret,i,j,length;
	void *verify;

	if (argc!=2)
	{
		printf("no (single) archive file defined as a command line parameter\n");
		return 0;
	}


	ret=container_initialize(&container,argv[1]);
	if (ret) printf("container_initialize failed with code %d\n",ret);
	if (!ret)
	{
		printf("\ncontainer '%s' opened...\n\n",argv[1]);

		printf("-------------------------------------------------------------------------------\n");
		printf("fileCache:\n");
		ret=container_fileCache(container,test_allocFunc);
		if (ret) printf("container_fileCache failed with code %d\n",ret);
		for (i=0;i<allocatedFileCount;i++)
		{
			/* we checked for errros once. should do it again ... but this is jsut a test */
			length=container_getFileSize(container,allocatedFilenames[i]);
			verify=malloc(length);
			ret=container_fileRead(container,verify,allocatedFilenames[i],length,0);
			if (ret!=length) printf("container_fileRead failed with code %d\n",ret);
			for (j=0;j<length;j++)
			{
				if (((char*)verify)[j]!=((char*)allocatedFiles[i])[j])
				{
					printf("verify failed for file %s\n",allocatedFilenames[i]);
					break;
				}
			}
			free(verify);
			free(allocatedFiles[i]);
		}

		printf("-------------------------------------------------------------------------------\n");
		printf("Examine:\n");
		ret=container_examine(container,test_registerFunc);
		if (ret) printf("container_examine failed with code %d\n",ret);

		printf("-------------------------------------------------------------------------------\n");
		container_uninitialize(container);
	}
	return 0;
}
