#include <QLoggingCategory>
#include <QCoreApplication>
#include <tcpsender.h>

int main(int argc, char *argv[])
{
    QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);

    QCoreApplication a(argc, argv);
    TCPSender tcps(nullptr);
    char testPayload [3] = {0x55,0x11,0x55};
    uint32_t testLength = 3;
    uint8_t testMode = SEND_MODE_WITHOUT_REPLY;
    tcps.add_job(testPayload,&testLength, testMode);
    tcps.add_job(testPayload,&testLength, testMode);
    tcps.add_job(testPayload,&testLength, testMode);
    tcps.add_job(testPayload,&testLength, testMode);

    //tcps.close_connection();

    return a.exec();
}
