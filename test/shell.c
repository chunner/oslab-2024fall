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

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");

    while (1)
    {
        printf("> root@UCAS_OS: ");
        // TODO [P3-task1]: call syscall to read UART port
        int c;
        char buffer[50];
        int i = 0;
        while ((c = sys_getchar()) != '\n' && c != '\r') {
            if (c >= 0 && c <= 127) {// if the input is not among ASCII
                if (c != '\b' && c != '\177') {     // backspace
                    buffer[i++] = c;
                } else if (i > 0) {
                    i--;
                }
                printf("%c", c);
            }
        }
        buffer[i] = '\0';
        printf("\n");
        // printf("buffer = %s\n", buffer);
        if (strcmp(buffer, "ps") == 0) {
            sys_ps();
        } else if (strcmp(buffer, "clear") == 0) {
            sys_screen_clear();
            sys_move_cursor(0, SHELL_BEGIN);
            printf("------------------- COMMAND -------------------\n");
        } else if (strncmp(buffer, "exec", 4) == 0) {
            char taskname[MAX_NAME_LEN];
            int i = 5;
            for (; buffer[i] != ' ' && buffer[i] != '\0'; i++) {
                taskname[i - 5] = buffer[i];
            }
            taskname[i - 5] = '\0';
            char *argv[MAX_ARGC];
            int argc = 0;
            argv[argc++] = taskname;    // the first arg is task name
            char argv_base[MAX_ARGV_LEN];
            int argv_base_i = 0;
            int waitpid_en = 1;
            if (buffer[i + 1] == '&') {
                waitpid_en = 0;
                i = i + 2;
            }
            while (buffer[i] != '\0') {
                i++;
                argv[argc++] = &argv_base[argv_base_i];
                while (buffer[i] != ' ' && buffer[i] != '\0') {
                    argv_base[argv_base_i++] = buffer[i++];
                }
            }
            int pid = sys_exec(taskname, argc, argv);
            if (pid < 0) {
                printf("Info: fail to excute %s\n", taskname);
            } else
                printf("Info: execute %s successfully, pid = %d ...\n", taskname, pid);
            if (waitpid_en) {
                sys_waitpid(pid);
            }
        } else if (strncmp(buffer, "waitpid", 7) == 0) {
            char num_str[MAX_NUM_BYTE];
            int num_str_idx = 0;
            int i = 8;
            while (buffer[i] > '0' && buffer[i] < '9' && num_str_idx < MAX_NUM_BYTE - 1) {
                num_str[num_str_idx++] = buffer[i++];
            }
            num_str[num_str_idx++] = '\0';
            int pid = atoi(num_str);
            sys_waitpid(pid);
            printf("Info: wait pid = %d ...\n", pid);
        } else if (strncmp(buffer, "kill", 4) == 0) {
            char num_str[MAX_NUM_BYTE];
            int num_str_idx = 0;
            int i = 5;
            while (buffer[i] > '0' && buffer[i] < '9' && num_str_idx < MAX_NUM_BYTE - 1) {
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
        } else if (strncmp(buffer, "taskset", 4) == 0) {
            int i = 5;
            char mask_str[16];
            int mask_str_idx = 0;
            uint64_t mask;
            char pid_str[MAX_NUM_BYTE];
            int pid_str_idx = 0;
            int pid;
            char taskname[MAX_NAME_LEN];
            int taskname_idx = 0;
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
                while (buffer[i] > '0' && buffer[i] < '9' && pid_str_idx < MAX_NUM_BYTE - 1) {
                    pid_str[pid_str_idx++] = buffer[i++];
                }
                pid_str[pid_str_idx++] = '\0';
                pid = atoi(pid_str);
                sys_taskset(taskname, pid, mask, 1);
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
                sys_taskset(taskname, pid, mask, 0);
            }
        } else {
            printf("Error: Unkown Command %s!\n", buffer);
        }

        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        // TODO [P3-task1]: ps, exec, kill, clear    

        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/
    }

    return 0;
}
