/*
 * Copyright (c) 2025 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define USING_LOG_PREFIX SQL_RESV
#include "sql/resolver/ddl/ob_explain_resolver.h"
#include "sql/resolver/ob_resolver.h"

#include "lib/string/ob_sql_string.h"
#include "sql/parser/ob_parser.h"
#include "sql/resolver/dml/ob_dml_stmt.h"
#include "sql/resolver/dml/ob_select_stmt.h"
#include "sql/resolver/expr/ob_raw_expr.h"
#include "sql/ob_file_scan_common.h"

using namespace oceanbase;
using namespace sql;
using namespace common;
const static int64_t EXPLAIN_FORMAT = 0;
const static int64_t EXPLAIN_DISPLAY_OPTION = 1;
const static int64_t EXPLAIN_INTO_TABLE = 2;
const static int64_t EXPLAIN_STMT_ID = 3;
const static int64_t EXPLAIN_CHILD_STMT = 4;

namespace
{

const char *file_column_type_name(const ObFileColumnType type)
{
  const char *name = "VARCHAR";
  switch (type) {
    case ObFileColumnType::BOOLEAN: name = "BOOLEAN"; break;
    case ObFileColumnType::BIGINT: name = "BIGINT"; break;
    case ObFileColumnType::DOUBLE: name = "DOUBLE"; break;
    case ObFileColumnType::DATE: name = "DATE"; break;
    case ObFileColumnType::DATETIME: name = "DATETIME"; break;
    case ObFileColumnType::NULL_TYPE:
    case ObFileColumnType::VARCHAR:
    default: name = "VARCHAR"; break;
  }
  return name;
}

const char *raw_expr_type_name(const ObObjType type)
{
  const char *name = "VARCHAR";
  switch (type) {
    case ObTinyIntType: name = "BOOLEAN"; break;
    case ObIntType:
    case ObInt32Type:
    case ObUInt64Type: name = "BIGINT"; break;
    case ObFloatType:
    case ObDoubleType:
    case ObNumberType:
    case ObUNumberType: name = "DOUBLE"; break;
    case ObDateType: name = "DATE"; break;
    case ObDateTimeType: name = "DATETIME"; break;
    default: name = "VARCHAR"; break;
  }
  return name;
}

bool has_file_table(const ObSelectStmt &stmt)
{
  bool found = false;
  const ObIArray<TableItem *> &table_items = stmt.get_table_items();
  for (int64_t i = 0; !found && i < table_items.count(); ++i) {
    found = NULL != table_items.at(i) && table_items.at(i)->is_file_table();
  }
  return found;
}

bool is_file_describe(const ParseNode &parse_tree, const ObSelectStmt &child_stmt)
{
  const ParseNode *display_node = parse_tree.children_[EXPLAIN_DISPLAY_OPTION];
  return NULL != display_node && T_DESCRIBE_COLUMNS == display_node->type_
         && NULL == parse_tree.children_[EXPLAIN_FORMAT]
         && NULL == parse_tree.children_[EXPLAIN_INTO_TABLE]
         && NULL == parse_tree.children_[EXPLAIN_STMT_ID]
         && has_file_table(child_stmt);
}

} // namespace

int ObExplainResolver::resolve(const ParseNode &parse_tree)
{
  int ret = OB_SUCCESS;
  ObExplainStmt *explain_stmt = NULL;
  if (!(T_EXPLAIN == parse_tree.type_  && 5 == parse_tree.num_child_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid EXPLAIN syntax",
             "type", parse_tree.type_,
             "# of child", parse_tree.num_child_);
  } else if (NULL == (explain_stmt = create_stmt<ObExplainStmt>())) {
    ret = OB_SQL_RESOLVER_NO_MEMORY;
    LOG_WARN("failed to create explain stmt");
  } else {
    ParseNode *child_node = parse_tree.children_[EXPLAIN_CHILD_STMT];
    ObStmt *child_stmt = NULL;
    ObExplainDisplayOpt opt;
    opt.with_tree_line_ = true;
    if (OB_ISNULL(child_node)) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("empty parse tree for stmt");
    } else if (!(T_SELECT == child_node->type_
                 || T_DELETE == child_node->type_
                 || T_INSERT == child_node->type_
                 || T_UPDATE == child_node->type_
                 || T_REPLACE == child_node->type_)) {
      ret = OB_NOT_SUPPORTED;
      LOG_WARN("invalid stmt type for EXPLAIN", "type", child_node->type_);
      LOG_USER_ERROR(OB_NOT_SUPPORTED, "stmt type for EXPLAIN");
    } else {
      ObResolver resolver(params_);
      if (OB_FAIL(resolver.resolve(ObResolver::IS_NOT_PREPARED_STMT,
                                   *child_node,
                                   child_stmt))) {
      } else if (OB_ISNULL(child_stmt)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("failed to generate stmt for child stmt");
      } else if (stmt::T_SELECT == child_stmt->get_stmt_type()
                 && is_file_describe(parse_tree, static_cast<ObSelectStmt &>(*child_stmt))) {
        ObStmt *meta_stmt = NULL;
        if (OB_FAIL(resolve_file_sql_describe(static_cast<ObSelectStmt &>(*child_stmt),
                                              meta_stmt))) {
          LOG_WARN("failed to describe file table", K(ret));
        } else if (OB_ISNULL(meta_stmt)) {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("null file describe stmt", K(ret));
        } else {
          stmt_ = meta_stmt;
        }
      } else {
        explain_stmt->set_explain_query_stmt(static_cast<ObDMLStmt *>(child_stmt));
        ParseNode *format = parse_tree.children_[EXPLAIN_FORMAT];
        if (NULL != format) {
          switch (format->type_) {
          case T_OUTLINE:
          {
            explain_stmt->set_explain_format(EXPLAIN_OUTLINE);
            opt.with_tree_line_ = true;
          } break;
          case T_EXTENDED:
          {
            explain_stmt->set_explain_format(EXPLAIN_EXTENDED);
            opt.with_tree_line_ = true;
          } break;
          case T_PARTITIONS:
          {
            explain_stmt->set_explain_format(EXPLAIN_PARTITIONS);
            opt.with_tree_line_ = true;
          } break;
          case T_TRADITIONAL:
          {
            explain_stmt->set_explain_format(EXPLAIN_TRADITIONAL);
            opt.with_tree_line_ = true;
          } break;
          case T_FORMAT_JSON:
          {
            explain_stmt->set_explain_format(EXPLAIN_FORMAT_JSON);
            opt.with_tree_line_ = false;
          } break;
          case T_BASIC: {
            explain_stmt->set_explain_format(EXPLAIN_BASIC);
            opt.with_tree_line_ = true;
            break;
          }
          case T_EXTENDED_NOADDR: {
            explain_stmt->set_explain_format(EXPLAIN_EXTENDED_NOADDR);
            opt.with_tree_line_ = true;
            break;
          }
          case T_PLANREGRESS: {
            explain_stmt->set_explain_format(EXPLAIN_PLANREGRESS);
            opt.with_tree_line_ = true;
            break;
          }
          default:
            ret = OB_ERROR;
            LOG_WARN("unknown explain format", "format", format->type_);
            break;
          }
        }
      }
    }
    ParseNode *opt_node = parse_tree.children_[EXPLAIN_DISPLAY_OPTION];
    if (OB_SUCC(ret) && NULL != opt_node) {
      switch (opt_node->type_) {
      case T_PRETTY_COLOR:
        opt.with_color_ = true;
        // pass
      case T_PRETTY:
        opt.with_tree_line_ = true;
        break;
      case T_DESCRIBE_COLUMNS:
        // Marker only; non-file DESCRIBE keeps the normal EXPLAIN output.
        break;
      default:
        ret = OB_ERR_UNEXPECTED;
      }
    }
    if (OB_SUCC(ret)) {
      explain_stmt->set_display_opt(opt);
    }
    ParseNode *into_table_node = parse_tree.children_[EXPLAIN_INTO_TABLE];
    if (OB_SUCC(ret) && NULL != into_table_node) {
      ObString into_table(into_table_node->str_len_, into_table_node->str_value_);
      explain_stmt->set_into_table(into_table);
    }
    ParseNode *statement_id_node = parse_tree.children_[EXPLAIN_STMT_ID];
    if (OB_SUCC(ret) && NULL != statement_id_node) {
      ObString statement_id(statement_id_node->str_len_, statement_id_node->str_value_);
      explain_stmt->set_statement_id(statement_id);
    }
  }

  return ret;
}

int ObExplainResolver::resolve_file_sql_describe(ObSelectStmt &inner_stmt, ObStmt *&out_stmt)
{
  int ret = OB_SUCCESS;
  ObSqlString sql;
  const ObIArray<SelectItem> &select_items = inner_stmt.get_select_items();
  const ObIArray<TableItem *> &table_items = inner_stmt.get_table_items();
  out_stmt = NULL;
  if (OB_ISNULL(params_.allocator_) || OB_ISNULL(session_info_)) {
    ret = OB_NOT_INIT;
  } else if (0 == select_items.count()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("describe target has no selected column", K(ret));
  } else if (OB_FAIL(sql.append("select column_0 as column_name, column_1 as column_type, "
                                "column_2 as `null`, column_3 as `key`, "
                                "column_4 as `default`, column_5 as extra from (values "))) {
    LOG_WARN("failed to build describe sql", K(ret));
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < select_items.count(); ++i) {
      const SelectItem &item = select_items.at(i);
      const ObFileTableColumnDef *file_column = NULL;
      if (NULL != item.expr_ && T_REF_COLUMN == item.expr_->get_expr_type()) {
        ObColumnRefRawExpr *col_expr = static_cast<ObColumnRefRawExpr *>(item.expr_);
        for (int64_t t = 0; NULL == file_column && t < table_items.count(); ++t) {
          const TableItem *table_item = table_items.at(t);
          if (NULL != table_item && table_item->is_file_table()
              && table_item->table_id_ == col_expr->get_table_id()
              && NULL != table_item->file_table_def_) {
            const int64_t column_idx =
                static_cast<int64_t>(col_expr->get_column_id()) - OB_APP_MIN_COLUMN_ID;
            if (column_idx >= 0
                && column_idx < table_item->file_table_def_->columns_.count()) {
              file_column = &table_item->file_table_def_->columns_.at(column_idx);
            }
          }
        }
      }
      ObString col_name = item.alias_name_;
      if (col_name.empty()) {
        col_name = item.expr_name_;
      }
      ObSqlString escaped_name;
      for (int64_t k = 0; OB_SUCC(ret) && k < col_name.length(); ++k) {
        const char c = col_name.ptr()[k];
        if ('\'' == c) {
          ret = escaped_name.append("''");
        } else {
          ret = escaped_name.append_fmt("%c", c);
        }
      }
      const char *type_name = NULL == file_column
                              ? raw_expr_type_name(NULL == item.expr_ ? ObMaxType
                                                                      : item.expr_->get_data_type())
                              : file_column_type_name(file_column->type_);
      const bool nullable = NULL == file_column
                            || file_column->nullable_;
      const char *nullable_str = nullable ? "YES" : "NO";
      if (OB_SUCC(ret)) {
        ret = sql.append_fmt("%srow('%.*s','%s','%s',NULL,NULL,NULL)",
                             i > 0 ? "," : "",
                             static_cast<int>(escaped_name.length()), escaped_name.ptr(),
                             type_name, nullable_str);
      }
    }
    if (OB_SUCC(ret)) {
      ret = sql.append(") v");
    }
  }
  if (OB_SUCC(ret)) {
    ObParser parser(*params_.allocator_, session_info_->get_sql_mode(),
                    session_info_->get_charsets4parser());
    ParseResult parse_result;
    if (OB_FAIL(parser.parse(sql.string(), parse_result))) {
      LOG_WARN("failed to parse describe sql", K(ret), K(sql));
    } else if (OB_ISNULL(parse_result.result_tree_)
               || OB_UNLIKELY(parse_result.result_tree_->num_child_ < 1)
               || OB_ISNULL(parse_result.result_tree_->children_[0])) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("unexpected describe parse result", K(ret), K(sql));
    } else {
      ObResolver resolver(params_);
      if (OB_FAIL(resolver.resolve(ObResolver::IS_NOT_PREPARED_STMT,
                                   *parse_result.result_tree_->children_[0], out_stmt))) {
        LOG_WARN("failed to resolve describe sql", K(ret), K(sql));
      }
    }
  }
  return ret;
}
