#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char buf[] = {'P'};

    int parent2child[2];
    int child2parent[2];
    if (pipe(parent2child) == -1) {
        fprintf(2, "Failed to create pipe parent2child\n");
        exit(1);
    }

    if (pipe(child2parent) == -1) {
        fprintf(2, "Failed to create pipe child2parent\n");
        exit(1);
    }

    int pid = fork();  // create child process
    if (pid < 0) {
        fprintf(2, "Failed to create the child process!\n");
        exit(1);
    } else if (pid == 0) {
        // child process, read from pipe parent2child,
        // write to pipe child2parent
        close(parent2child[1]);
        close(child2parent[0]);
        if (read(parent2child[0], buf, 1) != 1) {
            fprintf(2, "Child: Failed to read from pipe parent2child!\n");
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        if (write(child2parent[1], buf, 1) != 1) {
            fprintf(2, "Child: Failed to write to pipe child2parent!\n");
            exit(1);
        }
        exit(0);
    } else {
        // parent process, write to pipe parent2child,
        // read from pipe child2parent
        close(parent2child[0]);
        close(child2parent[1]);
        if (write(parent2child[1], buf, 1) != 1) {
            fprintf(2, "Parent: Failed to write to pipe parent2child!\n");
            exit(1);
        }
        if (read(child2parent[0], buf, 1) != 1) {
            fprintf(2, "Parent: Fail to read from pipe child2parent!\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
        exit(0);
    }
}