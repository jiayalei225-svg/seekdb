/*
 * Copyright (c) 2026 OceanBase.
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

#include <gtest/gtest.h>

#include <string>

#include "lib/allocator/page_arena.h"
#include "share/ob_errno.h"
#include "sql/parser/ob_parser.h"
#include "sql/parser/parse_node.h"

namespace oceanbase
{
namespace sql
{

namespace
{

bool contains_type(const ParseNode *node, const ObItemType type)
{
  bool found = nullptr != node && type == node->type_;
  for (int64_t i = 0; !found && nullptr != node && nullptr != node->children_
                    && i < node->num_child_; ++i) {
    found = contains_type(node->children_[i], type);
  }
  return found;
}

int64_t count_type(const ParseNode *node, const ObItemType type)
{
  int64_t count = nullptr != node && type == node->type_ ? 1 : 0;
  for (int64_t i = 0; nullptr != node && nullptr != node->children_
                    && i < node->num_child_; ++i) {
    count += count_type(node->children_[i], type);
  }
  return count;
}

} // namespace

class ObFileSqlParserTest : public ::testing::Test
{
protected:
  int parse(const std::string &sql, ParseResult &result)
  {
    ObParser parser(allocator_, 0);
    return parser.parse(common::ObString(static_cast<int32_t>(sql.length()), sql.c_str()),
                        result);
  }

  const ParseNode *statement(const ParseResult &result) const
  {
    return nullptr != result.result_tree_ && result.result_tree_->num_child_ > 0
           ? result.result_tree_->children_[0] : nullptr;
  }

  common::ObArenaAllocator allocator_;
};

TEST_F(ObFileSqlParserTest, read_csv_table_expression)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse("select id from read_csv('/tmp/a.csv')", result));
  const ParseNode *root = statement(result);
  ASSERT_NE(nullptr, root);
  EXPECT_EQ(T_SELECT, root->type_);
  EXPECT_TRUE(contains_type(root, T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, read_csv_options_and_alias)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse(
      "select f.id from read_csv('/tmp/a.csv', header = true, delimiter = ',', "
      "quote = '\"', escape = '\"', null_string = 'NULL', skip_rows = 1, "
      "ignore_errors = true, sample_size = -1, "
      "columns = {id BIGINT, name VARCHAR}) as f", result));
  EXPECT_TRUE(contains_type(statement(result), T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, read_json_and_ndjson)
{
  ParseResult json_result;
  ASSERT_EQ(OB_SUCCESS, parse(
      "select id from read_json('/tmp/a.jsonl', format = 'newline_delimited', "
      "columns = {id BIGINT})", json_result));
  EXPECT_TRUE(contains_type(statement(json_result), T_FILE_SCAN_TABLE));

  ParseResult ndjson_result;
  ASSERT_EQ(OB_SUCCESS, parse("select id from read_ndjson('/tmp/a.jsonl')", ndjson_result));
  EXPECT_TRUE(contains_type(statement(ndjson_result), T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, read_parquet_with_explicit_columns)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse(
      "select c1 from read_parquet('/tmp/a.parquet', columns = {c1 BIGINT, c2 VARCHAR})",
      result));
  EXPECT_TRUE(contains_type(statement(result), T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, glob_expression)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse("select file from glob('/tmp/*')", result));
  const ParseNode *root = statement(result);
  ASSERT_NE(nullptr, root);
  EXPECT_TRUE(contains_type(root, T_FILE_LIST_TABLE));
}

TEST_F(ObFileSqlParserTest, describe_select)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse("describe select * from read_csv('/tmp/a.csv')", result));
  const ParseNode *root = statement(result);
  ASSERT_NE(nullptr, root);
  EXPECT_EQ(T_EXPLAIN, root->type_);
  EXPECT_TRUE(contains_type(root, T_DESCRIBE_COLUMNS));
  EXPECT_TRUE(contains_type(root, T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, copy_to_csv_options)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse(
      "copy (select id, name from read_csv('/tmp/a.csv')) to '/tmp/out.csv' "
      "(format csv, header, delimiter ',', quote '\"', escape '\"', nullstr 'NULL')",
      result));
  const ParseNode *root = statement(result);
  ASSERT_NE(nullptr, root);
  EXPECT_TRUE(contains_type(root, T_INTO_OUTFILE));
  EXPECT_TRUE(contains_type(root, T_COPY_HEADER));
  EXPECT_TRUE(contains_type(root, T_COPY_NULLSTR));
  EXPECT_TRUE(contains_type(root, T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, join_two_file_tables)
{
  ParseResult result;
  ASSERT_EQ(OB_SUCCESS, parse(
      "select a.id from read_csv('/tmp/a.csv') a "
      "join read_json('/tmp/b.jsonl') b on a.id = b.id", result));
  EXPECT_EQ(2, count_type(statement(result), T_FILE_SCAN_TABLE));
}

TEST_F(ObFileSqlParserTest, rejects_invalid_positional_format)
{
  ParseResult result;
  EXPECT_NE(OB_SUCCESS, parse("select * from read_csv('/tmp/a.csv', 'xml')", result));
}

} // namespace sql
} // namespace oceanbase
