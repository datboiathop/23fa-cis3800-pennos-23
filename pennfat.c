#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "parser.h"
#include "commands.h"
#include <signal.h>
#include "ufilecalls.h"

int countArguments(struct parsed_command* parsed) {
    int i = 0;
    while(parsed->commands[0][i] != NULL) {
        i++;
    }
    return i;
}

int main(int argc, char** argv) {
    while (1) {
        // write prompt
        write(STDOUT_FILENO, PROMPT, strlen(PROMPT) + 1);

        // read input
        int maxLineLength = 4096;
        char cmd[maxLineLength];
        int numBytes = read(STDIN_FILENO, cmd, maxLineLength);
        //error check read function
        if (numBytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
            
            //if command d is pressed
        if (numBytes == 0) {
            write(STDOUT_FILENO, "\n", 2);
            break;
        }

        //add null character right after last letter of the command
        if (cmd[numBytes - 1] == '\n') {
            cmd[numBytes - 1] = '\0';
        } else {
            write(STDOUT_FILENO, "\n", 1);
            cmd[numBytes] = '\0';
            numBytes += 1;
        }

            
            // parse input
        struct parsed_command *parsed;
        int val = parse_command(cmd, &parsed);
        if (val < 0) {
            perror("parse_command");
            return 1;
        }
        if (val > 0) {
            printf("syntax error\n");
            continue;
        }
        //check if no commands were entered
        if (parsed->num_commands == 0) {
            free(parsed);
            continue;
        }

        if (strcmp(parsed->commands[0][0], "mkfs") == 0) {
            mkfs(parsed->commands[0][1], atoi(parsed->commands[0][2]), atoi(parsed->commands[0][3]));
        }

        if (strcmp(parsed->commands[0][0], "mount") == 0) {
            mount(parsed->commands[0][1]);
        }

        if (strcmp(parsed->commands[0][0], "unmount") == 0) {
            unmount();
        }

        if (strcmp(parsed->commands[0][0], "touch") == 0) {
            //count arguments in input
            int i = countArguments(parsed);

            if (i > 1) {
                touch(parsed->commands[0], i);
            }
        }

        if (strcmp(parsed->commands[0][0], "mv") == 0) {
            mv(parsed->commands[0][1], parsed->commands[0][2]);
        }

        if (strcmp(parsed->commands[0][0], "rm") == 0) {
            //count arguments in input
            int i = countArguments(parsed);

            if (i > 1) {
                rm(parsed->commands[0], i);
            }
        }

        if (strcmp(parsed->commands[0][0], "cat") == 0) {
            //get number of arguments
            int i = countArguments(parsed);

            //cat from terminal
            if (i > 1 && i < 4) {
                if (strcmp(parsed->commands[0][1], "-w") == 0) {
                    catW(parsed->commands[0][2]);
                } else if (strcmp(parsed->commands[0][1], "-w")) {
                    catA(parsed->commands[0][2]);
                }
                continue;
            }

            //cat from file(s)
            if (i >= 4) {
                if (strcmp(parsed->commands[0][i - 2], "-w") == 0) {
                    catWConcat(parsed->commands[0], i - 2, parsed->commands[0][i-1]);
                } else if (strcmp(parsed->commands[0][i - 2], "-w")) {
                    catAConcat(parsed->commands[0], i - 2, parsed->commands[0][i-1]);
                }
                continue;
            }
        }

        if (strcmp(parsed->commands[0][0], "cp") == 0) {
            //count arguments in input
            int i = countArguments(parsed);

            if (i == 3) {
                cpToFS(parsed->commands[0][2], parsed->commands[0][2], 0);
            } else if(i == 4) {
                if (strcmp(parsed->commands[0][1], "-h") == 0) {
                    cpToFS(parsed->commands[0][2], parsed->commands[0][2], 1);
                } else {
                    cpToH(parsed->commands[0][1], parsed->commands[0][3]);
                }
            }
        }

        // testing user read
        // if (strcmp(parsed->commands[0][0], "read") == 0) {
        //     char str[7];
        //     f_read(1, 6, str);
        //     str[6] = '\0';
        //     printf("Read: %s\n", str);
        // }

        // if (strcmp(parsed->commands[0][0], "read2") == 0) {
        //     char str[290];
        //     f_read(0, 289, str);
        //     str[289] = '\0';
        //     printf("Read: %s\n", str);
        // }
        
    }
}