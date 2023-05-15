// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/****************************************************************************
** Meta object code from reading C++ file 'qt_shim.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.3.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include "ui/qt/qt_shim.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qt_shim.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.3.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_qt__QtShim_t {
  QByteArrayData data[6];
  char stringdata[52];
};
#define QT_MOC_LITERAL(idx, ofs, len)                                       \
  Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(                  \
      len, qptrdiff(offsetof(qt_meta_stringdata_qt__QtShim_t, stringdata) + \
                    ofs - idx * sizeof(QByteArrayData)))
static const qt_meta_stringdata_qt__QtShim_t qt_meta_stringdata_qt__QtShim = {
    {QT_MOC_LITERAL(0, 0, 10), QT_MOC_LITERAL(1, 11, 11),
     QT_MOC_LITERAL(2, 23, 0), QT_MOC_LITERAL(3, 24, 4),
     QT_MOC_LITERAL(4, 29, 14), QT_MOC_LITERAL(5, 44, 7)},
    "qt::QtShim\0FontChanged\0\0font\0"
    "PaletteChanged\0palette"};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_qt__QtShim[] = {

    // content:
    7,      // revision
    0,      // classname
    0, 0,   // classinfo
    2, 14,  // methods
    0, 0,   // properties
    0, 0,   // enums/sets
    0, 0,   // constructors
    0,      // flags
    0,      // signalCount

    // slots: name, argc, parameters, tag, flags
    1, 1, 24, 2, 0x08 /* Private */, 4, 1, 27, 2, 0x08 /* Private */,

    // slots: parameters
    QMetaType::Void, QMetaType::QFont, 3, QMetaType::Void, QMetaType::QPalette,
    5,

    0  // eod
};

void qt::QtShim::qt_static_metacall(QObject* _o,
                                    QMetaObject::Call _c,
                                    int _id,
                                    void** _a) {
  if (_c == QMetaObject::InvokeMetaMethod) {
    QtShim* _t = static_cast<QtShim*>(_o);
    switch (_id) {
      case 0:
        _t->FontChanged((*reinterpret_cast<const QFont(*)>(_a[1])));
        break;
      case 1:
        _t->PaletteChanged((*reinterpret_cast<const QPalette(*)>(_a[1])));
        break;
      default:;
    }
  }
}

const QMetaObject qt::QtShim::staticMetaObject = {
    {&QObject::staticMetaObject, qt_meta_stringdata_qt__QtShim.data,
     qt_meta_data_qt__QtShim, qt_static_metacall, 0, 0}};

const QMetaObject* qt::QtShim::metaObject() const {
  return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject()
                                    : &staticMetaObject;
}

void* qt::QtShim::qt_metacast(const char* _clname) {
  if (!_clname) {
    return 0;
  }
  if (!strcmp(_clname, qt_meta_stringdata_qt__QtShim.stringdata)) {
    return static_cast<void*>(const_cast<QtShim*>(this));
  }
  if (!strcmp(_clname, "QtInterface")) {
    return static_cast<QtInterface*>(const_cast<QtShim*>(this));
  }
  return QObject::qt_metacast(_clname);
}

int qt::QtShim::qt_metacall(QMetaObject::Call _c, int _id, void** _a) {
  _id = QObject::qt_metacall(_c, _id, _a);
  if (_id < 0) {
    return _id;
  }
  if (_c == QMetaObject::InvokeMetaMethod) {
    if (_id < 2) {
      qt_static_metacall(this, _c, _id, _a);
    }
    _id -= 2;
  } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
    if (_id < 2) {
      *reinterpret_cast<int*>(_a[0]) = -1;
    }
    _id -= 2;
  }
  return _id;
}
QT_END_MOC_NAMESPACE
