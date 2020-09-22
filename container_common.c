/* Copyright (C) Teemu Suutari */

#include "container_private.h"

int container_simpleRead(void *dest,uint32_t length,uint32_t offset,struct container_state *container)
{
	int ret=container->readFunc(dest,length,offset,container->readContext);
	if (ret<0) return ret;
	if (ret!=length) return CONTAINER_ERROR_INVALID_FORMAT;
	return ret;
}

static int container_isLeapYear(uint32_t year)
{
	return (!(year&3U)&&(year%100)?1:!(year%400));
}

static const uint16_t container_daysPerMonthAccum[12]={0,31,59,90,120,151,181,212,243,273,304,334};

uint32_t container_dosTimeToAmiga(uint32_t timestamp,uint32_t *minutesSince,uint32_t *ticksSince)
{
	uint32_t seconds,minutes,hours,day,month,year;
	uint32_t daysSinceEpoch;

	seconds=(timestamp&31)<<1;
	minutes=(timestamp>>5)&63;
	hours=(timestamp>>11)&31;
	day=(timestamp>>16)&31;
	month=(timestamp>>21)&15;
	year=((timestamp>>25)&0x7f)+1980;

	if (seconds>=60U||minutes>=60U||hours>=60U||!day||day>31||!month||month>12)
	{
		*minutesSince=0;
		*ticksSince=0;
		return 0;
	}
#if 0
	printf("time %u.%u.%u %u:%u:%u\n",year,month,day,hours,minutes,seconds);
#endif
	/* days since 1978.1.1 */
	daysSinceEpoch=day-1+container_daysPerMonthAccum[month-1];
	if (month>2&&container_isLeapYear(year)) daysSinceEpoch++;
	/* this is slow. could be optimized, but fortunately does not really matter since the max is 127+2+1980... */
	daysSinceEpoch+=(year-1978)*365;
	year=(year+3)&~3U;
	while (year>1980)
	{
		year-=4;
		if (container_isLeapYear(year))
			daysSinceEpoch++;
	}
	*minutesSince=hours*60+minutes;
	*ticksSince=seconds*50;

	return daysSinceEpoch;
}

void container_sortAndInsert(struct container_state *container,struct container_cached_file_entry *entry)
{
	struct container_cached_file_entry *pos;
	const char *name1,*name2;

	/* Assumption for optimization. The files are in alphabetical order already */
	pos=container->lastEntry;
	if (!pos)
	{
		container->firstEntry=entry;
		container->lastEntry=entry;
		return;
	}

	/* now compare until we find the correct spot */
	while (pos)
	{
		/* string compare, but with both path + filename, shorter is before longer */
		name1=pos->filename;
		name2=entry->filename;

		while (*name1&&*name1==*name2)
		{
			name1++;
			name2++;
		}
		if (*name1<=*name2)
		{
			entry->next=pos->next;
			if (pos->next) pos->next->prev=entry;
				else container->lastEntry=entry;

			pos->next=entry;
			entry->prev=pos;
			return;
		}
		pos=pos->prev;
	}

	entry->next=container->firstEntry;
	container->firstEntry->prev=entry;
	container->firstEntry=entry;
}

static const char *container_sanitizePath(const char *name,uint32_t *length)
{
	uint32_t i;

	while (*name&&*name=='/') name++;
	for (i=0;name[i];i++);
	while (i&&name[i-1]=='/') i--;
	*length=i;
	return name;
}

/*
	returns -1 when no match
	returns 0 when exact entry match i.e. 'a/b' -> 'a/b'
	returns 1 when (implied) directory entry 'a/b' -> 'a/b/c' or 'a/b/' -> 'a/b/c
	returns 2 when directory structure too deep to be considered into direct parent dir 'a/b' -> 'a/b/c/d'
*/

static int container_isMatchingEntry(const struct container_cached_file_entry *entry,const char *name,uint32_t nameLength,uint32_t *matchedLength)
{
	uint32_t originalNameLength;
	const char *entryName;

	originalNameLength=nameLength;
	entryName=entry->filename;
	while (*entryName&&*entryName==*name&&nameLength)
	{
		entryName++;
		name++;
		nameLength--;
	}
	if (!nameLength||!*name)
	{
		*matchedLength=originalNameLength-nameLength;
		if (!*entryName) return 0;
		if (!*matchedLength)
		{
			while (*entryName) if(*(entryName++)=='/') return 2;
			return 1;
		}
		if (*entryName=='/')
		{
			entryName++;
			while (*entryName) if(*(entryName++)=='/') return 2;
			return 1;
		}
	}
	*matchedLength=0;
	return -1;
}

static const struct container_cached_file_entry *container_findFirstEntry(struct container_state *container,const char *name,uint32_t nameLength,uint32_t *matchedLength,int *passAsDirectory)
{
	uint32_t tmp;
	int compare;
	struct container_cached_file_entry *entry;

	entry=container->firstEntry;
	while (entry)
	{
		compare=container_isMatchingEntry(entry,name,nameLength,&tmp);
		if (!compare)
		{
			*passAsDirectory=0;
			*matchedLength=tmp;
			return entry;
		}

		if (compare>=1)
		{
			*passAsDirectory=1;
			*matchedLength=tmp;
			return entry;
		}
		entry=entry->next;
	}
	*passAsDirectory=0;
	*matchedLength=0;
	return 0;
}

static const struct container_cached_file_entry *container_findNextEntry(struct container_state *container,const struct container_cached_file_entry *entry,uint32_t nameLength,int *passAsDirectory)
{
	uint32_t dummy;
	int compare;
	const struct container_cached_file_entry *originalEntry;
	const char *name1,*name2;

	originalEntry=entry;
	while (entry)
	{
		compare=container_isMatchingEntry(entry,originalEntry->filename,nameLength,&dummy);
		if (compare==1)
		{
			*passAsDirectory=0;
			return entry;
		}
		if (compare==2)
		{
			if (!entry->prev)
			{
				*passAsDirectory=1;
				return entry;
			}
			name1=entry->prev->filename+nameLength+1;
			name2=entry->filename+nameLength+1;
			while (*name1&&*name2&&(*name1!='/'||*name2!='/'))
			{
				if (*name1!=*name2)
				{
					*passAsDirectory=1;
					return entry;
				}
				name1++;
				name2++;
			}
		}
		entry=entry->next;
	}
	return 0;
}

/*
   FIB functions must be run on a Big Endian machine for the results to be really meaningful
   Otherwise useful only for testing
*/

static void container_copyFirstName(struct FIB *dest,const struct container_cached_file_entry *entry,uint32_t nameLength)
{
	const char *name;
	uint32_t i,originalNameLength;

	originalNameLength=nameLength;
	name=entry->filename+nameLength;
	while (nameLength)
	{
		if (name[-1]=='/') break;
		name--;
		nameLength--;
	}
	nameLength=originalNameLength-nameLength;

	for (i=0;i<107&&i<nameLength;i++)
		dest->filename[i]=name[i];
	dest->filename[i]=0;
}

static void container_copyNextName(struct FIB *dest,const struct container_cached_file_entry *entry,uint32_t nameLength)
{
	const char *name;
	uint32_t i;

	name=entry->filename+nameLength;
	if (*name=='/') name++;
	for (i=0;i<107&&name[i]&&name[i]!='/';i++)
		dest->filename[i]=name[i];
	dest->filename[i]=0;
}

static void container_createFIB(struct FIB *dest,const struct container_cached_file_entry *entry,uint32_t nameLength)
{
	uint32_t i;

	dest->diskKey=0;
	dest->dirEntryType=entry->fileType;
	container_copyFirstName(dest,entry,nameLength);
	dest->protection=entry->protection;
	dest->entryType=entry->fileType;
	dest->size=entry->length;
	dest->blocks=(entry->length+511)&~511U;
	dest->mtimeDays=entry->mtimeDays;
	dest->mtimeMinutes=entry->mtimeMinutes;
	dest->mtimeTicks=entry->mtimeTicks;
	for (i=0;i<79&&entry->filenote[i];i++)
		dest->comment[i]=entry->filenote[i];
	dest->comment[i]=0;
	dest->uid=0;
	dest->gid=0;
}

static void container_createDefaultFIB(struct FIB *dest,const struct container_cached_file_entry *base,uint32_t nameLength)
{
	dest->diskKey=0;
	dest->dirEntryType=CONTAINER_TYPE_DIR;
	dest->protection=0;
	dest->entryType=CONTAINER_TYPE_DIR;
	dest->size=0;
	dest->blocks=0;
	dest->mtimeDays=base->mtimeDays;
	dest->mtimeMinutes=base->mtimeMinutes;
	dest->mtimeTicks=base->mtimeTicks;
	*dest->comment=0;
	dest->uid=0;
	dest->gid=0;
}

/* --- */

int container_common_getFileSize(struct container_state *container,const char *name)
{
	uint32_t nameLength,matchedLength;
	int passAsDirectory;
	const struct container_cached_file_entry *entry;

	name=container_sanitizePath(name,&nameLength);
	entry=container_findFirstEntry(container,name,nameLength,&matchedLength,&passAsDirectory);
	if (!entry) return CONTAINER_ERROR_FILE_NOT_FOUND;
	return entry->length;
}

int container_common_isCompressed(struct container_state *container,const char *name)
{
	uint32_t nameLength,matchedLength;
	int passAsDirectory;
	const struct container_cached_file_entry *entry;

	name=container_sanitizePath(name,&nameLength);
	entry=container_findFirstEntry(container,name,nameLength,&matchedLength,&passAsDirectory);
	if (!entry) return CONTAINER_ERROR_FILE_NOT_FOUND;
	return !!entry->dataType;
}

const struct container_cached_file_entry *container_common_findEntry(struct container_state *container,const char *name)
{
	uint32_t nameLength,matchedLength;
	int passAsDirectory;
	const struct container_cached_file_entry *entry;
	
	name=container_sanitizePath(name,&nameLength);
	entry=container_findFirstEntry(container,name,nameLength,&matchedLength,&passAsDirectory);
	if (!passAsDirectory)
		return entry;
	return 0;
}

int container_common_examine(struct FIB *dest_fib,struct container_examine_state *examine_state,const char *name)
{
	uint32_t nameLength,matchedLength;
	int passAsDirectory;
	struct container_state *container;
	const struct container_cached_file_entry *entry;
	
	container=examine_state->container;
	name=container_sanitizePath(name,&nameLength);
	entry=container_findFirstEntry(container,name,nameLength,&matchedLength,&passAsDirectory);
	examine_state->current=entry;
	examine_state->matchedLength=matchedLength;
	if (!entry)
		return CONTAINER_ERROR_FILE_NOT_FOUND;

	if (passAsDirectory)
	{
		container_createDefaultFIB(dest_fib,entry,matchedLength);
		container_copyFirstName(dest_fib,entry,matchedLength);
	} else {
		container_createFIB(dest_fib,entry,matchedLength);
		container_copyFirstName(dest_fib,entry,matchedLength);
		examine_state->current=entry->next;
	}
	return 0;
}

int container_common_exnext(struct FIB *dest_fib,struct container_examine_state *examine_state)
{
	int passAsDirectory;
	struct container_state *container;	
	const struct container_cached_file_entry *entry;
	
	container=examine_state->container;
	entry=examine_state->current;
	if (!entry)
		return CONTAINER_ERROR_END_OF_ENTRIES;

	entry=container_findNextEntry(container,entry,examine_state->matchedLength,&passAsDirectory);
	if (!entry)
	{
		examine_state->current=0;
		return CONTAINER_ERROR_END_OF_ENTRIES;
	}
	if (passAsDirectory)
	{
		container_createDefaultFIB(dest_fib,entry,examine_state->matchedLength);
		container_copyNextName(dest_fib,entry,examine_state->matchedLength);
	} else {
		container_createFIB(dest_fib,entry,examine_state->matchedLength);
		container_copyNextName(dest_fib,entry,examine_state->matchedLength);
	}
	examine_state->current=entry->next;
	return 0;
}
