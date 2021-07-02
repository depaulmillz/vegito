/*
 * The code is a part of our project called VEGITO, which retrofits
 * high availability mechanism to tame hybrid transaction/analytical
 * processing.
 *
 * Copyright (c) 2021 Shanghai Jiao Tong University.
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://ipads.se.sjtu.edu.cn/projects/vegito
 *
 */

#include "app/ch/ch_query.h"

#include <thread>

using namespace std;
using namespace nocc::oltp::ch;

namespace {

struct Ctx {
  // Store data
  BackupDB *db;
  uint64_t ver;

  // Partition information
  uint64_t ol_begin;
  uint64_t ol_end;

  // Query data
  int64_t ol_quantity;

  // Evaluation data
  uint64_t cnt;

  Ctx(BackupDB *d, uint64_t v, uint64_t b, uint64_t e) 
    : db(d), ver(v), ol_begin(b), ol_end(e), cnt(0), ol_quantity(0) { }
} __attribute__ ((aligned (CACHE_LINE_SZ))) ;

void query(void *arg) {
  Ctx &ctx = *(Ctx *) arg;
 
  register uint64_t cnt = 0;
  register uint64_t ol_quantity = 0;

  ctx.ver = 0;

  BackupStore *store = ctx.db->getStore(ORLI);
  for (int i = 0; i < 1; ++i) {
    unique_ptr<BackupStore::RowCursor> row_cur = store->getRowCursor(ctx.ver);
    if (row_cur.get() != nullptr) {
      // row store
      uint64_t off = store->locateCol(OL_DELIVERY_D, sizeof(uint32_t)); 
      row_cur->seekOffset(ctx.ol_begin, ctx.ol_end);
      while (row_cur->nextRow()) {
        const char *val = row_cur->value(); 
        int32_t order_cnt = *(uint32_t *) (val + off);
        ol_quantity += order_cnt;
        ++cnt ;
      } 
    } else {
      unique_ptr<BackupStore::ColCursor> col_cur = store->getColCursor(OL_DELIVERY_D, ctx.ver);
      assert(col_cur.get() != nullptr);
      uint64_t off = 0;
      // off = store->locateCol(OL_DELIVERY_D, sizeof(uint32_t));
      assert(off == 0); 
      col_cur->seekOffset(ctx.ol_begin, ctx.ol_end);
      while (col_cur->nextRow()) {
        const char *val = col_cur->value(); 
        int32_t order_cnt = *(uint32_t *) (val + off);
        ol_quantity += order_cnt;
        ++cnt ;
      }
    }
  
    ctx.ol_quantity = ol_quantity;
    ctx.cnt = cnt;
  }
}

}

namespace nocc {
namespace oltp {
namespace ch {

// Tranverse stock table and sum the unchanged field 'ol_quantity'
bool ChQueryWorker::micro_col_update2(yield_func_t &yield) {
  uint64_t ver = get_read_ver_();
  uint64_t cnt = 0;
  // Result data
  int64_t sum_ol_quantity = 0;

  /**************** Parallel Part Begin ********************/

  // Thread information
  vector<Ctx> ctxs;
  ctxs.reserve(num_thread_);

  // Calculate workload for each thread
  uint64_t ol_tbl_sz = db_->getStore(ORLI)->getOffsetHeader();
  uint64_t workload_per_thread = (ol_tbl_sz + num_thread_ - 1) / num_thread_;

  for (int i = 0; i < num_thread_; ++i) {
    uint64_t ol_begin = workload_per_thread * i;
    uint64_t ol_end = workload_per_thread * (i + 1);
    if (ol_end >= ol_tbl_sz)
      ol_end = ol_tbl_sz;
    ctxs.emplace_back(db_, ver, ol_begin, ol_end);
  }

  // start query
  for (int i = 1; i < subs_.size(); ++i) {
    subs_[i]->set_task(query, (void *) &ctxs[i]);
  }

  query((void *) &ctxs[0]);

  for (int i = 1; i < subs_.size(); ++i) 
    subs_[i]->clear();

  // Collect data from all slave threads
  for (const Ctx &ctx : ctxs) {
    sum_ol_quantity += ctx.ol_quantity;
    cnt += ctx.cnt;
  }

  printf("sum_ol_quantity = %ld, cnt = %lu\n", sum_ol_quantity, cnt);

  /**************** Parallel Part End ********************/

#if 0
#if SHOW_QUERY_RESULT
  printf("sum_ol_quantity\n");
  printf("%-lu\n", sum_ol_quantity);
#endif

  float seconds = float(end - begin) / SECOND_CYCLE_;
  printf("Query configuration: use %s and %s\n",
          stock_types[BACKUP_STORE_TYPE].c_str(),
          query_methods[QUERY_METHOD].c_str());
  printf("Total record cnt: %lu, result size: %d, %f secs\n",
          cnt, 1, seconds);
#endif
  return true;
}

}  // namespace ch
}  // namesapce oltp
}  // namespace nocc
