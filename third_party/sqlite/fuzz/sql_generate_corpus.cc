#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
// TODO(mpdenton) okay to include this? Otherwise I'm copying it into this file
// Tehcnically the std random number engines are banned in Chrome but if this
// used base::Rand* this turns milliseconds into hours.
#include "third_party/libFuzzer/src/FuzzerRandom.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"

using namespace sql_query_grammar;

// TODO(mpdenton):
// 2. Add functionality to start with a specific database so that the
// fuzzer doesn't waste so much time getting a sufficiently complicated
// database.
// 3. FTS3 Corpus

namespace {
constexpr int kMinNumInsertions = 15;
constexpr int kMaxNumInsertions = 20;
constexpr int kMinNumIndexes = 5;
constexpr int kMaxNumIndexes = 8;
constexpr int kMinNumSelects = 3;
constexpr int kMaxNumSelects = 6;
constexpr int kMinNumJoins = 3;
constexpr int kMaxNumJoins = 3;
constexpr int kMinNumUpdates = 15;
constexpr int kMaxNumUpdates = 20;
constexpr int kMinNumDeletes = 5;
constexpr int kMaxNumDeletes = 5;
constexpr int kMinNumOthers = 10;
constexpr int kMaxNumOthers = 10;
}  // namespace

fuzzer::Random& GetRandom() {
  static fuzzer::Random rand([] {
    unsigned seed = base::DefaultClock::GetInstance()
                        ->Now()
                        .ToDeltaSinceWindowsEpoch()
                        .InMicroseconds() +
                    getpid();
    return fuzzer::Random(seed);
  }());
  return rand;
}

// Inclusive range.
int RandInt(int min, int max) {
  return GetRandom()(max - min + 1) + min;
}

void RandBytes(base::span<uint8_t> output) {
  size_t offset = 0u;
  for (size_t i = 0u; i < output.size() / sizeof(size_t); i++) {
    size_t rand_num = GetRandom()();
    for (size_t j = 0u; j < sizeof(size_t); j++) {
      output[offset] = *reinterpret_cast<uint8_t*>(&rand_num);
      offset++;
      rand_num >>= 8;
    }
  }
  size_t rand_num = GetRandom()();
  for (size_t j = 0u; j < output.size() % sizeof(size_t); j++) {
    output[offset] = *reinterpret_cast<uint8_t*>(&rand_num);
    offset++;
    rand_num >>= 8;
  }
}

std::string RandBytesAsString(size_t length) {
  std::string result(length, '\0');
  RandBytes(base::as_writable_byte_span(result));
  return result;
}

uint64_t RandUint64() {
  if (sizeof(size_t) == sizeof(uint64_t)) {
    return GetRandom()();
  }

  CHECK(sizeof(size_t) == sizeof(uint32_t));
  uint64_t rand = GetRandom()();
  rand <<= 32;
  rand |= GetRandom()();
  return rand;
}

namespace i {
struct Table {
  uint32_t table_num;
  int num_columns;
  std::vector<CastTypeName::CastTypeNameEnum> col_types;
  std::vector<Expr> index_exprs;
};

struct Schema {
  int num_tables;
  std::vector<i::Table> tables;
};
}  // namespace i

// WOW, a template AND a macro??? :)
template <typename T>
int GetRandomEnum(T is_valid_fn, int min, int max) {
  int r;
  while (!is_valid_fn(r = RandInt(min, max)))
    ;
  return r;
}

#define RANDOM_ENUM(CLASS_NAME, ENUM_NAME)           \
  static_cast<CLASS_NAME::ENUM_NAME>(                \
      GetRandomEnum(CLASS_NAME::ENUM_NAME##_IsValid, \
                    CLASS_NAME::ENUM_NAME##_MIN, CLASS_NAME::ENUM_NAME##_MAX))

std::set<uint32_t> GetRandomNums(size_t size, uint32_t max_num) {
  std::set<unsigned int> ret;
  while (ret.size() < size) {
    ret.insert(RandInt(0, max_num));
  }
  return ret;
}

template <typename T>
std::set<T> GetRandomSubset(const std::set<T>& s, size_t size) {
  std::set<T> ret;
  std::set<uint32_t> indices = GetRandomNums(size, s.size() - 1);

  auto it = s.begin();
  for (unsigned int i = 0; i < s.size(); i++) {
    if (indices.count(i) > 0) {
      ret.insert(*it);
    }
    it++;
  }

  return ret;
}

inline ColumnDef* CreateDefaultColDef(ColumnDef* cd) {
  cd->mutable_col()->set_column(0);
  return cd;
}

inline ComplicatedExpr* CreateDefaultCompExpr(ComplicatedExpr* ce) {
  ce->mutable_lit_val();
  return ce;
}

inline void CreateColumn(Column* col_ptr, uint32_t col) {
  col_ptr->set_column(col);
}

inline void CreateTableFromUint32(Table* table_ptr, uint32_t table) {
  table_ptr->set_table(table);
}

inline void CreateSchemaTable(ExprSchemaTable* e, i::Table* table) {
  CreateTableFromUint32(e->mutable_table_name(), table->table_num);
}

inline void CreateColumnExpr(Expr* e, uint32_t col, i::Table* table) {
  ExprSchemaTableColumn* stc =
      CreateDefaultCompExpr(e->mutable_comp_expr())->mutable_expr_stc();
  CreateColumn(stc->mutable_col(), col);
  if (table) {
    CreateTableFromUint32(stc->mutable_table(), table->table_num);
  }
}

std::set<uint32_t> GenerateColumnList(ColumnList* ret, i::Table* table) {
  std::set<uint32_t> cols;
  for (int i = 0; i < RandInt(1, table->num_columns); i++) {
    cols.insert(RandInt(0, table->num_columns - 1));
  }
  std::set<uint32_t> cols_copy = cols;
  auto it = cols.begin();
  CreateColumn(ret->mutable_col(), *it);
  cols.erase(it);
  ret->mutable_extra_cols()->Reserve(cols.size());
  for (uint32_t col : cols) {
    CreateColumn(ret->mutable_extra_cols()->Add(), col);
  }
  return cols_copy;
}

void GenerateNumericLit(NumericLiteral* nl) {
  for (int i = 0; i < RandInt(1, 20); i++) {
    nl->add_digits(RandInt(0, 9));
  }
  nl->set_decimal_point(true);
  for (int i = 0; i < RandInt(1, 20); i++) {
    nl->add_dec_digits(RandInt(0, 9));
  }
}

void GenerateLiteralValue(LiteralValue* ret,
                          CastTypeName::CastTypeNameEnum type) {
  if (RandInt(1, 10) == 1) {
    ret->set_special_val(RANDOM_ENUM(LiteralValue, SpecialVal));
    return;
  }

  if (type == CastTypeName::INTEGER ||
      (type == CastTypeName::NUMERIC && RandInt(1, 2) == 1)) {
    if (RandInt(1, 3) == 1) {
      ret->set_num_lit((int64_t)RandInt(1, 3));
    } else {
      ret->set_num_lit((int64_t)RandUint64());
    }
  } else if (type == CastTypeName::TEXT) {
    if (RandInt(1, 3) == 1) {
      ret->set_string_lit("a");
    } else {
      // string literals too often have unreadable chars, so instead of rand
      // bytes just use a couple extra #'s
      ret->set_string_lit("#####");
    }
  } else if (type == CastTypeName::BLOB) {
    if (RandInt(1, 3) == 1) {
      ret->set_blob_lit("a");
    } else {
      ret->set_blob_lit(RandBytesAsString(5));
    }
  } else if (type == CastTypeName::REAL) {
    GenerateNumericLit(ret->mutable_numeric_lit());
  } else {
    ret->set_special_val(RANDOM_ENUM(LiteralValue, SpecialVal));
  }
}

void GenerateValuesStatement(ValuesStatement* v,
                             i::Table* table,
                             std::set<uint32_t> cols) {
  int rand_num_values = RandInt(1, 10);
  if (rand_num_values > 1) {
    v->mutable_extra_expr_lists()->Reserve(rand_num_values - 1);
  }
  for (int i = 0; i < rand_num_values; i++) {
    ExprList* el;
    if (i == 0) {
      el = v->mutable_expr_list();
    } else {
      el = v->mutable_extra_expr_lists()->Add();
    }
    auto it = cols.begin();
    GenerateLiteralValue(el->mutable_expr()->mutable_lit_val(),
                         table->col_types[*it]);
    it++;
    el->mutable_extra_exprs()->Reserve(cols.size() - 1);
    for (size_t c = 0; c < cols.size() - 1; c++) {
      GenerateLiteralValue(el->mutable_extra_exprs()->Add()->mutable_lit_val(),
                           table->col_types[*it]);
      it++;
    }
  }
}

void GenerateWhereStatement(WhereStatement* where,
                            i::Schema* schema,
                            i::Table* table,
                            bool join = false) {
  BinaryExpr* we = where->mutable_expr()
                       ->mutable_expr()
                       ->mutable_comp_expr()
                       ->mutable_binary_expr();

  // TODO(mpdenton) exclude joins for now.
  if (!join && table->index_exprs.size() != 0 && RandInt(1, 5) >= 4) {
    // Use an indexed expression
    *we->mutable_lhs() =
        table->index_exprs[RandInt(0, table->index_exprs.size() - 1)];
    we->set_op(BINOP_LEQ);
    GenerateLiteralValue(we->mutable_rhs()->mutable_lit_val(),
                         CastTypeName::NUMERIC);
    return;
  }

  // Otherwise just use a simple predicate
  uint32_t col = RandInt(0, table->num_columns - 1);
  ExprSchemaTableColumn* stc =
      we->mutable_lhs()->mutable_comp_expr()->mutable_expr_stc();
  CreateColumn(stc->mutable_col(), col);
  if (join) {
    CreateTableFromUint32(stc->mutable_table(), table->table_num);
  }
  if (table->col_types[col] == CastTypeName::BLOB) {
    we->set_op(BINOP_NOTEQ);
    we->mutable_rhs()->mutable_lit_val()->set_special_val(
        LiteralValue::VAL_NULL);
  } else if (table->col_types[col] == CastTypeName::TEXT) {
    we->set_op(BINOP_REGEXP);
    we->mutable_rhs()->mutable_lit_val()->set_string_lit(".*");
  } else {
    we->set_op(BINOP_LEQ);
    we->mutable_rhs()->mutable_lit_val()->set_num_lit(RandUint64());
  }
}

void GenerateInsertion(Insert* i, i::Schema* schema, i::Table* table) {
  // TODO(mpdenton) generate With statement
  // i->set_insert_type(RANDOM_ENUM(Insert, InsertType));

  if (RandInt(1, 2) == 1) {
    i->set_insert_type(Insert::INSERT);
  } else {
    i->set_insert_type(Insert::REPLACE);
  }

  SchemaTableAsAlias* staa = i->mutable_staa();
  CreateSchemaTable(staa->mutable_schema_table(), table);

  if (RandInt(1, 5) >= 2) {
    std::set<uint32_t> cols = GenerateColumnList(i->mutable_col_list(), table);
    GenerateValuesStatement(i->mutable_values(), table, cols);
  }
}

void GenerateUpdate(Update* u, i::Schema* schema, i::Table* table) {
  SchemaTableAsAlias* staa = u->mutable_qtn()->mutable_staa();
  CreateSchemaTable(staa->mutable_schema_table(), table);

  ColEqualsExpr* cee = u->mutable_ucp2()->mutable_cee();
  uint32_t col = RandInt(0, table->num_columns - 1);
  CreateColumn(cee->mutable_col(), col);
  GenerateLiteralValue(cee->mutable_expr()->mutable_lit_val(),
                       table->col_types[col]);

  if (RandInt(1, 10) >= 2) {
    GenerateWhereStatement(u->mutable_ucp2()->mutable_where_stmt(), schema,
                           table);
  }
}

void GenerateDelete(Delete* d, i::Schema* schema, i::Table* table) {
  SchemaTableAsAlias* staa = d->mutable_qtn()->mutable_staa();
  CreateSchemaTable(staa->mutable_schema_table(), table);

  if (RandInt(1, 20) >= 2) {
    GenerateWhereStatement(d->mutable_where(), schema, table);
  }
}

void GenerateCreateTable(CreateTable* ct, i::Schema* schema, i::Table* table) {
  ct->set_if_not_exists(false);
  if (RandInt(1, 4) == 1) {
    ct->set_temp_modifier(TM_TEMP);
  }

  CreateSchemaTable(ct->mutable_schema_table(), table);

  if (table->num_columns > 1) {
    ct->mutable_op1()->mutable_extra_col_defs()->Reserve(table->num_columns -
                                                         1);
  }

  for (int i = 0; i < table->num_columns; i++) {
    ColumnDef* col_def;
    if (i == 0) {
      col_def = ct->mutable_op1()->mutable_col_def();
    } else {
      col_def = ct->mutable_op1()->mutable_extra_col_defs()->Add();
    }
    CreateColumn(col_def->mutable_col(), i);
    col_def->mutable_type_name()->mutable_ctn()->set_type_enum(
        table->col_types[i]);
    // Set default values
    GenerateLiteralValue(
        col_def->add_col_constraints()->mutable_opt2()->mutable_lit_val(),
        table->col_types[i]);
  }
}

bool IsNumeric(CastTypeName::CastTypeNameEnum type) {
  return (type == CastTypeName::NUMERIC || type == CastTypeName::INTEGER ||
          type == CastTypeName::REAL);
}

Expr GenerateJoinConstraints(i::Table* table,
                             const std::vector<i::Table*>& join_tables) {
  std::vector<i::Table*> all_tables = join_tables;
  all_tables.push_back(table);
  // Decide some columns have to be equal
  std::vector<std::pair<ExprSchemaTableColumn, ExprSchemaTableColumn>>
      equal_cols;
  std::vector<BinaryOperator> comparison_ops;

  // Would be better if the num_constraints
  do {
    ExprSchemaTableColumn column_a;
    ExprSchemaTableColumn column_b;
    int table_index_a = RandInt(0, all_tables.size() - 1);
    CreateTableFromUint32(column_a.mutable_table(),
                          all_tables[table_index_a]->table_num);
    int table_index_b;
    while ((table_index_b = RandInt(0, all_tables.size() - 1)) == table_index_a)
      ;
    CreateTableFromUint32(column_b.mutable_table(),
                          all_tables[table_index_b]->table_num);

    uint32_t col_a = RandInt(0, all_tables[table_index_a]->num_columns - 1);
    uint32_t col_b = RandInt(0, all_tables[table_index_b]->num_columns - 1);
    CreateColumn(column_a.mutable_col(), col_a);
    CreateColumn(column_b.mutable_col(), col_b);

    equal_cols.push_back({std::move(column_a), std::move(column_b)});

    // If both columns are numeric, small chance of using a comparison op
    // instead.
    if (IsNumeric(all_tables[table_index_a]->col_types[col_a]) &&
        IsNumeric(all_tables[table_index_b]->col_types[col_b]) &&
        RandInt(1, 2) == 1) {
      comparison_ops.push_back(BINOP_LEQ);
    } else {
      comparison_ops.push_back(BINOP_EQ);
    }
  } while (RandInt(1, 3) >= 2);

  // Actually generate the expressions.
  Expr initial_expr;
  Expr* curr_expr = &initial_expr;
  for (size_t i = 0; i < equal_cols.size() - 1; i++) {
    BinaryExpr* bin_expr = CreateDefaultCompExpr(curr_expr->mutable_comp_expr())
                               ->mutable_binary_expr();
    BinaryExpr* lhs_bin_expr =
        bin_expr->mutable_lhs()->mutable_comp_expr()->mutable_binary_expr();
    *lhs_bin_expr->mutable_lhs()->mutable_comp_expr()->mutable_expr_stc() =
        std::move(equal_cols[i].first);
    lhs_bin_expr->set_op(comparison_ops[i]);
    *lhs_bin_expr->mutable_rhs()->mutable_comp_expr()->mutable_expr_stc() =
        std::move(equal_cols[i].second);

    if (RandInt(1, 2) == 1) {
      bin_expr->set_op(BINOP_AND);
    } else {
      bin_expr->set_op(BINOP_OR);
    }

    curr_expr = bin_expr->mutable_rhs();
  }
  // Finish off final expr
  size_t last_index = equal_cols.size() - 1;
  BinaryExpr* bin_expr = CreateDefaultCompExpr(curr_expr->mutable_comp_expr())
                             ->mutable_binary_expr();
  *bin_expr->mutable_lhs()->mutable_comp_expr()->mutable_expr_stc() =
      std::move(equal_cols[last_index].first);
  bin_expr->set_op(comparison_ops[last_index]);
  *bin_expr->mutable_rhs()->mutable_comp_expr()->mutable_expr_stc() =
      std::move(equal_cols[last_index].second);

  return initial_expr;
}

void GenerateFromStatement(FromStatement* from,
                           i::Schema* schema,
                           i::Table* table,
                           const std::vector<i::Table*>& join_tables) {
  // TODO(mpdenton) join statements?
  if (join_tables.size() == 0) {
    SchemaTableAsAlias* staa =
        from->mutable_tos3()->add_tos_list()->mutable_qtn()->mutable_staa();
    CreateSchemaTable(staa->mutable_schema_table(), table);
    return;
  }

  // Write some nice joins.
  CreateSchemaTable(from->mutable_tos3()
                        ->mutable_join_clause()
                        ->mutable_tos()
                        ->mutable_qtn()
                        ->mutable_staa()
                        ->mutable_schema_table(),
                    table);

  // For each table in join_tables, write a JoinClauseCore that inner joins
  // with some comparisons between any two columns
  for (i::Table* curr_table : join_tables) {
    JoinClauseCore* jcc =
        from->mutable_tos3()->mutable_join_clause()->add_clauses();

    // Just generate inner joins, fuzzer should be smart enough to find other
    // join types.
    jcc->mutable_join_op()->set_join_type(JoinOperator::INNER);

    // Fill in the join clause core with the current table
    CreateSchemaTable(jcc->mutable_tos()
                          ->mutable_qtn()
                          ->mutable_staa()
                          ->mutable_schema_table(),
                      curr_table);

    *jcc->mutable_join_constraint()->mutable_on_expr() =
        GenerateJoinConstraints(table, join_tables);
  }

  // TODO(mpdenton) multiple Tables with aliases?
}

void GenerateGroupByStatement(GroupByStatement* gbs,
                              i::Schema* schema,
                              i::Table* table,
                              bool join = false) {
  ExprSchemaTableColumn* stc = gbs->mutable_exprs()
                                   ->mutable_expr()
                                   ->mutable_comp_expr()
                                   ->mutable_expr_stc();
  // fine to just pick a single random column.
  CreateColumn(stc->mutable_col(), RandInt(0, table->num_columns - 1));
  if (join) {
    CreateTableFromUint32(stc->mutable_table(), table->table_num);
  }
}

std::set<uint32_t> GenerateSelectStatementCore(
    SelectStatementCore* ssc,
    i::Schema* schema,
    i::Table* table,
    std::vector<i::Table*> join_tables) {
  if (RandInt(1, 2) == 1) {
    ssc->set_s_or_d(SelectStatementCore::SELECT);
  } else {
    ssc->set_s_or_d(SelectStatementCore::SELECT_DISTINCT);
  }

  std::set<uint32_t> cols;
  if (join_tables.size() > 0) {
    // This is a join. Add columns from all the tables and include the table.
    for (size_t i = 0; i <= join_tables.size(); i++) {
      i::Table* table2;
      if (i == join_tables.size()) {
        table2 = table;
      } else {
        table2 = join_tables[i];
      }

      cols = GetRandomNums(RandInt(1, table2->num_columns - 1),
                           table2->num_columns - 1);
      for (uint32_t col : cols) {
        ExprSchemaTableColumn* stc = ssc->add_result_columns()
                                         ->mutable_eca()
                                         ->mutable_expr()
                                         ->mutable_comp_expr()
                                         ->mutable_expr_stc();
        CreateColumn(stc->mutable_col(), col);
        CreateTableFromUint32(stc->mutable_table(), table2->table_num);
      }
    }
  } else {
    if (RandInt(1, 2) == 1) {
      cols = GetRandomNums(RandInt(1, table->num_columns - 1),
                           table->num_columns - 1);
      for (uint32_t col : cols) {
        CreateColumn(ssc->add_result_columns()->mutable_col(), col);
      }
    } else {
      AggregateFn* af = ssc->add_result_columns()
                            ->mutable_eca()
                            ->mutable_expr()
                            ->mutable_comp_expr()
                            ->mutable_fn_expr()
                            ->mutable_aggregate_fn();
      af->set_fn_name(RANDOM_ENUM(AggregateFn, FnName));
      af->set_distinct((bool)RandInt(0, 1));
      CreateColumn(af->mutable_col1(), RandInt(0, table->num_columns - 1));
    }
  }

  bool join = join_tables.size() > 0;

  GenerateFromStatement(ssc->mutable_from(), schema, table, join_tables);

  if (RandInt(1, 3) >= 2) {
    GenerateWhereStatement(ssc->mutable_where(), schema, table, join);
  }

  if (RandInt(1, 3) == 1) {
    GenerateGroupByStatement(ssc->mutable_groupby(), schema, table, join);
  }

  return cols;
}

void GenerateOrderByStatement(OrderByStatement* obs,
                              i::Schema* schema,
                              i::Table* table,
                              std::set<uint32_t> cols_tmp,
                              bool join = false) {
  // TODO(mpdenton) exclude joins for now.
  if (!join && table->index_exprs.size() != 0 && RandInt(1, 5) >= 4) {
    // Use an indexed expression
    *obs->mutable_ord_term()->mutable_expr() =
        table->index_exprs[RandInt(0, table->index_exprs.size() - 1)];
    return;
  }

  std::set<uint32_t> cols =
      GetRandomSubset(cols_tmp, RandInt(1, cols_tmp.size() - 1));

  std::vector<uint32_t> v;
  std::copy(cols.begin(), cols.end(), std::back_inserter(v));
  std::shuffle(v.begin(), v.end(), GetRandom());

  i::Table* table_in_col = join ? table : nullptr;
  auto it = v.begin();
  CreateColumnExpr(obs->mutable_ord_term()->mutable_expr(), *it, table_in_col);
  it++;
  for (size_t i = 0; i < v.size() - 1; i++) {
    CreateColumnExpr(obs->add_extra_ord_terms()->mutable_expr(), *it,
                     table_in_col);
    it++;
  }
}

void GenerateSelect(Select* s,
                    i::Schema* schema,
                    i::Table* table,
                    std::vector<i::Table*> join_tables = {}) {
  // Could be empty.
  std::set<uint32_t> cols = GenerateSelectStatementCore(
      s->mutable_select_core(), schema, table, join_tables);
  // TODO(mpdenton)

  if (RandInt(1, 2) == 1) {
    GenerateOrderByStatement(s->mutable_orderby(), schema, table,
                             GetRandomNums(RandInt(1, table->num_columns - 1),
                                           table->num_columns - 1),
                             join_tables.size() > 0);
  }

  // Limits are not very interesting from a corpus standpoint.
}

void InsertUpdateSelectOrDelete(SQLQuery* q,
                                i::Schema* main_schema,
                                int table_num) {
  int rand = RandInt(1, 4);
  if (rand == 1) {
    GenerateInsertion(q->mutable_insert(), main_schema,
                      &main_schema->tables[table_num]);
  } else if (rand == 2) {
    GenerateDelete(q->mutable_delete_(), main_schema,
                   &main_schema->tables[table_num]);
  } else if (rand == 3) {
    GenerateUpdate(q->mutable_update(), main_schema,
                   &main_schema->tables[table_num]);
  } else if (rand == 4) {
    GenerateSelect(q->mutable_select(), main_schema,
                   &main_schema->tables[table_num]);
  }
}

inline ExprSchemaTableColumn* GetSTC(Expr* expr) {
  return CreateDefaultCompExpr(expr->mutable_comp_expr())->mutable_expr_stc();
}

std::optional<Expr> GenerateCreateIndex(CreateIndex* ci,
                                        i::Schema* schema,
                                        i::Table* table,
                                        std::set<uint32_t>& free_index_nums) {
  CHECK(free_index_nums.size() != 0);

  std::set<uint32_t> index_num_set = GetRandomSubset(free_index_nums, 1);
  uint32_t index_num = *index_num_set.begin();
  ci->mutable_index()->set_index(index_num);
  free_index_nums.erase(index_num);
  CreateTableFromUint32(ci->mutable_table(), table->table_num);

  if (RandInt(1, 3) >= 2) {
    Expr expr;
    int expr_type = RandInt(1, 2);
    if (expr_type == 1) {
      // Select two random columns of the table, add or subtract them.
      uint32_t col1 = RandInt(0, table->num_columns - 1);
      uint32_t col2 = RandInt(0, table->num_columns - 1);

      BinaryExpr* bin_expr = CreateDefaultCompExpr(expr.mutable_comp_expr())
                                 ->mutable_binary_expr();
      ExprSchemaTableColumn* lhs_stc = GetSTC(bin_expr->mutable_lhs());
      ExprSchemaTableColumn* rhs_stc = GetSTC(bin_expr->mutable_rhs());

      CreateColumn(lhs_stc->mutable_col(), col1);
      CreateColumn(rhs_stc->mutable_col(), col2);

      // TODO(mpdenton) perhaps set the tables here? The tables must not be set
      // for CREATE INDEX, but MUST be set for JOINs to avoid ambiguous columns.
      // Does it still count as the same expression if the table is included in
      // the JOIN but not the CREATE INDEX?
      if (RandInt(1, 2) == 1) {
        bin_expr->set_op(BINOP_PLUS);
      } else {
        bin_expr->set_op(BINOP_MINUS);
      }
    } else if (expr_type == 2) {
      // Or, apply abs to a single column.
      OneArgFn* oaf = CreateDefaultCompExpr(expr.mutable_comp_expr())
                          ->mutable_fn_expr()
                          ->mutable_simple_fn()
                          ->mutable_one_arg_fn();
      oaf->set_fn_enum(OneArgFn::ABS);
      uint32_t col = RandInt(0, table->num_columns - 1);
      ExprSchemaTableColumn* stc = GetSTC(oaf->mutable_arg1());
      CreateColumn(stc->mutable_col(), col);
      // TODO(mpdenton) see above about setting tables.
    }

    *ci->mutable_icol_list()->mutable_indexed_col()->mutable_expr() = expr;

    return expr;
  }

  IndexedColumnList* icol_list = ci->mutable_icol_list();
  std::set<uint32_t> cols =
      GetRandomNums(RandInt(1, table->num_columns - 1), table->num_columns - 1);
  bool first;
  for (uint32_t col : cols) {
    IndexedColumn* icol;
    if (first) {
      first = false;
      icol = icol_list->mutable_indexed_col();
    } else {
      icol = icol_list->add_extra_indexed_cols();
    }
    CreateColumn(icol->mutable_col(), col);
  }

  return std::nullopt;
}

namespace {
enum class GenQueryInstr {
  SUCCESS,
  MOVE_ON,
  TRY_AGAIN,
};
}

template <typename T>
void GenQueries(SQLQueries& queries,
                int min,
                int max,
                bool txn,
                int num_tables,
                T gen) {
  queries.mutable_extra_queries()->Reserve(queries.extra_queries_size() + max +
                                           2);
  if (txn) {
    SQLQuery query;
    query.mutable_begin_txn();  // constructs a begin txn.
    queries.mutable_extra_queries()->Add(std::move(query));
  }
  for (int i = 0; i < num_tables; i++) {
    for (int j = 0; j < RandInt(min, max); j++) {
      // continue;  // TODO(mpdenton)
      SQLQuery query;
      GenQueryInstr success = gen(&query, i);
      // Try again
      if (success != GenQueryInstr::SUCCESS) {
        if (success == GenQueryInstr::TRY_AGAIN) {
          j--;
        }
        continue;
      }
      queries.mutable_extra_queries()->Add(std::move(query));
    }
  }
  if (txn) {
    SQLQuery query;
    query.mutable_commit_txn();  // constructs a begin txn.
    queries.mutable_extra_queries()->Add(std::move(query));
  }
}

void FirstCreateTable(CreateTable* ct) {
  ct->mutable_schema_table()->mutable_schema_name()->set_schema(5);
  ct->mutable_schema_table()->mutable_schema_name()->set_main(false);
  ct->mutable_schema_table()->mutable_schema_name()->set_temp(false);
  ct->mutable_schema_table()->mutable_table_name()->set_table(0);
  ct->set_if_not_exists(false);
  ct->mutable_op();
}

SQLQueries GenCorpusEntry() {
  // Create the tables, and attached databases with tables
  // Schema schemas[i::kNumSchemas];
  // for (int i = 0; i < i::kNumSchemas; i++) {
  //   // schemas[i] = Schema{
  //   //   .num_tables = RandInt(1, 5);
  //   // };
  // }
  SQLQueries queries;
  FirstCreateTable(queries.mutable_create_table());

  // Just get rid of the first CreateTable, it will error out but not screw up
  // anything below

  i::Schema main_schema;
  main_schema.num_tables = RandInt(1, 5);

  std::set<uint32_t> free_index_nums;
  for (uint32_t i = 0; i < 10; i++) {
    free_index_nums.insert(i);
  }

  GenQueries(
      queries, 1, 1, false, main_schema.num_tables, [&](SQLQuery* q, int i) {
        i::Table t = i::Table{
            .table_num = static_cast<uint32_t>(i),
            .num_columns = RandInt(1, 8),
        };
        for (int j = 0; j < t.num_columns; j++) {
          t.col_types.push_back(RANDOM_ENUM(CastTypeName, CastTypeNameEnum));
        }
        main_schema.tables.push_back(std::move(t));
        GenerateCreateTable(q->mutable_create_table(), &main_schema,
                            &main_schema.tables[i]);
        return GenQueryInstr::SUCCESS;
      });

  GenQueries(queries, kMinNumIndexes, kMaxNumIndexes, false,
             main_schema.num_tables, [&](SQLQuery* q, int i) {
               if (free_index_nums.size() == 0) {
                 return GenQueryInstr::MOVE_ON;
               }

               std::optional<Expr> index_expr =
                   GenerateCreateIndex(q->mutable_create_index(), &main_schema,
                                       &main_schema.tables[i], free_index_nums);
               if (index_expr) {
                 main_schema.tables[i].index_exprs.push_back(
                     std::move(index_expr.value()));
               }
               return GenQueryInstr::SUCCESS;
             });

  // Generate a bunch of inserts in a transaction (for speed)
  GenQueries(queries, kMinNumInsertions, kMaxNumInsertions, true,
             main_schema.num_tables, [&](SQLQuery* q, int i) {
               GenerateInsertion(q->mutable_insert(), &main_schema,
                                 &main_schema.tables[i]);
               return GenQueryInstr::SUCCESS;
             });

  // Generate a bunch of interesting selects with GroupBys, OrderBys, aggregate
  // functions, etc.
  GenQueries(queries, kMinNumSelects, kMaxNumSelects, false,
             main_schema.num_tables, [&](SQLQuery* q, int i) {
               GenerateSelect(q->mutable_select(), &main_schema,
                              &main_schema.tables[i]);
               return GenQueryInstr::SUCCESS;
             });

  // Generate lots of interesting JOINs.
  if (main_schema.num_tables > 1) {
    GenQueries(queries, kMinNumJoins, kMaxNumJoins, false,
               main_schema.num_tables, [&](SQLQuery* q, int i) {
                 std::set<uint32_t> tables =
                     GetRandomNums(RandInt(1, main_schema.num_tables - 1),
                                   main_schema.num_tables - 1);
                 tables.erase((uint32_t)i);
                 if (tables.size() == 0) {
                   // try again
                   return GenQueryInstr::TRY_AGAIN;
                 }
                 std::vector<i::Table*> tables_p;
                 for (uint32_t t : tables) {
                   tables_p.push_back(&main_schema.tables[t]);
                 }
                 GenerateSelect(q->mutable_select(), &main_schema,
                                &main_schema.tables[i], tables_p);
                 return GenQueryInstr::SUCCESS;
               });
  }

  // Generate a bunch of interesting updates.
  GenQueries(queries, kMinNumUpdates, kMaxNumUpdates, true,
             main_schema.num_tables, [&](SQLQuery* q, int i) {
               GenerateUpdate(q->mutable_update(), &main_schema,
                              &main_schema.tables[i]);
               return GenQueryInstr::SUCCESS;
             });

  // Generate interesting deletes.
  GenQueries(queries, kMinNumDeletes, kMaxNumDeletes, true,
             main_schema.num_tables, [&](SQLQuery* q, int i) {
               GenerateDelete(q->mutable_delete_(), &main_schema,
                              &main_schema.tables[i]);
               return GenQueryInstr::SUCCESS;
             });

  // Do everything except joins.
  GenQueries(queries, kMinNumOthers, kMaxNumOthers, true,
             main_schema.num_tables, [&](SQLQuery* q, int i) {
               InsertUpdateSelectOrDelete(q, &main_schema, i);
               return GenQueryInstr::SUCCESS;
             });

  return queries;
}

int main(int argc, char** argv) {
  base::CommandLine cl(argc, argv);

  int num_entries;
  if (!cl.HasSwitch("num_entries")) {
    LOG(FATAL) << "num_entries not specified.";
  }
  if (!base::StringToInt(cl.GetSwitchValueASCII("num_entries"), &num_entries)) {
    LOG(FATAL) << "num_entries not parseable as an int.";
  }

  bool output_to_dir = cl.HasSwitch("corpus_dir");
  bool to_stdout = !output_to_dir || ::getenv("LPM_DUMP_NATIVE_INPUT");
  bool print_sqlite_errors = ::getenv("PRINT_SQLITE_ERRORS");

  base::FilePath dir_path;
  if (output_to_dir) {
    to_stdout = false;

    dir_path = cl.GetSwitchValuePath("corpus_dir");
    base::File dir(dir_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!dir.IsValid()) {
      LOG(FATAL) << "corpus_dir " << dir_path << " could not be opened.";
    }

    base::File::Info dir_info;
    if (!dir.GetInfo(&dir_info)) {
      LOG(FATAL) << "Could not get corpus_dir " << dir_path << " file info.";
    }
    if (!dir_info.is_directory) {
      LOG(FATAL) << "corpus_dir " << dir_path << " is not a directory.";
    }
  } else {
    LOG(INFO) << "corpus_dir not specified, writing serialized output to "
                 "stdout instead.";
  }

  int file_name_index = 0;
  for (int i = 0; i < num_entries; i++) {
    SQLQueries queries = GenCorpusEntry();

    if (to_stdout || print_sqlite_errors) {
      // Printing to stdout or printing the sql errors requires converting the
      // queries to strings first.
      std::vector<std::string> queries_str;
      std::transform(
          queries.extra_queries().begin(), queries.extra_queries().end(),
          std::back_inserter(queries_str), sql_fuzzer::SQLQueryToString);

      if (to_stdout) {
        std::cout << base::JoinString(queries_str, "\n") << std::endl;
      }

      if (print_sqlite_errors) {
        sql_fuzzer::RunSqlQueries(queries_str, ::getenv("LPM_SQLITE_TRACE"));
      }
    }

    // If we just want to print to stdout, skip the directory stuff below.
    if (!output_to_dir) {
      continue;
    }

    // It's okay to serialize without all required fields, as LPM uses
    // ParsePartial* as well.
    std::string proto_text;
    if (!queries.SerializePartialToString(&proto_text)) {
      LOG(FATAL) << "Could not serialize queries to string.";
    }

    // Create a file to write the `proto_text` to.
    base::FilePath file_path;
    base::File file;
    for (; !file.IsValid(); file_name_index++) {
      file_path =
          dir_path.Append("corpus_queries" + std::to_string(file_name_index));
      file.Initialize(file_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    }

    // Write the `proto_text` data to the file.
    if (file.Write(0, proto_text.data(), proto_text.length()) < 0) {
      LOG(FATAL) << "Failed to write to file " << file_path;
    }
  }

  return 0;
}
