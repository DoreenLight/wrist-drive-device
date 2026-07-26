/****************************************************************************
** Meta object code from reading C++ file 'protocolworker.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../protocolworker.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'protocolworker.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ProtocolWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto ProtocolWorker::qt_create_metaobjectdata<qt_meta_tag_ZN14ProtocolWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ProtocolWorker",
        "frameParsed",
        "",
        "uint8_t",
        "cmd",
        "payload",
        "sumOk",
        "endOk",
        "rxSum",
        "calcSum",
        "telemetryReceived",
        "QMap<uint8_t,float>",
        "values",
        "ackReceived",
        "ackCmd",
        "ioReportReceived",
        "ioId",
        "value",
        "rawDataReceived",
        "data",
        "rawDataSent",
        "statsChanged",
        "totalFrames",
        "checksumErrors",
        "parseError",
        "message",
        "portError",
        "portOpened",
        "portClosed",
        "openPort",
        "portName",
        "baudRate",
        "dataBits",
        "parity",
        "stopBits",
        "flowControl",
        "closePort",
        "sendPing",
        "sendIOWrite",
        "sendFloatWrite",
        "floatId",
        "sendBatchWrite",
        "BatchWriteItems",
        "items",
        "sendFlashSave",
        "onReadyRead"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'frameParsed'
        QtMocHelpers::SignalData<void(uint8_t, const QByteArray &, bool, bool, uint8_t, uint8_t)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QByteArray, 5 }, { QMetaType::Bool, 6 }, { QMetaType::Bool, 7 },
            { 0x80000000 | 3, 8 }, { 0x80000000 | 3, 9 },
        }}),
        // Signal 'telemetryReceived'
        QtMocHelpers::SignalData<void(const QMap<uint8_t,float> &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 12 },
        }}),
        // Signal 'ackReceived'
        QtMocHelpers::SignalData<void(uint8_t)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 14 },
        }}),
        // Signal 'ioReportReceived'
        QtMocHelpers::SignalData<void(uint8_t, uint8_t)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 16 }, { 0x80000000 | 3, 17 },
        }}),
        // Signal 'rawDataReceived'
        QtMocHelpers::SignalData<void(const QByteArray &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 19 },
        }}),
        // Signal 'rawDataSent'
        QtMocHelpers::SignalData<void(const QByteArray &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 19 },
        }}),
        // Signal 'statsChanged'
        QtMocHelpers::SignalData<void(int, int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 22 }, { QMetaType::Int, 23 },
        }}),
        // Signal 'parseError'
        QtMocHelpers::SignalData<void(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Signal 'portError'
        QtMocHelpers::SignalData<void(const QString &)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Signal 'portOpened'
        QtMocHelpers::SignalData<void()>(27, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'portClosed'
        QtMocHelpers::SignalData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'openPort'
        QtMocHelpers::SlotData<void(const QString &, int, int, int, int, int)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 30 }, { QMetaType::Int, 31 }, { QMetaType::Int, 32 }, { QMetaType::Int, 33 },
            { QMetaType::Int, 34 }, { QMetaType::Int, 35 },
        }}),
        // Slot 'closePort'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'sendPing'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'sendIOWrite'
        QtMocHelpers::SlotData<void(uint8_t, uint8_t)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 16 }, { 0x80000000 | 3, 17 },
        }}),
        // Slot 'sendFloatWrite'
        QtMocHelpers::SlotData<void(uint8_t, float)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 40 }, { QMetaType::Float, 17 },
        }}),
        // Slot 'sendBatchWrite'
        QtMocHelpers::SlotData<void(const BatchWriteItems &)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 42, 43 },
        }}),
        // Slot 'sendFlashSave'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onReadyRead'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ProtocolWorker, qt_meta_tag_ZN14ProtocolWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ProtocolWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ProtocolWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ProtocolWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14ProtocolWorkerE_t>.metaTypes,
    nullptr
} };

void ProtocolWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ProtocolWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->frameParsed((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[6]))); break;
        case 1: _t->telemetryReceived((*reinterpret_cast< std::add_pointer_t<QMap<uint8_t,float>>>(_a[1]))); break;
        case 2: _t->ackReceived((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1]))); break;
        case 3: _t->ioReportReceived((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[2]))); break;
        case 4: _t->rawDataReceived((*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 5: _t->rawDataSent((*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 6: _t->statsChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->parseError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->portError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->portOpened(); break;
        case 10: _t->portClosed(); break;
        case 11: _t->openPort((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[6]))); break;
        case 12: _t->closePort(); break;
        case 13: _t->sendPing(); break;
        case 14: _t->sendIOWrite((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[2]))); break;
        case 15: _t->sendFloatWrite((*reinterpret_cast< std::add_pointer_t<uint8_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<float>>(_a[2]))); break;
        case 16: _t->sendBatchWrite((*reinterpret_cast< std::add_pointer_t<BatchWriteItems>>(_a[1]))); break;
        case 17: _t->sendFlashSave(); break;
        case 18: _t->onReadyRead(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 16:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< BatchWriteItems >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(uint8_t , const QByteArray & , bool , bool , uint8_t , uint8_t )>(_a, &ProtocolWorker::frameParsed, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(const QMap<uint8_t,float> & )>(_a, &ProtocolWorker::telemetryReceived, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(uint8_t )>(_a, &ProtocolWorker::ackReceived, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(uint8_t , uint8_t )>(_a, &ProtocolWorker::ioReportReceived, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(const QByteArray & )>(_a, &ProtocolWorker::rawDataReceived, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(const QByteArray & )>(_a, &ProtocolWorker::rawDataSent, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(int , int )>(_a, &ProtocolWorker::statsChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(const QString & )>(_a, &ProtocolWorker::parseError, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)(const QString & )>(_a, &ProtocolWorker::portError, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)()>(_a, &ProtocolWorker::portOpened, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolWorker::*)()>(_a, &ProtocolWorker::portClosed, 10))
            return;
    }
}

const QMetaObject *ProtocolWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ProtocolWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ProtocolWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ProtocolWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    return _id;
}

// SIGNAL 0
void ProtocolWorker::frameParsed(uint8_t _t1, const QByteArray & _t2, bool _t3, bool _t4, uint8_t _t5, uint8_t _t6)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4, _t5, _t6);
}

// SIGNAL 1
void ProtocolWorker::telemetryReceived(const QMap<uint8_t,float> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void ProtocolWorker::ackReceived(uint8_t _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ProtocolWorker::ioReportReceived(uint8_t _t1, uint8_t _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void ProtocolWorker::rawDataReceived(const QByteArray & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void ProtocolWorker::rawDataSent(const QByteArray & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void ProtocolWorker::statsChanged(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void ProtocolWorker::parseError(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void ProtocolWorker::portError(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void ProtocolWorker::portOpened()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ProtocolWorker::portClosed()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}
QT_WARNING_POP
