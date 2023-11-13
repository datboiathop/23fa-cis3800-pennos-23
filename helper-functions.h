#include <stdint.h>
#include <time.h>

uint16_t findOpenBlock();
int fileExists(char* fileName);
int addFileToDirectory(const char* fileName, uint32_t size, uint16_t firstBlock, uint8_t type, uint8_t perm, time_t mtime);
int directoryIsFull();
int updateTime(char* fileName, time_t time);
int changeFileName(char* fileName, char* destFile);
int removeFile(char* fileName);
uint16_t getFirstBlock(const char* fileName);
uint16_t getLastBlock(char* fileName);
uint16_t getNextBlock(uint16_t prevBlock);
int canWrite(char* filename);
int canRead(char* filename);
int setFirstBlock(char* fileName, uint16_t firstBlock);
int removeRecursive(uint16_t currentBlock);
uint16_t allocSetFirstBlock(char* fileName);
uint16_t allocNextBlock(uint16_t prevBlock);
int setSize(char* fileName, uint32_t size);
int getSize(char* fileName);