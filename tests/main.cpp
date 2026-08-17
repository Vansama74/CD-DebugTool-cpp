// Shared entry point: runs every test class in one process so the three
// QTest classes (RS485, CRC32, IAP frame) can share a single test executable.
#include <QtTest>

int runTestRs485(int argc, char** argv);
int runTestCrc32(int argc, char** argv);
int runTestIapFrame(int argc, char** argv);
int runTestQingHai(int argc, char** argv);
int runTestScEtc(int argc, char** argv);
int runTestScMtc(int argc, char** argv);
int runTestScOl(int argc, char** argv);
int runTestShanDong(int argc, char** argv);
int runTestConfig(int argc, char** argv);
int runTestUpgradeEngine(int argc, char** argv);

int main(int argc, char** argv)
{
    int status = 0;
    status |= runTestRs485(argc, argv);
    status |= runTestCrc32(argc, argv);
    status |= runTestIapFrame(argc, argv);
    status |= runTestQingHai(argc, argv);
    status |= runTestScEtc(argc, argv);
    status |= runTestScMtc(argc, argv);
    status |= runTestScOl(argc, argv);
    status |= runTestShanDong(argc, argv);
    status |= runTestConfig(argc, argv);
    status |= runTestUpgradeEngine(argc, argv);
    return status;
}
