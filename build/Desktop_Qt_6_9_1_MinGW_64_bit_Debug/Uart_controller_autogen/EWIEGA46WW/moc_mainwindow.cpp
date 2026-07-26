/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../mainwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "onRefreshPorts",
        "",
        "onOpenClosePort",
        "onSendData",
        "onClearReceive",
        "onClearTransmit",
        "onSerialReadyRead",
        "onAutoSendTimeout",
        "showHomePage",
        "showSerialPage",
        "showProtocolPage",
        "showMotorPage",
        "showRehabPage",
        "onProtocolOpenClose",
        "onProtocolRefreshPorts",
        "onSendPing",
        "onSendStart",
        "onSendStop",
        "onSendFloatWrite",
        "onSendBatchWrite",
        "onClearProtocolLog",
        "onProtocolFrameParsed",
        "uint8_t",
        "cmd",
        "payload",
        "sumOk",
        "endOk",
        "rxSum",
        "calcSum",
        "onTelemetryReceived",
        "QMap<uint8_t,float>",
        "values",
        "onAckReceived",
        "ackCmd",
        "onIoReportReceived",
        "ioId",
        "value",
        "onProtocolRawReceived",
        "data",
        "onProtocolRawSent",
        "onProtocolStatsChanged",
        "totalFrames",
        "checksumErrors",
        "onProtocolParseError",
        "msg",
        "onProtocolPortError",
        "onProtocolPortOpened",
        "onProtocolPortClosed"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onRefreshPorts'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onOpenClosePort'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSendData'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearReceive'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearTransmit'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSerialReadyRead'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAutoSendTimeout'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showHomePage'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showSerialPage'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showProtocolPage'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showMotorPage'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showRehabPage'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProtocolOpenClose'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProtocolRefreshPorts'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSendPing'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSendStart'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSendStop'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSendFloatWrite'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSendBatchWrite'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearProtocolLog'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProtocolFrameParsed'
        QtMocHelpers::SlotData<void(uint8_t, const QByteArray &, bool, bool, uint8_t, uint8_t)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 23, 24 }, { QMetaType::QByteArray, 25 }, { QMetaType::Bool, 26 }, { QMetaType::Bool, 27 },
            { 0x80000000 | 23, 28 }, { 0x80000000 | 23, 29 },
        }}),
        // Slot 'onTelemetryReceived'
        QtMocHelpers::SlotData<void(const QMap<uint8_t,float> &)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 31, 32 },
        }}),
        // Slot 'onAckReceived'
        QtMocHelpers::SlotData<void(uint8_t)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 23, 34 },
        }}),
        // Slot 'onIoReportReceived'
        QtMocHelpers::SlotData<void(uint8_t, uint8_t)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 23, 36 }, { 0x80000000 | 23, 37 },
        }}),
        // Slot 'onProtocolRawReceived'
        QtMocHelpers::SlotData<void(const QByteArray &)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QByteArray, 39 },
        }}),
        // Slot 'onProtocolRawSent'
        QtMocHelpers::SlotData<void(const QByteArray &)>(40, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QByteArray, 39 },
        }}),
        // Slot 'onProtocolStatsChanged'
        QtMocHelpers::SlotData<void(int, int)>(41, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 42 }, { QMetaType::Int, 43 },
        }}),
        // Slot 'onProtocolParseError'
        QtMocHelpers::SlotData<void(const QString &)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 45 },
        }}),
        // Slot 'onProtocolPortError'
        QtMocHelpers::SlotData<void(const QString &)>(46, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 45 },
        }}),
        // Slot 'onProtocolPortOpened'
        QtMocHelpers::SlotData<void()>(47, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProtocolPortClosed'
        QtMocHelpers::SlotData<void()>(48, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onRefreshPorts(); break;
        case 1: _t->onOpenClosePort(); break;
        case 2: _t->onSendData(); break;
        case 3: _t->onClearReceive(); break;
        case 4: _t->onClearTransmit(); break;
        case 5: _t->onSerialReadyRead(); break;
        case 6: _t->onAutoSendTimeout(); break;
        case 7: _t->showHomePage(); break;
        case 8: _t->showSerialPage(); break;
        case 9: _t->showProtocolPage(); break;
        case 10: _t->showMotorPage(); break;
        case 11: _t->showRehabPage(); break;
        case 12: _t->onProtocolOpenClose(); break;
        case 13: _t->onProtocolRefreshPorts(); break;
        case 14: _t->onSendPing(); break;
        case 15: _t->onSendStart(); break;
        case 16: _t->onSendStop(); break;
        case 17: _t->onSendFloatWrite(); break;
        case 18: _t->onSendBatchWrite(); break;
        case 19: _t->onClearProtocolLog(); break;
        case 20: _t->onProtocolFrameParsed((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[6]))); break;
        case 21: _t->onTelemetryReceived((*reinterpret_cast< std::add_pointer_t<QMap<uint8_t,float>>>(_a[1]))); break;
        case 22: _t->onAckReceived((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1]))); break;
        case 23: _t->onIoReportReceived((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[2]))); break;
        case 24: _t->onProtocolRawReceived((*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 25: _t->onProtocolRawSent((*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 26: _t->onProtocolStatsChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 27: _t->onProtocolParseError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 28: _t->onProtocolPortError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 29: _t->onProtocolPortOpened(); break;
        case 30: _t->onProtocolPortClosed(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 31)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 31)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 31;
    }
    return _id;
}
QT_WARNING_POP
