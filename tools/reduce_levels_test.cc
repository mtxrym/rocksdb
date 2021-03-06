//  Copyright (c) 2013, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
#include "rocksdb/db.h"
#include "db/db_impl.h"
#include "db/version_set.h"
#include "util/logging.h"
#include "util/testutil.h"
#include "util/testharness.h"
#include "util/ldb_cmd.h"

namespace rocksdb {

class ReduceLevelTest : public testing::Test {
public:
  ReduceLevelTest() {
    dbname_ = test::TmpDir() + "/db_reduce_levels_test";
    DestroyDB(dbname_, Options());
    db_ = nullptr;
  }

  Status OpenDB(bool create_if_missing, int levels,
      int mem_table_compact_level);

  Status Put(const std::string& k, const std::string& v) {
    return db_->Put(WriteOptions(), k, v);
  }

  std::string Get(const std::string& k) {
    ReadOptions options;
    std::string result;
    Status s = db_->Get(options, k, &result);
    if (s.IsNotFound()) {
      result = "NOT_FOUND";
    } else if (!s.ok()) {
      result = s.ToString();
    }
    return result;
  }

  Status CompactMemTable() {
    if (db_ == nullptr) {
      return Status::InvalidArgument("DB not opened.");
    }
    DBImpl* db_impl = reinterpret_cast<DBImpl*>(db_);
    return db_impl->TEST_FlushMemTable();
  }

  void CloseDB() {
    if (db_ != nullptr) {
      delete db_;
      db_ = nullptr;
    }
  }

  bool ReduceLevels(int target_level);

  int FilesOnLevel(int level) {
    std::string property;
    EXPECT_TRUE(db_->GetProperty(
        "rocksdb.num-files-at-level" + NumberToString(level), &property));
    return atoi(property.c_str());
  }

private:
  std::string dbname_;
  DB* db_;
};

Status ReduceLevelTest::OpenDB(bool create_if_missing, int num_levels,
    int mem_table_compact_level) {
  rocksdb::Options opt;
  opt.num_levels = num_levels;
  opt.create_if_missing = create_if_missing;
  opt.max_mem_compaction_level = mem_table_compact_level;
  opt.max_background_flushes = 0;
  rocksdb::Status st = rocksdb::DB::Open(opt, dbname_, &db_);
  if (!st.ok()) {
    fprintf(stderr, "Can't open the db:%s\n", st.ToString().c_str());
  }
  return st;
}

bool ReduceLevelTest::ReduceLevels(int target_level) {
  std::vector<std::string> args = rocksdb::ReduceDBLevelsCommand::PrepareArgs(
      dbname_, target_level, false);
  LDBCommand* level_reducer = LDBCommand::InitFromCmdLineArgs(
      args, Options(), LDBOptions());
  level_reducer->Run();
  bool is_succeed = level_reducer->GetExecuteState().IsSucceed();
  delete level_reducer;
  return is_succeed;
}

TEST_F(ReduceLevelTest, Last_Level) {
  // create files on all levels;
  ASSERT_OK(OpenDB(true, 4, 3));
  ASSERT_OK(Put("aaaa", "11111"));
  ASSERT_OK(CompactMemTable());
  ASSERT_EQ(FilesOnLevel(3), 1);
  CloseDB();

  ASSERT_TRUE(ReduceLevels(3));
  ASSERT_OK(OpenDB(true, 3, 1));
  ASSERT_EQ(FilesOnLevel(2), 1);
  CloseDB();

  ASSERT_TRUE(ReduceLevels(2));
  ASSERT_OK(OpenDB(true, 2, 1));
  ASSERT_EQ(FilesOnLevel(1), 1);
  CloseDB();
}

TEST_F(ReduceLevelTest, Top_Level) {
  // create files on all levels;
  ASSERT_OK(OpenDB(true, 5, 0));
  ASSERT_OK(Put("aaaa", "11111"));
  ASSERT_OK(CompactMemTable());
  ASSERT_EQ(FilesOnLevel(0), 1);
  CloseDB();

  ASSERT_TRUE(ReduceLevels(4));
  ASSERT_OK(OpenDB(true, 4, 0));
  CloseDB();

  ASSERT_TRUE(ReduceLevels(3));
  ASSERT_OK(OpenDB(true, 3, 0));
  CloseDB();

  ASSERT_TRUE(ReduceLevels(2));
  ASSERT_OK(OpenDB(true, 2, 0));
  CloseDB();
}

TEST_F(ReduceLevelTest, All_Levels) {
  // create files on all levels;
  ASSERT_OK(OpenDB(true, 5, 1));
  ASSERT_OK(Put("a", "a11111"));
  ASSERT_OK(CompactMemTable());
  ASSERT_EQ(FilesOnLevel(1), 1);
  CloseDB();

  ASSERT_OK(OpenDB(true, 5, 2));
  ASSERT_OK(Put("b", "b11111"));
  ASSERT_OK(CompactMemTable());
  ASSERT_EQ(FilesOnLevel(1), 1);
  ASSERT_EQ(FilesOnLevel(2), 1);
  CloseDB();

  ASSERT_OK(OpenDB(true, 5, 3));
  ASSERT_OK(Put("c", "c11111"));
  ASSERT_OK(CompactMemTable());
  ASSERT_EQ(FilesOnLevel(1), 1);
  ASSERT_EQ(FilesOnLevel(2), 1);
  ASSERT_EQ(FilesOnLevel(3), 1);
  CloseDB();

  ASSERT_OK(OpenDB(true, 5, 4));
  ASSERT_OK(Put("d", "d11111"));
  ASSERT_OK(CompactMemTable());
  ASSERT_EQ(FilesOnLevel(1), 1);
  ASSERT_EQ(FilesOnLevel(2), 1);
  ASSERT_EQ(FilesOnLevel(3), 1);
  ASSERT_EQ(FilesOnLevel(4), 1);
  CloseDB();

  ASSERT_TRUE(ReduceLevels(4));
  ASSERT_OK(OpenDB(true, 4, 0));
  ASSERT_EQ("a11111", Get("a"));
  ASSERT_EQ("b11111", Get("b"));
  ASSERT_EQ("c11111", Get("c"));
  ASSERT_EQ("d11111", Get("d"));
  CloseDB();

  ASSERT_TRUE(ReduceLevels(3));
  ASSERT_OK(OpenDB(true, 3, 0));
  ASSERT_EQ("a11111", Get("a"));
  ASSERT_EQ("b11111", Get("b"));
  ASSERT_EQ("c11111", Get("c"));
  ASSERT_EQ("d11111", Get("d"));
  CloseDB();

  ASSERT_TRUE(ReduceLevels(2));
  ASSERT_OK(OpenDB(true, 2, 0));
  ASSERT_EQ("a11111", Get("a"));
  ASSERT_EQ("b11111", Get("b"));
  ASSERT_EQ("c11111", Get("c"));
  ASSERT_EQ("d11111", Get("d"));
  CloseDB();
}

}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
