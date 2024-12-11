/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SHELL_BEGIN 20
#define MAX_NAME_LEN 44
#define MAX_ARGC 10
#define MAX_ARGV_LEN 32
#define MAX_NUM_BYTE 8
void shell_prompt() {
    printf("> root@UCAS_OS: ");
}

void parse_input(char *buffer) {
    int c, i = 0;
    while ((c = sys_getchar()) != '\n' && c != '\r') {
        if (c >= 0 && c <= 127) {       // the input should be ASCII
            if (c != '\b' && c != '\177') { // backspace
                buffer[i++] = c;
            } else if (i > 0) {
                i--;
            }
            printf("%c", c);
        }
    }
    buffer[i] = '\0';
    printf("\n");
}

void handle_ps_command() {
    sys_ps();
}

void handle_clear_command() {
    sys_screen_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}

void handle_exec_command(char *buffer) {
    char taskname[MAX_NAME_LEN];
    char *argv[MAX_ARGC];
    char argv_base[MAX_ARGV_LEN];
    int argc = 0, i = 5, argv_base_i = 0, waitpid_en = 1;

    for (; buffer[i] != ' ' && buffer[i] != '\0'; i++) {
        taskname[i - 5] = buffer[i];
    }
    taskname[i - 5] = '\0';
    argv[argc++] = taskname;// the first argv point to taskname

    if (buffer[i + 1] == '&') {
        waitpid_en = 0;
        i += 2;
    }

    while (buffer[i] != '\0') {
        i++;
        argv[argc++] = &argv_base[argv_base_i];
        while (buffer[i] != ' ' && buffer[i] != '\0') {
            argv_base[argv_base_i++] = buffer[i++];
        }
        argv_base[argv_base_i++] = '\0';
    }

    int pid = sys_exec(taskname, argc, argv);
    if (pid < 0) {
        printf("Info: fail to execute %s\n", taskname);
    } else {
        printf("Info: execute %s successfully, pid = %d ...\n", taskname, pid);
        if (waitpid_en) {
            sys_waitpid(pid);
        }
    }
}

void handle_waitpid_command(char *buffer) {
    char num_str[MAX_NUM_BYTE];
    int i = 8, num_str_idx = 0;
    while (buffer[i] >= '0' && buffer[i] <= '9' && num_str_idx < MAX_NUM_BYTE - 1) {
        num_str[num_str_idx++] = buffer[i++];
    }
    num_str[num_str_idx++] = '\0';
    int pid = atoi(num_str);
    sys_waitpid(pid);
    printf("Info: wait pid = %d ...\n", pid);
}

void handle_kill_command(char *buffer) {
    char num_str[MAX_NUM_BYTE];
    int i = 5, num_str_idx = 0;
    while (buffer[i] >= '0' && buffer[i] <= '9' && num_str_idx < MAX_NUM_BYTE - 1) {
        num_str[num_str_idx++] = buffer[i++];
    }
    num_str[num_str_idx++] = '\0';
    int pid = atoi(num_str);
    int retval = sys_kill(pid);
    if (retval == 1) {
        printf("Info: kill pid = %d successfully ...\n", pid);
    } else {
        printf("Info: fail to find pid = %d\n", pid);
    }
}

void handle_taskset_command(char *buffer) {
    int i = 8;
    char mask_str[16];
    char pid_str[MAX_NUM_BYTE];
    char taskname[MAX_NAME_LEN];
    int mask_str_idx = 0, pid_str_idx = 0, taskname_idx = 0;
    uint64_t mask;
    int pid, retval;
    if (buffer[i] == '-' && buffer[i + 1] == 'p') { // taskset -p mask pid
        i += 3;
        //----------------decoding mask
        while (buffer[i] != ' ') {
            mask_str[mask_str_idx++] = buffer[i++];
        }
        mask_str[mask_str_idx++] = '\0';
        mask = atoi(mask_str);
        i++;
        //---------------decoding pid
        while (buffer[i] >= '0' && buffer[i] <= '9' && pid_str_idx < MAX_NUM_BYTE - 1) {
            pid_str[pid_str_idx++] = buffer[i++];
        }
        pid_str[pid_str_idx++] = '\0';
        pid = atoi(pid_str);
        retval = sys_taskset(taskname, pid, mask, 1);
    } else {    // taskset mask taskname
        while (buffer[i] != ' ') {
            mask_str[mask_str_idx++] = buffer[i++];
        }
        mask_str[mask_str_idx++] = '\0';
        mask = atoi(mask_str);
        i++;
        while (buffer[i] != ' ' && buffer[i] != '\0') {
            taskname[taskname_idx++] = buffer[i++];
        }
        taskname[taskname_idx++] = '\0';
        retval = sys_taskset(taskname, 0, mask, 0);
    }
    if (retval == 0) {
        printf("Info: taskset successfully\n");
    } else {
        printf("Error: fail to find\n");
    }
}

void handle_mkfs_command() {
    int retval = sys_mkfs();
    if (retval == 0) {
        printf("Info: mkfs successfully\n");
    } else {
        printf("Error: mkfs failed\n");
    }
}
void handle_statfs_command() {
    sys_statfs();
}

void handle_cd_command(char *buffer) {
    char path[MAX_NAME_LEN];
    int i = 3, path_idx = 0;
    while (buffer[i] != '\0') {
        path[path_idx++] = buffer[i++];
    }
    path[path_idx++] = '\0';
    sys_cd(path);
}
void handle_mkdir_command(char *buffer) {
    char path[MAX_NAME_LEN];
    int i = 6, path_idx = 0;
    while (buffer[i] != '\0') {
        path[path_idx++] = buffer[i++];
    }
    path[path_idx++] = '\0';
    int retval = sys_mkdir(path);
    if (retval == 0) {
        printf("Info: mkdir successfully\n");
    } else {
        printf("Error: fail to mkdir\n");
    }
}

void handle_rmdir_command(char *buffer) {
    char path[MAX_NAME_LEN];
    int i = 6, path_idx = 0;
    while (buffer[i] != '\0') {
        path[path_idx++] = buffer[i++];
    }
    path[path_idx++] = '\0';
    int retval = sys_rmdir(path);
    if (retval == 0) {
        printf("Info: rmdir successfully\n");
    } else {
        printf("Error: fail to rmdir\n");
    }
}
void handle_ls_command(char *buffer) {
    char path[MAX_NAME_LEN];
    int i = 3, path_idx = 0;
    int option = 0;
    while (buffer[i] != '\0') {
        if (buffer[i] == ' ') {
            i++;
            if (buffer[i] == '-') {
                i++;
                if (buffer[i] == 'l') {
                    option = 1;
                }
                break;
            }
        }
        path[path_idx++] = buffer[i++];
    }
    path[path_idx++] = '\0';
    sys_ls(path, option);
}



int main(void) {
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");

    while (1) {
        shell_prompt();
        char buffer[50];
        parse_input(buffer);

        if (strcmp(buffer, "ps") == 0) {
            handle_ps_command();
        } else if (strcmp(buffer, "clear") == 0) {
            handle_clear_command();
        } else if (strncmp(buffer, "exec", 4) == 0) {
            handle_exec_command(buffer);
        } else if (strncmp(buffer, "waitpid", 7) == 0) {
            handle_waitpid_command(buffer);
        } else if (strncmp(buffer, "kill", 4) == 0) {
            handle_kill_command(buffer);
        } else if (strncmp(buffer, "taskset", 7) == 0) {
            handle_taskset_command(buffer);
        } else if (strncmp(buffer, "mkfs", 4) == 0) {
            handle_mkfs_command();
        } else if (strncmp(buffer, "statfs", 6) == 0) {
            handle_statfs_command();
        } else if (strncmp(buffer, "cd", 2) == 0) {
            handle_cd_command(buffer);
        } else if (strncmp(buffer, "mkdir", 5) == 0) {
            handle_mkdir_command(buffer);
        } else if (strncmp(buffer, "rmdir", 5) == 0) {
            handle_rmdir_command(buffer);
        } else if (strncmp(buffer, "ls", 2) == 0) {
            handle_ls_command(buffer);
        } else {
            printf("Error: Unknown Command '%s'!\n", buffer);
        }
        /************************************************************/
    // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

    // TODO [P6-task2]: touch, cat, ln, ls -l, rm
    /************************************************************/
    /************************************************************/
    }

    return 0;
}
