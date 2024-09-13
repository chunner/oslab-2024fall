#include <kernel.h>
#define STRS_addr 0x59000000

#define MAX_NUM 20
#define MAX_LEN 10
void word2str(char *str, char word[MAX_NUM][MAX_LEN], int len);
int str_apart(char *str, char word[MAX_NUM][MAX_LEN]);
void myqsort(char word[MAX_NUM][MAX_LEN], int low, int high);
void swap(char word[MAX_NUM][MAX_LEN], int i, int j);
char *strcpy(char *dest, const char *src);
int strcmp(const char *str1, const char *str2);

int main(){
    char * str = (char *)STRS_addr;
    char word[MAX_NUM][MAX_LEN];   
    int len = str_apart(str, word);
    myqsort(word, 0, len - 1);
    word2str(str, word, len-1);
    bios_putstr(str);
    bios_putstr("-----------------------\n\r");
    return 0;
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
void myqsort(char word[MAX_NUM][MAX_LEN], int low, int high){
    if(low >= high)
        return;
    int mid = (low + high) / 2;
    swap(word, mid, low);
    int left_last = low;
    for(int i = low + 1; i <= high; i++){
        if(strcmp(word[i], word[low]) < 0){
            swap(word, ++ left_last, i);
        }
    }
    swap(word, low, left_last);
    myqsort(word, low, left_last - 1);
    myqsort(word, left_last + 1, high);
}
void swap(char word[MAX_NUM][MAX_LEN], int i, int j){
    char temp[MAX_LEN];
    strcpy(temp, word[i]);
    strcpy(word[i], word[j]);
    strcpy(word[j], temp);
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