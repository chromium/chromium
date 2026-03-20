# Changelog

All notable changes to memo-map are documented here.

## 0.3.3

* Added `MemoMap::get_or_insert_owned` and `MemoMap::get_or_try_insert_owned`.

## 0.3.2

* Added `MemoMap::get_mut`, `MemoMap::iter_mut` and `MemoMap::values_mut`.

## 0.3.1

* Added `MemoMap::replace`.

## 0.3.0

* Box up individual values.  Previously this crate did not survive
  resizes of the map in all circumstances.  This no longer requiers
  `StableDeref` now at the cost of potential extra allocations.

## 0.2.1

* Fixed an incorrect statement in the readme.

## 0.2.0

* Added support for `remove` and `clear`.

## 0.1.0

* Initial release.
