// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/****************************************************************************
** Meta object code from reading C++ file 'qt_shim.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <memory>
#include "ui/qt/qt_shim.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qt_shim.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_qt__QtShim_t {
  QByteArrayData data[4];
  char stringdata0[29];
};
#define QT_MOC_LITERAL(idx, ofs, len)                                        \
  Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(                   \
      len, qptrdiff(offsetof(qt_meta_stringdata_qt__QtShim_t, stringdata0) + \
                    ofs - idx * sizeof(QByteArrayData)))
static const qt_meta_stringdata_qt__QtShim_t qt_meta_stringdata_qt__QtShim = {
    {
        QT_MOC_LITERAL(0, 0, 10),   // "qt::QtShim"
        QT_MOC_LITERAL(1, 11, 11),  // "FontChanged"
        QT_MOC_LITERAL(2, 23, 0),   // ""
        QT_MOC_LITERAL(3, 24, 4)    // "font"

    },
    "qt::QtShim\0FontChanged\0\0font"};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_qt__QtShim[] = {

    // content:
    8,      // revision
    0,      // classname
    0, 0,   // classinfo
    1, 14,  // methods
    0, 0,   // properties
    0, 0,   // enums/sets
    0, 0,   // constructors
    0,      // flags
    0,      // signalCount

    // slots: name, argc, parameters, tag, flags
    1, 1, 19, 2, 0x08 /* Private */,

    // slots: parameters
    QMetaType::Void, QMetaType::QFont, 3,

    0  // eod
};

void qt::QtShim::qt_static_metacall(QObject* _o,
                                    QMetaObject::Call _c,
                                    int _id,
                                    void** _a) {
  if (_c == QMetaObject::InvokeMetaMethod) {
    auto* _t = static_cast<QtShim*>(_o);
    Q_UNUSED(_t)
    switch (_id) {
      case 0:
        _t->FontChanged((*reinterpret_cast<const QFont(*)>(_a[1])));
        break;
      default:;
    }
  }
}

QT_INIT_METAOBJECT const QMetaObject qt::QtShim::staticMetaObject = {
    {QMetaObject::SuperData::link<QObject::staticMetaObject>(),
     qt_meta_stringdata_qt__QtShim.data, qt_meta_data_qt__QtShim,
     qt_static_metacall, nullptr, nullptr}};

const QMetaObject* qt::QtShim::metaObject() const {
  return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject()
                                    : &staticMetaObject;
}

void* qt::QtShim::qt_metacast(const char* _clname) {
  if (!_clname)
    return nullptr;
  if (!strcmp(_clname, qt_meta_stringdata_qt__QtShim.stringdata0))
    return static_cast<void*>(this);
  if (!strcmp(_clname, "QtInterface"))
    return static_cast<QtInterface*>(this);
  return QObject::qt_metacast(_clname);
}

int qt::QtShim::qt_metacall(QMetaObject::Call _c, int _id, void** _a) {
  _id = QObject::qt_metacall(_c, _id, _a);
  if (_id < 0)
    return _id;
  if (_c == QMetaObject::InvokeMetaMethod) {
    if (_id < 1)
      qt_static_metacall(this, _c, _id, _a);
    _id -= 1;
  } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
    if (_id < 1)
      *reinterpret_cast<int*>(_a[0]) = -1;
    _id -= 1;
  }
  return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
