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
    unsigned int offset; // Offset for reading and writing
    unsigned int firstCluster; // First cluster number
    unsigned long firstClusterOffset; // Offset of first cluster in bytes
    unsigned long dirEntryOffset; // Offset for the dirEntry that represents the file
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
void remove_file(char* FILENAME);
void remove_directory(char* DIRNAME, char* FLAGS);
void clear_FAT(unsigned int clusterNumber);
void rename_(char* FILENAME, char* NEWFILENAME);
void write(char* FILENAME, char* STRING);
void dump(char* FILENAME); // Can also be used on directories

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
            free(input);
            free_tokens(tokens);
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
        }else if (strcmp(tokens->items[0], "rm")  == 0){
            if(tokens->items[1] != NULL){
                remove_file(tokens->items[1]);
            }else
                printf("ERROR: You must name a file to remove\n");

        }else if (strcmp(tokens->items[0], "rmdir") == 0){
            if(tokens->items[1] != NULL){
                if(strcmp(tokens->items[1], "-r") == 0)
                    remove_directory(tokens->items[2], "-r");
                else
                    remove_directory(tokens->items[1], " ");
            }else
                printf("ERROR: You must name a directory to remove\n");
        }else if (strcmp(tokens->items[0], "rename") == 0){
            if(tokens->items[1] != NULL && tokens->items[2] != NULL){
                rename_(tokens->items[1], tokens->items[2]);
            }else
                printf("ERROR: Incorrect # of arguments.\nUsage: rename [OLDNAME] [NEW]\n");
        }else if(strcmp(tokens->items[0], "write") == 0){
            int length = 0;
            int i;
            for(i = 2; i < tokens->size; i++){
                length += strlen(tokens->items[i]) + 2;
            }
            char* remainingTokens = malloc(sizeof(char) * length);
            for(i = 2; i < tokens->size; i++){
                strcat(remainingTokens, tokens->items[i]);
                strcat(remainingTokens, " ");
            }
            if(tokens->items[1] != NULL && tokens->items[2] != NULL){
                write(tokens->items[1], remainingTokens);
            }else
                printf("ERROR: Incorrect # of arguments.\nUsage: write [FILENAME] [STRING]\n");
            free(remainingTokens);
        }else if(strcmp(tokens->items[0], "dump") == 0){
            if(tokens->items[1] != NULL)
                dump(tokens->items[1]);
            else
                printf("ERROR: Specify a file you wish to dump the hex contents of\n");
        }
        //add_to_path(tokens->items[0]);      // move this out to its correct place; 
        free(input);
        free_tokens(tokens);
        printf("Freed tokens\n");
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
        fatEntryOffset = bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec + currCluster * 4;
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
       printf("ERROR: file or directory does not exist.\n");
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
        for(i = 0; i < 16; i++)
        {
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
        fread(&currCluster, sizeof(int), 1, fp);
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
                    fseek(fp, originalPos, SEEK_SET);
                    return;
                }
            }
            fatEntryOffset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec + (currCluster * 4));
            fseek(fp, fatEntryOffset, SEEK_SET);
            fread(&currCluster, sizeof(int), 1, fp);
        }
        expandCluster(cwd.cluster);
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
        remove_file(TO);
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
        if(wentIntoSubDir == 1){
            cd("..");
        }
        fseek(fp, originalPos, SEEK_SET);
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
        unsigned long byteSectorOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec; 
        fseek(fp, byteSectorOffset, SEEK_SET);
        for(i = 0; i < clusSize / 32; i++){ 
            fread(&tracker, sizeof(DirEntry), 1, fp);
            if(strcmp(tracker.DIR_Name, TO) == 0){ // Once dest file is found, begin copying
                unsigned int destinationCluster = (tracker.DIR_FstClusHi<<8) | tracker.DIR_FstClusLo;
                unsigned long destinationOffset = (firstDataSector + ((destinationCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                sourceOffset = (firstDataSector + ((sourceCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                fseek(toWriter, destinationOffset, SEEK_SET);
                fseek(fp, sourceOffset, SEEK_SET);
                int k;
                for(k = 0; k < totalClusters; k++){
                    expandCluster(destinationCluster);
                }
                int bytesWritten = 0;
                while(bytesWritten < source.DIR_FileSize){
                    char currByte;
                    int j;
                    for(j = 0; j < (clusSize); j++){
                        fread(&currByte, sizeof(char), 1, fp);
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
        fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
        fseek(fp, fatEntryOffset, SEEK_SET);
        fread(&currCluster, sizeof(int), 1, fp);
    }

    if(wentIntoSubDir == 1)
         cd("..");

    fclose(toWriter);
}
// Part 3 END

// Part 4 READ
void open(char* FILENAME, int FLAGS){
    int alreadyOpen = find(FILENAME);
    int i = 0;
    unsigned long originalPos = ftell(fp);

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

                strcpy(openFiles[i].path, cwd.path); 
                openFiles[i].offset = 0;
                openFiles[i].dirEntry = currEntry;
                openFiles[i].firstCluster = cluster;
                openFiles[i].firstClusterOffset = clusterOffset;
                openFiles[i].mode = FLAGS;

                int currCluster = cwd.cluster;
                int fatEntryOffset;
                DirEntry tracker;
                while(currCluster < bpb.BPB_TotSec32){
                    printf("cluster %d is smaller than total sectors\n", currCluster);
                    unsigned long byteOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                    int break_while = 0;
                    fseek(fp, byteOffset, SEEK_SET);
                    int j;
                    for(j = 0; j < clusSize / sizeof(DirEntry); j++){         
                        fread(&tracker, sizeof(DirEntry), 1, fp);
                        int k;
                        for(k = 0; k < 11; k++){
                            if(tracker.DIR_Name[k] == 0x20)
                                tracker.DIR_Name[k] = 0x00;
                        }        
                        if(strcmp(tracker.DIR_Name, FILENAME) == 0){  
                            openFiles[i].dirEntryOffset = ftell(fp) - sizeof(DirEntry);         
                            printf("Giving %s the dirEntryOffset of %lu\n", FILENAME, openFiles[i].dirEntryOffset);
                            break_while = 1;
                        }
                    }
                    if(break_while == 1)
                        break;
                    printf("end of cluster %d reached, chaining to cluster ", currCluster);
                    fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
                    fseek(fp, fatEntryOffset, SEEK_SET);
                    fread(&currCluster, sizeof(unsigned int), 1, fp);
                    printf("%d\n", currCluster);  
                }
                
                printf("Opened %s.\n", FILENAME);
                break;
            }
        }
        numFilesOpen++;
        fseek(fp, originalPos, SEEK_SET);
    }
}

void close(char* FILENAME){
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
        printf("ERROR: The file %s could not be found in the current working directory\n", FILENAME);
    }else
        printf("ERROR: %s is a directory\n", FILENAME);
}

void lsof(void){
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
        if((openFiles[i].mode == 0 || openFiles[i].mode == 2) && strcmp(openFiles[i].path, cwd.path) == 0 && strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0){
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

// Part 5 UPDATE
void write(char* FILENAME, char* STRING){
    int fileOpen = 1;
    int i;
    int targetFile;

    for(i = 0; i < 10; i++){
        if((openFiles[i].mode == 1 || openFiles[i].mode == 2) && strcmp(openFiles[i].path, cwd.path) == 0 && strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0){
            fileOpen = 0;
            targetFile = i;
        }
    }

    if(fileOpen == 1){
        printf("ERROR: No file named %s has been opened for writing.\n", FILENAME);
    }else{
        long originalPos = ftell(fp);
        char currChar;
        File* target = &openFiles[targetFile];
        int clustersChained = target->offset / (bpb.BPB_SecsPerClus * bpb.BPB_BytesPerSec);
        int byteInCurrentCluster = target->offset % (bpb.BPB_SecsPerClus * bpb.BPB_BytesPerSec);
        int currCluster = target->firstCluster;
        int currClusterOffset = target->firstClusterOffset;
        int fatEntryOffset;
        int charsWritten = 0;
        int chained = 0;
        int i;
        long currByte = byteInCurrentCluster;
        fseek(fp, currClusterOffset, SEEK_SET);
        int size = strlen(STRING);
        if(target->dirEntry.DIR_FileSize < target->offset + size){
            printf("write: This write will expand storage\n");
            int bytesNeeded = (target->offset + size) - target->dirEntry.DIR_FileSize;
            int extraClusters = (bytesNeeded + size) / (bpb.BPB_BytesPerSec * bpb.BPB_SecsPerClus);
            for(i = 0; i < extraClusters; i++)
                expandCluster(currCluster);
            target->dirEntry.DIR_FileSize += bytesNeeded;
            fseek(fp, target->dirEntryOffset, SEEK_SET);
            printf("write: Updating file entry at offset %lu to reflect extra %d bytes of storage\n", ftell(fp), bytesNeeded);
            fwrite(&target->dirEntry, sizeof(DirEntry), 1, fp);
        }
        
        while(charsWritten < size){
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
                    printf("write: After immediate chaining, currCluster is %d\n", currCluster);
                }
            }
            // If end of current cluster is reached:
            if(currByte % (bpb.BPB_SecsPerClus * bpb.BPB_BytesPerSec) == 0 && target->offset != 0){
                printf("write: Reached end of cluster at file offset %d\n", target->offset);
                fatEntryOffset = ((bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec) + (currCluster * 4));
                fseek(fp, fatEntryOffset, SEEK_SET);
                fread(&currCluster, sizeof(int), 1, fp);
                currClusterOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                fseek(fp, currClusterOffset, SEEK_SET);
            }
            fseek(fp, currClusterOffset + target->offset, SEEK_SET);
            printf("write: Writing a character now at file offset %d (%lu)\n", target->offset, ftell(fp));
            if(charsWritten < size)
                currChar = STRING[charsWritten++];
            else
                printf("Reached an out of bounds in STRING, index %d of 0-%d\n", charsWritten - 1, strlen(STRING) - 1);
            fwrite(&currChar, sizeof(char), 1, fp);
            currByte++;
            target->offset++;
        }
        fseek(fp, originalPos, SEEK_SET);
    }
}

void rename_(char* FILENAME, char* NEWFILENAME)
{
    int i,j;
    unsigned long originalPos = ftell(fp);
    fseek(fp, cwd.byteOffset, SEEK_SET);
    int currCluster = cwd.cluster;
    int fatEntryOffset;

    if (find(FILENAME) == -1)
    {
        printf("Error: %s does not exist", FILENAME);
        return;
    }
    else if (find(FILENAME) == 1 || find(FILENAME) == 0)
    {
        int break_while = 0;
        while(currCluster < bpb.BPB_TotSec32)
        {
            long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
            fseek(fp, byteOffsetOfCluster, SEEK_SET);
            for(i = 0; i < 16; i++)
            {
                fread(&currEntry, sizeof(DirEntry), 1, fp);
                if(currEntry.DIR_Attr == 0x0F || currEntry.DIR_Name[0] == 0x00)
                    continue;
                for(j = 0; j < 11; j++)
                {
                    if(currEntry.DIR_Name[j] == 0x20)
                        currEntry.DIR_Name[j] = 0x00;
                }
                if(strcmp(FILENAME, currEntry.DIR_Name) == 0)
                {
                    //currEntry.DIR_Name[0] = 0x00;
                    strcpy(currEntry.DIR_Name, NEWFILENAME);
                    //unsigned int clusterNumber = currEntry.DIR_FstClusLo + (currEntry.DIR_FstClusHi*65536);
                    fseek(fp, -32, SEEK_CUR);
                    fwrite(&currEntry, sizeof(DirEntry), 1, fp);
                    //clear_FAT(clusterNumber);
                    break_while = 1;
                    break;
                }
            }
            if (break_while){
                for(i = 0; i < 10; i++){
                    if(strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0 && strcmp(openFiles[i].path, cwd.path) == 0){
                        for(j = 0; j < 11; j++)
                            if(j < strlen(NEWFILENAME))
                                openFiles[i].dirEntry.DIR_Name[j]= NEWFILENAME[j];
                            else
                                openFiles[i].dirEntry.DIR_Name[j]= 0x0;
                        break;
                    }
                }
                break;
            }
            fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
            fseek(fp, fatEntryOffset, SEEK_SET);
            fread(&currEntry, sizeof(unsigned int), 1, fp);
        }
    }
    return;
}

// Part 5 END

// Part 6 DELETE

void remove_file(char* FILENAME)
{
    int i,j;
    unsigned long originalPos = ftell(fp);
    fseek(fp, cwd.byteOffset, SEEK_SET);
    int currCluster = cwd.cluster;
    int fatEntryOffset;

    if (find(FILENAME) == -1)
    {
        printf("Error: Directory does not exist\n");
        return;
    }
    else if (find(FILENAME) == 0)
    {
        printf("Error: %s is a directory, use rm command instead\n", FILENAME);
        return;
    }
    else if (find(FILENAME) == 1)
    {
        int break_while = 0;

        for(i = 0; i < 10; i++){
            if(strcmp(openFiles[i].dirEntry.DIR_Name, FILENAME) == 0 && strcmp(openFiles[i].path, cwd.path) == 0 && openFiles[i].mode != -1){
                printf("This file is opened and cannot be removed\n");
                return;
            }
        }

        while(currCluster < bpb.BPB_TotSec32)
        {
            long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
            fseek(fp, byteOffsetOfCluster, SEEK_SET);
            for(i = 0; i < 16; i++)
            {
                fread(&currEntry, sizeof(DirEntry), 1, fp);
                if(currEntry.DIR_Attr == 0x0F || currEntry.DIR_Name[0] == 0x00)
                    continue;

                for(j = 0; j < 11; j++)
                {
                    if(currEntry.DIR_Name[j] == 0x20)
                        currEntry.DIR_Name[j] = 0x00;
                }
                if(strcmp(FILENAME, currEntry.DIR_Name) == 0)
                {
                    close(FILENAME);
                    currEntry.DIR_Name[0] = 0x00;
                    unsigned int clusterNumber = currEntry.DIR_FstClusLo + (currEntry.DIR_FstClusHi*65536);
                    fseek(fp, -32, SEEK_CUR);
                    fwrite(&currEntry, sizeof(DirEntry), 1, fp);
                    if(clusterNumber > 2){
                        clear_FAT(clusterNumber);
                    }
                    break_while = 1;
                    break;
                }
            }
            if (break_while)
                break;
            fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
            fseek(fp, fatEntryOffset, SEEK_SET);
            fread(&currEntry, sizeof(unsigned int), 1, fp);
        }
    }
    fseek(fp, originalPos, SEEK_SET);
    return;
}

void remove_directory(char *DIRNAME, char* FLAGS)
{
    int i,j;
    int recursionOn;
    if(strcmp(FLAGS, "-r") == 0)
        recursionOn = 1;
    unsigned long originalPos = ftell(fp);
    fseek(fp, cwd.byteOffset, SEEK_SET);
    int currCluster = cwd.cluster;
    int fatEntryOffset;

    if (find(DIRNAME) == -1)
    {
        printf("Error: Directory does not exist");
        return;
    }
    else if (find(DIRNAME) == 1)
    {
        printf("Error: %s is a file, use rmdir command instead", DIRNAME);
        return;
    }
    else if (find(DIRNAME) == 0)
    {
        if(strcmp(DIRNAME, ".") == 0 || strcmp(DIRNAME, "..") == 0){
            printf("That is a special directory which may not be removed\n");
            return;
        }

        int break_while = 0;
        while(currCluster < bpb.BPB_TotSec32)
        {
            unsigned long byteOffsetOfCluster = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
            fseek(fp, byteOffsetOfCluster, SEEK_SET);
            for(i = 0; i < clusSize / sizeof(DirEntry); i++)
            {
                fread(&currEntry, sizeof(DirEntry), 1, fp);
                if(currEntry.DIR_Attr == 0x0F || currEntry.DIR_Name[0] == 0x00)
                    continue;

                for(j = 0; j < 11; j++)
                {
                    if(currEntry.DIR_Name[j] == 0x20)
                        currEntry.DIR_Name[j] = 0x00;
                }
                if(strcmp(DIRNAME, currEntry.DIR_Name) == 0)
                {
                    printf("FOUND\n");
                    unsigned int clusterNumber = currEntry.DIR_FstClusLo + (currEntry.DIR_FstClusHi*65536);
                    unsigned long parentPosition = ftell(fp);
                    int subCluster = clusterNumber;
                    DirEntry subEntry;
                    while(subCluster < bpb.BPB_TotSec32){
                        unsigned long byteOffsetOfSubCluster = (firstDataSector + ((subCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
                        fseek(fp, byteOffsetOfSubCluster, SEEK_SET);
                        for(j = 0; j < clusSize / sizeof(DirEntry); j++){
                            fread(&subEntry, sizeof(DirEntry), 1, fp);
                            int k;
                            for(k = 0; k < 11; k++)
                            {
                                if(subEntry.DIR_Name[k] == 0x20)
                                subEntry.DIR_Name[k] = 0x00;
                            }// below: if a valid directory entry that isn't . or .. or longname is found (i.e, the directory is NOT empty)
                            if(strcmp(subEntry.DIR_Name, ".") != 0 && strcmp(subEntry.DIR_Name, "..") != 0 && subEntry.DIR_Name[0] != 0x0 && subEntry.DIR_Attr != 0x0F){
                                if(recursionOn == 1){ // If recursion is on
                                    if(subEntry.DIR_Attr == 0x20){ // and current entry is a file
                                        cd(DIRNAME);
                                        remove_file(subEntry.DIR_Name); // Remove the file
                                        cd("..");
                                    }else if(subEntry.DIR_Attr == 0x10){ // else if current entry is a directory
                                        cd(DIRNAME);
                                        remove_directory(subEntry.DIR_Name, "-r"); // Recursively call to remove subdirectories
                                        cd("..");
                                    }
                                }else{ // If recursion is NOT on, return an error
                                    printf("ERROR: %s is not empty. For recursive directory deletion, use rmdir -r [DIRNAME]\n", DIRNAME);
                                    return;
                                }
                            }
                        }
                        fatEntryOffset = subCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
                        fseek(fp, fatEntryOffset, SEEK_SET);
                        fread(&subCluster, sizeof(unsigned int), 1, fp);
                    }
                    fseek(fp, parentPosition, SEEK_SET);
                    currEntry.DIR_Name[0] = 0x00;
                    fseek(fp, -sizeof(DirEntry), SEEK_CUR);
                    fwrite(&currEntry, sizeof(DirEntry), 1, fp);
                    printf("remove_file: First cluster is %d\n", clusterNumber);
                    clear_FAT(clusterNumber);
                    printf("CLEARED\n");
                    break_while = 1;
                    break;
                }
            }
            if (break_while == 1)
                break;
            fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
            fseek(fp, fatEntryOffset, SEEK_SET);
            fread(&currCluster, sizeof(unsigned int), 1, fp);
        }
    }
    fseek(fp, originalPos, SEEK_SET);
    return;
}

void clear_FAT(unsigned int clusterNumber)
{
    unsigned long originalPos = ftell(fp);
    unsigned int temp = clusterNumber;
    unsigned int temp2 = 0x0;

    while (temp < bpb.BPB_TotSec32)
    {
        fseek(fp, (bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec)+(temp*4), SEEK_SET);
        fread(&temp, sizeof(int), 1, fp);
        fseek(fp, -4, SEEK_CUR);
        printf("clear_FAT: clearing offset %lu with integer %d\n", ftell(fp), temp);
        fwrite(&temp2, sizeof(int), 1, fp);
    }

    fseek(fp, -4, SEEK_CUR);
    fwrite(&temp2, sizeof(int), 1, fp);

    fseek(fp, originalPos, SEEK_SET);
    return;
}

// Part 6 END

// Extra Credit START

void dump(char* FILENAME){
    int isFile = find(FILENAME);
    unsigned long originalPos = ftell(fp);
    unsigned short Hi = currEntry.DIR_FstClusHi;
    unsigned short Lo = currEntry.DIR_FstClusLo;
    unsigned int currCluster = (Hi<<8) | Lo;          

    if(currCluster == 0 || (currEntry.DIR_Attr == 0x20 &&currEntry.DIR_FileSize == 0)){ // Empty File
        printf("File contents are empty\n");
        return;
    }
    DirEntry tracker;
    int i; 
    int bytesRead = 0;
    int size = currEntry.DIR_FileSize;
    unsigned long fatEntryOffset;
    unsigned long byteOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec;
    fseek(fp, byteOffset, SEEK_SET);
    while(currCluster < bpb.BPB_TotSec32){
        byteOffset = (firstDataSector + ((currCluster - 2) * bpb.BPB_SecsPerClus)) * bpb.BPB_BytesPerSec; 
        if(bytesRead % (bpb.BPB_BytesPerSec * bpb.BPB_SecsPerClus) == 0 && bytesRead != 0)
            fseek(fp, byteOffset, SEEK_SET);

        if(isFile == 0){
            unsigned char currByte;
            for(i = 0; i < clusSize / sizeof(DirEntry); i++){
                fread(&currByte, sizeof(char), 1, fp);
                bytesRead++;
                printf("%02X", currByte);
                if(i % 2 == 0 && i != 0)
                    printf(" ");
            }
            printf("\n");
        }else if(isFile == 1){
            if(bytesRead >= size)
                break;
            unsigned char currByte;
            for(i = 0; i < bpb.BPB_BytesPerSec * bpb.BPB_SecsPerClus; i++){
                fread(&currByte, sizeof(char), 1, fp);
                bytesRead++;
                printf("%02X", currByte);
                if(i % 50 == 0 && i != 0)
                    printf("\n"); 
                else
                    printf(" ");
                if(bytesRead >= size)
                    break;
            }
        }
        if(bytesRead % (bpb.BPB_BytesPerSec * bpb.BPB_SecsPerClus) == 0 && bytesRead != 0){
            fatEntryOffset = currCluster * 4 + bpb.BPB_RsvdSecCnt * bpb.BPB_BytesPerSec;
            fseek(fp, fatEntryOffset, SEEK_SET);
            fread(&currCluster, sizeof(unsigned int), 1, fp);
            printf("\n");
        }
    }
    if(isFile == 1)
        printf("\n");
    fseek(fp, originalPos, SEEK_SET);
}

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
