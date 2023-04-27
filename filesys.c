// starter file provided by operating systems ta's

// include libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 40

#define PATH_SIZE            4096   // max possible size for path string.
#define OPEN_FILE_TABLE_SIZE 10     // max files for files. 
#define MAX_NAME_LENGTH      11     // 11 in total but 3 for extensions, we only use 8.

// data structures for FAT32 
// Hint: BPB, DIR Entry, Open File Table -- how will you structure it?
typedef struct __attribute__((packed)){
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char DIR_NTRes;
    unsigned char DIR_CrtTimeTenth;
    unsigned short DIR_CrtTime;
    unsigned short DIR_CrtDate;
    unsigned short DIR_LastAccDate;
    unsigned short DIR_FstClusHi;
    unsigned short DIR_WrtTime;
    unsigned short DIR_WrtDate;
    unsigned short DIR_FstClusLo;
    unsigned int DIR_FileSize;
} DirEntry;

typedef struct __attribute__((packed)){
    unsigned char BS_jmpBoot[3];
    unsigned char BS_OEMName[8];
    unsigned short BPB_BytesPerSec;
    unsigned char BPB_SecsPerClus;
    unsigned short BPB_RsvdSecCnt;
    unsigned char BPB_NumFATs;
    unsigned short BPB_RootEntCnt;
    unsigned short BPB_TotSec16;
    unsigned char BPB_Media;
    unsigned short BPB_FATSz16;
    unsigned short BPB_SecPerTrk;
    unsigned short BPB_NumHeads;
    unsigned int BPB_HiddSec;
    unsigned int BPB_TotSec32;
    unsigned int BPB_FATSz32;
    unsigned short BPB_ExtFlags;
    unsigned short BPB_FSVer;
    unsigned int BPB_RootClus;
    unsigned short BPB_FSInfo;
    unsigned short BPB_BkBootSe;
    unsigned char BPB_Reserved[12];
    unsigned char BS_DrvNum;
    unsigned char BS_Reserved1;
    unsigned char BS_BootSig;
    unsigned int BS_VollD;
    unsigned char BS_VolLab[11];
    unsigned char BS_FilSysType[8];
    unsigned char empty[420];
    unsigned short Signature_word;
} BPB;

typedef struct{
    char path[PATH_SIZE];
    DirEntry dirEntry;
    unsigned int offset;
    unsigned int firstCluster; // First cluster location
    unsigned int firstClusterOffset; // Offset of first cluster in bytes
    int mode; // 2=rw, 1=w, 0=r, -1=not open (i.e, -1 is a "free" spot)
} File;


// stack implementation -- you will have to implement a dynamic stack
// Hint: For removal of files/directories

typedef struct {
    char path[PATH_SIZE]; // path string
    unsigned int cluster;
    unsigned long byteOffset;
    unsigned long rootOffset;
} CWD;

typedef struct  {
    int size;
    char ** items;
} tokenlist;

tokenlist * tokenize(char * input);
void free_tokens(tokenlist * tokens);
char * get_input();
void add_token(tokenlist * tokens, char * item);
void add_to_path(char * dir);
void info(void);
void cd(char* DIRNAME);
void ls(void);
void lseek(char* FILENAME, unsigned int OFFSET);
void read(char* FILENAME, unsigned int size);
void size(char* FILENAME);
void open(char* FILENAME, int FLAGS);
void lsof(void);
void close(char* FILENAME);
void mkdir(char* DIRNAME);
void creat(char* FILENAME);
void cp(char* FILENAME, char* TO);

// global variables
CWD cwd;
BPB bpb;
DirEntry currEntry;
FILE* fp;
int rootDirSectors;
int firstDataSector;
long firstDataSectorOffset;
File openFiles[10];
int numFilesOpen = 0;
int clusSize;
char* imageFile;

int main(int argc, char * argv[]) {
    // error checking for number of arguments.
    if(argc != 2){
        printf("Incorrect number of arguments.\n");
        return 0;
    }
    // read and open argv[1] in file pointer.
    fp = fopen(argv[1], "r+");
    if(fp == NULL){
        printf("Could not open file %s\n", argv[1]);
        return 0;
    }
    imageFile = argv[1];
    
    // obtain important information from bpb as well as initialize any important global variables
    fread(&bpb, sizeof(BPB), 1, fp);
    rootDirSectors = ((bpb.BPB_RootEntCnt * 32) + (bpb.BPB_BytesPerSec - 1)) / bpb.BPB_BytesPerSec;
    firstDataSector = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz32) + rootDirSectors;
    firstDataSectorOffset = firstDataSector * bpb.BPB_BytesPerSec;
    cwd.rootOffset = firstDataSectorOffset;
    cwd.byteOffset = cwd.rootOffset;
    cwd.cluster = bpb.BPB_RootClus;
    memset(cwd.path, 0, PATH_SIZE);
    strcat(cwd.path, argv[1]);
    clusSize = bpb.BPB_BytesPerSec * bpb.BPB_SecsPerClus;

    printf("Fat starts at byte %lu and ends at byte %lu, root offset is %lu\n", bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec, (bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32) * bpb.BPB_BytesPerSec, cwd.rootOffset);
    // Initialize all empty File objects in openFiles[10] to be closed
    int i;
    for(i = 0; i < 10; i++)
        openFiles[i].mode = -1;

    // parser
    char *input;

    while(1) {
        printf("%s/>", cwd.path);
        input = get_input();
        tokenlist * tokens = tokenize(input);    
        if(strcmp(tokens->items[0], "info") == 0){
            info();
        }else if(strcmp(tokens->items[0], "exit") == 0){
            fclose(fp);
            return 0;
        }else if(strcmp(tokens->items[0], "cd") == 0){
            if(tokens->items[1] != NULL)
                cd(tokens->items[1]);
            else
                printf("ERROR: You must enter a directory to switch to.\n");
        }else if (strcmp(tokens->items[0], "ls") == 0){
            ls();
        }else if (strcmp(tokens->items[0], "size") == 0){
            if(tokens->items[1] != NULL)
                size(tokens->items[1]);
            else
                printf("ERROR: Enter a filename to determine the size of.\n");
        }else if (strcmp(tokens->items[0], "lseek") == 0){
            if(tokens->items[1] != NULL && tokens->items[2] != NULL){
                lseek(tokens->items[1], atoi(tokens->items[2]));
            }else
                printf("ERROR: Incorrect # of arguments.\nUsage: lseek [FILENAME] [OFFSET]\n");
        }else if (strcmp(tokens->items[0], "read") == 0){
            if(tokens->items[1] != NULL && tokens->items[2] != NULL){
                read(tokens->items[1], atoi(tokens->items[2]));
            }else
                printf("ERROR: Incorrect # of arguments.\nUsage: read [FILENAME] [SIZE]\n");
        }else if (strcmp(tokens->items[0], "open") == 0){
            if(tokens->items[1] != NULL && tokens->items[2] != NULL){
                int flags;
                if (strcmp(tokens->items[2], "-r") == 0) flags = 0;
                if (strcmp(tokens->items[2], "-w") == 0) flags = 1;
                if (strcmp(tokens->items[2], "-rw") == 0) flags = 2;
                if (strcmp(tokens->items[2], "-wr") == 0) flags = 2;
                open(tokens->items[1], flags);
            }else
                printf("ERROR: Incorrect # of arguments.\nUsage: open [FILENAME] [MODE] (-r|-w|-rw)\n");
        }else if (strcmp(tokens->items[0], "lsof") == 0){
            lsof();
        }else if (strcmp(tokens->items[0], "close") == 0){
            if(tokens->items[1] != NULL)
                close(tokens->items[1]);
            else
                printf("ERROR: You must specify a file to close\n");
        }else if (strcmp(tokens->items[0], "mkdir") == 0){
            if(tokens->items[1] != NULL)
                mkdir(tokens->items[1]);
            else
                printf("ERROR: You must name a directory to be created\n");
        }else if (strcmp(tokens->items[0], "creat") == 0){
            if(tokens->items[1] != NULL)
                creat(tokens->items[1]);
            else
                printf("ERROR: You must name a file to be created\n");
        }else if (strcmp(tokens->items[0], "cp") == 0){
            if(tokens->items != NULL && tokens->items[2] != NULL)
                cp(tokens->items[1], tokens->items[2]);
            else 
                printf("ERROR: Incorrect # of arguments.\nUsage: cp [FILENAME] [DESTINATION]\n");
        }
        //add_to_path(tokens->items[0]);      // move this out to its correct place; 
        free(input);
        free_tokens(tokens);
    }

    return 0;
}

// helper functions -- to navigate file image
// HELPER ACCESSORS START
int find(char* DIRNAME){ // If found, currEntry will hold directory in question
    int i, j;
    unsigned long originalPos = ftell(fp);
    unsigned int currCluster = cwd.cluster;
    unsigned long fatEntryOffset;
    fseek(fp, cwd.byteOffset, SEEK_SET);
    while(currCluster < bpb.BPB_TotSec32){
        unsigned long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
        fseek(fp, byteOffsetOfCluster, SEEK_SET);
        for(i = 0; i < 16; i++){
            fread(&currEntry, sizeof(DirEntry), 1, fp);
            if(currEntry.DIR_Attr == 0x0F || currEntry.DIR_Name == 0x00)
                continue;
            for(j = 0; j < 11; j++){
                if(currEntry.DIR_Name[j] == 0x20)
                    currEntry.DIR_Name[j] = 0x00;
            }
            if(strcmp(currEntry.DIR_Name, DIRNAME) == 0){
                if(currEntry.DIR_Attr == 0x10){
                    fseek(fp, originalPos, SEEK_SET);
                    return 0; // Directory exists
                }else{
                    fseek(fp, originalPos, SEEK_SET);
                    return 1; // Directory exists but is a file
                }
            }
        }
        fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec + (currCluster * 4));
        fseek(fp, fatEntryOffset, SEEK_SET);
        fread(&currCluster, sizeof(int), 1, fp);
    } 
    fseek(fp, originalPos, SEEK_SET);
    return -1; // Directory does NOT exist at all
}

int findNextFreeFatEntry(){ // Returns cluster num of next free fat entry
    unsigned long originalPos = ftell(fp);
    fseek(fp, bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec, SEEK_SET);
    unsigned int currCluster = 0xFFFFFFF;
    unsigned long clusterOffset; 
    while(currCluster != 0x0000000){
        fread(&currCluster, sizeof(unsigned int), 1, fp);
        clusterOffset = ftell(fp) - sizeof(unsigned int);
    }
    fseek(fp, originalPos, SEEK_SET);
    return (clusterOffset - (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec)) / 4;
}

// HELPER ACCESSORS END

// HELPER MUTATORS START
void expandCluster(int cluster){ // Expands the current cluster by finding its' endpoint and creating a new one it can chain to, also updates FAT
    unsigned long originalPos = ftell(fp);
    unsigned long fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (cluster * 4);
    unsigned int currCluster;
    char empty[512] = {0x0}; 
    fseek(fp, fatEntryOffset, SEEK_SET); 
    fread(&currCluster, sizeof(int), 1, fp);
    while(currCluster < bpb.BPB_TotSec32){
        fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec + (currCluster * 4));
        fseek(fp, fatEntryOffset, SEEK_SET);
        fread(&currCluster, sizeof(int), 1, fp);
    } 
    fseek(fp, -sizeof(int), SEEK_CUR);
    unsigned int newCluster = findNextFreeFatEntry();
    unsigned long newClusterOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (newCluster * 4);
    unsigned long newDataClusterOffset = (firstDataSector + ((newCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
    unsigned int EOC = 0xFFFFFFF;
    fwrite(&newCluster, sizeof(int), 1, fp);
    fseek(fp, newClusterOffset, SEEK_SET);
    fwrite(&EOC, sizeof(int), 1, fp);
    fseek(fp, newDataClusterOffset, SEEK_SET);
    fwrite(&empty, sizeof(empty), 1, fp);

    fseek(fp, originalPos, SEEK_SET);

}
// HELPER MUTATORS END
// commands -- all commands mentioned in part 2-6 (17 cmds)

// Part 1 MOUNT

void info(void){
   unsigned int totalDataSectors = bpb.BPB_TotSec32 - (bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz32) + rootDirSectors);
   unsigned int totalClusters = totalDataSectors / bpb.BPB_SecsPerClus;
   printf("Position of root cluster: %d\n", bpb.BPB_RootClus);
   printf("Bytes per sector: %d\n", bpb.BPB_BytesPerSec);
   printf("Sectors per cluster: %d\n", bpb.BPB_SecsPerClus);
   printf("Total # of clusters in data region: %d\n", totalClusters);
   printf("# of entries in one fat: %d\n", ((bpb.BPB_FATSz32 * bpb.BPB_BytesPerSec) / 4));
   printf("Size of image (bytes): %d\n", bpb.BPB_TotSec32 * bpb.BPB_BytesPerSec);
}

// Part 2 NAVIGATION
void cd(char* DIRNAME){
    int result = find(DIRNAME);
    if(result == 0){
        unsigned short Hi = currEntry.DIR_FstClusHi;
        unsigned short Lo = currEntry.DIR_FstClusLo;
        unsigned int cluster = (Hi<<8) | Lo;
        unsigned long byteOffset = (firstDataSector + ((cluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
        if(cluster == 0){
            cwd.cluster = bpb.BPB_RootClus;
            cwd.byteOffset = cwd.rootOffset;
        }
        else{              
            cwd.cluster = cluster;  
            cwd.byteOffset = byteOffset;
        }
        if(strcmp(DIRNAME, "..") == 0){
            int i = strlen(cwd.path);
            unsigned char* current = " ";
            while(strcmp(current, "/") != 0){
                cwd.path[i--] = '\0';
                current = &cwd.path[i];
            }
            cwd.path[i] = '\0';
        }else if(strcmp(DIRNAME, ".") != 0){
            strcat(cwd.path, "/");
            strcat(cwd.path, DIRNAME);
        }
    }else if(result == 1){
       printf("ERROR: %s is a file.\n", DIRNAME);
    }else if(result == -1){
       printf("ERROR: file or directory does not exist.\n", result);
    }
}

void ls(void){
    // print all directories/files in cwd
    int i, j;
    unsigned long originalPos = ftell(fp);
    fseek(fp, cwd.byteOffset, SEEK_SET);
    unsigned int currCluster = cwd.cluster;
    long fatEntryOffset;
    while(currCluster < bpb.BPB_TotSec32){
        unsigned long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
        fseek(fp, byteOffsetOfCluster, SEEK_SET);
        for(i = 0; i < 16; i++){
            fread(&currEntry, sizeof(DirEntry), 1, fp);
            if(currEntry.DIR_Attr == 0x0F || currEntry.DIR_Name[0] == 0x00)
                continue; // File is long file name or is deleted/does not exist
            for(j = 0; j < 11; j++){
                if(currEntry.DIR_Name[j] == 0x20)
                    currEntry.DIR_Name[j] = 0x00;
            }
            printf("%s ", currEntry.DIR_Name);
        }
        printf("\n");
        fatEntryOffset = ((bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (currCluster * 4));
        fseek(fp, fatEntryOffset, SEEK_SET);
        printf("ls: cluster %d end, chaining to cluster ", currCluster);
        fread(&currCluster, sizeof(int), 1, fp);
        printf("%d (%X)(offset %d)\n", currCluster, currCluster, ftell(fp));
    }
    fseek(fp, originalPos, SEEK_SET);
}
// Part 2 END

// Part 3 CREATE
void mkdir(char* DIRNAME){
    // If DIRNAME does NOT exist
    // create directory DIRNAME in cwd
    // else
    // printf("Error: file or directory with that name already exists");
    int result = find(DIRNAME);
    DirEntry newEntry;
    if(result != -1){
        printf("Error: file or directory with that name already exists\n");
    }else{
        unsigned long originalPos = ftell(fp);
        fseek(fp, cwd.byteOffset, SEEK_SET);
        unsigned int currCluster = cwd.cluster;
        unsigned int lastCluster;
        unsigned long fatEntryOffset;
        while(currCluster < bpb.BPB_TotSec32){
            int i = 0;
            unsigned long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec; 
            fseek(fp, byteOffsetOfCluster, SEEK_SET);
            for(i = 0; i < 16; i++){
                fread(&currEntry, sizeof(DirEntry), 1, fp);
                if(currEntry.DIR_Name[0] != 0x00)
                    continue; // Entry is taken
                else{
                    fseek(fp, -sizeof(DirEntry), SEEK_CUR);
                    unsigned int newCluster = findNextFreeFatEntry();
                    unsigned int newClusterOffset = (firstDataSector + ((newCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                    unsigned short newClusHi = newCluster / 65536;
                    unsigned short newClusLo = newCluster % 65536;
                    char empty[512] = {0x0};
                    int EOC = 0xFFFFFFF;
                    int cwdResult = (cwd.cluster == 2) ? -1 : find(".");

                    DirEntry dot, dotDot;
                    strcpy(newEntry.DIR_Name, DIRNAME);
                    newEntry.DIR_Attr = 0x10; // Directory
                    newEntry.DIR_FileSize = 0x00;
                    newEntry.DIR_FstClusHi = newClusHi;
                    newEntry.DIR_FstClusLo = newClusLo;
                    fwrite(&newEntry, sizeof(DirEntry), 1, fp); // Create directory entry
                    fseek(fp, newClusterOffset, SEEK_SET);
                    fwrite(&empty, 1, sizeof(empty), fp); // Initialize cluster to 0
                    fseek(fp, newClusterOffset, SEEK_SET);
                    dot = newEntry; 
                    
                    if(cwdResult != -1){ // if not root directory
                        dotDot = currEntry;
                        int i;
                        for(i = 0; i < 11; i++){
                            if(i < 2)
                                dotDot.DIR_Name[i] = '.';
                            else
                                dotDot.DIR_Name[i] = 0x0;

                            if(i < 1)
                                dot.DIR_Name[i] = '.';
                            else    
                                dot.DIR_Name[i] = 0x0;
                        }
                    }else{
                        int i;
                        for(i = 0; i < 11; i++){
                            if(i < 2)
                                dotDot.DIR_Name[i] = '.';
                            else
                                dotDot.DIR_Name[i] = 0x0;

                            if(i < 1)
                                dot.DIR_Name[i] = '.';   
                            else    
                                dot.DIR_Name[i] = 0x0; 
                        }
                        dotDot.DIR_Attr = 0x10;
                        dotDot.DIR_FstClusHi = 0x00;
                        dotDot.DIR_FstClusLo = 0x00;
                    }
                    fwrite(&dot, sizeof(DirEntry), 1, fp);
                    fwrite(&dotDot, sizeof(DirEntry), 1, fp);
                    fseek(fp, (newCluster * 4) + (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec), SEEK_SET); // Update FAT table
                    fwrite(&EOC, sizeof(int), 1, fp); // Entry for new directory's cluster will have EOC since it is the only one

                    fseek(fp, originalPos, SEEK_SET);
                    fseek(fp, (cwd.cluster * 4) + (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec), SEEK_SET);
                    fread(&currCluster, sizeof(int), 1, fp);
                    fseek(fp, originalPos, SEEK_SET);
                    return;
                }
            }
            fatEntryOffset = ((bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (currCluster * 4));
            fseek(fp, fatEntryOffset, SEEK_SET);
            fread(&currCluster, sizeof(int), 1, fp);
        }
        // If you've reached here, you've run out of clusters so:
        expandCluster(cwd.cluster); // Add another cluster
        fseek(fp, originalPos, SEEK_SET);
        mkdir(DIRNAME); // Then try again
        return;
    }
}

void creat(char* FILENAME){
    // If DIRNAME does NOT exist
    // create file DIRNAME in cwd
    // else
    // printf("Error: file or directory with that name already exists");
    int result = find(FILENAME);
    if(result != -1){
        printf("Error: file or directory with that name already exists\n");
    }else{
        unsigned long originalPos = ftell(fp);
        DirEntry newEntry;
        DirEntry tracker;
        strcpy(newEntry.DIR_Name, FILENAME);
        newEntry.DIR_Attr = 0x20;
        newEntry.DIR_FileSize = 0x0;
        newEntry.DIR_FstClusLo = 0x0;
        newEntry.DIR_FstClusHi = 0x0;
        int i;
        unsigned int currCluster = cwd.cluster; 
        unsigned long fatEntryOffset;
        fseek(fp, cwd.byteOffset, SEEK_SET);
        while(currCluster < bpb.BPB_TotSec32){
            unsigned long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
            fseek(fp, byteOffsetOfCluster, SEEK_SET);
            for(i = 0; i < (clusSize / 32); i++){
                fread(&tracker, sizeof(DirEntry), 1, fp);
                if(tracker.DIR_Name[0] == 0x00){
                    fseek(fp, -sizeof(DirEntry), SEEK_CUR);
                    fwrite(&newEntry, sizeof(DirEntry), 1, fp);
                    printf("creat: wrote directory into final cluster %d at offset %lu\n", currCluster, ftell(fp));
                    fseek(fp, originalPos, SEEK_SET);
                    return;
                }
            }
            fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec + (currCluster * 4));
            fseek(fp, fatEntryOffset, SEEK_SET);
            printf("creat: found end of cluster %d, chaining now to cluster ", currCluster);
            fread(&currCluster, sizeof(int), 1, fp);
            printf("%d\n", currCluster);
        }
        expandCluster(cwd.cluster);
        printf("creat: expanded cluster %d\n", cwd.cluster);
        fseek(fp, originalPos, SEEK_SET);
        creat(FILENAME);
    }
}

void cp(char* FILENAME, char* TO){
    int toResult = find(TO);
    int fileResult = find(FILENAME);
    char nameIfDir[strlen(TO) + 1];
    strcpy(nameIfDir, TO);
    int wentIntoSubDir = 0;

    unsigned int originalClus = cwd.cluster;
    unsigned int originalOffset = cwd.byteOffset;
    unsigned long originalPos = ftell(fp);   
    int totalClusters = 1;

    unsigned short sourceHi = currEntry.DIR_FstClusHi;
    unsigned short sourceLo = currEntry.DIR_FstClusLo;
    unsigned int sourceCluster = (sourceHi<<8) | sourceLo;
    unsigned sourceOffset = (firstDataSector + ((sourceCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
    
    FILE* toWriter = fopen(imageFile, "r+");
    DirEntry tracker;
    DirEntry source = currEntry;

    unsigned long fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (sourceCluster * 4);
    unsigned int currCluster;
    fseek(fp, fatEntryOffset, SEEK_SET);
    fread(&currCluster, sizeof(int), 1, fp);
    while(source.DIR_FileSize != 0x0 && currCluster < bpb.BPB_TotSec32){
        fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec + (currCluster * 4));
        fseek(fp, fatEntryOffset, SEEK_SET);
        fread(&currCluster, sizeof(int), 1, fp);
        totalClusters++;
    } // Find out how many clusters are in the source, we will allocate the same amount for the destination

    if(strcmp(TO, ".") == 0 && fileResult == 1){
        printf("%s already exists here\n", FILENAME);
    }


    if(fileResult == -1){
        printf("ERROR: %s does not exist\n", FILENAME);
        cwd.cluster = originalClus;
        cwd.byteOffset = originalOffset;
        fseek(fp, originalPos, SEEK_SET);
        return;
    }else if(toResult == 0){ // If TO is a directory
        cd(TO);  
        TO = FILENAME;
        toResult = find(TO);
        if(toResult == -1){ // If not found, create it
            creat(TO);
            wentIntoSubDir++;
        }else{
            printf("ERROR: the directory %s contains a subdirectory %s of the same name\n", TO, FILENAME);
            cd("..");
            fseek(fp, originalPos, SEEK_SET);
            return;
        }
    }else if(toResult == 1){ // If it exists, delete it and start anew
//        rm(TO);
        creat(TO);
    }else if(fileResult == 0){
        printf("ERROR: %s is a directory\n", FILENAME);
        cwd.cluster = originalClus;
        cwd.byteOffset = originalOffset;
        fseek(fp, originalPos, SEEK_SET);
        return;
    }else{
        creat(TO);
    }

    if(source.DIR_FileSize == 0x0){ 
        printf("cp: condition checked\n");
        /*if(wentIntoSubDir == 1){
            printf("cp: Going into cd\n");
            char* pause;
            size_t s;
            getline(&pause, &s, stdin);
            cd("..");
            printf("cp: Made it out of cd\n");
        }*/
       // fseek(fp, originalPos, SEEK_SET);
        return;
    }

    


    // Give the new file a cluster to start with
    int newCluster = findNextFreeFatEntry(); 
    unsigned short newClusHi = newCluster / 65536;
    unsigned short newClusLo = newCluster % 65536;
    unsigned long newClusterOffset = (firstDataSector + ((newCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
    currCluster = cwd.cluster;
    fatEntryOffset = newCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
    fseek(fp, fatEntryOffset, SEEK_SET);
    unsigned int EOC = 0xFFFFFFF;
    fwrite(&EOC, sizeof(unsigned int), 1, fp);
    fseek(fp, cwd.byteOffset, SEEK_SET);
    int clusterAllocated = 0;
    while(currCluster < bpb.BPB_TotSec32 && clusterAllocated != 1){
        int i;
        unsigned long byteOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;  
        fseek(fp, byteOffset, SEEK_SET);
        for(i = 0; i < clusSize / 32; i++){
            fread(&tracker, sizeof(DirEntry), 1, fp);
            if(strcmp(tracker.DIR_Name, TO) == 0){
                fseek(fp, -sizeof(DirEntry), SEEK_CUR);
                DirEntry newEntry = tracker;
                char empty[512] = {0x0};
                newEntry.DIR_FstClusHi = newClusHi;
                newEntry.DIR_FstClusLo = newClusLo;
                newEntry.DIR_FileSize = source.DIR_FileSize;
                fwrite(&newEntry, sizeof(DirEntry), 1, fp);
                fseek(fp, newClusterOffset, SEEK_SET);
                fwrite(&empty, sizeof(empty), 1, fp);
                clusterAllocated = 1;
                printf("cp: Gave the file the cluster %d, with size %d\n", newCluster, newEntry.DIR_FileSize);
                break;
            }
        }
        fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
        fseek(fp, fatEntryOffset, SEEK_SET);
        fread(&currCluster, sizeof(unsigned int), 1, fp);
    }


    fseek(fp, cwd.byteOffset, SEEK_SET);
    currCluster = cwd.cluster;
    while(currCluster < bpb.BPB_TotSec32){ // Find the destination file
        int i;
        printf("cp: cluster %d is smaller than total sectors %d\n", currCluster, bpb.BPB_TotSec32);
        unsigned long byteSectorOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec; 
        fseek(fp, byteSectorOffset, SEEK_SET);
        for(i = 0; i < clusSize / 32; i++){ 
            fread(&tracker, sizeof(DirEntry), 1, fp);
            if(strcmp(tracker.DIR_Name, TO) == 0){ // Once dest file is found, begin copying
                unsigned int destinationCluster = (tracker.DIR_FstClusHi<<8) | tracker.DIR_FstClusLo;
                printf("cp: Starting copying into %s, whose contents are located at cluster %d\n", TO, destinationCluster);
                unsigned long destinationOffset = (firstDataSector + ((destinationCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                sourceOffset = (firstDataSector + ((sourceCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                fseek(toWriter, destinationOffset, SEEK_SET);
                fseek(fp, sourceOffset, SEEK_SET);
                int k;
                for(k = 0; k < totalClusters; k++){
                    expandCluster(destinationCluster);
                }
                printf("cluster expanded by %d\n", totalClusters);
                int bytesWritten = 0;
                while(bytesWritten < source.DIR_FileSize){
                    char currByte;
                    int j;
                    for(j = 0; j < (clusSize); j++){
                        fread(&currByte, sizeof(char), 1, fp);
                        printf("Writing byte %c from cluster %d at offset %lu to cluster %d at offset %lu\n", currByte, sourceCluster, ftell(fp), destinationCluster, ftell(toWriter));
                        fwrite(&currByte, sizeof(char), 1, toWriter);
                        bytesWritten++;
                    }
                    unsigned long destinationFatEntry = destinationCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
                    unsigned long sourceFatEntry = sourceCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
                    fseek(fp, sourceFatEntry, SEEK_SET);
                    fseek(toWriter, destinationFatEntry, SEEK_SET);
                    fread(&sourceCluster, sizeof(int), i, fp);
                    fread(&destinationCluster, sizeof(int), i, toWriter); 
                }
                fseek(fp, originalPos, SEEK_SET);
                if(wentIntoSubDir == 1)
                    cd("..");
                return; // Copying is done
            }
        }
        printf("cp: File not found in cluster %d, chaining to next cluster at ", currCluster);
        fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
        fseek(fp, fatEntryOffset, SEEK_SET);
        fread(&currCluster, sizeof(int), 1, fp);
        printf("%d (offset %lu)\n", currCluster, ftell(fp));
    }

    if(wentIntoSubDir == 1)
         cd("..");

    fclose(toWriter);
}
// Part 3 END

// Part 4 READ
void open(char* FILENAME, int FLAGS){
    // If file exists in cwd, not already opened, and flag is valid
    // Initialize FILE * offset to 0
    // Add cwd/FILENAME to open file table
    int alreadyOpen = find(FILENAME);
    int i = 0;

    if(alreadyOpen == 0){
        printf("ERROR: %s is a directory\n", FILENAME);
        return;
    }else if(alreadyOpen == -1){
        printf("ERROR: File %s does not exist\n", FILENAME); 
        return;
    }

    for(i = 0; i < 10; i++)
    {
        if(openFiles[i].mode != - 1 && strcmp(openFiles[i].path, cwd.path) == 0  && strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0)
        {
            printf("Error: %s is already open\n", FILENAME);
            return;
        }
        if(openFiles[i].mode != -1)
            numFilesOpen++;
    }
    if (numFilesOpen == 10)
    {
        printf("Error: Max number of files already open\n");
        return;
    }else
    {
        for(i = 0; i < 10; i++){
            if(openFiles[i].mode == -1){
                unsigned short Hi = currEntry.DIR_FstClusHi;
                unsigned short Lo = currEntry.DIR_FstClusLo;
                unsigned int cluster = (Hi<<8) | Lo;
                unsigned long clusterOffset = (firstDataSector + ((cluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                if(cluster == 0){ // Edge case of ".."
                    cluster = bpb.BPB_RootClus;
                    clusterOffset = cwd.rootOffset;
                }

                strcpy(openFiles[i].path, cwd.path); 
                openFiles[i].offset = 0;
                openFiles[i].dirEntry = currEntry;
                openFiles[i].firstCluster = cluster;
                openFiles[i].firstClusterOffset = clusterOffset;
                openFiles[i].mode = FLAGS;
                break;
            }
        }
        printf("Opened %s with size %d\n", openFiles[i].dirEntry.DIR_Name, openFiles[i].dirEntry.DIR_FileSize);
        numFilesOpen++;
    }
}

void close(char* FILENAME){
    // If file exists in cwd and is opened
    // lazy delete file entry from open file table
    if(find(FILENAME) == 1){
        int i;
        for(i = 0; i < 10; i++){
            if(openFiles[i].mode != - 1 && strcmp(openFiles[i].path, cwd.path) == 0  && strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0){
                openFiles[i].mode = -1;
                printf("Closed %s\n", FILENAME);
                return;
            }
        }
    }else if(find(FILENAME) == -1){
        printf("ERROR: The file %s could not be find in the current working directory\n", FILENAME);
    }else
        printf("ERROR: %s is a directory\n", FILENAME);
}

void lsof(void){
    // Read from open file table
    // If table is not empty
    // print("Index in table, FILENAME, mode (flags), offset of FILE *, full path").
    // else
    // print("No files are currently open");
    int i =  0;

    if (numFilesOpen == 0)
    {
        printf("No files are currently open\n");
        return;
    }else
    {
        printf("INDEX NAME      MODE      OFFSET      PATH\n");
        for (i = 0; i < 10; i++)
        {
            if(openFiles[i].mode != -1){
                char* mode;
                if(openFiles[i].mode == 0) mode = "r";
                if(openFiles[i].mode == 1) mode = "w";
                if(openFiles[i].mode == 2) mode = "rw";
                printf("%-6d%-10s%-10s%-12d%-s\n", i, openFiles[i].dirEntry.DIR_Name, mode, openFiles[i].offset, openFiles[i].path);
            }
        }
    }
}

void size(char* FILENAME){
    int result = find(FILENAME);
    if (result == -1){
        printf("%s does not exist\n", FILENAME);
    }else if (result == 0){
        printf("%s is a directory\n", FILENAME);
    }else{
        printf("%d %s\n", currEntry.DIR_FileSize, FILENAME);
    }
    // print("Size in bytes: ");
    // else
    // print("Error: file does not exist");
}

void lseek(char* FILENAME, unsigned int OFFSET){
    int fileOpen = 1;
    int i;
    int targetFile;
    for(i = 0; i < 10; i++){
        if(openFiles[i].mode != -1 && strcmp(openFiles[i].path, cwd.path) == 0 && strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0){
            fileOpen = 0;
            targetFile = i;
        }
    }
    if(fileOpen == 1){
        printf("ERROR: No file named %s has been opened.\n", FILENAME);
    }else if(OFFSET > openFiles[targetFile].dirEntry.DIR_FileSize){
        printf("Size of file %s is %d\n", openFiles[targetFile].dirEntry.DIR_Name, openFiles[targetFile].dirEntry.DIR_FileSize);
        printf("ERROR: Offset is larger than file size.\n");
    }else{
        openFiles[targetFile].offset = OFFSET;
    }
}

void read(char* FILENAME, unsigned int size){
    int fileOpen = 1;
    int i;          
    int targetFile;
    for(i = 0; i < 10; i++){   
        if(openFiles[i].mode != -1 && strcmp(openFiles[i].path, cwd.path) == 0 && strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0){
            fileOpen = 0;
            targetFile = i;
        }
    }

    if(fileOpen == 1){
        printf("ERROR: No file named %s has been opened for reading.\n", FILENAME);
    }else{
        long originalPos = ftell(fp);
        char currChar;
        File* target = &openFiles[targetFile];
        int clustersChained = target->offset / (bpb.BPB_SecsPerClus * bpb.BPB_BytesPerSec);
        int byteInCurrentCluster = target->offset % (bpb.BPB_SecsPerClus * bpb.BPB_BytesPerSec);
        int currCluster = target->firstCluster;
        int currClusterOffset = target->firstClusterOffset;
        int fatEntryOffset;
        int charsRead = 0;
        int chained = 0;
        int i;
        long currByte = byteInCurrentCluster;
        fseek(fp, currClusterOffset, SEEK_SET);
        if(target->dirEntry.DIR_FileSize < target->offset + size)
            size = target->dirEntry.DIR_FileSize - target->offset;
        if(target->dirEntry.DIR_FileSize <= target->offset)
            return;
        while(charsRead < size && target->offset < target->dirEntry.DIR_FileSize){
            if(chained++ == 0){
                for(i = 0; i < clustersChained; i++){ // Immediately chain to current cluster and jump to byte where offset is
                    fatEntryOffset = ((bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (currCluster * 4));
                    fseek(fp, fatEntryOffset, SEEK_SET);
                    fread(&currCluster, sizeof(int), 1, fp);
                    currClusterOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                    if(i == (clustersChained - 1)){
                        currByte = currClusterOffset + byteInCurrentCluster;
                        fseek(fp, currByte, SEEK_SET);
                    }
                    printf("After immediate chaining, currCluster is %d\n", currCluster);
                }
            }
            // If end of current cluster is reached:
            if(currByte % (bpb.BPB_SecsPerClus * bpb.BPB_BytesPerSec) == 0 && target->offset != 0){
                fatEntryOffset = ((bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (currCluster * 4));
                fseek(fp, fatEntryOffset, SEEK_SET);
                fread(&currCluster, sizeof(int), 1, fp);
                currClusterOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                fseek(fp, currClusterOffset, SEEK_SET);
            }
            fseek(fp, currClusterOffset + target->offset, SEEK_SET);
            fread(&currChar, sizeof(char), 1, fp);
            printf("%c", currChar);
            currByte++;
            charsRead++;
            target->offset++;
        }
        fseek(fp, originalPos, SEEK_SET);
    }
}

// Part 4 END


// add directory string to cwd path -- helps keep track of where we are in image.
void add_to_path(char * dir) {
    if(dir == NULL) {
        return;
    }
    else if(strcmp(dir, "..") == 0) {     
        char *last = strrchr(cwd.path, '/');
        if(last != NULL) {
            *last = '\0';
        }
    } else if(strcmp(dir, ".") != 0) {
        strcat(cwd.path, "/");
        strcat(cwd.path, dir);
    }
}

void free_tokens(tokenlist *tokens)
{
    int i;
    for (i = 0; i < tokens->size; i++)
        free(tokens->items[i]);
    free(tokens->items);
    free(tokens);
}

// take care of delimiters {'\"', ' '}
tokenlist * tokenize(char * input) {
    int is_in_string = 0;
    tokenlist * tokens = (tokenlist *) malloc(sizeof(tokenlist));
    tokens->size = 0;

    tokens->items = (char **) malloc(sizeof(char *));
    char ** temp;
    int resizes = 1;
    char * token = input;

    for(; *input != '\0'; input++) {
        if(*input == '\"' && !is_in_string) {
            is_in_string = 1;
            token = input + 1;
        } else if(*input == '\"' && is_in_string) {
            *input = '\0';
            add_token(tokens, token);
            while(*(input + 1) == ' ') {
                input++;
            }
            token = input + 1;
            is_in_string = 0;                    
        } else if(*input == ' ' && !is_in_string) {
            *input = '\0';
            while(*(input + 1) == ' ') {
                input++;
            }
            add_token(tokens, token);
            token = input + 1;
        }
    }
    if(is_in_string) {
        printf("error: string not properly enclosed.\n");
        tokens->size = -1;
        return tokens;
    }

    // add in last token before null character.
    if(*token != '\0') {
        add_token(tokens, token);
    }

    return tokens;
}

void add_token(tokenlist *tokens, char *item)
{
    int i = tokens->size;
    tokens->items = (char **) realloc(tokens->items, (i + 2) * sizeof(char *));
    tokens->items[i] = (char *) malloc(strlen(item) + 1);
    tokens->items[i + 1] = NULL;
    strcpy(tokens->items[i], item);
    tokens->size += 1;
}

char * get_input() {
    char * buf = (char *) malloc(sizeof(char) * BUFSIZE);
    memset(buf, 0, BUFSIZE);
    char c;
    int len = 0;
    int resizes = 1;

    int is_leading_space = 1;

    while((c = fgetc(stdin)) != '\n' && !feof(stdin)) {

        // remove leading spaces.
        if(c != ' ') {
            is_leading_space = 0;
        } else if(is_leading_space) {
            continue;
        }
        
        buf[len] = c;

        if(++len >= (BUFSIZE * resizes)) {
            buf = (char *) realloc(buf, (BUFSIZE * ++resizes) + 1);
            memset(buf + (BUFSIZE * (resizes - 1)), 0, BUFSIZE);
        }        
    }
    buf[len + 1] = '\0';

    // remove trailing spaces.
    char * end = &buf[len - 1];

    while(*end == ' ') {
        *end = '\0';
        end--;
    }   

    return buf;
}
