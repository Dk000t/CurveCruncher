#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/resource.h>
#include <linux/limits.h>

#define PROGRAM "./y-cruncher"
#define RAM_PATH "/dev/shm/"
#define BUFFER_SIZE 256

char RAM_CFG_PATH[PATH_MAX] = {0};
FILE *RAM_CFG = NULL;

volatile sig_atomic_t stop_requested = 0;

void signal_handler(int sig) {
    stop_requested = 1;
}

void cleanup(void) {
    if (RAM_CFG) {
        fclose(RAM_CFG);
        RAM_CFG = NULL;
    }
    remove(RAM_CFG_PATH);
}

char *load_file_to_buffer(const char *file, size_t *size) {
    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "Error: unable to open original config\n");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    rewind(fp);

    char *buffer = malloc(*size);
    if (!buffer) {
        fprintf(stderr, "Error: unable to allocate buffer\n");
        fclose(fp);
        return NULL;
    }

    if (fread(buffer, 1, *size, fp) != *size) {
        fprintf(stderr, "Error: failed to read file completely\n");
        free(buffer);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return buffer;
}

int main (int argc, char **argv) {
    int opt;
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int physical_cores = num_cores / 2;
    int core_index = -1;
    char command[BUFFER_SIZE] = {0};
    char *FS_CFG = NULL;

    while ((opt = getopt(argc, argv, ":c:")) != -1) {
        switch (opt) {
            case 'c':
                FS_CFG = optarg;
                break;
            case ':':
                fprintf(stderr, "Error: The -%c option requires a configuration file.\n", optopt);
                exit(EXIT_FAILURE);
            case '?':
                fprintf(stderr, "Error: Unrecognized option -%c.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    if (!FS_CFG) {
        fprintf(stderr, "Error: Configuration file not specified.\n");
        fprintf(stderr, "Usage: %s -c <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (geteuid() != 0) {
        fprintf(stderr, "Error: Insufficient privileges.\n");
        fprintf(stderr, "Run the program as root: sudo %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (access(PROGRAM, F_OK) == -1) {
        fprintf(stderr, "Error: %s does not exist in the current path\n", PROGRAM);
        exit(EXIT_FAILURE);
    }

    if (access(FS_CFG, F_OK) == -1) {
        fprintf(stderr, "Error: %s does not exist in the current path\n", FS_CFG);
        exit(EXIT_FAILURE);
    }

    atexit(cleanup);

    size_t len = strlen(RAM_PATH) + strlen(FS_CFG) + 1;
    snprintf(RAM_CFG_PATH, len, "%s%s", RAM_PATH, FS_CFG);

    sigset_t new_mask, old_mask;
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGINT);
    sigaddset(&new_mask, SIGTERM);
    sigaddset(&new_mask, SIGTSTP);

    sigprocmask(SIG_BLOCK, &new_mask, &old_mask);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    size_t size;
    char *buffer = load_file_to_buffer(FS_CFG, &size);
    if (!buffer) {
        exit(EXIT_FAILURE);
    }

    RAM_CFG = fopen(RAM_CFG_PATH, "w");
    if (!RAM_CFG) {
        fprintf(stderr, "Error: unable to open .cfg in ram\n");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    fwrite(buffer, 1, size, RAM_CFG);
    free(buffer);

    fclose(RAM_CFG);
    RAM_CFG = NULL;

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    while (!stop_requested) {

        core_index = (core_index + 1) % physical_cores;
        snprintf(command, sizeof(command), "sed -i 's/LogicalCores : \\[.*\\]/LogicalCores : [%d]/g' %s", core_index, RAM_CFG_PATH);
        system(command);

        pid_t child = fork();
        if (child < 0) {
            fprintf(stderr, "Error: child process creation error");
            exit(EXIT_FAILURE);
        }

        if (child == 0) {
            signal(SIGINT,  SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if(setpriority(PRIO_PROCESS, 0, -10) != 0) {
                fprintf(stderr, "WARNING: unable to set process priority\n");
            }

            execl(PROGRAM, PROGRAM, "config", RAM_CFG_PATH, (char*)NULL);
            fprintf(stderr, "Error: child process failed to execute task");
            _exit(EXIT_FAILURE);
        }

        int status;
        while (waitpid(child, &status, 0) == -1) {
            if (errno == EINTR) {
                if (stop_requested) break;
            } else {
                fprintf(stderr, "Error in waitpid");
                break;
            }
        }

        if (WIFSIGNALED(status)) {
            int term_sig = WTERMSIG(status);
            if (term_sig == SIGINT || term_sig == SIGTERM || term_sig == SIGTSTP) {
                stop_requested = 1;
            }
        }
    }

    exit(EXIT_SUCCESS);
}
