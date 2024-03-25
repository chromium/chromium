// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "third_party/sqlite/fuzz/icu_codes.pb.h"
#include "third_party/sqlite/fuzz/sql_queries.pb.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"

using namespace sql_query_grammar;

#define CONV_FN(TYPE, VAR_NAME) std::string TYPE##ToString(const TYPE& VAR_NAME)

#define RETURN_IF_DISABLED_QUERY(TYPE)     \
  if (disabled_queries_.count(#TYPE) != 0) \
    return "";

namespace sql_fuzzer {

namespace {
constexpr uint32_t kMaxColumnNumber = 20;
#if !defined(FUZZ_FTS3)
constexpr uint32_t kMaxTableNumber = 8;
#endif
constexpr uint32_t kMaxSchemaNumber = 4;
constexpr uint32_t kMaxWindowNumber = 5;

constexpr uint32_t kMaxColumnConstraintNumber = 10;
constexpr uint32_t kMaxTableConstraintNumber = 10;

constexpr uint32_t kMaxIndexNumber = 10;

// should be less than kMaxTableNumber
constexpr uint32_t kMaxFTS3TableNumber = 2;

constexpr uint32_t kMaxStrLength =
    200;  // So these are readable and somewhat performant, keep a maximum
          // string length......

#if !defined(FUZZ_OMIT_SAVEPOINT)
constexpr uint32_t kMaxSavePointNumber = 10;
#endif

constexpr uint32_t kMaxViewNumber = 5;
constexpr uint32_t kMaxTriggerNumber = 10;

std::set<std::string> disabled_queries_;
}  // namespace

CONV_FN(Expr, expr);
CONV_FN(Select, select);
CONV_FN(TableOrSubquery, tos);
CONV_FN(FTS3Table, ft);
CONV_FN(FTS3NearQuery, fnq);
CONV_FN(FTS3AuxiliaryFn, faf);
CONV_FN(FTS3MatchFormat, fmf);
CONV_FN(DateAndTimeFn, sfn);
CONV_FN(ExprSchemaTableFn, estf);

// ~~~~Numbered values to string~~~

// WARNING does not include space at the end
CONV_FN(Column, col) {
  if (col.has_rowid() && col.rowid())
    return "rowid";
#if defined(FUZZ_FTS3)
  if (col.has_fts3_docid() && col.fts3_docid())
    return "docid";
  if (col.has_fts3_table())
    return FTS3TableToString(col.fts3_table());
#endif
  std::string ret("Col");
  ret += std::to_string(col.column() % kMaxColumnNumber);
  return ret;
}

// WARNING does not include space at the end
CONV_FN(Table, table) {
  std::string ret("Table");
#if defined(FUZZ_FTS3)
  // only fuzzing FTS3 tables, clamp to the max FTS3 table num.
  ret += std::to_string(table.table() & kMaxFTS3TableNumber);
  if (table.fts3_content_table())
    ret += "_content";
#else
  ret += std::to_string(table.table() % kMaxTableNumber);
#endif
  return ret;
}

// WARNING does not include space at the end
CONV_FN(Schema, schema) {
  if (schema.main()) {
    return "main";
  }
  if (schema.temp()) {
    return "temp";
  }
  std::string ret("Schema");
  ret += std::to_string(schema.schema() % kMaxSchemaNumber);
  return ret;
}

// WARNING does not include space at the end
CONV_FN(WindowName, win) {
  std::string ret("Window");
  ret += std::to_string(win.window_name() % kMaxWindowNumber);
  return ret;
}

// WARNING does not include space at the end
CONV_FN(ColumnConstraintName, cc) {
  std::string ret("ColConstraint");
  ret += std::to_string(cc.constraint_name() % kMaxColumnConstraintNumber);
  return ret;
}

// WARNING does not include space at the end
CONV_FN(TableConstraintName, tc) {
  std::string ret("TableConstraint");
  ret += std::to_string(tc.constraint_name() % kMaxTableConstraintNumber);
  return ret;
}

// WARNING does not include space at the end
CONV_FN(Index, index) {
  std::string ret("Index");
  ret += std::to_string(index.index() % kMaxIndexNumber);
  return ret;
}

#if !defined(FUZZ_OMIT_SAVEPOINT)
CONV_FN(SavePoint, sp) {
  std::string ret("SavePoint");
  ret += std::to_string(sp.savepoint_num() % kMaxSavePointNumber);
  return ret;
}
#endif

CONV_FN(View, v) {
  std::string ret("View");
  ret += std::to_string(v.view() % kMaxViewNumber);
  return ret;
}

CONV_FN(Trigger, t) {
  std::string ret("Trigger");
  ret += std::to_string(t.trigger() % kMaxTriggerNumber);
  return ret;
}

// ~~~~Utility functions~~~~

std::string AscDescToString(AscDesc a) {
  switch (a) {
    case ASCDESC_NONE:
      return " ";
    case ASCDESC_ASC:
      return "ASC ";
    case ASCDESC_DESC:
      return "DESC ";
    default:
      return " ";
  }
}

std::string StrToLower(std::string s) {
  std::transform(
      s.begin(), s.end(), s.begin(),
      [](unsigned char c) -> unsigned char { return std::tolower(c); });
  return s;
}

std::string StripTrailingUnderscores(std::string s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return ch != '_'; })
              .base(),
          s.end());
  return s;
}

// Converts underscores to spaces in a string.
// This is because many enums like SET_NULL will be displayed at SET NULL in
// the query, and I want to use protobuf's enum to string function to save time.
std::string EnumStrReplaceUnderscores(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) -> unsigned char {
                   if (c == '_')
                     return ' ';
                   return c;
                 });
  return s;
}

// Takes garbage data and produces a string, with quotes escaped.
// Caps the number of bytes received from the protobuf at kMaxStrLength.
// The final string could be as much as kMaxStrLength*2 as we added an extra
// single quote for every single quote in the input string.
std::string ConvertToSqlString(const std::string& s) {
  std::string ret;
  ret.reserve(kMaxStrLength * 2);
  for (size_t i = 0; i < s.length() && i < kMaxStrLength; i++) {
    ret += s[i];
    if (s[i] == '\'')
      ret += '\'';
  }
  return ret;
}

// WARNING does not include space
std::string BytesToHex(const std::string& str) {
  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  for (size_t i = 0; i < str.size() && i < kMaxStrLength; i++) {
    ss << std::setw(2) << static_cast<int>(str[i]);
  }
  return ss.str();
}

// WARNING no space at end
CONV_FN(ExprSchemaTable, st) {
  std::string ret;
  if (st.has_schema_name()) {
    ret += SchemaToString(st.schema_name());
    ret += ".";
  }
  ret += TableToString(st.table_name());
  return ret;
}

CONV_FN(ExprSchemaTableColumn, stc) {
  std::string ret;
  if (stc.has_schema()) {
    ret += SchemaToString(stc.schema());
    ret += ".";
  }
  if (stc.has_table()) {
    ret += TableToString(stc.table());
    ret += ".";
  }
  ret += ColumnToString(stc.col());
  return ret;
}

// WARNING does not include parentheses, nor a space at the end
CONV_FN(ColumnList, cl) {
  std::string ret = ColumnToString(cl.col());
  for (int i = 0; i < cl.extra_cols_size(); i++) {
    ret += ", ";
    ret += ColumnToString(cl.extra_cols(i));
  }
  return ret;
}

CONV_FN(ExprList, me) {
  std::string ret = ExprToString(me.expr());
  for (int i = 0; i < me.extra_exprs_size(); i++) {
    ret += ", ";
    ret += ExprToString(me.extra_exprs(i));
  }
  return ret;
}

// WARNING does not include space
CONV_FN(CollateType, collate_type) {
  std::string ct = CollateType_Name(collate_type);
  ct.erase(0, std::string("COLLATE_").length());
  return ct;
}

// WARNING does not include space
CONV_FN(IndexedColumn, ic) {
  std::string ret;
  if (ic.has_expr()) {
    ret += ExprToString(ic.expr());
  } else {
    ret += ColumnToString(ic.col());
  }
  ret += " ";
  if (ic.has_collate_type()) {
    ret += "COLLATE ";
    ret += CollateTypeToString(ic.collate_type());
    ret += " ";
  }
  ret += AscDescToString(ic.asc_desc());

  return ret;
}

CONV_FN(IndexedColumnList, ic_list) {
  std::string ret;
  ret += IndexedColumnToString(ic_list.indexed_col());
  for (int i = 0; i < ic_list.extra_indexed_cols_size(); i++) {
    ret += ", ";
    ret += IndexedColumnToString(ic_list.extra_indexed_cols(i));
  }
  return ret;
}

CONV_FN(SchemaTableAsAlias, staa) {
  std::string ret;
  ret += ExprSchemaTableToString(staa.schema_table());
  ret += " ";

  if (staa.has_as_table_alias()) {
    ret += "AS ";
    ret += TableToString(staa.as_table_alias());
    ret += " ";
  }
  return ret;
}

// ~~~~Expression stuff~~~~

// WARNING does not include space
CONV_FN(NumericLiteral, nl) {
  static constexpr char hex_digits[] = "0123456789ABCDEF";
  static constexpr char digits[] = "0123456789";
  std::string ret;
  if (nl.hex_digits_size() > 0) {
    ret += "0x";
    for (int i = 0; i < nl.hex_digits_size(); i++) {
      ret += hex_digits[nl.hex_digits(i) % sizeof(hex_digits)];
    }

    return ret;
  }
  for (int i = 0; i < nl.digits_size(); i++) {
    ret += digits[nl.digits(i) % sizeof(digits)];
  }
  if (nl.decimal_point()) {
    ret += ".";
    if (nl.dec_digits_size() == 0) {
      ret += "0";
    } else {
      for (int i = 0; i < nl.dec_digits_size(); i++) {
        ret += digits[nl.dec_digits(i) % sizeof(digits)];
      }
    }
  }
  if (nl.exp_digits_size() > 0) {
    ret += "E";
    if (nl.negative_exp())
      ret += "-";
    if (nl.exp_digits_size() == 0) {
      ret += "0";
    } else {
      for (int i = 0; i < nl.exp_digits_size(); i++) {
        ret += digits[nl.exp_digits(i) % sizeof(digits)];
      }
    }
  }

  return ret;
}

// WARNING does not include space
CONV_FN(LiteralValue, lit_val) {
  std::string ret;
  using LitValType = LiteralValue::LitValOneofCase;
  switch (lit_val.lit_val_oneof_case()) {
    case LitValType::kNumLit:
      return std::to_string(lit_val.num_lit());
    case LitValType::kStringLit:
      ret += '\'';
      ret += ConvertToSqlString(lit_val.string_lit());
      ret += '\'';
      return ret;
    case LitValType::kBlobLit:
      ret += "x\'";
      ret += BytesToHex(lit_val.blob_lit());
      ret += '\'';
      return ret;
    case LitValType::kSpecialVal:
      // special case for NULL, TRUE, FALSE
      if (lit_val.special_val() == LiteralValue::VAL_NULL)
        return "NULL";
      if (lit_val.special_val() == LiteralValue::VAL_TRUE)
        return "TRUE";
      if (lit_val.special_val() == LiteralValue::VAL_FALSE)
        return "FALSE";
      // do not remove underscores
      return LiteralValue_SpecialVal_Name(lit_val.special_val());
    case LitValType::kNumericLit:
      return NumericLiteralToString(lit_val.numeric_lit());
    default:
      return "1";
  }
}

CONV_FN(UnaryExpr, uexpr) {
  std::string ret;
  switch (uexpr.unary_op()) {
    case UNOP_MINUS:
      ret += "-";
      break;
    case UNOP_PLUS:
      ret += "+";
      break;
    case UNOP_TILDE:
      ret += "~";
      break;
    case UNOP_NOT:
      ret += "NOT ";
      break;
    default:
      break;
  }
  ret += ExprToString(uexpr.expr());
  return ret;
}

CONV_FN(BinaryOperator, bop) {
  switch (bop) {
    case BINOP_CONCAT:
      return " || ";
    case BINOP_STAR:
      return " * ";
    case BINOP_SLASH:
      return " / ";
    case BINOP_PERCENT:
      return " % ";
    case BINOP_PLUS:
      return " + ";
    case BINOP_MINUS:
      return " - ";
    case BINOP_LELE:
      return " << ";
    case BINOP_GRGR:
      return " >> ";
    case BINOP_AMPERSAND:
      return " & ";
    case BINOP_PIPE:
      return " | ";
    case BINOP_LE:
      return " < ";
    case BINOP_LEQ:
      return " <= ";
    case BINOP_GR:
      return " > ";
    case BINOP_GREQ:
      return " >= ";
    case BINOP_EQ:
      return " = ";
    case BINOP_EQEQ:
      return " == ";
    case BINOP_NOTEQ:
      return " != ";
    case BINOP_LEGR:
      return " <> ";
    case BINOP_IS:
      return " IS ";
    case BINOP_ISNOT:
      return " IS NOT ";
    case BINOP_IN:
      return " IN ";  // CORPUS specialize?
    case BINOP_LIKE:
      return " LIKE ";  // CORPUS specialize?
    case BINOP_GLOB:
      return " GLOB ";  // CORPUS
    case BINOP_MATCH:
      return " MATCH ";  // CORPUS
    case BINOP_REGEXP:
      return " REGEXP ";  // CORPUS
    case BINOP_AND:
      return " AND ";
    case BINOP_OR:
      return " OR ";
    default:
      return " AND ";
  }
}

// TODO(mpdenton) generate better REGEXP queries in non-fts3 case. (in
// ColumnComparison as well)
// TODO(mpdenton) generate better MATCH queries in non-fts3 case.
CONV_FN(BinaryExpr, bexpr) {
  std::string ret;
  ret += ExprToString(bexpr.lhs());
  ret += BinaryOperatorToString(bexpr.op());
#if defined(FUZZ_FTS3)
  if (bexpr.op() == BINOP_MATCH && bexpr.has_fmt()) {
    ret += FTS3MatchFormatToString(bexpr.fmt());
    return ret;
  }
#endif
  ret += ExprToString(bexpr.rhs());
  return ret;
}

CONV_FN(AggregateFn, af) {
  std::string ret;
  ret += StrToLower(AggregateFn_FnName_Name(af.fn_name()));
  ret += "(";
  if (af.fn_name() == AggregateFn::COUNT && af.count_star())
    return ret + "*) ";
  if (af.distinct())
    ret += "DISTINCT ";
  if (af.has_col1()) {
    ret += ColumnToString(af.col1());
    if (af.fn_name() == AggregateFn::GROUP_CONCAT && af.has_col2()) {
      ret += ", ";
      ret += ColumnToString(af.col2());
    }
  } else {
    ret += ExprToString(af.expr1());
    if (af.fn_name() == AggregateFn::GROUP_CONCAT && af.has_expr2()) {
      ret += ", ";
      ret += ExprToString(af.expr2());
    }
  }

  ret += ") ";
  return ret;
}

CONV_FN(ZeroArgFn, zaf) {
  std::string func = ZeroArgFn_Name(zaf);
  // Remove ZFN_ prefix
  func.erase(0, std::string("ZFN_").length());
  return StrToLower(func) + "() ";
}

CONV_FN(OneArgFn, oaf) {
  std::string ret;
  ret += StripTrailingUnderscores(
      StrToLower(OneArgFn_OneArgFnEnum_Name(oaf.fn_enum())));
  ret += "(";
  ret += ExprToString(oaf.arg1());
  ret += ") ";
  return ret;
}

CONV_FN(TwoArgFn, taf) {
  std::string ret;
  ret += StrToLower(TwoArgFn_TwoArgFnEnum_Name(taf.fn_enum()));
  ret += "(";
  ret += ExprToString(taf.arg1());
  ret += ", ";
  ret += ExprToString(taf.arg2());
  ret += ") ";
  return ret;
}

CONV_FN(ThreeArgFn, taf) {
  std::string ret;
  ret += StrToLower(ThreeArgFn_ThreeArgFnEnum_Name(taf.fn_enum()));
  ret += "(";
  ret += ExprToString(taf.arg1());
  ret += ", ";
  ret += ExprToString(taf.arg2());
  ret += ", ";
  ret += ExprToString(taf.arg3());
  ret += ") ";
  return ret;
}

CONV_FN(VarNumFn, vfn) {
  std::string ret;
  ret += StrToLower(VarNumFn_VarNumFnEnum_Name(vfn.fn_enum()));
  ret += "(";
  ret += ExprToString(vfn.arg1());
  ret += ", ";
  ret += ExprToString(vfn.arg2());
  for (int i = 0; i < vfn.other_args_size(); i++) {
    ret += ", ";
    ret += ExprToString(vfn.other_args(i));
  }
  ret += ") ";
  return ret;
}

CONV_FN(CharFn, cfn) {
  std::string ret("char(");
  ret += std::to_string(cfn.char_());
  for (int i = 0; i < cfn.extra_chars_size(); i++) {
    ret += ", ";
    ret += std::to_string(cfn.extra_chars(i));
  }
  ret += ") ";
  return ret;
}

CONV_FN(SimpleFn, sfn) {
  // oneof
  if (sfn.has_zero_arg_fn()) {
    return ZeroArgFnToString(sfn.zero_arg_fn());
  } else if (sfn.has_one_arg_fn()) {
    return OneArgFnToString(sfn.one_arg_fn());
  } else if (sfn.has_two_arg_fn()) {
    return TwoArgFnToString(sfn.two_arg_fn());
  } else if (sfn.has_three_arg_fn()) {
    return ThreeArgFnToString(sfn.three_arg_fn());
  } else if (sfn.has_varnum_fn()) {
    return VarNumFnToString(sfn.varnum_fn());
  } else if (sfn.has_char_fn()) {
    return CharFnToString(sfn.char_fn());
  } else {
    return "changes() ";
  }
}

CONV_FN(PrintfFormatSpecifier, pfs) {
  std::string ret("%");
  for (int i = 0; i < pfs.flags_size(); i++) {
    switch (pfs.flags(i)) {
      case PrintfFormatSpecifier::MINUS:
        ret += "-";
        break;
      case PrintfFormatSpecifier::PLUS:
        ret += "+";
        break;
      case PrintfFormatSpecifier::SPACE:
        ret += " ";
        break;
      case PrintfFormatSpecifier::ZERO:
        ret += "0";
        break;
      case PrintfFormatSpecifier::HASH:
        ret += "#";
        break;
      case PrintfFormatSpecifier::COMMA:
        ret += ",";
        break;
      case PrintfFormatSpecifier::BANG:
        ret += "!";
        break;
    }
  }
  if (pfs.has_width()) {
    ret += std::to_string(pfs.width());
  } else if (pfs.width_star()) {
    ret += "*";
  }
  if (pfs.has_precision()) {
    ret += ".";
    ret += std::to_string(pfs.precision());
  }
  if (pfs.has_length()) {
    if (pfs.length() % 3 == 1) {
      ret += "l";
    } else if (pfs.length() % 3 == 2) {
      ret += "ll";
    }
  }
  if (pfs.percent()) {
    ret += "%";
  } else {
    std::string specifier = PrintfFormatSpecifier_SubType_Name(pfs.sub_type());
    if (pfs.lowercase())
      specifier = StrToLower(specifier);
    ret += specifier;
  }
  return ret;
}

CONV_FN(Printf, p) {
  std::string ret("printf(\'");
  for (int i = 0; i < p.specifiers_size(); i++) {
    ret += PrintfFormatSpecifierToString(p.specifiers(i));
    if (i < p.strings_size()) {
      ret += ConvertToSqlString(p.strings(i));
    }
  }
  ret += "\'";
  for (int i = 0; i < p.exprs_size(); i++) {
    ret += ", ";
    ret += ExprToString(p.exprs(i));
  }
  ret += ") ";
  return ret;
}

CONV_FN(Fn, fn) {
  // oneof
  if (fn.has_simple_fn()) {
    return SimpleFnToString(fn.simple_fn());
  } else if (fn.has_fts_aux_fn()) {
#if defined(FUZZ_FTS3)
    return FTS3AuxiliaryFnToString(fn.fts_aux_fn()) + " ";
#else
    return "changes() ";
#endif
  } else if (fn.has_dat_fn()) {
    return DateAndTimeFnToString(fn.dat_fn());
  } else if (fn.has_aggregate_fn()) {
    return AggregateFnToString(fn.aggregate_fn());
  } else if (fn.has_printf()) {
    return PrintfToString(fn.printf());
  } else {
    return "changes() ";
  }
}

CONV_FN(ParenthesesExpr, pexpr) {
  std::string ret("(");
  ret += ExprToString(pexpr.expr());
  for (int i = 0; i < pexpr.other_exprs_size(); i++) {
    ret += ", ";
    ret += ExprToString(pexpr.other_exprs(i));
  }
  ret += ") ";
  return ret;
}

CONV_FN(CastExpr, cexpr) {
  std::string ret("CAST(");
  ret += ExprToString(cexpr.expr());
  ret += " AS ";
  ret += EnumStrReplaceUnderscores(
      CastTypeName_CastTypeNameEnum_Name(cexpr.type_name().type_enum()));
  ret += ") ";
  return ret;
}

CONV_FN(CollateExpr, cexpr) {
  std::string ret;
  ret += ExprToString(cexpr.expr());
  ret += " COLLATE ";
  ret += CollateTypeToString(cexpr.collate_type());
  return ret;
}

CONV_FN(Expr1, e) {
  std::string ret;
  ret += ExprToString(e.expr1());
  ret += " ";
  if (e.not_())
    ret += "NOT ";
  ret += EnumStrReplaceUnderscores(Expr1_PossibleKeywords_Name(e.keyword()));
  ret += " ";
  ret += ExprToString(e.expr2());
  ret += " ";  //
  if (e.has_escape_expr()) {
    ret += "ESCAPE ";
    ret += ExprToString(e.escape_expr());
    ret += " ";  //
  }
  return ret;
}

CONV_FN(ExprNullTests, e) {
  std::string ret = ExprToString(e.expr());
  ret += " ";
  ret += EnumStrReplaceUnderscores(
      ExprNullTests_PossibleKeywords_Name(e.keyword()));
  ret += " ";
  return ret;
}

CONV_FN(ExprIs, e) {
  std::string ret = ExprToString(e.expr1());
  ret += " IS ";
  if (e.not_())
    ret += "NOT ";
  ret += ExprToString(e.expr2());
  ret += " ";  //
  return ret;
}

CONV_FN(ExprBetween, e) {
  std::string ret;
  ret += ExprToString(e.expr1());
  ret += " ";
  if (e.not_())
    ret += "NOT ";
  ret += "BETWEEN ";
  ret += ExprToString(e.expr2());
  ret += " AND ";
  ret += ExprToString(e.expr3());
  return ret;
}

CONV_FN(ExprInParen, e) {
  std::string ret("(");
  // oneof
  if (e.has_select()) {
    ret += SelectToString(e.select());
  } else if (e.has_exprs()) {
    ret += ExprListToString(e.exprs());
  }

  ret += ") ";
  return ret;
}

CONV_FN(ExprIn, e) {
  std::string ret = ExprToString(e.expr());
  ret += " ";
  if (e.not_())
    ret += "NOT ";
  if (e.has_expr_in_paren()) {
    ret += ExprInParenToString(e.expr_in_paren());
  } else if (e.has_schema_table()) {
    ret += ExprSchemaTableToString(e.schema_table());
  } else if (e.has_schema_table_fn()) {
    ret += ExprSchemaTableFnToString(e.schema_table_fn());
  } else {
    ret += "()";
  }
  return ret + " ";
}

CONV_FN(ExprExists, e) {
  std::string ret;
  if (e.not_())
    ret += "NOT EXISTS ";
  else if (e.exists())
    ret += "EXISTS ";
  ret += "(";
  ret += SelectToString(e.select());
  ret += ") ";
  return ret;
}

// WARNING no space at end
CONV_FN(ExprWhenThen, e) {
  std::string ret("WHEN ");
  ret += ExprToString(e.when_expr());
  ret += " THEN ";
  ret += ExprToString(e.then_expr());
  return ret;
}

CONV_FN(ExprCase, e) {
  std::string ret("CASE ");
  if (e.has_expr()) {
    ret += ExprToString(e.expr());
    ret += " ";
  }
  ret += ExprWhenThenToString(e.when_then());
  ret += " ";
  for (int i = 0; i < e.extra_when_thens_size(); i++) {
    ret += ExprWhenThenToString(e.extra_when_thens(i));
    ret += " ";
  }
  if (e.has_else_expr()) {
    ret += "ELSE ";
    ret += ExprToString(e.else_expr());
    ret += " ";
  }
  ret += "END ";
  return ret;
}

CONV_FN(ExprRaiseFn, e) {
  std::string ret("RAISE(");
  if (e.ignore()) {
    ret += "IGNORE";
  } else {
    ret +=
        EnumStrReplaceUnderscores(ExprRaiseFn_RaiseFnEnum_Name(e.raise_fn()));
    ret += " ";
    ret += ", \'";
    ret += ConvertToSqlString(e.error_msg());
    ret += "\'";
  }
  ret += ") ";
  return ret;
}

CONV_FN(ComplicatedExpr, expr) {
  using ExprType = ComplicatedExpr::ComplicatedExprOneofCase;
  switch (expr.complicated_expr_oneof_case()) {
    case ExprType::kExprStc:
      return ExprSchemaTableColumnToString(expr.expr_stc());
    case ExprType::kUnaryExpr:
      return UnaryExprToString(expr.unary_expr());
    case ExprType::kBinaryExpr:
      return BinaryExprToString(expr.binary_expr());
    case ExprType::kFnExpr:
      return FnToString(expr.fn_expr());
    case ExprType::kParExpr:
      return ParenthesesExprToString(expr.par_expr());
    case ExprType::kCastExpr:
      return CastExprToString(expr.cast_expr());
    case ExprType::kCollateExpr:
      return CollateExprToString(expr.collate_expr());
    case ExprType::kExpr1:
      return Expr1ToString(expr.expr1());
    case ExprType::kExprNullTests:
      return ExprNullTestsToString(expr.expr_null_tests());
    case ExprType::kExprIs:
      return ExprIsToString(expr.expr_is());
    case ExprType::kExprBetween:
      return ExprBetweenToString(expr.expr_between());
    case ExprType::kExprIn:
      return ExprInToString(expr.expr_in());
    case ExprType::kExprExists:
      return ExprExistsToString(expr.expr_exists());
    case ExprType::kExprCase:
      return ExprCaseToString(expr.expr_case());
    case ExprType::kExprRaise:
      return ExprRaiseFnToString(expr.expr_raise());
    default:
      return "1";
  }
}

// TODO(mpdenton) wrap in parentheses???
CONV_FN(Expr, expr) {
  if (expr.has_lit_val()) {
    return LiteralValueToString(expr.lit_val());
  } else if (expr.has_comp_expr()) {
    return ComplicatedExprToString(expr.comp_expr());
  } else {  // default
    return "1";
  }
}

// ~~~~Other~~~~

std::string ForeignKeyClauseNotMatchToString(
    const ForeignKeyClauseNotMatch& nm) {
  std::string ret("ON ");
  ret += EnumStrReplaceUnderscores(
      ForeignKeyClauseNotMatch_DeleteOrUpdate_Name(nm.del_or_update()));
  ret += " ";
  ret += EnumStrReplaceUnderscores(
      ForeignKeyClauseNotMatch_Action_Name(nm.action()));
  ret += " ";
  return ret;
}

CONV_FN(ForeignKeyClauseCore, fkc) {
  if (fkc.has_fkey_not_match())
    return ForeignKeyClauseNotMatchToString(fkc.fkey_not_match());

  return "MATCH PARTIAL";  // Sqlite does not actually parse MATCH clauses. This
                           // is assumed to be MATCH SIMPLE.
}

CONV_FN(DeferStrategy, ds) {
  std::string ret;
  if (ds.not_()) {
    ret += "NOT ";
  }
  ret += "DEFERRABLE ";
  ret +=
      EnumStrReplaceUnderscores(DeferStrategy_DeferStratEnum_Name(ds.strat()));
  return ret;
}

CONV_FN(ForeignKeyClause, fkey_clause) {
  std::string ret("REFERENCES ");
  ret += TableToString(fkey_clause.foreign_table());
  if (fkey_clause.has_col_list()) {
    ret += "(";
    ret += ColumnListToString(fkey_clause.col_list());
    ret += ")";
  }
  ret += " ";
  for (int i = 0; i < fkey_clause.fkey_cores_size(); i++) {
    ret += ForeignKeyClauseCoreToString(fkey_clause.fkey_cores(i));
    ret += " ";
  }
  if (fkey_clause.has_defer_strat()) {
    ret += DeferStrategyToString(fkey_clause.defer_strat());
    ret += " ";
  }
  return ret;
}

CONV_FN(ConflictClause, conf) {
  if (!conf.has_on_conflict())
    return " ";

  std::string ret("ON CONFLICT ");
  ret += EnumStrReplaceUnderscores(
      ConflictClause_OnConflict_Name(conf.on_conflict()));
  ret += " ";
  return ret;
}

CONV_FN(ColConstraintOpt1, opt1) {
  std::string ret("PRIMARY KEY ");
  ret += AscDescToString(opt1.asc_desc());
  // space at the end already
  ret += ConflictClauseToString(opt1.conflict());
  if (opt1.autoincrement())
    ret += "AUTOINCREMENT ";

  return ret;
}

CONV_FN(ColConstraintOpt2, opt2) {
  std::string ret("DEFAULT ");
  if (opt2.has_expr()) {
    ret += "(";
    ret += ExprToString(opt2.expr());
    ret += ")";
  } else {
    ret += LiteralValueToString(opt2.lit_val());
  }

  ret += " ";
  return ret;
}

CONV_FN(ColumnConstraint, col_constr) {
  std::string ret;
  if (col_constr.has_constraint_name()) {
    ret += "CONSTRAINT ";
    ret += ColumnConstraintNameToString(col_constr.constraint_name());
    ret += " ";
  }

  using ColConstrType = ColumnConstraint::ColConstraintOneofCase;
  switch (col_constr.col_constraint_oneof_case()) {
    case ColConstrType::kOpt1:
      ret += ColConstraintOpt1ToString(col_constr.opt1());
      break;
    case ColConstrType::kNotNullConfClause:
      ret += "NOT NULL ";
      ret += ConflictClauseToString(col_constr.not_null_conf_clause());
      break;
    case ColConstrType::kUniqueConfClause:
      ret += "UNIQUE ";
      ret += ConflictClauseToString(col_constr.unique_conf_clause());
      break;
    case ColConstrType::kCheckExpr:
      ret += "CHECK(";
      ret += ExprToString(col_constr.check_expr());
      ret += ") ";
      break;
    case ColConstrType::kOpt2:
      ret += ColConstraintOpt2ToString(col_constr.opt2());
      break;
    case ColConstrType::kCollate:
      ret += "COLLATE ";
      ret += CollateTypeToString(col_constr.collate());
      ret += " ";
      break;
    case ColConstrType::kFkeyClause:
      ret += ForeignKeyClauseToString(col_constr.fkey_clause());
      break;
    default:
      ret += ColConstraintOpt2ToString(col_constr.opt2_fallback());
  }

  return ret;
}

CONV_FN(TypeName, type_name) {
  std::string ret;
  ret += EnumStrReplaceUnderscores(
      CastTypeName_CastTypeNameEnum_Name(type_name.ctn().type_enum()));
  if (type_name.has_sn()) {
    ret += "(";
    ret += std::to_string(type_name.sn());
    ret += ")";
  }
  ret += " ";
  return ret;
}

CONV_FN(ColumnDef, col_def) {
  std::string ret;
  ret += ColumnToString(col_def.col());
  ret += " ";
  if (col_def.has_type_name()) {
    ret += TypeNameToString(col_def.type_name());
    ret += " ";
  }

  for (int i = 0; i < col_def.col_constraints_size(); i++) {
    ret += ColumnConstraintToString(col_def.col_constraints(i));
    ret += " ";
  }

  return ret;
}

CONV_FN(TableConstraintOpt1, opt1) {
  std::string ret;
  ret += EnumStrReplaceUnderscores(
      TableConstraintOpt1_ConstraintType_Name(opt1.constraint_type()));
  ret += "(";
  ret += IndexedColumnListToString(opt1.indexed_col_list());
  ret += ") ";
  ret += ConflictClauseToString(opt1.conf_clause());

  return ret;
}

CONV_FN(TableConstraintOpt2, opt2) {
  std::string ret("FOREIGN KEY (");
  ret += ColumnListToString(opt2.cols());
  ret += ") ";

  ret += ForeignKeyClauseToString(opt2.fkey_clause());
  return ret;
}

CONV_FN(TableConstraint, t_constr) {
  std::string ret;
  if (t_constr.has_name()) {
    ret += "CONSTRAINT ";
    ret += TableConstraintNameToString(t_constr.name());
    ret += " ";
  }

  if (t_constr.has_opt1()) {
    ret += TableConstraintOpt1ToString(t_constr.opt1());
  } else if (t_constr.has_check_expr()) {
    ret += "CHECK(";
    ret += ExprToString(t_constr.check_expr());  // TODO(mpdenton)
    ret += ") ";
  } else if (t_constr.has_opt2()) {
    ret += TableConstraintOpt2ToString(t_constr.opt2());
  } else {
    // default to no constraint
    ret += "CHECK(1)";
  }

  ret += " ";
  return ret;
}

CONV_FN(CreateTableOpt1, opt1) {
  std::string ret("(");
  ret += ColumnDefToString(opt1.col_def());
  for (int i = 0; i < opt1.extra_col_defs_size(); i++) {
    ret += ", ";
    ret += ColumnDefToString(opt1.extra_col_defs(i));
  }
  for (int i = 0; i < opt1.table_constraints_size(); i++) {
    ret += ", ";
    ret += TableConstraintToString(opt1.table_constraints(i));
  }
  ret += ") ";

  if (opt1.without_rowid())
    ret += "WITHOUT ROWID ";

  return ret;
}

CONV_FN(CreateTable, create_table) {
  RETURN_IF_DISABLED_QUERY(CreateTable);
#if defined(FUZZ_FTS3)
  return "";  // Don't create normal tables in FTS3 fuzzing mode.
#else
  std::string ret("CREATE ");
  if (create_table.has_temp_modifier()) {
    ret += EnumStrReplaceUnderscores(
               TempModifier_Name(create_table.temp_modifier()))
               .erase(0, std::string("TM_").length());
    ret += " ";
  }
  ret += "TABLE ";
  if (create_table.if_not_exists())
    ret += "IF NOT EXISTS ";

  ret += ExprSchemaTableToString(create_table.schema_table());
  ret += " ";

  // TODO(mpdenton) need spaces at the end here???
  using TableCreationType = CreateTable::CreateTableOneofCase;
  switch (create_table.create_table_oneof_case()) {
    case TableCreationType::kOp1:
      ret += CreateTableOpt1ToString(create_table.op1());
      break;
    case TableCreationType::kAsSelectStmt:
      ret += SelectToString(create_table.as_select_stmt());
      break;
    default:
      ret += CreateTableOpt1ToString(create_table.op());
      break;
  }

  return ret;  // TODO(mpdenton)
#endif
}

// ~~~~For INSERT and SELECT~~~~

CONV_FN(CommonTableExpr, cte) {
  std::string ret;
  ret += TableToString(cte.table());
  if (cte.columns_size() > 0) {
    ret += "(";
    ret += ColumnToString(cte.columns(0));
    for (int i = 1; i < cte.columns_size(); i++) {
      ret += ", ";
      ret += ColumnToString(cte.columns(i));
    }
    ret += ")";
  }
  ret += " AS (";
  ret += SelectToString(cte.select());
  // Avert infinite recursion.
  ret += " LIMIT 1000)";
  return ret;
}

CONV_FN(WithStatement, ws) {
  std::string ret("WITH ");
  if (ws.recursive())
    ret += "RECURSIVE ";
  ret += CommonTableExprToString(ws.table_expr());
  ret += " ";
  for (int i = 0; i < ws.extra_table_exprs_size(); i++) {
    ret += CommonTableExprToString(ws.extra_table_exprs(i));
    ret += " ";
  }
  ret += " ";
  return ret;
}

// ~~~~INSERT~~~~

// WARNING no space at end
CONV_FN(ColumnComparison, cc) {
  std::string ret;
  ret += ExprSchemaTableColumnToString(cc.col());
  ret += BinaryOperatorToString(cc.op());
#if defined(FUZZ_FTS3)
  if (cc.op() == BINOP_MATCH && cc.has_fmt()) {
    ret += FTS3MatchFormatToString(cc.fmt());
    return ret;
  }
#endif
  ret += ExprToString(cc.expr());
  return ret;
}

// WARNING no space at end
CONV_FN(ExprComparisonHighProbability, echp) {
  if (echp.has_cc()) {
    return ColumnComparisonToString(echp.cc());
  } else if (echp.has_expr()) {
    return ExprToString(echp.expr());
  } else {
    return "Col0 = 1";  // default
  }
}

CONV_FN(WhereStatement, ws) {
  return "WHERE " + ExprComparisonHighProbabilityToString(ws.expr()) + " ";
}

#ifndef SQLITE_OMIT_UPSERT
CONV_FN(UpsertClausePart1, uc1) {
  std::string ret;
  ret += "(";
  ret += IndexedColumnListToString(uc1.icol_list());
  ret += ") ";
  if (uc1.has_where_stmt()) {
    ret += WhereStatementToString(uc1.where_stmt());
    ret += " ";
  }
  return ret;
}
#endif

// WARNING no space at end
CONV_FN(ColEqualsExpr, cee) {
  std::string ret;
  if (cee.has_col()) {
    ret += ColumnToString(cee.col());
  } else if (cee.has_col_list()) {
    ret += ColumnListToString(cee.col_list());
  } else {
    ret += "Col0";  // default
  }
  ret += " = ";
  ret += ExprToString(cee.expr());
  return ret;
}

CONV_FN(UpsertClausePart2, uc2) {
  std::string ret("SET ");
  ret += ColEqualsExprToString(uc2.cee());
  for (int i = 0; i < uc2.extra_cees_size(); i++) {
    ret += ", ";
    ret += ColEqualsExprToString(uc2.extra_cees(i));
  }
  if (uc2.has_where_stmt()) {
    ret += " ";
    ret += WhereStatementToString(uc2.where_stmt());
  }
  ret += " ";
  return ret;
}

CONV_FN(UpsertClause, uc) {
#ifndef SQLITE_OMIT_UPSERT
  std::string ret("ON CONFLICT ");
  if (uc.has_uclause_p1()) {
    ret += UpsertClausePart1ToString(uc.uclause_p1());
  }
  ret += "DO ";
  if (uc.has_uclause_p2()) {
    ret += "UPDATE ";
    ret += UpsertClausePart2ToString(uc.uclause_p2());
  } else {
    ret += "NOTHING ";
  }
  return ret;
#else
  return "";  // fine to return empty string here
#endif
}

CONV_FN(ValuesStatement, values) {
  std::string ret("VALUES (");
  ret += ExprListToString(values.expr_list());
  ret += ")";
  for (int i = 0; i < values.extra_expr_lists_size(); i++) {
    ret += ", (";
    ret += ExprListToString(values.extra_expr_lists(i));
    ret += ")";
  }
  ret += " ";
  return ret;
}

CONV_FN(Insert, insert) {
  RETURN_IF_DISABLED_QUERY(Insert);
  std::string ret;
  if (insert.has_with()) {
    ret += WithStatementToString(insert.with());
    ret += " ";
  }

  ret +=
      EnumStrReplaceUnderscores(Insert_InsertType_Name(insert.insert_type()));
  ret += " INTO ";
  ret += SchemaTableAsAliasToString(insert.staa());

  if (insert.has_col_list()) {
    ret += "(";
    ret += ColumnListToString(insert.col_list());
    ret += ") ";
  }

  // oneof
  if (insert.has_values()) {
    ret += ValuesStatementToString(insert.values());
    ret += " ";
  } else if (insert.has_select()) {
    ret += SelectToString(insert.select());
    ret += " ";
  } else {
    ret += "DEFAULT VALUES ";
  }

  if (insert.has_upsert_clause()) {
    ret += UpsertClauseToString(insert.upsert_clause());
    ret += " ";
  }
  return ret;
}

// ~~~~DELETE~~~~

CONV_FN(QualifiedTableName, qtn) {
  std::string ret;
  ret += SchemaTableAsAliasToString(qtn.staa());
  ret += " ";
  if (qtn.indexed()) {
    if (qtn.not_indexed()) {
      ret += "NOT INDEXED ";
    } else {
      ret += "INDEXED BY ";
      ret += IndexToString(qtn.indexed_by());
      ret += " ";
    }
  }
  return ret;
}

CONV_FN(Delete, delete_) {
  RETURN_IF_DISABLED_QUERY(Delete);
  std::string ret;
  if (delete_.has_with()) {
    ret += WithStatementToString(delete_.with());
    ret += " ";
  }
  ret += "DELETE FROM ";
  ret += QualifiedTableNameToString(delete_.qtn());
  if (delete_.has_where()) {
    ret += WhereStatementToString(delete_.where());
  }
  ret += " ";
  return ret;
}

// ~~~~UPDATE~~~~
// WARNING no space at end
CONV_FN(Update, update) {
  RETURN_IF_DISABLED_QUERY(Update);
  std::string ret;
  if (update.has_with()) {
    ret += WithStatementToString(update.with());
    ret += " ";
  }
  ret += "UPDATE ";
  if (update.has_update_type()) {
    ret +=
        EnumStrReplaceUnderscores(Update_UpdateType_Name(update.update_type()));
    ret += " ";
  }
  ret += QualifiedTableNameToString(update.qtn());
  ret += " ";
  ret += UpsertClausePart2ToString(update.ucp2());
  return ret;
}
// TODO(mpdenton) restrictions on UPDATEs in CREATE TRIGGER????

// ~~~~SELECT~~~~

CONV_FN(ExprColAlias, eca) {
  std::string ret;
  ret += ExprToString(eca.expr());
  ret += " ";
  if (eca.has_col_alias()) {
    if (eca.as()) {
      ret += "AS ";
    }
    ret += ColumnToString(eca.col_alias());
    ret += " ";
  }
  return ret;
}

// WARNING no space at end
CONV_FN(ResultColumn, rc) {
  std::string ret;
  // oneof
  if (rc.has_col()) {
    return ColumnToString(rc.col());
  } else if (rc.has_eca()) {
    return ExprColAliasToString(rc.eca());
  } else if (rc.has_table_star()) {
    return TableToString(rc.table_star()) + ".*";
  } else if (rc.has_fts3_fn()) {
#if defined(FUZZ_FTS3)
    return FTS3AuxiliaryFnToString(rc.fts3_fn());
#else
    return "*";
#endif
  } else {
    return "*";
  }
}

CONV_FN(AsTableAlias, ata) {
  std::string ret;
  if (ata.as()) {
    ret += "AS ";
  }
  ret += TableToString(ata.table_alias());
  ret += " ";
  return ret;
}

// WARNING no space at end
CONV_FN(JoinOperator, jo) {
  if (jo.comma())
    return ",";

  std::string ret;
  if (jo.natural())
    ret += "NATURAL ";

  if (jo.join_type() != JoinOperator::NONE) {
    ret +=
        EnumStrReplaceUnderscores(JoinOperator_JoinType_Name(jo.join_type()));
    ret += " ";
  }
  ret += "JOIN ";
  return ret;
}

CONV_FN(JoinConstraint, jc) {
  // oneof
  if (jc.has_on_expr()) {
    return "ON " + ExprToString(jc.on_expr()) + " ";
  } else if (jc.has_using_expr()) {
    std::string ret("(");
    ret += ColumnListToString(jc.using_expr().col_list());
    ret += ") ";
    return ret;
  }
  return " ";
}

CONV_FN(JoinClauseCore, jcc) {
  std::string ret;
  ret += JoinOperatorToString(jcc.join_op());
  ret += " ";
  ret += TableOrSubqueryToString(jcc.tos());
  ret += " ";
  ret += JoinConstraintToString(jcc.join_constraint());
  ret += " ";
  return ret;
}

CONV_FN(JoinClause, jc) {
  std::string ret;
  ret += TableOrSubqueryToString(jc.tos());
  ret += " ";
  for (int i = 0; i < jc.clauses_size(); i++) {
    ret += JoinClauseCoreToString(jc.clauses(i));
  }
  ret += " ";
  return ret;
}

// TODO(mpdenton) ExprIn needs it schematablefn!!!!!

CONV_FN(ExprSchemaTableFn, estf) {
  std::string ret;
  const TableFn& tfn = estf.table_fn();
  // oneof for pragma fns
  if (tfn.has_foreign_key_list()) {
    ret += "pragma_foreign_key_list(\'";
    ret += TableToString(tfn.foreign_key_list());
    ret += "\') ";
  } else if (tfn.has_index_info()) {
    ret += "pragma_index_info(\'";
    ret += IndexToString(tfn.index_info());
    ret += "\') ";
  } else if (tfn.has_index_list()) {
    ret += "pragma_index_list(\'";
    ret += TableToString(tfn.index_list());
    ret += "\') ";
  } else if (tfn.has_index_xinfo()) {
    ret += "pragma_index_xinfo(\'";
    ret += IndexToString(tfn.index_xinfo());
    ret += "\') ";
  } else if (tfn.has_integrity_check()) {
    ret += "pragma_integrity_check(\'";
    ret += std::to_string(tfn.integrity_check());
    ret += "\') ";
  } else if (tfn.has_optimize()) {
    ret += "pragma_optimize(\'";
    ret += std::to_string(tfn.optimize());
    ret += "\') ";
  } else if (tfn.has_quick_check()) {
    ret += "pragma_quick_check(\'";
    ret += std::to_string(tfn.quick_check());
    ret += "\') ";
  } else if (tfn.has_table_info()) {
    ret += "pragma_table_info(\'";
    ret += TableToString(tfn.table_info());
    ret += "\') ";
  } else if (tfn.has_table_xinfo()) {
    ret += "pragma_table_xinfo(\'";
    ret += TableToString(tfn.table_xinfo());
    ret += "\') ";
  } else {
    ret += StrToLower(PragmaFnZeroArgOneResult_Name(tfn.no_arg_one_result()))
               .erase(0, std::string("PFN_ZO_").length());
    ret += "() ";
  }
  return ret;
}

CONV_FN(TableOrSubqueryOption2, toso2) {
  std::string ret;
  ret += ExprSchemaTableFnToString(toso2.schema_table_fn());
  ret += " ";
  if (toso2.has_as_table_alias()) {
    ret += AsTableAliasToString(toso2.as_table_alias());
  }
  return ret;
}

CONV_FN(TableOrSubqueryOption3, tos3) {
  std::string ret;
  if (tos3.tos_list_size() > 0) {
    ret += TableOrSubqueryToString(tos3.tos_list(0));
    for (int i = 1; i < tos3.tos_list_size(); i++) {
      ret += ", ";
      ret += TableOrSubqueryToString(tos3.tos_list(i));
    }
  } else {
    ret += JoinClauseToString(tos3.join_clause());
  }
  return ret;
}

CONV_FN(TableOrSubqueryOption4, tos4) {
  std::string ret("(");
  ret += SelectToString(tos4.select());
  ret += ") ";
  if (tos4.has_as_table_alias()) {
    ret += AsTableAliasToString(tos4.as_table_alias());
    ret += " ";
  }
  return ret;
}

CONV_FN(TableOrSubquery, tos) {
  // oneof
  if (tos.has_qtn()) {
    return QualifiedTableNameToString(tos.qtn()) + " ";
  } else if (tos.has_toso2()) {
    return TableOrSubqueryOption2ToString(tos.toso2()) + " ";
  } else if (tos.has_toso3()) {
    return "(" + TableOrSubqueryOption3ToString(tos.toso3()) + ") ";
  } else if (tos.has_toso4()) {
    return TableOrSubqueryOption4ToString(tos.toso4()) + " ";
  } else {
    return ExprSchemaTableToString(tos.schema_table_expr()) + " ";
  }
}

CONV_FN(FromStatement, fs) {
  return "FROM " + TableOrSubqueryOption3ToString(fs.tos3());
}

CONV_FN(GroupByStatement, gbs) {
  std::string ret("GROUP BY ");
  ret += ExprListToString(gbs.exprs());
  ret += " ";
  if (gbs.has_having_expr()) {
    ret += "HAVING ";
    ret += ExprToString(gbs.having_expr());
    ret += " ";
  }
  return ret;
}

CONV_FN(WindowStatement, ws) {
#if !defined(SQLITE_OMIT_WINDOWFUNC)
  return "";
#else
  return "";
#endif
}

CONV_FN(SelectStatementCore, ssc) {
  std::string ret;
  ret += EnumStrReplaceUnderscores(
      SelectStatementCore_SelectOrDistinct_Name(ssc.s_or_d()));
  ret += " ";
  if (ssc.result_columns_size() == 0) {
    ret += "* ";
  } else {
    ret += ResultColumnToString(ssc.result_columns(0));
    for (int i = 1; i < ssc.result_columns_size(); i++) {
      ret += ", ";
      ret += ResultColumnToString(ssc.result_columns(i));
    }
    ret += " ";
  }
  if (ssc.has_from()) {
    ret += FromStatementToString(ssc.from());
    ret += " ";
  }
  if (ssc.has_where()) {
    ret += WhereStatementToString(ssc.where());
    ret += " ";
  }
  if (ssc.has_groupby()) {
    ret += GroupByStatementToString(ssc.groupby());
    ret += " ";
  }
  if (ssc.has_window()) {
    ret += WindowStatementToString(ssc.window());
    ret += " ";
  }
  return ret;
}

CONV_FN(SelectSubStatement, sss) {
  // oneof
  if (sss.has_select_core()) {
    return SelectStatementCoreToString(sss.select_core());
  } else if (sss.has_values()) {
    return ValuesStatementToString(sss.values());
  } else {
    return ValuesStatementToString(sss.values_fallback());
  }
}

CONV_FN(ExprOrderingTerm, eot) {
  std::string ret = ExprToString(eot.expr());
  ret += " ";
  if (eot.has_collate_type()) {
    ret += "COLLATE ";
    ret += CollateTypeToString(eot.collate_type());
    ret += " ";
  }
  ret += AscDescToString(eot.asc_desc());
  return ret;
}

CONV_FN(OrderByStatement, obs) {
  std::string ret("ORDER BY ");
  ret += ExprOrderingTermToString(obs.ord_term());
  for (int i = 0; i < obs.extra_ord_terms_size(); i++) {
    ret += ", ";
    ret += ExprOrderingTermToString(obs.extra_ord_terms(i));
  }
  ret += " ";
  return ret;
}

CONV_FN(LimitStatement, ls) {
  std::string ret("LIMIT ");
  ret += ExprToString(ls.limit_expr());
  ret += " ";
  if (ls.has_second_expr()) {
    if (ls.offset()) {
      ret += "OFFSET ";
    } else {
      ret += ", ";
    }
    ret += ExprToString(ls.second_expr());
    ret += " ";
  }
  return ret;
}

CONV_FN(ExtraSelectSubStatement, esss) {
  std::string ret, enum1;
  enum1 = CompoundOperator_Name(esss.compound_op());
  // erase prefix
  enum1.erase(0, std::string("CO_").length());
  ret += EnumStrReplaceUnderscores(enum1);
  ret += " ";
  ret += SelectSubStatementToString(esss.select_substatement());
  return ret;
}

CONV_FN(Select, select) {
  RETURN_IF_DISABLED_QUERY(Select);
  std::string ret;
  if (select.has_with()) {
    ret += WithStatementToString(select.with());
    ret += " ";
  }
  ret += SelectStatementCoreToString(select.select_core());
  for (int i = 0; i < select.extra_select_substatements_size(); i++) {
    ret +=
        ExtraSelectSubStatementToString(select.extra_select_substatements(i));
    ret += " ";
  }
  if (select.has_orderby()) {
    ret += OrderByStatementToString(select.orderby());
    ret += " ";
  }
  if (select.has_limit()) {
    ret += LimitStatementToString(select.limit());
  }
  return ret;
}

// ~~~~FTS3~~~~

// CORPUS currently relying on normal SELECTs to generate good compound
// queries for FTS, like AND, OR, and NOT. Generate a corpus entry with a lot of
// creative FTS queries.

CONV_FN(FTS3Table, ft) {
  // std::string ret("FTS3Table");
  std::string ret(
      "Table");  // for now, use the same naming scheme as normal tables.
  ret += std::to_string(ft.table() % kMaxFTS3TableNumber);
  return ret;
}

CONV_FN(FTS3MatchToken, fmt) {
  std::string ret;
  if (fmt.has_col()) {
    ret += ColumnToString(fmt.col());
    ret += ":";
  }
  if (fmt.negate()) {
    ret += "-";
  }
  if (fmt.token().length() == 0)
    ret += "a";
  else
    ret += ConvertToSqlString(
        fmt.token());  // TODO(mpdenton) good enough? Need something better????
  if (fmt.prefix())
    ret += "*";
  return ret;
}

CONV_FN(FTS3PhraseQuery, fpq) {
  std::string ret("\"");
  ret += FTS3MatchTokenToString(fpq.mt());
  for (int i = 0; i < fpq.extra_mts_size(); i++) {
    ret += " ";
    ret += FTS3MatchTokenToString(fpq.extra_mts(i));
  }
  ret += "\"";
  return ret;
}

CONV_FN(FTS3MatchFormatCore, fmfc) {
  // oneof
  if (fmfc.has_pq()) {
    return FTS3PhraseQueryToString(fmfc.pq());
  } else if (fmfc.has_nq()) {
    return FTS3NearQueryToString(fmfc.nq());
  } else {
    return FTS3MatchTokenToString(fmfc.mt_fallback());
  }
}

CONV_FN(FTS3NearQuery, fnq) {
  std::string ret = FTS3MatchFormatCoreToString(fnq.format_core1());
  ret += " NEAR";
  if (fnq.has_num_tokens_near()) {
    ret += "/";
    ret += std::to_string(fnq.num_tokens_near());
  }
  ret += " ";
  ret += FTS3MatchFormatCoreToString(fnq.format_core2());
  return ret;
}

CONV_FN(FTS3CompoundAndCore, fcac) {
  std::string ret(" ");
  ret += FTS3CompoundAndCore_CompoundOp_Name(fcac.op());
  ret += " ";
  ret += FTS3MatchFormatCoreToString(fcac.core());
  return ret;
}

CONV_FN(FTS3MatchCompoundFormat, fmcf) {
  std::string ret = FTS3MatchFormatCoreToString(fmcf.core());
  for (int i = 0; i < fmcf.compound_and_cores_size(); i++) {
    ret += FTS3CompoundAndCoreToString(fmcf.compound_and_cores(i));
  }
  return ret;
}

CONV_FN(FTS3MatchFormat, fmf) {
  std::string ret("\'");
  if (fmf.ft_size() > 0) {
    ret += FTS3MatchCompoundFormatToString(fmf.ft(0));
  }
  for (int i = 1; i < fmf.ft_size(); i++) {
    ret += " ";
    ret += FTS3MatchCompoundFormatToString(fmf.ft(i));
  }
  ret += "\'";
  return ret;
}

CONV_FN(FTS3SpecialCommand, fsc) {
  RETURN_IF_DISABLED_QUERY(FTS3SpecialCommand);
  std::string ret("INSERT INTO ");
  ret += FTS3TableToString(fsc.table());
  ret += "(";
  ret += FTS3TableToString(fsc.table());
  ret += ") VALUES(\'";
  switch (fsc.command()) {
    case FTS3SpecialCommand::OPTIMIZE:
      ret += "optimize";
      break;
    case FTS3SpecialCommand::REBUILD:
      ret += "rebuild";
      break;
    case FTS3SpecialCommand::INTEGRITY_CHECK:
      ret += "integrity-check";
      break;
    case FTS3SpecialCommand::MERGE:
      ret += "merge=";
      ret += std::to_string(fsc.val1());
      ret += ",";
      ret += std::to_string(fsc.val2());
      break;
    case FTS3SpecialCommand::AUTOMERGE:
      ret += "automerge=";
      ret += std::to_string(fsc.val1() % 16);
      break;
  }
  ret += "\')";
  return ret;
}

// WARNING no space at end
CONV_FN(FTS3SelectMatch, fsm) {
  RETURN_IF_DISABLED_QUERY(FTS3SelectMatch);
  std::string ret("SELECT * FROM ");
  ret += FTS3TableToString(fsm.table());
  ret += " WHERE ";
  ret += ColumnToString(fsm.col());
  ret += " MATCH ";
  ret += FTS3MatchFormatToString(fsm.match_pattern());
  return ret;
}

CONV_FN(FTS3SpecificQuery, fsq) {
  RETURN_IF_DISABLED_QUERY(FTS3SpecificQuery);
#if defined(FUZZ_FTS3)
  // oneof
  if (fsq.has_command()) {
    return FTS3SpecialCommandToString(fsq.command());
  } else if (fsq.has_select()) {
    return FTS3SelectMatchToString(fsq.select());
  } else {
    return "";
  }

#else
  return "";
#endif
}

CONV_FN(ICULocale, il) {
  std::string ret;
  std::string lc = IsoLangCode_Name(il.iso_lang_code());
  lc.erase(0, std::string("ISO_LANG_CODE_").length());
  ret += lc;
  ret += "_";
  // extract country code from integer
  ret += (char)((il.country_code() & 0xFF) % 26) + 'A';
  ret += (char)(((il.country_code() & 0xFF00) >> 8) % 26) + 'A';
  return ret;
}

CONV_FN(CreateFTS3Table, cft) {
  RETURN_IF_DISABLED_QUERY(CreateFTS3Table);
  std::string ret("CREATE VIRTUAL TABLE ");
  if (cft.if_not_exists())
    ret += "IF NOT EXISTS ";
  if (cft.has_schema()) {
    ret += SchemaToString(cft.schema());
    ret += ".";
  }
  ret += FTS3TableToString(cft.table());
  ret += " USING fts3(";
  // TODO(mpdenton) not using schema here, should I???
  if (cft.has_col_list()) {
    ret += ColumnListToString(cft.col_list());
    if (cft.has_tokenizer_type())
      ret += ", ";
  }
  if (cft.has_tokenizer_type()) {
    ret += "tokenize=";
    std::string tt = TokenizerType_Name(cft.tokenizer_type());
    tt.erase(0, std::string("TT_").length());
    tt = StrToLower(tt);
#if defined(SQLITE_DISABLE_FTS3_UNICODE)
    if (tt == "unicode61")
      tt = "porter";
#endif
    ret += tt;
    // now generate locales for ICU
    if (cft.tokenizer_type() == TokenizerType::TT_ICU) {
      ret += " ";
      ret += ICULocaleToString(cft.locale());
    } else if (cft.tokenizer_type() == TokenizerType::TT_UNICODE61) {
      // Chrome does not actually enable this option. FIXME in the future.
    }
  }
  ret += ")";
  return ret;
}

// WARNING no space at end
CONV_FN(FTS3OffsetsFn, fof) {
  return "offsets(" + FTS3TableToString(fof.table()) + ")";
}

// WARNING no space at end
CONV_FN(FTS3SnippetsFn, fsf) {
  std::string ret("snippets(");
  ret += FTS3TableToString(fsf.table());
  // Now (possibly) emit the five optional arguments.
  int num_args = (int)fsf.num_optional_args();
  if (num_args >= 1) {
    ret += ", \'";
    ret += ConvertToSqlString(fsf.start_match());
    ret += "\'";
  }
  if (num_args >= 2) {
    ret += ", \'";
    ret += ConvertToSqlString(fsf.end_match());
    ret += "\'";
  }
  if (num_args >= 3) {
    ret += ", \'";
    ret += ConvertToSqlString(fsf.ellipses());
    ret += "\'";
  }
  if (num_args >= 4) {
    ret += ", ";
    if (fsf.has_col_number()) {
      ret += std::to_string(fsf.col_number() % kMaxColumnNumber);
    } else {
      ret += "-1";
    }
  }
  if (num_args >= 5) {
    ret += ", ";
    ret +=
        std::to_string((fsf.num_tokens() % 129) - 64);  // clamp into [-64, 64]
  }
  ret += ")";
  return ret;
}

// WARNING no space at end
CONV_FN(FTS3MatchInfoFn, fmi) {
  constexpr static char matchinfo_chars[] = {
      'p', 'c', 's', 'x', 'y', 'b',
      // 'n', 'a', 'l', // These characters only available for FTS4.
  };
  std::string ret("matchinfo(");
  ret += FTS3TableToString(fmi.table());
  if (fmi.chars_size() > 0) {
    ret += ", \'";
    for (int i = 0; i < fmi.chars_size(); i++) {
      ret += matchinfo_chars[fmi.chars(i) % sizeof(matchinfo_chars)];
    }
    ret += "\'";
  }
  ret += ")";
  return ret;
}

// WARNING no space at end
CONV_FN(FTS3AuxiliaryFn, faf) {
  if (faf.has_snippets()) {
    return FTS3SnippetsFnToString(faf.snippets());
  } else if (faf.has_matchinfo()) {
    return FTS3MatchInfoFnToString(faf.matchinfo());
  } else {
    return FTS3OffsetsFnToString(faf.offsets_fallback());
  }
}

CONV_FN(FTS3HiddenTable, fht) {
  std::string tab = FTS3HiddenTable_HiddenTableVal_Name(fht.htv());
  tab = StrToLower(tab);
  return FTS3TableToString(fht.table()) + "_" + tab;
}

CONV_FN(FTS3HiddenTableColumn, fhtc) {
  std::string tab = FTS3HiddenTableColumn_Name(fhtc);
  tab = tab.erase(0, std::string("FTS3_HT_").length());
  tab = StrToLower(tab);
  return tab;
}

CONV_FN(FTS3HiddenTableInsert, fi) {
  RETURN_IF_DISABLED_QUERY(FTS3HiddenTableInsert);
  std::string ret("INSERT INTO ");
  ret += FTS3HiddenTableToString(fi.fht());
  if (fi.col_vals_size() == 0) {
    ret += " DEFAULT VALUES";
    return ret;
  }
  ret += "(";
  ret += FTS3HiddenTableColumnToString(fi.col_vals(0).col());
  for (int i = 1; i < fi.col_vals_size(); i++) {
    ret += ", ";
    ret += FTS3HiddenTableColumnToString(fi.col_vals(i).col());
  }
  ret += ") VALUES(";
  ret += ExprToString(fi.col_vals(0).expr());
  for (int i = 0; i < fi.col_vals_size(); i++) {
    ret += ", ";
    ret += ExprToString(fi.col_vals(i).expr());
  }
  ret += ")";
  return ret;
}

CONV_FN(FTS3HiddenTableUpdate, fu) {
  RETURN_IF_DISABLED_QUERY(FTS3HiddenTableUpdate);
  std::string ret("UPDATE ");
  ret += FTS3HiddenTableToString(fu.fht());
  ret += " ";
  if (fu.col_vals_size() == 0) {
    ret += "start_block = 0";
    return ret;
  }
  ret += "SET ";
  ret += FTS3HiddenTableColumnToString(fu.col_vals(0).col());
  ret += " = ";
  ret += ExprToString(fu.col_vals(0).expr());
  for (int i = 1; i < fu.col_vals_size(); i++) {
    ret += ", ";
    ret += FTS3HiddenTableColumnToString(fu.col_vals(i).col());
    ret += " = ";
    ret += ExprToString(fu.col_vals(i).expr());
  }
  if (fu.has_col_where()) {
    ret += " WHERE ";
    ret += FTS3HiddenTableColumnToString(fu.col_where());
    ret += BinaryOperatorToString(fu.bin_op());
    ret += ExprToString(fu.comp_expr());
  }
  return ret;
}

CONV_FN(FTS3HiddenTableDelete, fd) {
  RETURN_IF_DISABLED_QUERY(FTS3HiddenTableDelete);
  std::string ret("DELETE FROM ");
  ret += FTS3HiddenTableToString(fd.fht());
  if (fd.has_col_where()) {
    ret += " WHERE ";
    ret += FTS3HiddenTableColumnToString(fd.col_where());
    ret += BinaryOperatorToString(fd.bin_op());
    ret += ExprToString(fd.comp_expr());
  }
  return ret;
}

// ~~~~TRANSACTIONS/SAVEPOINTS
CONV_FN(BeginTransaction, bt) {
  RETURN_IF_DISABLED_QUERY(BeginTransaction);
  std::string ret("BEGIN ");
  if (bt.has_type()) {
    ret += BeginTransaction_TransactionType_Name(bt.type());
    ret += " ";
  }
  ret += "TRANSACTION";
  return ret;
}

CONV_FN(CommitTransaction, ct) {
  RETURN_IF_DISABLED_QUERY(CommitTransaction);
  return EnumStrReplaceUnderscores(
      CommitTransaction_CommitText_Name(ct.text()));
}

CONV_FN(RollbackStatement, rt) {
  RETURN_IF_DISABLED_QUERY(RollbackStatement);
#if !defined(FUZZ_OMIT_SAVEPOINT)
  if (rt.has_save_point()) {
    return "ROLLBACK TO SAVEPOINT " + SavePointToString(rt.save_point());
  }
#endif
  return "ROLLBACK TRANSACTION";
}

#if !defined(FUZZ_OMIT_SAVEPOINT)
CONV_FN(CreateSavePoint, csp) {
  RETURN_IF_DISABLED_QUERY(CreateSavePoint);
  return "SAVEPOINT " + SavePointToString(csp.save_point());
}

CONV_FN(ReleaseSavePoint, rsp) {
  RETURN_IF_DISABLED_QUERY(ReleaseSavePoint);
  return "RELEASE SAVEPOINT " + SavePointToString(rsp.save_point());
}
#endif

CONV_FN(Analyze, a) {
  RETURN_IF_DISABLED_QUERY(Analyze);
  std::string ret("ANALYZE");
  if (a.has_schema_name()) {
    ret += " ";
    ret += SchemaToString(a.schema_name());
    if (a.has_table_name()) {
      ret += ".";
      ret += TableToString(a.table_name());
    } else if (a.has_index_name()) {
      ret += ".";
      ret += IndexToString(a.index_name());
    }
  } else if (a.has_table_name()) {
    ret += " ";
    ret += TableToString(a.table_name());
  } else if (a.has_index_name()) {
    ret += " ";
    ret += IndexToString(a.index_name());
  }

  return ret;
}

// ~~~~VACUUM~~~~
CONV_FN(Vacuum, v) {
  RETURN_IF_DISABLED_QUERY(Vacuum);
  std::string ret("VACUUM");
  if (v.has_schema()) {
    ret += " ";
    ret += SchemaToString(v.schema());
  }
  return ret;
}

// ~~~~PRAGMA~~~~
CONV_FN(Pragma, p) {
  RETURN_IF_DISABLED_QUERY(Pragma);
#if defined(FUZZ_OMIT_PRAGMA)
  return "";
#else
  constexpr static const char* locking_modes[] = {"NORMAL", "EXCLUSIVE"};
  constexpr static const char* journal_modes[] = {
      "DELETE", "TRUNCATE", "PERSIST", "MEMORY", "WAL", "OFF"};

  Table table;
  std::string ret("PRAGMA ");
  if (p.has_schema()) {
    ret += SchemaToString(p.schema());
    ret += ".";
  }
  ret += StripTrailingUnderscores(
      StrToLower(Pragma_PragmaCommand_Name(p.command())));
  switch (p.command()) {
    case Pragma::AUTO_VACUUM:
      ret += " = ";
      ret += std::to_string((uint32_t)p.arg1() % 3);
      break;
    case Pragma::WRITEABLE_SCHEMA:
      ret += " = ";
      ret += std::to_string((uint32_t)p.arg1() % 2);
      break;
    case Pragma::LOCKING_MODE:
      ret += " = ";
      ret += locking_modes[(uint32_t)p.arg1() % 2];
      break;
    case Pragma::TEMP_STORE:
      ret += " = ";
      ret += std::to_string((uint32_t)p.arg1() % 3);
      break;
    case Pragma::PAGE_SIZE_:
      ret += " = ";
      ret += std::to_string(p.arg1());
      break;
    case Pragma::TABLE_INFO:
      ret += "(\'";
      table.set_table((uint32_t)p.arg1());
      ret += TableToString(table);
      ret += "\')";
      break;
    case Pragma::JOURNAL_MODE:
      ret += " = ";
      ret += journal_modes[(uint32_t)p.arg1() % 6];
      break;
    case Pragma::MMAP_SIZE:
      ret += " = ";
      ret += std::to_string(p.arg1());
      break;
  }
  return ret;
#endif
}

// ~~~~CREATE INDEX~~~~
CONV_FN(CreateIndex, ci) {
  RETURN_IF_DISABLED_QUERY(CreateIndex);
  std::string ret("CREATE ");
  if (ci.unique())
    ret += "UNIQUE ";
  ret += "INDEX ";
  if (ci.if_not_exists())
    ret += "IF NOT EXISTS ";
  if (ci.has_schema()) {
    ret += SchemaToString(ci.schema());
    ret += ".";
  }
  ret += IndexToString(ci.index());
  ret += " ON ";
  ret += TableToString(ci.table());
  ret += "(";
  ret += IndexedColumnListToString(ci.icol_list());
  ret += ")";
  if (ci.has_where()) {
    ret += " ";
    ret += WhereStatementToString(ci.where());
  }
  return ret;
}

// ~~~~CREATE VIEW~~~~
CONV_FN(CreateView, cv) {
  RETURN_IF_DISABLED_QUERY(CreateView);
  std::string ret("CREATE ");
  if (cv.has_temp_modifier()) {
    ret += EnumStrReplaceUnderscores(TempModifier_Name(cv.temp_modifier()))
               .erase(0, std::string("TM_").length());
    ret += " ";
  }
  ret += "VIEW ";
  if (cv.if_not_exists())
    ret += "IF NOT EXISTS ";

  if (cv.has_schema()) {
    ret += SchemaToString(cv.schema());
    ret += ".";
  }
  ret += ViewToString(cv.view());
  ret += " ";
  if (cv.has_col_list()) {
    ret += "(";
    ret += ColumnListToString(cv.col_list());
    ret += ") ";
  }
  ret += SelectToString(cv.select());
  return ret;
}

// ~~~~CREATE TRIGGER~~~~

CONV_FN(TypicalQuery, tq) {
  // oneof
  if (tq.has_update())
    return UpdateToString(tq.update());
  else if (tq.has_insert())
    return InsertToString(tq.insert());
  else if (tq.has_select())
    return SelectToString(tq.select());
  else
    return DeleteToString(tq.delete_fallback());
}

// WARNING no space at end
CONV_FN(CreateTrigger, ct) {
  RETURN_IF_DISABLED_QUERY(CreateTrigger);
  std::string ret("CREATE ");
  if (ct.has_temp_modifier()) {
    ret += EnumStrReplaceUnderscores(TempModifier_Name(ct.temp_modifier()))
               .erase(0, std::string("TM_").length());
    ret += " ";
  }
  ret += "TRIGGER ";
  if (ct.if_not_exists())
    ret += "IF NOT EXISTS ";

  if (ct.has_schema()) {
    ret += SchemaToString(ct.schema());
    ret += " ";
  }

  ret += TriggerToString(ct.trigger());
  ret += " ";
  if (ct.has_trigger_type()) {
    ret += EnumStrReplaceUnderscores(
        CreateTrigger_TriggerType_Name(ct.trigger_type()));
    ret += " ";
  }
  ret += CreateTrigger_TriggerInstr_Name(ct.trigger_instr());
  ret += " ";
  if (ct.trigger_instr() == CreateTrigger::UPDATE) {
    ret += "OF ";
    ret += ColumnListToString(ct.col_list());
    ret += " ";
  }
  ret += "ON ";
  ret += TableToString(ct.table());
  ret += " ";
  if (ct.for_each_row())
    ret += "FOR EACH ROW ";

  if (ct.has_when()) {
    ret += "WHEN ";
    ret += ExprComparisonHighProbabilityToString(ct.when());
    ret += " ";
  }

  ret += "BEGIN ";
  ret += TypicalQueryToString(ct.tq());
  ret += "; ";
  for (int i = 0; i < ct.extra_tqs_size(); i++) {
    ret += TypicalQueryToString(ct.extra_tqs(i));
    ret += "; ";
  }
  ret += "END";
  return ret;
}

// ~~~~REINDEX~~~~
CONV_FN(ReIndex, ri) {
  RETURN_IF_DISABLED_QUERY(ReIndex);
// Chrome doesn't use REINDEX
#if !defined(SQLITE_OMIT_REINDEX)
  if (ri.empty())
    return "REINDEX";
  std::string ret("REINDEX ");
  if (ri.has_collate_type()) {
    ret += CollateTypeToString(ri.collate_type());
    return ret;
  }
  if (ri.has_schema()) {
    ret += SchemaToString(ri.schema());
    ret += ".";
  }
  if (ri.has_table())
    ret += TableToString(ri.table());
  else
    ret += IndexToString(ri.index());

  return ret;
#else
  return "";
#endif
}

CONV_FN(Drop, d) {
  RETURN_IF_DISABLED_QUERY(Drop);
  std::string ret("DROP ");
  std::string if_exists("");
  std::string schema("");
  if (d.if_exists())
    if_exists = "IF EXISTS ";
  if (d.has_schema()) {
    schema = SchemaToString(d.schema());
    schema += " ";
  }
  // oneof
  if (d.has_index()) {
    ret += "INDEX ";
    ret += if_exists;
    ret += schema;
    ret += IndexToString(d.index());
  } else if (d.has_table()) {
    ret += "TABLE ";
    ret += if_exists;
    ret += schema;
    ret += TableToString(d.table());
  } else if (d.has_trigger()) {
    ret += "TRIGGER ";
    ret += if_exists;
    ret += schema;
    ret += TriggerToString(d.trigger());
  } else {
    ret += "VIEW ";
    ret += if_exists;
    ret += schema;
    ret += ViewToString(d.view_fallback());
  }
  return ret;
}

// ~~~~ALTER TABLE~~~~
CONV_FN(AlterTable, at) {
  RETURN_IF_DISABLED_QUERY(AlterTable);
  std::string ret("ALTER TABLE ");
  ret += ExprSchemaTableToString(at.schema_table());
  ret += " ";
  if (at.has_col()) {
    ret += "RENAME ";
    if (at.column())
      ret += "COLUMN ";
    ret += ColumnToString(at.col());
    ret += " TO ";
    ret += ColumnToString(at.col_to());
  } else if (at.has_col_def()) {
    ret += "ADD ";
    if (at.column())
      ret += "COLUMN ";
    ret += ColumnDefToString(at.col_def());
  } else {
    ret += "RENAME TO ";
    ret += TableToString(at.table_fallback());
  }
  return ret;
}

// ~~~~ATTACH DATABASE~~~~
CONV_FN(AttachDatabase, ad) {
  RETURN_IF_DISABLED_QUERY(AttachDatabase);
  std::string ret("ATTACH DATABASE \'");
  if (ad.in_memory()) {
    if (ad.file_uri()) {
      ret += "file:";
      std::string add;
      if (ad.has_db_name()) {
        ret += SchemaToString(ad.db_name());
        ret += "?mode=memory";
        add = "&";
      } else {
        ret += ":memory:";
        add = "?";
      }

      if (ad.shared_cache()) {
        ret += add;
        ret += "cache=shared";
      }
    }
  }
  ret += "\' AS ";
  ret += SchemaToString(ad.schema());
  return ret;
}

// ~~~~DETACH DATABASE~~~~
CONV_FN(DetachDatabase, dd) {
  RETURN_IF_DISABLED_QUERY(DetachDatabase);
  std::string ret("DETACH DATABASE ");
  ret += SchemaToString(dd.schema());
  return ret;
}

// ~~~~Time and date fns~~~~
CONV_FN(HoursStuff, hs) {
  std::string ret;
  if (hs.has_hh()) {
    ret += std::to_string(hs.hh() % 100);
    if (hs.has_mm()) {
      ret += ":";
      ret += std::to_string(hs.mm() % 100);
      if (hs.has_ss()) {
        ret += ":";
        ret += std::to_string(hs.ss() % 100);
        if (hs.has_sss()) {
          ret += ".";
          ret += std::to_string(hs.sss() % 1000);
        }
      }
    }
  }
  return ret;
}

CONV_FN(TimeString, ts) {
  std::string ret;
  if (ts.has_yyyy()) {
    // FIXME in the future add zeroes for integers < 1000.
    ret += std::to_string(ts.yyyy() % 10000);
    ret += "-";
    ret += std::to_string(ts.mm() % 100);
    ret += "-";
    ret += std::to_string(ts.dd() % 100);
    if (ts.extra_t())
      ret += "T";
    if (ts.has_hs())
      ret += HoursStuffToString(ts.hs());
  } else if (ts.has_hs()) {
    ret += HoursStuffToString(ts.hs());
  } else if (ts.has_dddddddddd()) {
    ret += std::to_string(ts.dddddddddd() % 10000000000);
  } else if (ts.now()) {
    ret += "now";
  } else {
    ret += ConvertToSqlString(ts.random_bytes());
  }

  if (ts.has_tz_plus()) {
    if (ts.z()) {
      ret += "Z";
    } else {
      if (ts.plus())
        ret += "+";
      else
        ret += "-";
      ret += std::to_string(ts.tz_hh() % 100);
      ret += std::to_string(ts.tz_mm() % 100);
    }
  }
  return ret;
}

CONV_FN(TimeModifier, tm) {
  std::string ret;
  if (tm.has_nm()) {
    ret += std::to_string(tm.num());
    ret += " ";
    if (tm.has_dot_num()) {
      ret += ".";
      ret += std::to_string(tm.dot_num());
    }
    ret += StrToLower(TimeModifier_NumberedModifiers_Name(tm.nm()));
  } else {
    ret += StrToLower(
        EnumStrReplaceUnderscores(TimeModifier_OtherModifiers_Name(tm.om())));
  }
  if (tm.om() == TimeModifier::WEEKDAY) {
    ret += " ";
    ret += std::to_string(tm.num());
  }
  return ret;
}

CONV_FN(SimpleDateAndTimeFn, sfn) {
  std::string ret;
  ret += StrToLower(SimpleDateAndTimeFn_FnName_Name(sfn.fn_name()));
  ret += "(\'";
  ret += TimeStringToString(sfn.time_string());
  ret += "\'";
  for (int i = 0; i < sfn.modifiers_size(); i++) {
    ret += ", \'";
    ret += TimeModifierToString(sfn.modifiers(i));
    ret += "\'";
  }
  ret += ") ";
  return ret;
}

CONV_FN(StrftimeFormat, sf) {
  std::string ret;
  if (sf.has_subs()) {
    std::string subs = StrftimeFormat_Substitution_Name(sf.subs());
    if (sf.lowercase())
      subs = StrToLower(subs);
    ret += "%" + subs;
  } else {
    ret += "%%";
  }

  ret += ConvertToSqlString(sf.bytes());
  return ret;
}

CONV_FN(StrftimeFn, sfn) {
  std::string ret("strftime(\'");
  for (int i = 0; i < sfn.fmts_size(); i++) {
    ret += StrftimeFormatToString(sfn.fmts(i));
  }
  ret += "\', \'";
  ret += TimeStringToString(sfn.time_string());
  ret += "\'";
  for (int i = 0; i < sfn.modifiers_size(); i++) {
    ret += ", \'";
    ret += TimeModifierToString(sfn.modifiers(i));
    ret += "\'";
  }
  ret += ") ";
  return ret;
}

CONV_FN(DateAndTimeFn, dat) {
  if (dat.has_simple())
    return SimpleDateAndTimeFnToString(dat.simple());
  else
    return StrftimeFnToString(dat.strftime());
}

// ~~~~QUERY~~~~
CONV_FN(SQLQuery, query) {
  using QueryType = SQLQuery::QueryOneofCase;
  switch (query.query_oneof_case()) {
    case QueryType::kSelect:
      return SelectToString(query.select());
    case QueryType::kCreateTable:
      return CreateTableToString(query.create_table());
    case QueryType::kInsert:
      return InsertToString(query.insert());
    case QueryType::kDelete:
      return DeleteToString(query.delete_());
    case QueryType::kFts3Table:
      return CreateFTS3TableToString(query.fts3_table());
    case QueryType::kFtsQuery:
      return FTS3SpecificQueryToString(query.fts_query());
    case QueryType::kBeginTxn:
      return BeginTransactionToString(query.begin_txn());
    case QueryType::kCommitTxn:
      return CommitTransactionToString(query.commit_txn());
    case QueryType::kRollbackStmt:
      return RollbackStatementToString(query.rollback_stmt());
#if !defined(FUZZ_OMIT_SAVEPOINT)
    case QueryType::kCreateSavePoint:
      return CreateSavePointToString(query.create_save_point());
    case QueryType::kReleaseSavePoint:
      return ReleaseSavePointToString(query.release_save_point());
#endif
    case QueryType::kAnalyze:
      return AnalyzeToString(query.analyze());
    case QueryType::kVacuum:
      return VacuumToString(query.vacuum());
    case QueryType::kPragma:
      return PragmaToString(query.pragma());
    case QueryType::kUpdate:
      return UpdateToString(query.update());
    case QueryType::kCreateIndex:
      return CreateIndexToString(query.create_index());
    case QueryType::kCreateView:
      return CreateViewToString(query.create_view());
    case QueryType::kCreateTrigger:
      return CreateTriggerToString(query.create_trigger());
    case QueryType::kReindex:
      return ReIndexToString(query.reindex());
    case QueryType::kDrop:
      return DropToString(query.drop());
    case QueryType::kAlterTable:
      return AlterTableToString(query.alter_table());
    case QueryType::kAttachDb:
      return AttachDatabaseToString(query.attach_db());
    case QueryType::kDetachDb:
      return DetachDatabaseToString(query.detach_db());
#if defined(FUZZ_FTS3)
    case QueryType::kFts3Insert:
      return FTS3HiddenTableInsertToString(query.fts3_insert());
    case QueryType::kFts3Update:
      return FTS3HiddenTableUpdateToString(query.fts3_update());
    case QueryType::kFts3Delete:
      return FTS3HiddenTableDeleteToString(query.fts3_delete());
#endif
    default:
      return "";
  }
}

std::vector<std::string> SQLQueriesToVec(const SQLQueries& sql_queries) {
  std::vector<std::string> queries;
  queries.reserve(sql_queries.extra_queries_size() + 1);
  queries.push_back(CreateTableToString(sql_queries.create_table()) + ";");
  for (int i = 0; i < sql_queries.extra_queries_size(); i++) {
    std::string query = SQLQueryToString(sql_queries.extra_queries(i));
    if (query == "")
      continue;
    query += ";";
    queries.push_back(query);
  }
  return queries;
}

CONV_FN(SQLQueries, sql_queries) {
  std::string queries;

  for (std::string& query : SQLQueriesToVec(sql_queries)) {
    queries += query;
    queries += "\n";
  }

  return queries;
}

void SetDisabledQueries(std::set<std::string> disabled_queries) {
  disabled_queries_ = disabled_queries;
}

}  // namespace sql_fuzzer
