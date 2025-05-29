/* ICCS227: Project 1: icsh
 * Name: Panjapol Chayasana
 * StudentID: 6580059
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_LINE 1024

pid_t fg_pid = -1;
int last_exit_status = 0;

void handle_sigint(int sig) {
    if (fg_pid > 0) {
        kill(-fg_pid, SIGINT);
    }
}

void handle_sigtstp(int sig) {
    if (fg_pid > 0) {
        kill(-fg_pid, SIGTSTP);
    }
}

int main(int argc, char *argv[]) {
    FILE *input = stdin;
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("Failed to open script file");
            return 1;
        }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);

    char line[MAX_LINE];
    char last_command[MAX_LINE] = {0};

    if (input == stdin)
        printf("Starting IC shell\n");

    while (1) {
        if (input == stdin)
            printf("icsh $ ");
        fflush(stdout);

        if (!fgets(line, MAX_LINE, input)) {
            break;
        }

        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0) {
            continue;
        }

        if (strcmp(line, "!!") == 0) {
            if (strlen(last_command) == 0)
                continue;
            if (input == stdin) {
                printf("%s\n", last_command);
            }
            strcpy(line, last_command);
        } else {
            strcpy(last_command, line);
        }

        char *cmd = strtok(line, " ");
        if (!cmd) continue;

        if (strcmp(cmd, "echo") == 0) {
            char *arg = strtok(NULL, "");
            if (arg && strcmp(arg, "$?") == 0)
                printf("%d\n", last_exit_status);
            else if (arg) printf("%s\n", arg);
            else printf("\n");
            last_exit_status = 0;
        } else if (strcmp(cmd, "exit") == 0) {
            char *arg = strtok(NULL, " ");
            int exit_code = arg ? atoi(arg) % 256 : 0;
            if (input != stdin) fclose(input);
            printf("bye\n");
            exit(exit_code);
        } else {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                continue;
            } else if (pid == 0) {
                setpgid(0, 0);

                char *args[MAX_LINE / 2 + 1];
                int i = 0;
                int redirect_in = 0, redirect_out = 0;
                char *input_file = NULL, *output_file = NULL;

                args[i++] = cmd;
                char *token;
                while ((token = strtok(NULL, " ")) != NULL) {
                    if (strcmp(token, "<") == 0) {
                        redirect_in = 1;
                        input_file = strtok(NULL, " ");
                        if (!input_file) {
                            fprintf(stderr, "Syntax error: no input file\n");
                            exit(1);
                        }
                    } else if (strcmp(token, ">") == 0) {
                        redirect_out = 1;
                        output_file = strtok(NULL, " ");
                        if (!output_file) {
                            fprintf(stderr, "Syntax error: no output file\n");
                            exit(1);
                        }
                    } else {
                        args[i++] = token;
                    }
                }
                args[i] = NULL;

                if (redirect_in) {
                    int fd = open(input_file, O_RDONLY);
                    if (fd < 0) {
                        perror("Input file error");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                if (redirect_out) {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (fd < 0) {
                        perror("Output file error");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                execvp(cmd, args);
                perror("exec failed");
                exit(1);
            } else {
                fg_pid = pid;
                setpgid(pid, pid);
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (WIFEXITED(status)) {
                    last_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_exit_status = 128 + WTERMSIG(status);
                } else if (WIFSTOPPED(status)) {
                    last_exit_status = 128 + WSTOPSIG(status);
                } else {
                    last_exit_status = 1;
                }
                fg_pid = -1;
            }
        }
    }

    if (input != stdin)
        fclose(input);

    return 0;
}


