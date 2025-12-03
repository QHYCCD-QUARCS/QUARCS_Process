/****************************************************************************
** Meta object code from reading C++ file 'quarcsmonitor.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/quarcsmonitor.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'quarcsmonitor.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_QuarcsMonitor_t {
    QByteArrayData data[25];
    char stringdata0[382];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_QuarcsMonitor_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_QuarcsMonitor_t qt_meta_stringdata_QuarcsMonitor = {
    {
QT_MOC_LITERAL(0, 0, 13), // "QuarcsMonitor"
QT_MOC_LITERAL(1, 14, 14), // "processUpdated"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 6), // "status"
QT_MOC_LITERAL(4, 37, 14), // "monitorProcess"
QT_MOC_LITERAL(5, 52, 24), // "checkQtServerInitSuccess"
QT_MOC_LITERAL(6, 77, 12), // "killQTServer"
QT_MOC_LITERAL(7, 90, 13), // "reRunQTServer"
QT_MOC_LITERAL(8, 104, 21), // "checkVueClientVersion"
QT_MOC_LITERAL(9, 126, 13), // "isForceUpdate"
QT_MOC_LITERAL(10, 140, 19), // "updateCurrentClient"
QT_MOC_LITERAL(11, 160, 11), // "fileVersion"
QT_MOC_LITERAL(12, 172, 11), // "forceUpdate"
QT_MOC_LITERAL(13, 184, 15), // "onUnzipFinished"
QT_MOC_LITERAL(14, 200, 8), // "exitCode"
QT_MOC_LITERAL(15, 209, 20), // "QProcess::ExitStatus"
QT_MOC_LITERAL(16, 230, 10), // "exitStatus"
QT_MOC_LITERAL(17, 241, 12), // "onUnzipError"
QT_MOC_LITERAL(18, 254, 22), // "QProcess::ProcessError"
QT_MOC_LITERAL(19, 277, 5), // "error"
QT_MOC_LITERAL(20, 283, 21), // "onUpdateProcessOutput"
QT_MOC_LITERAL(21, 305, 23), // "onUpdateProcessFinished"
QT_MOC_LITERAL(22, 329, 20), // "onUpdateProcessError"
QT_MOC_LITERAL(23, 350, 13), // "startQTServer"
QT_MOC_LITERAL(24, 364, 17) // "tryGetHostAddress"

    },
    "QuarcsMonitor\0processUpdated\0\0status\0"
    "monitorProcess\0checkQtServerInitSuccess\0"
    "killQTServer\0reRunQTServer\0"
    "checkVueClientVersion\0isForceUpdate\0"
    "updateCurrentClient\0fileVersion\0"
    "forceUpdate\0onUnzipFinished\0exitCode\0"
    "QProcess::ExitStatus\0exitStatus\0"
    "onUnzipError\0QProcess::ProcessError\0"
    "error\0onUpdateProcessOutput\0"
    "onUpdateProcessFinished\0onUpdateProcessError\0"
    "startQTServer\0tryGetHostAddress"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_QuarcsMonitor[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   94,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    0,   97,    2, 0x0a /* Public */,
       5,    0,   98,    2, 0x0a /* Public */,
       6,    0,   99,    2, 0x0a /* Public */,
       7,    0,  100,    2, 0x0a /* Public */,
       8,    1,  101,    2, 0x0a /* Public */,
       8,    0,  104,    2, 0x2a /* Public | MethodCloned */,
      10,    1,  105,    2, 0x0a /* Public */,
      12,    0,  108,    2, 0x0a /* Public */,
      13,    2,  109,    2, 0x0a /* Public */,
      17,    1,  114,    2, 0x0a /* Public */,
      20,    0,  117,    2, 0x0a /* Public */,
      21,    2,  118,    2, 0x0a /* Public */,
      22,    1,  123,    2, 0x0a /* Public */,
      23,    0,  126,    2, 0x0a /* Public */,
      24,    0,  127,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 15,   14,   16,
    QMetaType::Void, 0x80000000 | 18,   19,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 15,   14,   16,
    QMetaType::Void, 0x80000000 | 18,   19,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void QuarcsMonitor::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<QuarcsMonitor *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->processUpdated((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->monitorProcess(); break;
        case 2: _t->checkQtServerInitSuccess(); break;
        case 3: _t->killQTServer(); break;
        case 4: _t->reRunQTServer(); break;
        case 5: _t->checkVueClientVersion((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: _t->checkVueClientVersion(); break;
        case 7: _t->updateCurrentClient((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 8: _t->forceUpdate(); break;
        case 9: _t->onUnzipFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QProcess::ExitStatus(*)>(_a[2]))); break;
        case 10: _t->onUnzipError((*reinterpret_cast< QProcess::ProcessError(*)>(_a[1]))); break;
        case 11: _t->onUpdateProcessOutput(); break;
        case 12: _t->onUpdateProcessFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QProcess::ExitStatus(*)>(_a[2]))); break;
        case 13: _t->onUpdateProcessError((*reinterpret_cast< QProcess::ProcessError(*)>(_a[1]))); break;
        case 14: _t->startQTServer(); break;
        case 15: _t->tryGetHostAddress(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (QuarcsMonitor::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QuarcsMonitor::processUpdated)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject QuarcsMonitor::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_QuarcsMonitor.data,
    qt_meta_data_QuarcsMonitor,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *QuarcsMonitor::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QuarcsMonitor::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_QuarcsMonitor.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QuarcsMonitor::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void QuarcsMonitor::processUpdated(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
