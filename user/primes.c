#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define prime_num 35

void run_child(int *read_pipe) {
    close(read_pipe[1]);
    int n, status;
    if ((status = read(read_pipe[0], &n, sizeof(int))) == 0) { // fetch the first number in the row
        close(read_pipe[0]);  // no more numbers in the pipeline
        exit(0);
    } else if(status == -1){
        fprintf(2, "Parent: Fail to read from pipe!\n");
        exit(1);
    }
    printf("prime %d\n", n);  // print out the first number as the prime
    int parent_pipe[2];
    if (pipe(parent_pipe) == -1) {
        fprintf(2, "Failed to create pipe!\n");
        exit(1);
    }
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "Failed to create child process!\n");
        exit(1);
    } else if (pid == 0) {
        close(read_pipe[0]);
        run_child(parent_pipe);  // receive number in the new child process
    } else {
        close(parent_pipe[0]);
        int num;
        while ((status = read(read_pipe[0], &num, sizeof(int))) > 0) { 
            // filter and send numbers to new child process
            if (num % n != 0) {
                if(write(parent_pipe[1], &num, sizeof(int)) == -1){
                    fprintf(2, "Parent: Fail to write to pipe!\n");
                    exit(1);
                }
            }
        }
        if(status == -1){
            fprintf(2, "Parent: Fail to read from pipe!\n");
            exit(1);
        }
        close(parent_pipe[1]);
        wait((int *) 0);
    }
}

int main(int argc, char *argv[]) {
    int input_pipe[2];
    if (pipe(input_pipe) == -1) {
        fprintf(2, "Failed to create pipe!\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "Failed to create child process!\n");
        exit(1);
    } else if (pid == 0) {
        run_child(input_pipe); // child process to filter the numbers
    } else {
        close(input_pipe[0]);
        for (int i = 2; i <= prime_num; i++) { // send 2~35 to the child process
            if(write(input_pipe[1], &i, sizeof(int)) == -1){
                fprintf(2, "Parent: Fail to write to pipe!\n");
                exit(1);
            }
        }
        close(input_pipe[1]);
        wait((int *) 0);
    }
    exit(0);
}