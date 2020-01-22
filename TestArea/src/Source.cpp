#include <iostream>

int main() {

	float val = (1 << 32);
	uint32_t t = 0;
	float val2 = ~(t);

	std::cout << val << "\t" << val2 << "\n";
	return 0;
}