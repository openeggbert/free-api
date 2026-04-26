#include "windows.h"

// WinMain entry bridge for legacy projects without explicit main() - Status: STUB
#if !defined(_WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

int main(int argc, char** argv)
{
    return FreeApiRunWinMain(&WinMain, argc, argv);
}
#endif