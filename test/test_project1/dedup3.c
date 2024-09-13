#include <kernel.h>
#define STRS_addr 0x59000000
#define MAX_NUM 20
#define MAX_LEN 10
void word2str(char *str, char word[MAX_NUM][MAX_LEN], int len);
int dedup(char word[MAX_NUM][MAX_LEN], int len);
void deleteword(char word[MAX_NUM][MAX_LEN], int i, int len);
int strcmp(const char *str1, const char *str2);
char *strcpy(char *dest, const char *src);
int str_apart(char *str, char word[MAX_NUM][MAX_LEN]);
int main(){
    char * str = (char *)STRS_addr;
    char word[MAX_NUM][MAX_LEN];   
    int len = str_apart(str, word);
    len = dedup(word, len - 1) + 1;
    word2str(str, word, len-1);
    bios_putstr(str);  
    bios_putstr("-----------------------\n\r");
    return 0; 
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
int dedup(char word[MAX_NUM][MAX_LEN], int len){
    int reallen = len;
    int i;
    for(i = 0; i <= reallen; i++){
        if(strcmp(word[i], word[i+1]) == 0){
            deleteword(word, i--, reallen--);
        }
    }
    return reallen;
}
void deleteword(char word[MAX_NUM][MAX_LEN], int i, int len){
    for(int j = i; j<len; j++){
        strcpy(word[j], word[j+1]);
    }
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
char *strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}