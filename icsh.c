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
#define MAX_JOBS 100

typedef enum { RUNNING, STOPPED, DONE } JobStatus;

typedef struct {
    int job_id;
    pid_t pid;
    char command[MAX_LINE];
    JobStatus status;
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;
int next_job_id = 1;

pid_t fg_pid = -1;
int last_exit_status = 0;

void add_job(pid_t pid, const char *cmd, JobStatus status) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].job_id = next_job_id++;
        jobs[job_count].pid = pid;
        jobs[job_count].status = status;
        strncpy(jobs[job_count].command, cmd, MAX_LINE);
        job_count++;
    }
}

void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; ++i) {
        if (jobs[i].pid == pid) {
            for (int j = i; j < job_count - 1; ++j)
                jobs[j] = jobs[j + 1];
            job_count--;
            break;
        }
    }
}

Job *find_job_by_id(int id) {
    for (int i = 0; i < job_count; ++i)
        if (jobs[i].job_id == id)
            return &jobs[i];
    return NULL;
}

Job *find_job_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; ++i)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

void print_job_status(Job *job) {
    printf("[%d] %s                 %s\n", job->job_id,
        job->status == RUNNING ? "Running" : (job->status == STOPPED ? "Stopped" : "Done"),
        job->command);
}

void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Job *job = find_job_by_pid(pid);
        if (!job) continue;
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->status = DONE;
            printf("\n[%d]+  Done                 %s\n", job->job_id, job->command);
            remove_job(pid);
        } else if (WIFSTOPPED(status)) {
            job->status = STOPPED;
            printf("\n[%d]+  Stopped              %s\n", job->job_id, job->command);
        } else if (WIFCONTINUED(status)) {
            job->status = RUNNING;
        }
    }
}

void handle_sigint(int sig) {
    if (fg_pid > 0) kill(-fg_pid, SIGINT);
}

void handle_sigtstp(int sig) {
    if (fg_pid > 0) kill(-fg_pid, SIGTSTP);
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

    signal(SIGCHLD, handle_sigchld);
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

        if (!fgets(line, MAX_LINE, input)) break;
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (strcmp(line, "!!") == 0) {
            if (strlen(last_command) == 0) continue;
            if (input == stdin) printf("%s\n", last_command);
            strcpy(line, last_command);
        } else {
            strcpy(last_command, line);
        }

        int bg = 0;
        if (line[strlen(line) - 1] == '&') {
            bg = 1;
            line[strlen(line) - 1] = '\0';
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
        } else if (strcmp(cmd, "jobs") == 0) {
            for (int i = 0; i < job_count; ++i) print_job_status(&jobs[i]);
        } else if (strcmp(cmd, "fg") == 0) {
            char *arg = strtok(NULL, " ");
            if (!arg || arg[0] != '%') continue;
            int id = atoi(arg + 1);
            Job *job = find_job_by_id(id);
            if (!job) continue;
            fg_pid = job->pid;
            kill(-fg_pid, SIGCONT);
            int status;
            waitpid(fg_pid, &status, WUNTRACED);
            fg_pid = -1;
        } else if (strcmp(cmd, "bg") == 0) {
            char *arg = strtok(NULL, " ");
            if (!arg || arg[0] != '%') continue;
            int id = atoi(arg + 1);
            Job *job = find_job_by_id(id);
            if (!job) continue;
            job->status = RUNNING;
            kill(-job->pid, SIGCONT);
            printf("[%d]+ %s &\n", job->job_id, job->command);
        } else {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                continue;
            } else if (pid == 0) {
                setpgid(0, 0);
                char *args[MAX_LINE / 2 + 1];
                int i = 0;
                args[i++] = cmd;
                char *token;
                while ((token = strtok(NULL, " ")) != NULL) {
                    args[i++] = token;
                }
                args[i] = NULL;
                execvp(cmd, args);
                perror("exec failed");
                exit(1);
            } else {
                setpgid(pid, pid);
                if (bg) {
                    add_job(pid, last_command, RUNNING);
                    printf("[%d] %d\n", next_job_id - 1, pid);
                } else {
                    fg_pid = pid;
                    int status;
                    waitpid(pid, &status, WUNTRACED);
                    if (WIFEXITED(status)) {
                        last_exit_status = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        last_exit_status = 128 + WTERMSIG(status);
                    } else if (WIFSTOPPED(status)) {
                        last_exit_status = 128 + WSTOPSIG(status);
                        add_job(pid, last_command, STOPPED);
                    } else {
                        last_exit_status = 1;
                    }
                    fg_pid = -1;
                }
            }
        }
    }

    if (input != stdin) fclose(input);
    return 0;
}
