// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

/****************************************************************************
** Meta object code from reading C++ file 'qt_shim.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <memory>
#include "ui/qt/qt_shim.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qt_shim.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_qt__QtShim_t {
  const uint offsetsAndSize[26];
  char stringdata0[151];
};
#define QT_MOC_LITERAL(ofs, len) \
  uint(offsetof(qt_meta_stringdata_qt__QtShim_t, stringdata0) + ofs), len
static const qt_meta_stringdata_qt__QtShim_t qt_meta_stringdata_qt__QtShim = {
    {
        QT_MOC_LITERAL(0, 10),   // "qt::QtShim"
        QT_MOC_LITERAL(11, 11),  // "FontChanged"
        QT_MOC_LITERAL(23, 0),   // ""
        QT_MOC_LITERAL(24, 4),   // "font"
        QT_MOC_LITERAL(29, 14),  // "PaletteChanged"
        QT_MOC_LITERAL(44, 7),   // "palette"
        QT_MOC_LITERAL(52, 11),  // "ScreenAdded"
        QT_MOC_LITERAL(64, 8),   // "QScreen*"
        QT_MOC_LITERAL(73, 6),   // "screen"
        QT_MOC_LITERAL(80, 13),  // "ScreenRemoved"
        QT_MOC_LITERAL(94, 25),  // "LogicalDotsPerInchChanged"
        QT_MOC_LITERAL(120, 3),  // "dpi"
        QT_MOC_LITERAL(124, 26)  // "PhysicalDotsPerInchChanged"

    },
    "qt::QtShim\0FontChanged\0\0font\0"
    "PaletteChanged\0palette\0ScreenAdded\0"
    "QScreen*\0screen\0ScreenRemoved\0"
    "LogicalDotsPerInchChanged\0dpi\0"
    "PhysicalDotsPerInchChanged"};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_qt__QtShim[] = {

    // content:
    10,     // revision
    0,      // classname
    0, 0,   // classinfo
    6, 14,  // methods
    0, 0,   // properties
    0, 0,   // enums/sets
    0, 0,   // constructors
    0,      // flags
    0,      // signalCount

    // slots: name, argc, parameters, tag, flags, initial metatype offsets
    1, 1, 50, 2, 0x08, 1 /* Private */, 4, 1, 53, 2, 0x08, 3 /* Private */, 6,
    1, 56, 2, 0x08, 5 /* Private */, 9, 1, 59, 2, 0x08, 7 /* Private */, 10, 1,
    62, 2, 0x08, 9 /* Private */, 12, 1, 65, 2, 0x08, 11 /* Private */,

    // slots: parameters
    QMetaType::Void, QMetaType::QFont, 3, QMetaType::Void, QMetaType::QPalette,
    5, QMetaType::Void, 0x80000000 | 7, 8, QMetaType::Void, 0x80000000 | 7, 8,
    QMetaType::Void, QMetaType::QReal, 11, QMetaType::Void, QMetaType::QReal,
    11,

    0  // eod
};

void qt::QtShim::qt_static_metacall(QObject* _o,
                                    QMetaObject::Call _c,
                                    int _id,
                                    void** _a) {
  if (_c == QMetaObject::InvokeMetaMethod) {
    auto* _t = static_cast<QtShim*>(_o);
    (void)_t;
    switch (_id) {
      case 0:
        _t->FontChanged((*reinterpret_cast<std::add_pointer_t<QFont>>(_a[1])));
        break;
      case 1:
        _t->PaletteChanged(
            (*reinterpret_cast<std::add_pointer_t<QPalette>>(_a[1])));
        break;
      case 2:
        _t->ScreenAdded(
            (*reinterpret_cast<std::add_pointer_t<QScreen*>>(_a[1])));
        break;
      case 3:
        _t->ScreenRemoved(
            (*reinterpret_cast<std::add_pointer_t<QScreen*>>(_a[1])));
        break;
      case 4:
        _t->LogicalDotsPerInchChanged(
            (*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])));
        break;
      case 5:
        _t->PhysicalDotsPerInchChanged(
            (*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])));
        break;
      default:;
    }
  }
}

const QMetaObject qt::QtShim::staticMetaObject = {
    {QMetaObject::SuperData::link<QObject::staticMetaObject>(),
     qt_meta_stringdata_qt__QtShim.offsetsAndSize, qt_meta_data_qt__QtShim,
     qt_static_metacall, nullptr,
     qt_incomplete_metaTypeArray<
         qt_meta_stringdata_qt__QtShim_t,
         QtPrivate::TypeAndForceComplete<QtShim, std::true_type>,
         QtPrivate::TypeAndForceComplete<void, std::false_type>,
         QtPrivate::TypeAndForceComplete<const QFont&, std::false_type>,
         QtPrivate::TypeAndForceComplete<void, std::false_type>,
         QtPrivate::TypeAndForceComplete<const QPalette&, std::false_type>,
         QtPrivate::TypeAndForceComplete<void, std::false_type>,
         QtPrivate::TypeAndForceComplete<QScreen*, std::false_type>,
         QtPrivate::TypeAndForceComplete<void, std::false_type>,
         QtPrivate::TypeAndForceComplete<QScreen*, std::false_type>,
         QtPrivate::TypeAndForceComplete<void, std::false_type>,
         QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
         QtPrivate::TypeAndForceComplete<void, std::false_type>,
         QtPrivate::TypeAndForceComplete<qreal, std::false_type>

         >,
     nullptr}};

const QMetaObject* qt::QtShim::metaObject() const {
  return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject()
                                    : &staticMetaObject;
}

void* qt::QtShim::qt_metacast(const char* _clname) {
  if (!_clname) {
    return nullptr;
  }
  if (!strcmp(_clname, qt_meta_stringdata_qt__QtShim.stringdata0)) {
    return static_cast<void*>(this);
  }
  if (!strcmp(_clname, "QtInterface")) {
    return static_cast<QtInterface*>(this);
  }
  return QObject::qt_metacast(_clname);
}

int qt::QtShim::qt_metacall(QMetaObject::Call _c, int _id, void** _a) {
  _id = QObject::qt_metacall(_c, _id, _a);
  if (_id < 0) {
    return _id;
  }
  if (_c == QMetaObject::InvokeMetaMethod) {
    if (_id < 6) {
      qt_static_metacall(this, _c, _id, _a);
    }
    _id -= 6;
  } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
    if (_id < 6) {
      *reinterpret_cast<QMetaType*>(_a[0]) = QMetaType();
    }
    _id -= 6;
  }
  return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
