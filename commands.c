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
#include <time.h>

#define max(x, y) (x > y ? x : y)
#define min(x, y) (x < y ? x : y)

int diskFile = -1;
uint16_t *fat = NULL;
int mBlocks = -1;
int mBlockSize = -1;
int mFatSize = -1;
int mDataSize = -1;

int mkfs(char* fSName, int blocks, int blockSize) {
    //need to implement f_open function for this.
    int fd = open(fSName, O_RDWR | O_CREAT | O_TRUNC, 0644);
    
    if (fd == -1) {
        perror("error");
        return -1;
    }

    //get fatSize and dataRegionSize
    int multiple = 1;
    for (int i = 0; i < blockSize; i++) {
        multiple *= 2;
    }

    int fatSize = blocks * 256 * multiple;
    int dataRegionSize;
    if (blocks == 32 && blockSize == 4) {
        dataRegionSize = 256 * multiple * ((fatSize / 2) - 2);
    } else {
        //last block is empty since 0xFFFF is used as end marker
        dataRegionSize = 256 * multiple * ((fatSize / 2) - 1);
    }

    //fill disk file with null characters, assign certain size
    if (ftruncate(fd, fatSize + dataRegionSize) == -1) {
        perror("Disk file initialization error");
        close(diskFile);
        return -1;
    }

    //set msb and lsb
    int metaData[1];
    metaData[0] = (0xff00 & (blocks << 8)) | (0x00ff & blockSize);
    //write file system meta data to disk
    write(fd, metaData, 2);
    //assign 1st entry to 0xffff since directory ends in first block
    metaData[0] = 0xffff;
    write(fd, metaData, 2);
    close(fd);

    return 1;

}

int mount(char *fsName) {
    diskFile = open(fsName, O_RDWR, 0644);

    if(diskFile == -1) {
        perror("File not found");
        return -1;
    }

    //get MSB and LSB
    int metaData[1];
    read(diskFile, metaData, 1);
    mBlockSize = metaData[0];
    read(diskFile, metaData, 1);
    mBlocks = metaData[0];

    //set mFatSize and mDataSize
    int multiple = 1;
    for (int i = 0; i < mBlockSize; i++) {
        multiple *= 2;
    }

    //set fat size in bytes
    mFatSize = mBlocks * 256 * multiple;

    if (mBlockSize == 4 && mBlocks == 32) {
        //last block is empty since 0xFFFF is used as end marker
        mDataSize = 256 * multiple * ((mFatSize / 2) - 2);
        //decrement size so we cannot address the last block
        mFatSize -= 2;
    } else {
        mDataSize = 256 * multiple * ((mFatSize / 2) - 1);
    }

    //set block size in bytes
    mBlockSize = multiple * 256;

    //map the fat region to memory
    fat = mmap(NULL, mFatSize, PROT_READ | PROT_WRITE, MAP_SHARED, diskFile, 0);
    if (fat == NULL) {
        return -1;
    }
    return 1;
}

int unmount() {
    if (fat != NULL) {
        munmap(fat, mFatSize);
        perror("unmounted");
        fat = NULL;
        mBlocks = -1;
        mBlockSize = -1;
        mFatSize = -1;
        mDataSize = -1;
        close(diskFile);
    } else {
        return -1;
    }
    return 1;
}

int touch(char** fileNames, int lastFileIndex) {
    // for any file that needs to be created, first find an open block.
    // we update the fat for that block, then add that block to the directory
    // if the directory is already full, we find an open block, update the FAT

    for (int i = 1; i < lastFileIndex; i++) {
        if (fileExists(fileNames[i]) == 1) {
            //update the timestamp
            updateTime(fileNames[i], time(NULL));
        } else {
            addFileToDirectory(fileNames[i], (uint32_t) 0, 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
        }
    }
    return 1;
}

int mv(char* sourceFile, char* destFile) {
    //check if the file exists, if so, rename the file in the directory
    //NEED TO DELETE ANY FILE IN THE DISK THAT IS NAMED DESTFILE BEFORE
    changeFileName(sourceFile, destFile);
    return 1;
}

int rm(char** fileNames, int lastFileIndex) {
    //for each file in the list, remove it if it exists
    for (int i = 1; i < lastFileIndex; i++) {
        uint16_t firstBlock = getFirstBlock(fileNames[i]);
        if (firstBlock != 0) {
            removeFile(fileNames[i]);
        }
    }
    return 1;
}

int catWConcat(char** fileNames, int lastFileIndex, char* outputFileName) {
    //copy each file to new file
    if (outputFileName == NULL) {
        for (int i = 0; i < lastFileIndex; i++) {
            if (fileExists(fileNames[i]) == 0) {
                continue;
            }
            if (canRead(fileNames[i]) == 0) {
                continue;
            }
            uint32_t fileSize = getSize(fileNames[i]);
            uint32_t bytesLeft = fileSize;
            uint16_t blockFrom = getFirstBlock(fileNames[i]);
            char buffer[mBlockSize];
            while (bytesLeft > 0) {
                read(diskFile, buffer, min(mBlockSize, bytesLeft));
                write(STDOUT_FILENO, buffer, min(mBlockSize, bytesLeft));
                bytesLeft -= min(mBlockSize, bytesLeft);
                blockFrom = getNextBlock(blockFrom);
            }
        }
    } else {
        // //case where file argument is included
        // if (fileExists(outputFileName) == 1) {
        //     if (canWrite(outputFileName) == 1) {
        //         removeFile(outputFileName);
        //         addFileToDirectory(outputFileName, (uint32_t) 0, (uint16_t) 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
        //     } else {
        //         //file does not have write permissions, return
        //         return 0;
        //     }
        // } else {
        //     //create the file since it does not exist
        //     addFileToDirectory(outputFileName, (uint32_t) 0, (uint16_t) 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
        // }

        // uint32_t blockTo = allocSetFirstBlock(outputFileName);
        // uint32_t outputSize = 0;
        // for (int i = 0; i < lastFileIndex, i++) {
        //     if (fileExists(fileNames[i]) == 0) {
        //         continue;
        //     }
        //     if (canRead(fileNames[i]) == 0) {
        //         continue;
        //     }
        //     uint32_t fileSize = getSize(fileNames[i]);
        //     uint32_t bytesLeft = fileSize;
        //     uint16_t blockFrom = getFirstBlock(fileNames[i]);
        //     char buffer[mBlockSize];

        //     while (bytesLeft > 0) {
        //         if (firstBlockOutput == 0xFFFF) {
        //             allocSetFirstBlock
        //         }
        //         lseek(diskFile, mFatSize + (mBlockSize * (blockFrom - 1)), SEEK_SET);
        //         read(diskFile, buffer, min(outputSize % mBlockSize, bytesLeft));
        //         lseek(diskFile, mFatSize + (mBlockSize * (blockTo - 1)) + mBlockSize - bytesLeft, SEEK_SET);
        //         write(diskFile, buffer, min(outputSize % mBlockSize, bytesLeft));
        //         bytesLeft -= min(outputSize % mBlockSize, bytesLeft);
        //         blockFrom = getNextBlock(blockFrom);
        //         outputSize +=  min(outputSize % mBlockSize, bytesLeft);
        //         if (outputSize % mBlockSize == 0) {
        //             blockTo = allocNextBlock(blockTo);
        //         }
        //     }
        // }
        // if (outputSize == 0)
        // }
    }
    return 1;
}


int catAConcat(char** fileNames, int lastFileIndex, char* outputFileName) {
    return 1;
}

int catW(char* outputFileName) {

    //check if file does not exist
    if (fileExists(outputFileName) == 1) {
        if (canWrite(outputFileName) == 1) {
            removeFile(outputFileName);
            addFileToDirectory(outputFileName, (uint32_t) 0, (uint16_t) 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
        } else {
            //file does not have write permissions, return
            return 0;
        }
    } else {
        //create the file since it does not exist
        addFileToDirectory(outputFileName, (uint32_t) 0, (uint16_t) 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
    }


    uint16_t currentBlock = 0;
    uint32_t size = 0;
    uint32_t bytesRead = 0;
    uint32_t bytesLeft = mBlockSize;
    int i = 0;
    char buffer[mBlockSize];
    while ((bytesRead = read(STDIN_FILENO, buffer, bytesLeft)) > 0) {
        //allocate first block for file, requires updating the directory
        if (i == 0) {
            currentBlock = allocSetFirstBlock(outputFileName);
        }
        //adjust the pointer for the next block
        lseek(diskFile, mFatSize + (mBlockSize * (currentBlock - 1)) + mBlockSize - bytesLeft, SEEK_SET);
        //write buffer to the disk
        write(diskFile, buffer, bytesRead);
        bytesLeft -= bytesRead;
        if (bytesLeft == 0) {
            currentBlock = allocNextBlock(currentBlock);
            bytesLeft = mBlockSize;
        }
        i++;
        size += bytesRead;
    }
    setSize(outputFileName, size);
    return 1;

}

int catA(char* outputFileName) {
    //check if file does not exist
    if (fileExists(outputFileName) == 1) {
        if (canWrite(outputFileName) == 0) {
            //file does not have write permissions, return
            return 0;
        }
    } else {
        //create the file since it does not exist
        addFileToDirectory(outputFileName, (uint32_t) 0, 0xFFFF, (uint8_t) 1, (uint8_t) 6, time(NULL));
    }


    uint16_t currentBlock = 0;
    uint32_t sizeIn = 0;
    uint32_t sizeBefore = getSize(outputFileName);
    uint32_t bytesRead = 0;
    uint32_t bytesLeft = sizeBefore % mBlockSize;
    int i = 0;
    char buffer[mBlockSize];
    while ((bytesRead = read(STDIN_FILENO, buffer, bytesLeft)) > 0) {
        //allocate first block for file, requires updating the directory
        if (i == 0) {
            currentBlock = getLastBlock(outputFileName);
            if (currentBlock == 0xFFFF) {
                currentBlock = allocSetFirstBlock(outputFileName);
            }
        }
        //adjust the pointer for the next block
        lseek(diskFile, mFatSize + (mBlockSize * (currentBlock - 1)) + mBlockSize - bytesLeft, SEEK_SET);
        //write buffer to the disk
        write(diskFile, buffer, bytesRead);
        
        bytesLeft -= bytesRead;
        if (bytesLeft == 0) {
            currentBlock = allocNextBlock(currentBlock);
            bytesLeft = mBlockSize;
        }
        i++;
        sizeIn += bytesRead;
    }
    setSize(outputFileName, sizeBefore + sizeIn);
    return 1;
}

int cpToFS(char* sourceFile, char* destFile, int fromHost) {
    return 1;
}

int cpToH(char* sourceFile, char* destFile) {
    return 1;
}

int ls() {
    return 1;
}

int chmd(char* fileName) {
    return 1;
}