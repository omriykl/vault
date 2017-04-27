#define BORDERS_SIZE 16
#define LEFT_BORDER "<<<<<<<<"
#define RIGHT_BORDER ">>>>>>>>"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h> // for time measurement
#include <sys/time.h>

struct fileMetaData
{
	char name[256];
	ssize_t size;
	mode_t protection;
	time_t insertion;
	off_t block1;
	ssize_t block1Len;
	off_t block2;
	ssize_t block2Len;
	off_t block3;
	ssize_t block3Len;

};

struct catalog
{
	ssize_t totalSize;
	ssize_t availableSpace;
	ssize_t maxDataSize;
	time_t created;
	time_t lastMod;
	short numOfFiles;
	struct fileMetaData* files;
};

struct offsetsAndSizes
{
	ssize_t* blocks;
	off_t* offsets;
};



int createVault(char* filename,ssize_t sizeInBytes)
{
	struct catalog *c;
	struct fileMetaData *files;
	struct timeval tv;
	char* data;
	ssize_t dataSize;
	int fd;

	c = (struct catalog *) malloc(sizeof(struct catalog));
	files = (struct fileMetaData *) calloc(sizeof(struct fileMetaData),100);

	gettimeofday(&tv, NULL);

	c->created = tv.tv_sec;
	c->lastMod = tv.tv_sec;
	c->numOfFiles = 0;
	c->totalSize = sizeInBytes;
	c->files = files;

	dataSize = sizeInBytes - sizeof(struct catalog) - sizeof(struct fileMetaData)*100;

	if (dataSize <=0)
	{
		printf("Desired size is too small!\n");
		return -1;
	}

	c->availableSpace = dataSize;
	c->maxDataSize = dataSize;

	data = (char*) malloc(dataSize);

	fd = open(filename,O_RDWR|O_CREAT|O_TRUNC,0644);
	if (fd < 0)
	{
		printf("Error opening file: %s\n", strerror(errno));
		return -1;
	}

	//writing the catalog data
	if (write(fd,c,sizeof(struct catalog)) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}
	free(c);

	//writing the fileTable
	if (write(fd,files,sizeof(struct fileMetaData)*100) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}
	free(files);

	//writing the data
	if (write(fd,data,dataSize) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}
	free(data);

	close(fd);

	return 1;
}

int getVaultFromFile(char* filename, struct catalog ** cat)
{
	int fd;
	struct fileMetaData *files;
	int i,r;

	fd = open(filename,O_RDONLY);

	if (fd < 0)
	{
		printf("Error opening file: %s\n", strerror(errno));
		return -1;
	}

	*cat = (struct catalog *) malloc(sizeof(struct catalog));
	files = (struct fileMetaData *) malloc((sizeof(struct fileMetaData))*100);

	if (read(fd,*cat,sizeof(struct catalog)) < sizeof(struct catalog))
	{
		printf("Error reading from file: %s\n", strerror(errno));
		return -1;
	}

	for (i=0;i<100;i++)
	{
		if (i<=10)
			lseek(fd,sizeof(struct catalog) + sizeof(struct fileMetaData)*i,SEEK_SET);
		else
			lseek(fd,sizeof(struct catalog) + sizeof(struct fileMetaData)*i,SEEK_SET);
		r = read(fd,files+(i*1),(sizeof(struct fileMetaData)*1));
		if (r < 0)
		{
			printf("Error reading from file: %s\n", strerror(errno));
			return -1;
		}
	}

	(*cat)->files = files;

//	dataSize = (*cat)->totalSize - sizeof(struct catalog) - sizeof(struct fileMetaData)*100;
//	*data = (char*) malloc(dataSize);
//
//	r = read(fd,*data,dataSize);
//	if (r < 0)
//	{
//		printf("Error reading from file: %s\n", strerror(errno));
//		return -1;
//	}

	close(fd);

	return 1;
}

void lowerStr(char** str)
{
	int i;
	for(i=0;i< strlen(*str); i++)
		(*str)[i] = tolower((*str)[i]);
}

/**
 * returns the number of bytes if the str is valid, otherwise returns -1
 */
long long getBytesFromStr(char* str)
{
	int i=0;
	long long mul,size,totalSize;
	char* sizeOfBuffer;

	while (str[i] != '\0')
	{
		if (isdigit(str[i]))
			i++;
		else
			break;
	}
	if (i==0) // no digits at all
		return -1;
	if (str[i+1] == '\0')
	{
		switch (str[i])
		{
			case 'G':
			case 'g':
				mul = 1024*1024*1024;
				break;
			case 'M':
			case 'm':
				mul = 1024*1024;
				break;
			case 'K':
			case 'k':
				mul = 1024;
				break;
			case 'B':
			case 'b':
				mul=1;
				break;
			default:
				return -1;
		}
		sizeOfBuffer = (char*) malloc(i+1);
		if (sizeOfBuffer == NULL)
		{
			printf("Error allocating: %s\n", strerror(errno));
				return -1;
		}
		strncpy(sizeOfBuffer,str,i);
		size = atol(sizeOfBuffer);
		free(sizeOfBuffer);
		totalSize = size*mul;
		return totalSize;
	}
	else
		return -1;
}

void addToPQ(ssize_t** blocks,off_t** offsets,ssize_t newVal, off_t newOff)
{
	if ((*blocks)[0] <= newVal)
	{
		(*blocks)[2] = (*blocks)[1];
		(*blocks)[1] = (*blocks)[0];
		(*blocks)[0] = newVal;

		(*offsets)[2] = (*offsets)[1];
		(*offsets)[1] = (*offsets)[0];
		(*offsets)[0] = newOff;
	}
	else if ((*blocks)[1] <= newVal)
	{
		(*blocks)[2] = (*blocks)[1];
		(*blocks)[1] = newVal;

		(*offsets)[2] = (*offsets)[1];
		(*offsets)[1] = newOff;
	}
	else if ((*blocks)[2] <= newVal)
	{
		(*blocks)[2] = newVal;

		(*offsets)[2] = newOff;
	}
}

struct offsetsAndSizes findSpace(char* vaultFile,struct stat fileStat,struct catalog* cat)
{
	int vfd,cur=0,len,i,j,inFile=0;
	char buffer[1024];
	off_t offset = 0;
	//TODO: check allocation
	ssize_t* blocks = (ssize_t*)calloc(3, sizeof(ssize_t));
	off_t* offsets = (off_t*)calloc(3, sizeof(off_t));
	offsets[0]= -1;
	offsets[1]= -1;
	offsets[2]= -1;

	struct offsetsAndSizes offAndSizes;
	offAndSizes.blocks = blocks;
	offAndSizes.offsets = offsets;


	vfd = open(vaultFile,O_RDONLY);
	if (vfd < 0)
	{
		printf("Error opening file: %s\n", strerror(errno));
		return offAndSizes;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100,SEEK_SET);

	while (cur < cat->maxDataSize)
	{
		len = read(vfd,buffer,1024);
		if (len < 0)
			return offAndSizes;
		for (i=0;i<len;i++)
		{
			if (inFile==0)
			{
				if (buffer[i] == '<')
				{
					for (j=i+1;j<i+8;j++)
					{
						if (buffer[j] != '<')
							break;
					}
					if (j == i+8)
					{
						addToPQ(&blocks,&offsets,cur-offset,offset);
						if (blocks[0] >= fileStat.st_size+BORDERS_SIZE)
						{
							offsets[1] = -1;
							offsets[2] = -1;

							close(vfd);
							return offAndSizes;
						}
						inFile=1;
					}
				}
			}
			else
			{
				if (buffer[i] == '>')
				{
					for (j=i+1;j<i+8;j++)
					{
						if (buffer[j] != '>')
							break;
					}
					if (j == i+8)
					{
						offset = cur+8;
						inFile=0;
						cur+=7;
						i+=7;
					}
				}
			}

			cur++;
		}
	}
	close(vfd);

	if (inFile == 0)
		addToPQ(&blocks,&offsets,cur-offset,offset);

	if (blocks[0] >= fileStat.st_size+BORDERS_SIZE)
	{
		offsets[1] = -1;
		offsets[2] = -1;
		return offAndSizes;
	}
	else if (blocks[0] + blocks[1] >= fileStat.st_size+(BORDERS_SIZE)*2)
	{
		offsets[2] = -1;
		return offAndSizes;
	}
	else if (blocks[0] + blocks[1] + blocks[3] >= fileStat.st_size+(BORDERS_SIZE)*3)
	{
		return offAndSizes;
	}

	offsets[0] = -1;
	offsets[1] = -1;
	offsets[2] = -1;

	return offAndSizes;
}

int addFile(char* vaultFile,struct catalog* c, char* fileToAdd)
{

	struct offsetsAndSizes offAndSizes;
	struct stat fileStat;
	int i;

	if (stat(fileToAdd,&fileStat) < 0)
	{
		printf("error reading file %s\n",fileToAdd);
		return -1;
	}
	if (fileStat.st_size + BORDERS_SIZE > c->availableSpace)
	{
		printf("No free space available.\n");
		return -1;
	}
	if (c->numOfFiles >=100)
	{
		printf("No free space available.\n");
		return -1;
	}

	for (i=0;i<c->numOfFiles;i++)
	{
		if (strcmp(c->files[i].name,basename(fileToAdd))== 0)
		{
			printf("File already exists!\n");
			return -1;
		}
	}

	offAndSizes = findSpace(vaultFile,fileStat,c);

	//printf("[%ld,%ld,%ld]\n",offAndSizes.offsets[0],offAndSizes.offsets[1],offAndSizes.offsets[2]);
	//printf("[%ld,%ld,%ld]\n",offAndSizes.blocks[0],offAndSizes.blocks[1],offAndSizes.blocks[2]);

	if (offAndSizes.offsets[0] >= 0)
	{
		if (insertData(vaultFile,c,fileToAdd,offAndSizes,fileStat) < 0)
			return -1;
		return 0;
	}

	printf("cant insert the file in less than 4 parts!\n");

	return -1;
}

int insertData(char* vaultFile,struct catalog* c, char* fileToAdd, struct offsetsAndSizes offAndSizes, struct stat fileStat)
{
	struct timeval tv;
	int fd,vfd,numOfBlocks = 1;
	ssize_t readBytes = 0;
	ssize_t bufferSize = 1024;
	ssize_t blockSize,blockSize1,blockSize2;
	char buffer[1024];
	int len;

	fd = open(fileToAdd,O_RDONLY);
	if (fd < 0)
	{
		printf("Error opening file: %s\n", strerror(errno));
		return -1;
	}

	vfd = open(vaultFile,O_RDWR);
	if (vfd < 0)
	{
		printf("Error opening file: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[0],SEEK_SET);

	if (write(vfd,LEFT_BORDER,8) < 0)
	{
		return -1;
	}

	if (offAndSizes.offsets[1] == -1 && offAndSizes.offsets[2] == -1)
		blockSize = fileStat.st_size;
	else
		blockSize = offAndSizes.blocks[0] - BORDERS_SIZE;

	while (readBytes < blockSize)
	{
		if (bufferSize > blockSize - readBytes)
			len = read(fd,buffer,blockSize - readBytes);
		else
			len = read(fd,buffer,bufferSize);
		if (len < 0)
		{
			lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[0],SEEK_SET);
			write(vfd,"00000000",8);
			return -1;
		}
		if (write(vfd,buffer,len) < 0)
		{
			lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[0],SEEK_SET);
			write(vfd,"00000000",8);
			return -1;
		}
		readBytes += len;
	}

	if (write(vfd,RIGHT_BORDER,8) < 0)
	{
		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[0],SEEK_SET);
		write(vfd,"00000000",8);
		return -1;
	}

	if (offAndSizes.offsets[1] >= 0)
	{
		numOfBlocks++;
		readBytes=0;

		if (offAndSizes.offsets[2] == -1)
			blockSize1 = fileStat.st_size - blockSize;
		else
			blockSize1 = offAndSizes.blocks[1] - BORDERS_SIZE;

		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[1],SEEK_SET);

		if (write(vfd,LEFT_BORDER,8) < 0)
		{
			return -1;
		}

		while (readBytes < blockSize1)
		{
			if (bufferSize > blockSize1 - readBytes)
				len = read(fd,buffer,blockSize1 - readBytes);
			else
				len = read(fd,buffer,bufferSize);
			if (len < 0)
			{
				lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[1],SEEK_SET);
				write(vfd,"00000000",8);
				return -1;
			}
			if (write(vfd,buffer,len) < 0)
			{
				lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[1],SEEK_SET);
				write(vfd,"00000000",8);
				return -1;
			}
			readBytes += len;
		}

		if (write(vfd,RIGHT_BORDER,8) < 0)
		{
			lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[1],SEEK_SET);
			write(vfd,"00000000",8);
			return -1;
		}
	}

	if (offAndSizes.offsets[2] >= 0)
	{
		numOfBlocks++;

		readBytes=0;

		blockSize2 = fileStat.st_size - blockSize - blockSize1;

		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[2],SEEK_SET);

		if (write(vfd,LEFT_BORDER,8) < 0)
		{
			return -1;
		}

		while (readBytes < blockSize2)
		{
			if (bufferSize > blockSize2 - readBytes)
				len = read(fd,buffer,blockSize2 - readBytes);
			else
				len = read(fd,buffer,bufferSize);
			if (len < 0)
			{
				lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[2],SEEK_SET);
				write(vfd,"00000000",8);
				return -1;
			}
			if (write(vfd,buffer,len) < 0)
			{
				lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[2],SEEK_SET);
				write(vfd,"00000000",8);
				return -1;
			}
			readBytes += len;
		}

		if (write(vfd,RIGHT_BORDER,8) < 0)
		{
			lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + offAndSizes.offsets[2],SEEK_SET);
			write(vfd,"00000000",8);
			return -1;
		}

	}

	gettimeofday(&tv, NULL);

	c->availableSpace -= fileStat.st_size + BORDERS_SIZE*numOfBlocks;
	c->numOfFiles +=1;
	c->lastMod = tv.tv_sec;
	c->files[c->numOfFiles -1].block1 = offAndSizes.offsets[0];
	c->files[c->numOfFiles -1].block1Len = blockSize;
	if (offAndSizes.offsets[1] >=0)
	{
		c->files[c->numOfFiles -1].block2 = offAndSizes.offsets[1];
		c->files[c->numOfFiles -1].block2Len = blockSize1;
	}
	if (offAndSizes.offsets[2] >=0)
	{
		c->files[c->numOfFiles -1].block3 = offAndSizes.offsets[2];
		c->files[c->numOfFiles -1].block3Len = blockSize2;
	}
	c->files[c->numOfFiles -1].insertion = tv.tv_sec;
	strcpy(c->files[c->numOfFiles -1].name,basename(fileToAdd));
	c->files[c->numOfFiles -1].protection = fileStat.st_mode;
	c->files[c->numOfFiles -1].size = fileStat.st_size;

	lseek(vfd,0,SEEK_SET);

	//writing the catalog data
	if (write(vfd,c,sizeof(struct catalog)) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}

	//writing the fileTable
	if (write(vfd,c->files,sizeof(struct fileMetaData)*100) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}

	close(vfd);
	close(fd);

	return 0;

}

int fetchFileFromVault(char* vaultFile, struct fileMetaData file)
{
	int vfd, fd,len;
	ssize_t readBytes = 0;
	ssize_t bufferSize = 1024;
	char buffer[1024];

	vfd = open(vaultFile,O_RDONLY);
	if (vfd < 0)
	{
		printf("Error opening vault file: %s\n", strerror(errno));
		return -1;
	}

	fd = open(file.name,O_RDWR|O_CREAT|O_TRUNC,file.protection);
	if (fd < 0)
	{
		printf("Error opening file to write to: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block1 +(BORDERS_SIZE/2),SEEK_SET);

	while (readBytes < file.block1Len)
	{
		if (file.block1Len - readBytes < bufferSize)
			len = read(vfd,buffer,file.block1Len - readBytes);
		else
			len = read(vfd,buffer,bufferSize);
		if (len < 0)
			return -1;
		if (write(fd,buffer,len) < 0)
			return -1;
		readBytes += len;
	}

	if (file.block2Len > 0)
	{
		readBytes = 0;
		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block2 +(BORDERS_SIZE/2),SEEK_SET);

		while (readBytes < file.block2Len)
		{
			if (file.block2Len - readBytes < bufferSize)
				len = read(vfd,buffer,file.block2Len - readBytes);
			else
				len = read(vfd,buffer,bufferSize);
			if (len < 0)
				return -1;
			if (write(fd,buffer,len) < 0)
				return -1;
			readBytes += len;
		}
	}

	if (file.block3Len > 0)
	{
		readBytes = 0;
		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block3 +(BORDERS_SIZE/2),SEEK_SET);

		while (readBytes < file.block3Len)
		{
			if (file.block3Len - readBytes < bufferSize)
				len = read(vfd,buffer,file.block3Len - readBytes);
			else
				len = read(vfd,buffer,bufferSize);
			if (len < 0)
				return -1;
			if (write(fd,buffer,len) < 0)
				return -1;
			readBytes += len;
		}
	}

	close(fd);
	close(vfd);

	return 0;


}

int deleteFile(char* vaultFile, struct fileMetaData file,int fileIndex, struct catalog* cat)
{
	int i,vfd,numOfBlocks=1;
	struct timeval tv;

	vfd = open(vaultFile,O_RDWR);
	if (vfd < 0)
	{
		printf("Error opening vault file: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block1,SEEK_SET);
	if (write(vfd,"00000000",8) < 0)
	{
		printf("Error writing to vault file: %s\n", strerror(errno));
		return -1;
	}
	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block1+(BORDERS_SIZE/2)+file.block1Len,SEEK_SET);
	if (write(vfd,"00000000",8) < 0)
	{
		printf("Error writing to vault file: %s\n", strerror(errno));
		return -1;
	}

	if (file.block2Len > 0)
	{
		numOfBlocks++;

		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block2,SEEK_SET);
		if (write(vfd,"00000000",8) < 0)
		{
			printf("Error writing to vault file: %s\n", strerror(errno));
			return -1;
		}
		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block2+(BORDERS_SIZE/2)+file.block2Len,SEEK_SET);
		if (write(vfd,"00000000",8) < 0)
		{
			printf("Error writing to vault file: %s\n", strerror(errno));
			return -1;
		}
	}

	if (file.block3Len > 0)
	{
		numOfBlocks++;

		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block3,SEEK_SET);
		if (write(vfd,"00000000",8) < 0)
		{
			printf("Error writing to vault file: %s\n", strerror(errno));
			return -1;
		}
		lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + file.block3+(BORDERS_SIZE/2)+file.block3Len,SEEK_SET);
		if (write(vfd,"00000000",8) < 0)
		{
			printf("Error writing to vault file: %s\n", strerror(errno));
			return -1;
		}
	}


	for (i=fileIndex;i<cat->numOfFiles -1;i++)
	{
		cat->files[i]= cat->files[i+1];
	}

	gettimeofday(&tv, NULL);

	cat->availableSpace += file.size+BORDERS_SIZE*numOfBlocks;
	cat->numOfFiles--;
	cat->lastMod = tv.tv_sec;

	lseek(vfd,0,SEEK_SET);

	//writing the catalog data
	if (write(vfd,cat,sizeof(struct catalog)) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}

	//writing the fileTable
	if (write(vfd,cat->files,sizeof(struct fileMetaData)*100) < 0)
	{
		printf("Error writing to file: %s\n", strerror(errno));
		return -1;
	}

	close(vfd);

	return 0;

}

int printStatus(char* vaultFile, struct catalog* cat)
{
	int vfd,len;
	int startOffset,endOffset,cur=0,i,j,found=0;
	char buffer[1024];
	int gapsSize = 0;
	int filesSize=0;
	float frag = 0.0;

	vfd = open(vaultFile,O_RDONLY);
	if (vfd < 0)
	{
		printf("Error opening vault file: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100,SEEK_SET);

	while (cur < cat->maxDataSize)
	{
		len = read(vfd,buffer,1024);
		if (len < 0)
			return -1;
		for (i=0;i<len;i++)
		{
			if (buffer[i] == '<')
			{
				for (j=i+1;j<i+8;j++)
				{
					if (buffer[j] != '<')
						break;
				}
				if (j == i+8)
				{
					startOffset = cur;
					found=1;
					break;
				}
			}
			cur++;
		}
		if (found == 1)
			break;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + startOffset,SEEK_SET);

	while (cur < cat->maxDataSize)
	{
		len = read(vfd,buffer,1024);
		if (len < 0)
			return -1;
		for (i=0;i<len;i++)
		{
			if (buffer[i] == '>')
			{
				for (j=i+1;j<i+8;j++)
				{
					if (buffer[j] != '>')
						break;
				}
				if (j == i+7)
					endOffset = cur+7;
			}
			cur++;
		}
	}

	filesSize = cat->maxDataSize - cat->availableSpace;
	gapsSize = (endOffset - startOffset) - filesSize;
	frag= gapsSize*1.0 / (endOffset - startOffset);

	//printf("avail space = %ld\n",cat->availableSpace);
	//printf("max len = %ld\n",cat->maxDataSize);

	printf("Number of files:	%d\n",cat->numOfFiles);
	printf("Total size:	%dB\n",filesSize);
	printf("Fragmentation ratio: %.1f\n",frag);
	//printf("first in: %d, last in: %d, total size: %d, total gaps: %d,total size of vault: %d\n",startOffset,endOffset,filesSize,gapsSize,cat->totalSize);

	close(vfd);

	return 0;
}

void printElapsedTime(struct timeval start)
{
	struct timeval end;
	long seconds, useconds;
	double mtime;

	gettimeofday(&end,NULL);

	seconds = end.tv_sec -start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;

	mtime = ((seconds) * 1000.0 + useconds/1000.0);
	printf("Total time took: %.3lf MS\n",mtime);
}

int moveBlock(char* vault, off_t fileStart,off_t fileEnd, off_t lastFile)
{
	int vfd,fd,len;
	char buffer[1024];
	off_t cur = fileStart, readBytes =0;
	off_t blockSize= fileEnd -fileStart+1;

	fileEnd++;

	if (lastFile == 0)
		lastFile = -1;

	vfd = open(vault,O_RDWR);
	if (vfd < 0)
	{
		printf("Error opening vault file: %s\n", strerror(errno));
		return -1;
	}

	fd = open("tmpVault.txt",O_RDWR|O_CREAT|O_TRUNC,0644);
	if (fd < 0)
	{
		printf("Error opening file to write to: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + fileStart,SEEK_SET);

	while (cur < fileEnd)
	{
		if (fileEnd - cur < 1024)
			len = read(vfd,buffer,fileEnd-cur);
		else
			len = read(vfd,buffer,1024);

		printf("the len is: %d, beffer is: %s\n",len,buffer);

		if (len < 0)
			return -1;

		if (write(fd,buffer,len) < 0)
		{
			return -1;
		}
		cur+=len;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + fileStart,SEEK_SET);

	if (write(vfd,"00000000",8) < 0)
	{
		printf("Error writing to vault file: %s\n", strerror(errno));
		return -1;
	}
	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + fileEnd-8,SEEK_SET);
	if (write(vfd,"00000000",8) < 0)
	{
		printf("Error writing to vault file: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + lastFile+1,SEEK_SET);
	lseek(fd,0,SEEK_SET);

	while (readBytes < blockSize)
	{
		if (1024 > blockSize - readBytes)
			len = read(fd,buffer,blockSize - readBytes);
		else
			len = read(fd,buffer,1024);
		if (len < 0)
		{
			return -1;
		}
		if (write(vfd,buffer,len) < 0)
		{
			return -1;
		}
		readBytes += len;
	}

	close(vfd);
	close(fd);

	return 0;

}

int findFileAndmoveBlock(char* vault,struct catalog* cat, off_t fileStart, off_t fileEnd, off_t lastFileEnd)
{
	int i,found=0,vfd;

	for (i=0;i<cat->numOfFiles;i++)
	{
		if (cat->files[i].block1 == fileStart)
		{
			if (moveBlock(vault,fileStart,fileEnd,lastFileEnd) < 0)
			{
				return -1;
			}

			cat->files[i].block1 = (lastFileEnd == 0) ? 0 : lastFileEnd+1;
			found =1;
			break;
		}
		else if (cat->files[i].block2 == fileStart)
		{
			if (moveBlock(vault,fileStart,fileEnd,lastFileEnd) < 0)
			{
				return -1;
			}
			cat->files[i].block2 = (lastFileEnd == 0) ? 0 : lastFileEnd+1;
			found =1;
			break;
		}
		else if (cat->files[i].block3 == fileStart)
		{
			if (moveBlock(vault,fileStart,fileEnd,lastFileEnd) < 0)
			{
				return -1;
			}
			cat->files[i].block3 = (lastFileEnd == 0) ? 0 : lastFileEnd+1;
			found =1;
			break;
		}
	}
	if (found ==1)
	{
		vfd = open(vault,O_RDWR);
		if (vfd < 0)
		{
			printf("Error opening vault file: %s\n", strerror(errno));
			return -1;
		}

		lseek(vfd,0,SEEK_SET);

		//writing the catalog data
		if (write(vfd,cat,sizeof(struct catalog)) < 0)
		{
			printf("Error writing to file: %s\n", strerror(errno));
			return -1;
		}

		//writing the fileTable
		if (write(vfd,cat->files,sizeof(struct fileMetaData)*100) < 0)
		{
			printf("Error writing to file: %s\n", strerror(errno));
			return -1;
		}

		return 0;
	}
	return -1;


}

int defrag(char* vaultFile,struct catalog* cat)
{
	int vfd,len,i,j,inFile = 0;
	off_t cur=0,lastFileEnd=0,moveFileStart=-1,moveFileEnd=-1;
	char buffer[1024];

	vfd = open(vaultFile,O_RDONLY);
	if (vfd < 0)
	{
		printf("Error opening vault file: %s\n", strerror(errno));
		return -1;
	}

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100,SEEK_SET);

	while (cur < cat->maxDataSize)
	{
		len = read(vfd,buffer,1024);
		if (len < 0)
			return -1;

		for (i=0;i<len;i++)
		{
			if (inFile==0)
			{
				if (buffer[i] == '<')
				{
					for (j=i+1;j<i+8;j++)
					{
						if (buffer[j] != '<')
							break;
					}
					if (j == i+8)
					{
						printf("file in %ld\n",cur);
						if (cur - lastFileEnd > 1)
						{
							moveFileStart = cur;
						}
						inFile=1;
					}
				}
			}
			else
			{
				if (buffer[i] == '>')
				{
					for (j=i+1;j<i+8;j++)
					{
						if (buffer[j] != '>')
							break;
					}
					if (j == i+8)
					{
						if (moveFileStart > 0)
						{
							moveFileEnd=cur+7;
							findFileAndmoveBlock(vaultFile,cat,moveFileStart,moveFileEnd,lastFileEnd);
							close(vfd);

							return 1;
						}
						else
						{
							cur+=7;
							i+=7;
						}
						inFile=0;
						lastFileEnd = cur;
						moveFileStart = -1;
					}
				}
			}

			cur++;

		}

	}

	close(vfd);
	return 0;
}

int main(int argc, char** argv)
{
	struct timeval start;

	gettimeofday(&start,NULL);

	if (argc < 3)
	{
		printf("Invalid operation!\n");
		printElapsedTime(start);
		return -1;
	}
	lowerStr(&argv[2]);

	if (strcmp(argv[2],"init") == 0)
	{
		if (argc != 4)
		{
			printf("Invalid parameters for init operation!\n");
			printElapsedTime(start);
			return -1;
		}
		ssize_t size = getBytesFromStr(argv[3]);
		if (size < 0)
		{
			printf("Invalid parameters for init operation!\n");
			printElapsedTime(start);
			return -1;
		}

		if (createVault(argv[1],size) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		printf("Created.\n");
		printElapsedTime(start);
		return 0;
	}
	else if (strcmp(argv[2],"list") == 0)
	{
		struct catalog *cat;
		int i;
		ssize_t fileSize;

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}
		if (cat->numOfFiles == 0)
			printf("The vault is empty.\n");
		else
		{
			for (i=0;i<cat->numOfFiles;i++)
			{
				if (cat->files[i].size < 1024)
				{
					printf("%s	%dB 	%o	%s\n",cat->files[i].name,cat->files[i].size,cat->files[i].protection,ctime(&cat->files[i].insertion));
				}
				else if (cat->files[i].size < 1024*1024)
				{
					printf("%s	%dK 	%o	%s\n",cat->files[i].name,cat->files[i].size /1024,cat->files[i].protection,ctime(&cat->files[i].insertion));
				}
				else if (cat->files[i].size < 1024*1024*1024)
				{
					printf("%s	%dM 	%o	%s\n",cat->files[i].name,cat->files[i].size /1024*1024,cat->files[i].protection,ctime(&cat->files[i].insertion));
				}

			}
		}

		printElapsedTime(start);

		return 0;
	}
	else if (strcmp(argv[2],"add") == 0)
	{
		struct catalog *cat;

		if (argc != 4)
		{
			printf("Invalid parameters for add operation!\n");
			printElapsedTime(start);
			return -1;
		}

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		if (addFile(argv[1],cat,argv[3]) < 0)
		{
			printElapsedTime(start);
			return -1;
		}
		printf("File added!\n");
		printElapsedTime(start);
		return 0;

	}
	else if (strcmp(argv[2],"fetch") == 0)
	{
		struct catalog *cat;
		int i;

		if (argc != 4)
		{
			printf("Invalid parameters for fetch operation!\n");
			printElapsedTime(start);
			return -1;
		}

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		if (cat->numOfFiles == 0)
		{
			printf("File %s was not found!\n",argv[3]);
			printElapsedTime(start);
			return -1;
		}

		for (i=0;i<cat->numOfFiles;i++)
		{
			if (strcmp(argv[3],cat->files[i].name) == 0)
			{
				if (fetchFileFromVault(argv[1],cat->files[i]) < 0)
				{
					printf("error fetching file.\n");
					printElapsedTime(start);
					return -1;
				}
				printf("%s created.\n",argv[3]);
				printElapsedTime(start);
				return 0;
			}
		}

		printf("File %s was not found!\n",argv[3]);
		printElapsedTime(start);
		return -1;
	}
	else if (strcmp(argv[2],"rm") == 0)
	{
		struct catalog *cat;
		int i;

		if (argc != 4)
		{
			printf("Invalid parameters for rm operation!\n");
			printElapsedTime(start);
			return -1;
		}

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		if (cat->numOfFiles == 0)
		{
			printf("File %s was not found!\n",argv[3]);
			printElapsedTime(start);
			return -1;
		}

		for (i=0;i<cat->numOfFiles;i++)
		{
			if (strcmp(argv[3],cat->files[i].name) == 0)
			{
				if (deleteFile(argv[1],cat->files[i],i,cat) < 0)
				{
					printf("error deleting file.\n");
					printElapsedTime(start);
					return -1;
				}
				printf("%s deleted.\n",argv[3]);
				printElapsedTime(start);
				return 0;
			}
		}

		printf("File %s was not found!\n",argv[3]);
		printElapsedTime(start);
		return -1;

	}
	else if (strcmp(argv[2],"status") == 0)
	{
		struct catalog *cat;

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		if (printStatus(argv[1],cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		printElapsedTime(start);

		return 0;
	}
	else if (strcmp(argv[2],"defrag") == 0)
	{
		struct catalog *cat;
		int defResult;

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			printElapsedTime(start);
			return -1;
		}

		defResult = defrag(argv[1],cat);
		if (defResult < 0)
		{
			printElapsedTime(start);
			return -1;
		}
		while(defResult == 1)
		{
			defResult = defrag(argv[1],cat);
		}

		printElapsedTime(start);

		return 0;
	}
	else
	{
		printf("Invalid operation!\n");
		printElapsedTime(start);
		return -1;
	}


	return 0;
}
