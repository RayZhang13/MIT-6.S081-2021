#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(2, "Usage: sleep <ticks>\n");  //参数数量不正确，打印错误信息
        exit(1);
    }
    int time = atoi(argv[1]);  //使用atoi转换，见user.h和ulib.c
    sleep(time);
    exit(0);
}