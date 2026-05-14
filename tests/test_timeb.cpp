#include <sys/timeb.h>
#include <iostream>

int main()
{
    struct timeb tb;
    int ret = ftime(&tb);

    if (ret != 0) {
        std::cerr << "[timeb] ftime returned " << ret << ", expected 0\n";
        return 1;
    }
    if (tb.millitm >= 1000) {
        std::cerr << "[timeb] millitm=" << tb.millitm << " is out of range [0, 999]\n";
        return 1;
    }
    if (tb.time <= 0) {
        std::cerr << "[timeb] time=" << tb.time << " is not a valid epoch\n";
        return 1;
    }

    int ret_null = ftime(nullptr);
    if (ret_null != -1) {
        std::cerr << "[timeb] ftime(nullptr) returned " << ret_null << ", expected -1\n";
        return 1;
    }

    std::cout << "[timeb] SUCCESS: time=" << tb.time << " millitm=" << tb.millitm << "\n";
    return 0;
}
