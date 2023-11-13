#include "commands.h"
#include "helper-functions.h"
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
#include <ctype.h>

extern int mBlockSize;
extern int mFatSize;
extern int diskFile;
extern uint16_t *fat;

struct File
{
    const char* name;
    int fd;
    int mode;
    struct File *next;
    int writePointer;
    int readPointer;
};

struct File* files;

void addFile(struct File* newFile) {
    if (files == NULL) {
        files = newFile;
        return;
    }
    struct File* prev;
    int curPos = 0;
    struct File *cur;
    for (cur = files; cur != NULL; cur = cur->next) {
        // places new file in the position that its fd correesponds to
        if (curPos == newFile->fd) {
            if (prev == NULL) {
                files = newFile;
            } else {
                prev->next = newFile;
            }
            newFile->next = cur;
            break;
        }
        prev = cur;
        curPos++;
    }
    if (cur == NULL) {
        prev->next = newFile;
    }
}

int checkName(const char* name) {
    int len = strlen(name);
    for (int i=0; i < len; i++) {
        char cur = name[i];
        if (isalpha(cur) != 0 && isdigit(cur) != 0 && cur != '.' && cur != '_' && cur != '-') {
            return -1;
        }
    }
    return 0;
}

struct File *createFile(const char* name, int mode) {
    struct File *newFile = malloc(sizeof(struct File));
    // determining fd
    if (files == NULL) {
        newFile->fd = 0;
    } else {
        struct File *prev = NULL;
        struct File *cur;
        for (cur = files; cur != NULL; cur = cur->next) {
            // find the first open fd
            if (prev == NULL && cur->fd != 0) {
                newFile->fd = 0;
                break;
            }
            if (prev != NULL && prev->fd + 1 != cur->fd) {
                newFile->fd = prev->fd + 1;
                break;
            }
            prev = cur;
        }
        // reached end of linkedlist
        if (cur == NULL) {
            newFile->fd = prev->fd + 1;
        }
   }
   int check = checkName(name);
   if (check == -1) {
        perror("Please only use valid characters in file naemes!");
        free(newFile);
        return NULL;
   }
   newFile->name = name;
   newFile->mode = mode;
   newFile->writePointer = 0;
   newFile->readPointer = 0;
   newFile->next = NULL;
   addFile(newFile);
   return newFile;
}


// printing linked list of files
int printLL() {
    for (struct File *cur = files; cur != NULL; cur = cur->next) {
        printf("fd: %d", cur->fd);
        printf(" name: %s", cur->name);
        printf(" mode: %d\n", cur->mode);
    }
    return 0;
}

// assumes
// F_WRITE == 0
// F_READ == 1
// F_APPEND == 2

#define F_WRITE 0
#define F_READ 1
#define F_APPEND 2


#define FALSE 0
#define TRUE 1

struct File* lookupFile(const char* name) {
    if (files == NULL) {
        return NULL;
    }
    for (struct File *cur = files; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) {
            return cur;
        }
    }
    return NULL;
}

struct File* lookupFilebyFd(int fd) {
    if (files == NULL) {
        return NULL;
    }
    for (struct File *cur = files; cur != NULL; cur = cur->next) {
        if (cur->fd == fd) {
            return cur;
        }
    }
    return NULL;
}

int lookupWrite(const char* name) {
    if (files == NULL) {
        return FALSE;
    }
    for (struct File *cur = files; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) != 0 && cur->mode == F_WRITE) {
            return TRUE;
        }
    }
    return FALSE;
}


int f_open(const char *fname, int mode) {
    struct File* file;
    if (mode == F_WRITE) {
        int writeOpen = lookupWrite(fname);
        if (writeOpen == TRUE) {
            // implement error handling
            perror("Cannot open multiple file with the F_WRITE mode at the same time!");
            return -1;
        }
        
        file = lookupFile(fname);
        if (file == NULL) {
            file = createFile(fname, mode);
            // implement adding file to directory
            addFileToDirectory(fname, (uint32_t) 0, 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
        } else {
            // how to implement deleting the contents of a file?
            file->writePointer = 0;
            file->readPointer = 0;
        }
    } else if (mode == F_READ) {
        file = lookupFile(fname);
        if(file == NULL) {
            // iomplement error handling, like freeing mallocs?
            perror("File does not exist when trying to open with read mode!");
            return -1;
        }
        file->mode = F_READ;
    } else if (mode == F_APPEND) {
        file = lookupFile(fname);
        if(file == NULL) {
            file = createFile(fname, mode);
            addFileToDirectory(fname, (uint32_t) 0, 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
        } else {
            file->mode = mode;
        }
    } else {
        return -1;
    }
    if (file == NULL) {
        return -1;
    }
    return file->fd;
}



int readHelper(char* buf, int n, uint16_t curBlockNum, int startPos) {
    int spaceLeftInBlock = mBlockSize - startPos;
    lseek(diskFile, mFatSize, SEEK_SET);
    // going to block
    lseek(diskFile, (curBlockNum * mBlockSize), SEEK_CUR);
    // go to current read pos
    lseek(diskFile, startPos, SEEK_CUR);
    spaceLeftInBlock = (n < mBlockSize) ? n : mBlockSize;
    int charsRead = 0;

    // read first chars in block
    if ((charsRead = read(diskFile, buf, spaceLeftInBlock)) == -1) {
        perror("Error reading file!");
        return -1;
    }
    n -= charsRead;
    while (n > 0) {
        // do I need to seek to next block; implement
        curBlockNum = getNextBlock(curBlockNum+1);
        curBlockNum -= 1;
        lseek(diskFile, mFatSize, SEEK_SET);
        lseek(diskFile, (curBlockNum * mBlockSize), SEEK_CUR);

        spaceLeftInBlock = (n < mBlockSize) ? n : mBlockSize;
        startPos = 0;
        char* str = malloc(spaceLeftInBlock);
        int temp = read(diskFile, str, spaceLeftInBlock);
        memcpy(&buf[charsRead], str, temp);
        charsRead += temp;
        n -= temp;
        free(str);
    }
    return 0;

}

int f_read(int fd, int n, char* buf) {
    struct File* file = lookupFilebyFd(fd);
    if (file == NULL) {
        perror("Attempting to read from a non-existent file!");
        return -1;
    }
    // find where to start in the block
    int startPos = file->readPointer % mBlockSize;
    // find how many blocks need to travel to get to starting block
    int numBlocksToStartBlock = file->readPointer / mBlockSize;
    uint16_t currBlock = getFirstBlock(file->name);
    for (int i = 0; i < numBlocksToStartBlock; i++) {
        currBlock = getNextBlock(currBlock);
    }
    int ret = readHelper(buf, n, currBlock-1, startPos);
    if (ret == 0) {
        file->readPointer += n;
    }
    return ret;
}