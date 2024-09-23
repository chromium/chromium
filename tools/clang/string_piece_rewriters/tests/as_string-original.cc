// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>


void RemoveAsStringInExplicitStringConstruction() {
  base::StringPiece piece = "Hello";
  std::string str{piece.as_string()};
}

void RemoveAsStringWhenConstructingStringPiece() {
  auto* piece_ptr = new base::StringPiece("Hello");
  const base::StringPiece& piece_ref = piece_ptr->as_string();
}

void RemoveAsStringForMembers() {
  base::StringPiece piece = "Hello";
  piece.as_string().begin();
  piece.as_string().cbegin();
  piece.as_string().end();
  piece.as_string().cend();
  piece.as_string().rbegin();
  piece.as_string().crbegin();
  piece.as_string().rend();
  piece.as_string().crend();
  piece.as_string().at(0);
  piece.as_string().front();
  piece.as_string().back();
  piece.as_string().size();

  auto* piece_ptr = &piece;
  piece_ptr->as_string().length();
  piece_ptr->as_string().max_size();
  piece_ptr->as_string().empty();
  piece_ptr->as_string().copy(nullptr, 0);
  piece_ptr->as_string().compare(piece_ptr->as_string());
  piece_ptr->as_string().find('\0');
  piece_ptr->as_string().rfind('\0');
  piece_ptr->as_string().find_first_of('\0');
  piece_ptr->as_string().find_last_of('\0');
  piece_ptr->as_string().find_first_not_of('\0');
  piece_ptr->as_string().find_last_not_of('\0');
  piece_ptr->as_string().npos;

  // Negative tests, where simply removing as_string() is incorrect. It should
  // rather be replaced by an explicit std::string construction.
  piece.as_string().data();
  piece_ptr->as_string().substr(0);
}

void RemoveAsStringForOperators() {
  base::StringPiece piece = "Hello";
  std::cout << piece.as_string();
  piece.as_string() == "Hello";
  piece.as_string() != "Hello";
  piece.as_string() < "Hello";
  piece.as_string() > "Hello";
  piece.as_string() <= "Hello";
  piece.as_string() >= "Hello";

  // Negative tests, where simply removing as_string() is incorrect. It should
  // rather be replaced by an explicit std::string construction.
  piece.as_string() += "Hello";
  piece.as_string() + "Hello";
  piece.as_string() = "Hello";
  piece.as_string()[0];
}

void RemoveAsStringWhenConstructingStringMember() {
  class S {
   public:
    explicit S(base::StringPiece piece) : str_(piece.as_string()) {}

   private:
    std::string str_;
  };
}

void RewriteCStyleStringInitialization() {
  auto piece_ptr = std::make_unique<base::StringPiece>("Hello");
  const std::string str = piece_ptr->as_string();
}

void ReplaceAsStringWithStringConstructor() {
  auto piece_ptr = std::make_unique<base::StringPiece>("Hello");
  std::string str = piece_ptr->as_string().append(" World");
}
