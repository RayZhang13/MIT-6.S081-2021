#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path) {
    char *p;
    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p; //返回末尾的文件名
}

void find(char *directory, char *filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(directory, 0)) < 0) {
        fprintf(2, "ls: cannot open %s\n", directory);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "ls: cannot stat %s\n", directory);
        close(fd);
        return;
    }
    switch (st.type) { //查看路径类型
        case T_FILE: //是文件
            if (strcmp(fmtname(directory), filename) == 0) { //比较路径末尾文件名和查找的文件名
                printf("%s \n", directory);
            }
            break;
        case T_DIR: //是目录
            if (strlen(directory) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("find: path is too long \n");
                break;
            }
            strcpy(buf, directory);
            p = buf + strlen(buf);
            *p++ = '/'; //拼凑当前目录路径
            while (read(fd, &de, sizeof(de)) == sizeof(de)) { //遍历目录
                if (de.inum == 0 || strcmp(".", de.name) == 0 ||
                    strcmp("..", de.name) == 0) {
                    continue;
                }
                memmove(p, de.name, DIRSIZ); //拼凑当前文件路径
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, filename); //调用find递归查找
            }
            break;
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}