#include <kernel.h>
#define STRS_addr 0x59000000

#define MAX_NUM 20
#define MAX_LEN 12

void word2str(char *str, char word[MAX_NUM][MAX_LEN], int len);
int str_apart(char *str, char word[MAX_NUM][MAX_LEN]);


char *strcpy(char *dest, const char *src);
int strcmp(const char *str1, const char *str2);
void append(char word[MAX_NUM][MAX_LEN], int len);

int main(){
    char * str = (char *)STRS_addr;
    char word[MAX_NUM][MAX_LEN];   
    int len = str_apart(str, word);
    append(word, len - 1);
    len ++;
    word2str(str, word, len-1);
    bios_putstr(str);
    bios_putstr("-----------------------\n\r");
    return 0;
}
void append(char word[MAX_NUM][MAX_LEN], int len){
    for(int i = 0; i <= len; i ++){
        int j;
        for(j = 0; word[i][j] != '\0'; j++);
        for( ; j>=0; j--){
            word[i][j+2] = word[i][j];
        }
        word[i][0] = i + '0';
        word[i][1] = ':';
    }
    strcpy(word[len + 1], "aihuachun");
}

int str_apart(char *str, char word[MAX_NUM][MAX_LEN]){      // take apart the string to words
    int j = 0;
    int k = 0;
    for(int i=0;; i++){
        if(str[i] == '\n'){
            word[j][k++] = '\0';
            k = 0;
            j++;
        }else if (str[i] == '\0'){   
            break;
        }else{
            word[j][k++] = str[i];
        }
    }
    return j;
}
void word2str(char *str, char word[MAX_NUM][MAX_LEN], int len){
    int k = 0;
    for(int i = 0; i <= len ; i++){
        for(int j = 0; word[i][j] !='\0'; j++){
            str[k++] = word[i][j];
        }
        str[k++] = '\n';
    }
    str[k] = '\0';
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
int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}