#ifndef LED_H
#define LED_H

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDebug>
#include <QThread>
#include <thread>

class Led {
public:
    Led();
    ~Led();

    void initLed();
    void openLed();
    void closeLed();
    void fastFlash();
    void slowFlash();
    void triggerLed(bool enable);
    void getPiModel();
    void setLedSpeed(const QString &speed);
    void flashLed();

private:
    std::thread flashThread;
    QString LedPath;
    bool LedStatus;
    QString LedSpeed;
    QString currentLedSpeed;
    int PiModel;
};

#endif // LED_H
