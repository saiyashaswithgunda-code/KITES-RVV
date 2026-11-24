#include <gtest/gtest.h>
#include <QLoggingCategory>
#include <QApplication>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Set console output code page to UTF-8 (65001)
    SetConsoleOutputCP(65001);
#endif

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(themes);
    Q_INIT_RESOURCE(instructions);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}