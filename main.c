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



int createVault(char* filename,ssize_t sizeInBytes)
{
	struct catalog *c;
	struct fileMetaData *files;
	struct timeval tv;
	char* data;
	ssize_t dataSize;
	int fd;
	int i;

	c = (struct catalog *) malloc(sizeof(struct catalog));
	files = (struct fileMetaData *) calloc(sizeof(struct fileMetaData),100);

	for (i=0;i<100;i++)
		files[i].size = i;

	gettimeofday(&tv, NULL);

	c->created = tv.tv_sec;
	c->lastMod = tv.tv_sec;
	c->numOfFiles = 0;
	c->totalSize = sizeInBytes;
	c->files = files;

	dataSize = sizeInBytes - sizeof(struct catalog) - sizeof(struct fileMetaData)*100;

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

int addFile(char* vaultFile,struct catalog* c, char* fileToAdd)
{
	int i;
	off_t startOffset = 0;
	struct stat fileStat;
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

	for (i=0;i<c->numOfFiles;i++)
	{
		if (c->files[i].block1 + c->files[i].block1Len+ BORDERS_SIZE > startOffset)
			startOffset = c->files[i].block1 + c->files[i].block1Len+ BORDERS_SIZE;
		if (c->files[i].block2 + c->files[i].block2Len+ BORDERS_SIZE > startOffset)
			startOffset = c->files[i].block2 + c->files[i].block2Len+ BORDERS_SIZE;
		if (c->files[i].block3 + c->files[i].block3Len+ BORDERS_SIZE > startOffset)
			startOffset = c->files[i].block3 + c->files[i].block3Len+ BORDERS_SIZE;
	}

	if (startOffset + fileStat.st_size + BORDERS_SIZE < c->maxDataSize)
	{
		if (insertData1Block(vaultFile,c,fileToAdd,startOffset,fileStat) < 0)
		{
			return -1;
		}
	}

	return 0;
}

int insertData1Block(char* vaultFile,struct catalog* c, char* fileToAdd, off_t startOffset, struct stat fileStat)
{
	struct timeval tv;
	int fd,vfd;
	ssize_t readBytes = 0;
	ssize_t bufferSize = 1024;
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

	lseek(vfd,sizeof(struct catalog)+ sizeof(struct fileMetaData)*100 + startOffset,SEEK_SET);

	if (write(vfd,LEFT_BORDER,8) < 0)
	{
		return -1;
	}

	while (readBytes < fileStat.st_size)
	{
		len = read(fd,buffer,bufferSize);
		if (len < 0)
			return -1;
		if (write(vfd,buffer,len) < 0)
		{
			return -1;
		}
		readBytes += len;
	}

	if (write(vfd,RIGHT_BORDER,8) < 0)
	{
		return -1;
	}

	gettimeofday(&tv, NULL);

	c->availableSpace -= fileStat.st_size;
	c->numOfFiles +=1;
	c->lastMod = tv.tv_sec;
	c->files[c->numOfFiles -1].block1 = startOffset;
	c->files[c->numOfFiles -1].block1Len = fileStat.st_size;
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

	fd = open(file.name,O_RDWR|O_CREAT|O_TRUNC,0644);
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

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		printf("Invalid operation!\n");
		return -1;
	}
	lowerStr(&argv[2]);

	if (strcmp(argv[2],"init") == 0)
	{
		if (argc != 4)
		{
			printf("Invalid parameters for init operation!\n");
			return -1;
		}
		ssize_t size = getBytesFromStr(argv[3]);
		if (size < 0)
		{
			printf("Invalid parameters for init operation!\n");
			return -1;
		}

		createVault(argv[1],size);

		printf("Created.\n");
		return 0;
	}
	else if (strcmp(argv[2],"list") == 0)
	{
		struct catalog *cat;
		int i;

		if (getVaultFromFile(argv[1],&cat) < 0)
			return -1;
		if (cat->numOfFiles == 0)
			printf("The vault is empty.\n");
		else
		{
			for (i=0;i<cat->numOfFiles;i++)
			{
				printf("%s	%dB 	%d	%d	%d\n",cat->files[i].name,cat->files[i].size,cat->files[i].protection,cat->files[i].insertion, cat->files[i].block1);
			}
		}

		return 0;
	}
	else if (strcmp(argv[2],"add") == 0)
	{
		struct catalog *cat;

		if (argc != 4)
		{
			printf("Invalid parameters for add operation!\n");
			return -1;
		}

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			return -1;
		}

		if (addFile(argv[1],cat,argv[3]) < 0)
		{
			return -1;
		}
		printf("File added!\n");
		return 0;

	}
	else if (strcmp(argv[2],"fetch") == 0)
	{
		struct catalog *cat;
		int i;

		if (argc != 4)
		{
			printf("Invalid parameters for fetch operation!\n");
			return -1;
		}

		if (getVaultFromFile(argv[1],&cat) < 0)
		{
			return -1;
		}

		if (cat->numOfFiles == 0)
		{
			printf("File %s was not found!\n",argv[3]);
			return -1;
		}

		for (i=0;i<cat->numOfFiles;i++)
		{
			if (strcmp(argv[3],cat->files[i].name) == 0)
			{
				if (fetchFileFromVault(argv[1],cat->files[i]) < 0)
				{
					printf("error fetching file.\n");
					return -1;
				}
				printf("%s created.\n",argv[3]);
				return 0;
			}
		}

		printf("File %s was not found!\n",argv[3]);
		return -1;
	}

	struct catalog *cat;
	char* data;

	createVault("boom.vlt",1024*1024*2);
	getVaultFromFile("boom.vlt",&cat);

	printf("%d %d %d %d %d %d\n\n",cat->created,cat->lastMod,cat->numOfFiles,cat->totalSize,cat->files[0].size,cat->files[1].size);


//	struct timeval stop, tv;
//	double mtime;
//
//	gettimeofday(&tv, NULL);
//
//	mtime = (tv.tv_sec) * 1000.0 + (tv.tv_usec) / 1000.0 ;
//
//	time_t t = (tv.tv_sec);
//
//	printf("took %d\n", t);
//
//	printf("%d\n",sizeof(struct catalog));
//	struct catalog *c;
//	struct catalog *c2;
//	struct fileMetaData *files;
//	struct fileMetaData *filesBack;
//
//	c = (struct catalog *) malloc(sizeof(struct catalog)+1);
//
//	files = (struct fileMetaData *) malloc((sizeof(struct fileMetaData)+1)*100);
//
//	files[22].size = 500;
//
//	c->created = 5;
//	c->lastMod = 6;
//	c->numOfFiles = 10;
//	c->totalSize = 104;
//	c->files = files;
//
//	printf("%d %d %d %d %d\n",c->created,c->lastMod,c->numOfFiles,c->totalSize,c->files[22].size);
//
//	int fd = open("a.vlt",O_RDWR|O_CREAT|O_TRUNC,0777);
//	if (fd < 0)
//	{
//		printf("Error writing to file: %s\n", strerror(errno));
//		return -1;
//	}
//
//	if (write(fd,c,sizeof(struct catalog)) < 0)
//	{
//		printf("error!");
//		return 0;
//	}
//
//	if (write(fd,files,sizeof(struct fileMetaData)*100) < 0)
//	{
//		printf("error!");
//		return 0;
//	}
//
//	close(fd);
//
//	fd = open("a.vlt",O_RDONLY);
//	c2 = (struct catalog *) malloc(sizeof(struct catalog));
//	filesBack = (struct fileMetaData *) malloc(sizeof(struct fileMetaData)*100);
//
//	printf("%d\n",filesBack[0].size);
//
//	read(fd,c2,sizeof(struct catalog)+1);
//	read(fd,filesBack,(sizeof(struct fileMetaData)*100)+1);
//
//	c2->files = filesBack;
//
//	printf("%d\n",filesBack[0].size);
//
//	printf("%d %d %d %d %d\n",c2->created,c2->lastMod,c2->numOfFiles,c2->totalSize,filesBack[22].size);

	return 1;
}
