#include <iostream>
#include <string>
#include <string.h>

std::string
wstr2str(const std::wstring& s){
	char *c = (char*)malloc(s.size() + 1);
	memset(c, 0, s.size());
	
	size_t sr = wcstombs(c, s.c_str(), s.size() + 1);
	if(sr == 0xffffffffffffffffull - 1)
		memset(c, 0, s.size() + 1);
	std::string Result(c);
	free(c);
	return Result;
}


int main(){
	std::wstring volname = L"/dev/sda";
	char c = 2;
	volname += std::to_wstring(2);
	std::string r = wstr2str(volname);
	std::cout<<r;
	return 0;
}
