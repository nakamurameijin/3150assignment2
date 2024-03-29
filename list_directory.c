#include "list_directory.h"
#include "data_struct.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

struct BootEntry bootent;
unsigned int start_of_FAT;
unsigned int start_of_Data;

int print_entry(int disk, unsigned int fat[], unsigned int cluster_num)
{
	unsigned int num_of_dirent = (bootent.BPB_SecPerClus * bootent.BPB_BytsPerSec) / 32;
	unsigned int count = 1;

	//traverse link list
	for(;cluster_num < 0x0ffffff8; cluster_num = fat[cluster_num])
	{
		unsigned int cluster_address = start_of_Data + (cluster_num - 2) * bootent.BPB_SecPerClus * bootent.BPB_BytsPerSec;

		unsigned int rec;
		for(rec=0; rec < num_of_dirent; rec++)
		{
			struct DirEntry dirent;
			pread(disk, &dirent, sizeof(dirent), cluster_address + rec * 32);

			//skip LFN and empty entries
			if((dirent.DIR_Attr & 0x0F == 0x0F ) || (dirent.DIR_Name[0] == 0)) 
				continue;

			//get filename
			unsigned char filename[13];
			int i;
			for(i=0;(dirent.DIR_Name[i] != 0x20) && (i<8);i++)
				filename[i] = dirent.DIR_Name[i];
			//place a dot if contains extension
			if(dirent,dirent.DIR_Name[8] != 0x20)
			{
				filename[i++] = '.';
				//get extension
				int j;
				for(j=8;(dirent.DIR_Name[j] != 0x20) && (j<11);j++,i++)
					filename[i] = dirent.DIR_Name[j];
			}
			//end with NULL char
			filename[i] = '\0';

			if(filename[0] == 0xe5) //replace 0xE5 in deleted files
				filename[0] = '?';

			printf("%d, %s", count++, filename);

			//if is ./ or ../ Directory, skip the remaining
			if(((dirent.DIR_Attr & 0x10) == 0x10) && filename[0] == 0x2e)
			{
				printf("/\n");
				continue;
			}
			else if ((dirent.DIR_Attr & 0x10) == 0x10) //if it is a Directory
				printf("/");

			//print filesize and starting cluster num
			unsigned int starting_cluster_num = (unsigned int)dirent.DIR_FstClusLO + ((unsigned int)dirent.DIR_FstClusHI)*16*16*16*16;
			printf(", %u, %u\n", dirent.DIR_FileSize, starting_cluster_num);
		}
	}

	return 0;
}

int find_directory(unsigned char target[], int disk, unsigned int fat[], unsigned int cluster_num)
{
	unsigned int num_of_dirent = (bootent.BPB_SecPerClus * bootent.BPB_BytsPerSec) / 32;

	//traverse link list
	for(;cluster_num < 0x0ffffff8; cluster_num = fat[cluster_num])
	{
		unsigned int cluster_address = start_of_Data + (cluster_num - 2) * bootent.BPB_SecPerClus * bootent.BPB_BytsPerSec;

		unsigned int rec;
		for(rec=0; rec < num_of_dirent; rec++)
		{
			struct DirEntry dirent;
			pread(disk, &dirent, sizeof(dirent), cluster_address + rec * 32);

			//skip normal files, LFN and empty entries
			if(((dirent.DIR_Attr & 0x10) != 0x10) || (dirent.DIR_Attr & 0x0F == 0x0F ) || (dirent.DIR_Name[0] == 0)) 
			{
				continue;
			}

			//get filename
			unsigned char filename[13];
			int i;
			for(i=0;(dirent.DIR_Name[i] != 0x20) && (i<8);i++)
				filename[i] = dirent.DIR_Name[i];
			//place a dot if contains extension
			if(dirent,dirent.DIR_Name[8] != 0x20)
			{
				filename[i++] = '.';
				//get extension
				int j;
				for(j=8;(dirent.DIR_Name[j] != 0x20) && (j<11);j++,i++)
					filename[i] = dirent.DIR_Name[j];
			}
			//end with NULL char
			filename[i] = '\0';
			if(strcmp(filename,target) == 0) //target found!
			{
				unsigned int starting_cluster_num = (unsigned int)dirent.DIR_FstClusLO + ((unsigned int)dirent.DIR_FstClusHI)*16*16*16*16;
				if(starting_cluster_num == 0) //re map cluster 0 to 2
					starting_cluster_num = 2;
				return starting_cluster_num;
			}

		}	
	}

	return 0;
}

int list_directory(char* dev_name, char* target)
{
	printf("%s-%s\n", dev_name, target);
	printf("size of boot entry: %d\n", (int)sizeof(struct BootEntry));
	printf("size of dir entry: %d\n", (int)sizeof(struct DirEntry));
	
	//open & read boot entry
	//bootent is a global var
	int disk = open(dev_name,O_RDONLY);
	pread(disk,&bootent,sizeof(bootent),0);

	//start_of_FAT & start_of_Data is global var
	start_of_FAT = bootent.BPB_RsvdSecCnt * bootent.BPB_BytsPerSec;
	start_of_Data = start_of_FAT + bootent.BPB_FATSz32 * bootent.BPB_NumFATs * bootent.BPB_BytsPerSec; 
	printf("start of FAT: %d\n", start_of_FAT);
	printf("start of Data Area: %d\n",start_of_Data);

	//read FAT array
	unsigned int fat_size = bootent.BPB_FATSz32 * bootent.BPB_BytsPerSec; //in bytes
	unsigned int fat[fat_size / 4];
	pread(disk, fat, fat_size, start_of_FAT);


	//cluster to be print
	unsigned int cluster_num = 2; //defalut is 2 (root)
	//go into sub-directory
	char* temp = strtok(target,"/");
	int k = 0;
	while(temp)
	{
		//try to update cluster_num
		cluster_num = find_directory(temp, disk, fat, cluster_num);
		if(cluster_num == 0) //not found
		{
			printf("not found!\n");
			return 0;
		}
		temp = strtok(NULL,"/");
	}

	print_entry(disk, fat, cluster_num);

	close(disk);

	return 0;
}
