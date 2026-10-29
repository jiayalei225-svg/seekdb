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

#pragma once

#include "lib/container/ob_se_array.h"
#include "lib/string/ob_string.h"
#include "lib/utility/ob_print_utils.h"

namespace oceanbase
{
namespace sql
{

enum class ObFileTableKind : int32_t
{
  SCAN = 0,
  LIST,
  SCHEMA,
  INVALID
};

enum class ObFileFormat : int32_t
{
  AUTO = 0,
  CSV,
  JSONL,
  PARQUET,
  INVALID
};

enum class ObFileColumnType : int32_t
{
  NULL_TYPE = 0,
  BOOLEAN,
  BIGINT,
  DOUBLE,
  VARCHAR,
  DATE,
  DATETIME,
  INVALID
};

struct ObFileTableColumnDef
{
  ObFileTableColumnDef()
    : source_name_(), column_name_(), source_type_name_(),
      type_(ObFileColumnType::NULL_TYPE), nullable_(false), max_length_(0)
  {}

  common::ObString source_name_;
  common::ObString column_name_;
  common::ObString source_type_name_;
  ObFileColumnType type_;
  bool nullable_;
  int64_t max_length_;
  TO_STRING_KV(K_(source_name), K_(column_name), K_(source_type_name),
               K_(type), K_(nullable), K_(max_length));
};

struct ObFileTableDef
{
  ObFileTableDef()
    : kind_(ObFileTableKind::INVALID), canonical_path_(), glob_pattern_(), secure_file_priv_(),
      format_(ObFileFormat::INVALID), columns_(), source_columns_(), estimated_rows_(0), device_(0),
      inode_(0), file_size_(0), modified_time_ns_(0),
      header_(true), header_specified_(false), delimiter_(','), delimiter_specified_(false),
      quote_('"'), has_quote_(true), escape_('"'), has_escape_(true),
      skip_rows_(0), ignore_errors_(false), null_string_(), has_explicit_columns_(false)
  {}

  ObFileTableKind kind_;
  common::ObString canonical_path_;
  common::ObString glob_pattern_;
  common::ObString secure_file_priv_;
  ObFileFormat format_;
  common::ObSEArray<ObFileTableColumnDef, 16> columns_;
  common::ObSEArray<ObFileTableColumnDef, 16> source_columns_;
  int64_t estimated_rows_;
  uint64_t device_;
  uint64_t inode_;
  int64_t file_size_;
  int64_t modified_time_ns_;
  bool header_;
  bool header_specified_;
  int32_t delimiter_;
  bool delimiter_specified_;
  int32_t quote_;
  bool has_quote_;
  int32_t escape_;
  bool has_escape_;
  int64_t skip_rows_;
  bool ignore_errors_;
  common::ObString null_string_;
  bool has_explicit_columns_;
  TO_STRING_KV(K_(kind), K_(canonical_path), K_(glob_pattern), K_(secure_file_priv), K_(format),
               K_(columns), K_(source_columns),
               K_(estimated_rows), K_(device), K_(inode), K_(file_size), K_(modified_time_ns),
               K_(header), K_(header_specified), K_(delimiter), K_(delimiter_specified),
               K_(quote), K_(has_quote), K_(escape), K_(has_escape), K_(skip_rows),
               K_(ignore_errors), K_(null_string), K_(has_explicit_columns));
};

} // namespace sql
} // namespace oceanbase
