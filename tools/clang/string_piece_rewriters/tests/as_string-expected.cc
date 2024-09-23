// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>


void RemoveAsStringInExplicitStringConstruction() {
  base::StringPiece piece = "Hello";
  std::string str{piece};
}

void RemoveAsStringWhenConstructingStringPiece() {
  auto* piece_ptr = new base::StringPiece("Hello");
  const base::StringPiece& piece_ref = *piece_ptr;
}

void RemoveAsStringForMembers() {
  base::StringPiece piece = "Hello";
  piece.begin();
  piece.cbegin();
  piece.end();
  piece.cend();
  piece.rbegin();
  piece.crbegin();
  piece.rend();
  piece.crend();
  piece.at(0);
  piece.front();
  piece.back();
  piece.size();

  auto* piece_ptr = &piece;
  piece_ptr->length();
  piece_ptr->max_size();
  piece_ptr->empty();
  piece_ptr->copy(nullptr, 0);
  piece_ptr->compare(std::string(*piece_ptr));
  piece_ptr->find('\0');
  piece_ptr->rfind('\0');
  piece_ptr->find_first_of('\0');
  piece_ptr->find_last_of('\0');
  piece_ptr->find_first_not_of('\0');
  piece_ptr->find_last_not_of('\0');
  piece_ptr->npos;

  // Negative tests, where simply removing as_string() is incorrect. It should
  // rather be replaced by an explicit std::string construction.
  std::string(piece).data();
  std::string(*piece_ptr).substr(0);
}

void RemoveAsStringForOperators() {
  base::StringPiece piece = "Hello";
  std::cout << piece;
  piece == "Hello";
  piece != "Hello";
  piece < "Hello";
  piece > "Hello";
  piece <= "Hello";
  piece >= "Hello";

  // Negative tests, where simply removing as_string() is incorrect. It should
  // rather be replaced by an explicit std::string construction.
  std::string(piece) += "Hello";
  std::string(piece) + "Hello";
  std::string(piece) = "Hello";
  std::string(piece)[0];
}

void RemoveAsStringWhenConstructingStringMember() {
  class S {
   public:
    explicit S(base::StringPiece piece) : str_(piece) {}

   private:
    std::string str_;
  };
}

void RewriteCStyleStringInitialization() {
  auto piece_ptr = std::make_unique<base::StringPiece>("Hello");
  const std::string str(*piece_ptr);
}

void ReplaceAsStringWithStringConstructor() {
  auto piece_ptr = std::make_unique<base::StringPiece>("Hello");
  std::string str = std::string(*piece_ptr).append(" World");
}
