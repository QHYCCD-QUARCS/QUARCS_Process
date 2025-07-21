#include "led.h"
 
 
#define LED_PATH "/sys/class/leds/"
#define MODEL_PATH "/proc/device-tree/model"

// Led类的构造函数
Led::Led() {
    initLed();
    flashThread = std::thread(&Led::flashLed, this); // 创建新线程来执行flashLed方法
}

// Led类的析构函数
Led::~Led() {
    if (flashThread.joinable()) {
        flashThread.join(); // 如果线程可连接，则在析构函数中连接线程
    }
}

// *********** LED灯控制 ***********
void Led::initLed()
{
    // 初始化LED灯
    // 读取LED灯状态
    QDir directory(LED_PATH);
    QStringList leds = directory.entryList(QStringList() << "*", QDir::Dirs);
    foreach(QString led, leds) {
        if (led.contains("ACT")) {
            QFile file(LED_PATH + led + "/brightness");
            if (file.open(QIODevice::ReadOnly)) {
                QTextStream in(&file);
                QString line = in.readLine();
                LedPath = LED_PATH + led ;
                LedStatus = true;
                file.close();
                break;
            } else {
                qDebug() << "LED is not accessible.";
                LedStatus = false;
            }
        }
        
    }
    LedSpeed = "fast";
    currentLedSpeed = "";

}

void Led::openLed()
{
    QString command = "echo 1 | sudo tee "+LedPath+"/brightness";
    int result = system(command.toStdString().c_str());
    if (result != 0) {
        qDebug() << "Failed to open LED";
    }
}

void Led::closeLed()
{
    QString command = "echo 0 | sudo tee "+LedPath+"/brightness";
    int result = system(command.toStdString().c_str());
    if (result != 0) {
        qDebug() << "Failed to close LED";
    }
}

void Led::fastFlash()
{
    openLed();
    QThread::msleep(100); // 闪烁间隔为100毫秒
    closeLed();
    QThread::msleep(100); // 闪烁间隔为100毫秒
}

void Led::slowFlash()
{
    openLed();
    QThread::msleep(3000); // 闪烁间隔为500毫秒
    closeLed();
    QThread::msleep(1000); // 闪烁间隔为500毫秒
}

void Led::triggerLed(bool enable)
{
    QString command;
    if (enable) {
        // 启用默认触发器
        command = "echo mmc0 | sudo tee "+LedPath+"/trigger";
    } else {
        // 禁用触发器
        command = "echo none | sudo tee "+LedPath+"/trigger";
    }
    int result = system(command.toStdString().c_str());
    if (result == 0) {
        qDebug() << "Command executed successfully.";
    } else {
        qDebug() << "Command execution failed.";
    }
}



// 获取Pi型号
void Led::getPiModel()
{
    QFile file(MODEL_PATH);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in(&file);
            QString line = in.readLine();
            if (line.contains("Raspberry Pi 4")) {
                PiModel = 4;
            } else if (line.contains("Raspberry Pi 5")) {
                PiModel = 5;
            }
            // qDebug() << "Model:" << line;
            file.close();
        }
    } else {
        qDebug() << "Failed to open /proc/device-tree/model";
        PiModel = 4;
    }
}

void Led::setLedSpeed(const QString &speed)
{
    LedSpeed = speed;
}

void Led::flashLed()
{
    while (true) { // 循环执行
        if (LedSpeed == "fast") {
            fastFlash();
        } else if (LedSpeed == "slow") {
            slowFlash();
        }
        // QThread::msleep(1000); // 每次闪烁后，线程休眠1秒
    }
}

