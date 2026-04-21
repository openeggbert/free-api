#include <windows.h>
#include <iostream>

int main() {
    DWORD start = GetTickCount();
    std::cout << "Starting Sleep(100)..." << std::endl;
    Sleep(100);
    DWORD end = GetTickCount();
    
    std::cout << "Elapsed time: " << (end - start) << " ms" << std::endl;
    
    if ((end - start) >= 100) {
        std::cout << "SUCCESS" << std::endl;
        return 0;
    } else {
        std::cout << "FAILURE" << std::endl;
        return 1;
    }
}
