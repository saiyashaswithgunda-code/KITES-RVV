#include <gtest/gtest.h>
#include <QLoggingCategory>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Set console output code page to UTF-8 (65001)
    SetConsoleOutputCP(65001);
#endif

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}