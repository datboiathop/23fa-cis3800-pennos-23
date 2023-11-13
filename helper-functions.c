#include "commands.h"
#include "helper-functions.h"
#include "ufilecalls.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "parser.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

extern int diskFile;
extern uint16_t *fat;
extern int mBlocks;
extern int mBlockSize;
extern int mFatSize;
extern int mDataSize;

//openFile node in linked list
struct openFile
{
    int fd;
    int mode;
    struct openFile *next;
    int writePointer;
    int readPointer;
};

//returns -1 if there are no open blocks
uint16_t findOpenBlock() {
    for (uint16_t i = 1; i < (mFatSize / 2); i++) {
        if (fat[i] == 0) {
            return i;
        }
    }
    return -1;
}

int fileExists(char* fileName) {
    char name[32];
    //move to start of directory
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        //read the current name
        read(diskFile, name, 32);

        //check if file is in directory
        if(strcmp(name, fileName) == 0) {
            return 1;
        }

        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}


//NEED TO CREATE NEW DIRECTORY FILE IF CURRENT IS FULL, TAKE CARE OF THIS
int addFileToDirectory(const char* fileName, uint32_t size, uint16_t firstBlock, uint8_t type, uint8_t perm, time_t mtime) {
    lseek(diskFile, mFatSize, SEEK_SET);
    uint16_t firstEntry[1];

    //ensure that the string is null terminated
    char fileNameFixed[32];
    for (int i = 0; i < strlen(fileName); i++) {
        fileNameFixed[i] = fileName[i];
    }
    for (int i = strlen(fileName); i < 32; i++) {
        fileNameFixed[i] = '\0';
    }

    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, firstEntry, 1);
        lseek(diskFile, -1, SEEK_CUR);
        if (firstEntry[0] == 0 || firstEntry[0] == 1) {
            //write all the infomation to directory entry
            write(diskFile, fileNameFixed, 32);
            write(diskFile, &size, 4);
            write(diskFile, &firstBlock, 2);
            write(diskFile, &type, 1);
            write(diskFile, &perm, 1);
            write(diskFile, &mtime, 8);
            break;
        }
        lseek(diskFile, 64, SEEK_CUR);
    }

    createFile(fileName, 0);
    return 1;
}

int updateTime(char* fileName, time_t time) {
    char name[32];
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if file is in directory
        if(strcmp(name, fileName) == 0) {
            lseek(diskFile, 8, SEEK_CUR);
            write(diskFile, &time, 8);
            return 1;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}

int changeFileName(char* fileName, char* destFile) {
    if (strlen(destFile) > 32) {
        return -1;
    }
    char name[32];
    char newFileName[32];
    //create char array for destFile
    for (int i = 0; i < strlen(destFile); i++) {
        newFileName[i] = destFile[i];
    }
    for (int i = strlen(destFile); i < 32; i++) {
        newFileName[i] = '\0';
    }
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if file is in directory
        if(strcmp(name, fileName) == 0) {
            //push pointer back
            lseek(diskFile, -32, SEEK_CUR);
            //write new name
            write(diskFile, newFileName, 32);
            //reset file pointer
            return 1;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return -1;
}


int removeFile(char* fileName) {
    char name[32];
    uint16_t firstBlock = getFirstBlock(fileName);
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if file is in directory
        if(strcmp(name, fileName) == 0) {
            //push pointer back
            lseek(diskFile, -32, SEEK_CUR);
            //deleted entry need to change eventually with kernel integration
            uint16_t status = 1;
            write(diskFile, &status, 1);
            //set appropriate fat entries to 0
            removeRecursive(firstBlock);
            return 1;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return -1;
}

int removeRecursive(uint16_t currentBlock) {
    if (fat[currentBlock] == 0xFFFF) {
        fat[currentBlock] = 0;
        return 1;
    } else {
        uint16_t temp = fat[currentBlock];
        fat[currentBlock] = 0;
        return removeRecursive(temp);
    }
}

uint16_t getFirstBlock(const char* fileName) {
    char name[32];
    lseek(diskFile, mFatSize, SEEK_SET);
    uint16_t firstEntry[1];
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if current file is fileName
        if(strcmp(name, fileName) == 0) {
            //move pointer forward
            lseek(diskFile, 4, SEEK_CUR);
            //get the first blocl
            read(diskFile, firstEntry, 2);
            //reset file pointer
            return firstEntry[0];
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}

//get the next block
uint16_t getNextBlock(uint16_t prevBlock) {
    if (prevBlock == 0xFFFF) {
        return 0xFFFF;
    } else {
        return fat[prevBlock];
    }
}

//get the last block in a file
uint16_t getLastBlock(char* fileName) {
    uint16_t currentBlock = getFirstBlock(fileName);
    //if size is 0
    if (currentBlock == 0xFFFF) {
        return currentBlock;
    }

    while (fat[currentBlock] != 0xFFFF) {
        currentBlock = fat[currentBlock];
    }
    return currentBlock;
}

int canWrite(char* fileName) {
    char name[32];
    uint8_t perm;
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if file is in directory
        if(strcmp(name, fileName) == 0) {
            lseek(diskFile, 7, SEEK_CUR);
            read(diskFile, &perm, 1);
            if (perm == 2 || perm == 6 || perm == 7) {
                return 1;
            }
            return 0;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}

int canRead(char* fileName) {
    char name[32];
    uint8_t perm;
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if file is in directory
        if(strcmp(name, fileName) == 0) {
            lseek(diskFile, 7, SEEK_CUR);
            read(diskFile, &perm, 1);
            if (perm == 4 || perm==5 || perm == 6 || perm == 7) {
                return 1;
            }
            return 0;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}


int setFirstBlock(char* fileName, uint16_t firstBlock) {
    char name[32];
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if current file is fileName
        if(strcmp(name, fileName) == 0) {
            //move pointer forward
            lseek(diskFile, 4, SEEK_CUR);
            //get the first blocl
            write(diskFile, &firstBlock, 2);
            //reset file pointer
            return 1;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}

//updates firstBlock in disk, returns the first block
uint16_t allocSetFirstBlock(char* fileName) {
    uint16_t openBlock = findOpenBlock();
    //update the first block
    setFirstBlock(fileName, openBlock);
    //update the FAT
    fat[openBlock] = 0xFFFF;
    return openBlock;
}

//finds another open block, update fat
uint16_t allocNextBlock(uint16_t prevBlock) {
    uint16_t openBlock = findOpenBlock();
    fat[prevBlock] = openBlock;
    fat[openBlock] = 0xFFFF;
    return openBlock;
}

int setSize(char* fileName, uint32_t size) {
    char name[32];
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if current file is fileName
        if(strcmp(name, fileName) == 0) {
            //get the first block
            write(diskFile, &size, 4);
            //reset file pointer
            return 1;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}

int getSize(char* fileName) {
    char name[32];
    uint32_t size;
    lseek(diskFile, mFatSize, SEEK_SET);
    for (int i = 0; i < mBlockSize / 64; i++) {
        read(diskFile, name, 32);
        //check if current file is fileName
        if(strcmp(name, fileName) == 0) {
            //get the first block
            read(diskFile, &size, 4);
            //reset file pointer
            return 1;
        }
        //move to the next name in the directory
        lseek(diskFile, 32, SEEK_CUR);
    }
    return 0;
}