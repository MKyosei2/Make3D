#include <Windows.h>
#include <string>

std::string convertToAnsi(const std::wstring& wide) {
    int len = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, 0);
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}