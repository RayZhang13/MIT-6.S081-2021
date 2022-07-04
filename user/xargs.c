#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLEN 1024

int main(int argc, char *argv[]) {
    char *_argv[MAXARG];  // exec最终需要执行的参数
    char buf[MAXLEN];  // 从标准输入传入的参数，以\0隔开
    char ch; //每次读取到一字节
    int stat;

    if (argc < 2) {
        fprintf(2, "usage: xargs command\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++){  // 把args后的参数读入参数列表
        _argv[i - 1] = argv[i];
    }

    while (1) { // 标准输入中还有数据
        int index = 0;    // buf尾指针，表示当前buf中填充到的位置
        int arg_begin = 0;  // 表示当前参数在buf中开始的位置
        int argv_cnt = argc - 1;
        while (1) { // 读取一行，到回车结束
            stat = read(0, &ch, 1);  //从标准输入中读取
            if (stat == 0)          // 没有数据则退出
                exit(0);
            if (ch == ' ' || ch == '\n') {
                buf[index++] = '\0';  //参数字符串之间用\0隔开
                _argv[argv_cnt++] = &buf[arg_begin]; //将当前参数首地址传递给参数数组
                arg_begin = index; //移动到下一个参数的头部
                if (ch == '\n') break; //如果遇到了回车，就跳出不读了
            } else {
                buf[index++] = ch; //将读取到字节写入buf
            }
        }
        _argv[argv_cnt] = (char *)0;  //结束标志
        if (fork() == 0) {
            exec(_argv[0], _argv);
        } else {
            wait((int *)0);
        }
    }
    exit(0);
}
