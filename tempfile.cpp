#include <string.h>
#include <stdio.h>
#include <string>

int main() {
    
    std::wstring Result = L"randOM_draft";
        
    
    wchar_t t[8];
    swprintf(t, 8, L"%c", (char)'C');
    
    Result += L"-";
    Result += std::wstring(t);
    Result += L".extesino";
    printf("%S\n", Result.c_str());
    //printf("Generated binary file name %S\n", Result.c_str());
    return 0;
}
