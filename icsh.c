/* ICCS227: Project 1: icsh
 * Name:Panjapol Chayasana
 * StudentID:6580059
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024

int main() {
    char line[MAX_LINE];
    char last_command[MAX_LINE] = {0};

    printf("Starting IC shell\n");

    while (1) {
        printf("icsh $ ");
        fflush(stdout);

        if (!fgets(line, MAX_LINE, stdin)) {
            break;
        }

        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0) {
            continue;
        }

        if (strcmp(line, "!!") == 0) {
            if (strlen(last_command) == 0) {
                continue;
            }
            printf("%s\n", last_command);
            strcpy(line, last_command);
        } else {
            strcpy(last_command, line);
        }

        char *cmd = strtok(line, " ");
        if (!cmd) continue;

        if (strcmp(cmd, "echo") == 0) {
            char *arg = strtok(NULL, "");
            if (arg) printf("%s\n", arg);
            else printf("\n");
        } else if (strcmp(cmd, "exit") == 0) {
            char *arg = strtok(NULL, " ");
            int exit_code = arg ? atoi(arg) % 256 : 0;
            printf("bye\n");
            exit(exit_code);
        } else {
            printf("bad command\n");
        }
    }

    return 0;
}

