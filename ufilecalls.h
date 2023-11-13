int f_open(const char *fname, int mode);
int printLL();
int f_read(int fd, int n, char* buf);
struct File *createFile(const char* name, int mode);