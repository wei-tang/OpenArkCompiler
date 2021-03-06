/*
 * Copyright (c) [2019-2021] Huawei Technologies Co.,Ltd.All rights reserved.
 *
 * OpenArkCompiler is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *     http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
 * FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "me_ssa_tab.h"
#include "me_cfg.h"
#include <cstdlib>
#include "mpl_timer.h"

// allocate the data structure to store SSA information
namespace maple {
AnalysisResult *MeDoSSATab::Run(MeFunction *func, MeFuncResultMgr *m, ModuleResultMgr*) {
  auto cfg = static_cast<MeCFG*>(m->GetAnalysisResult(MeFuncPhase_MECFG, func));
  MPLTimer timer;
  timer.Start();
  if (DEBUGFUNC(func)) {
    LogInfo::MapleLogger() << "\n============== SSA and AA preparation =============" << '\n';
  }
  MemPool *memPool = NewMemPool();
  // allocate ssaTab including its SSAPart to store SSA information for statements
  auto *ssaTab = memPool->New<SSATab>(memPool, func->GetVersMp(), &func->GetMIRModule(), func);
  func->SetMeSSATab(ssaTab);
#if DEBUG
  globalSSATab = ssaTab;
#endif
  // pass through the program statements
  for (auto bIt = cfg->valid_begin(); bIt != cfg->valid_end(); ++bIt) {
    auto *bb = *bIt;
    for (auto &stmt : bb->GetStmtNodes()) {
      ssaTab->CreateSSAStmt(stmt, bb);  // this adds the SSANodes for exprs
    }
  }
  if (DEBUGFUNC(func)) {
    timer.Stop();
    LogInfo::MapleLogger() << "ssaTab consumes cumulatively " << timer.Elapsed() << "seconds " << '\n';
  }
  return ssaTab;
}
}  // namespace maple
