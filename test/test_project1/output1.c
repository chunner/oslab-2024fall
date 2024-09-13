#include <kernel.h>
// #include<string.h>
#define STRS_addr 0x59000000
char *strcpy(char *dest, const char *src);

char output[] = "sister\n"
                "apple\n"
                "apple\n"
                "banana\n"
                "if\n"
                "else\n"
                "apple\n"
                "if\n\0";
int main(){     
    bios_putstr(output);
    bios_putstr("-----------------------\n\r");
    char * strs =  (char *)STRS_addr;
    strcpy(strs, output);
    return 0;
}

char *strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}