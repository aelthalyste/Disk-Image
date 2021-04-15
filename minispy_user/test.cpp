//#include "restore.cpp"
//#include "platform_io.cpp"




// forces templated functions to be compiled
void
nar_compilation_force(){
    //restore_stream *test = InitFileRestoreStream(std::wstring(L"hello"), 0, 0, 0);    
}

#include <stdio.h>

int fact(int n){
    if(n > 0)
        return n*fact(n-1);
    return 1;
}

int main(){
	int sayi;
    printf("bir sayi giriniz : ");
    scanf("%d",&sayi);
    printf("factoriyel : %d\n", fact(sayi));
    return 0;
}



int mcbs(unsigned int n){
    unsigned int i = 0;
    
    // skip to first set bit
    while(n >>= 1 && i < 32) i++;
    unsigned int Result = 0;
    
    for(; i<32;i++){
        unsigned int c;
        unsigned int s = i;
        while(n>>=1 && i<32) {
            i++;
        }
        if(n){
            unsigned int l = i - s;
            if(l > Result) 
                Result = l;
        }
    }
    
    return Result;
}




