/*
 * Copyright (c) [2020-2021] Huawei Technologies Co.,Ltd.All rights reserved.
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
#ifndef MAPLEME_INCLUDE_ME_PREGRENAMER_H
#define MAPLEME_INCLUDE_ME_PREGRENAMER_H
#include "me_irmap.h"
#include "ssa_pre.h"

namespace maple {
class PregRenamer {
 public:
  PregRenamer(MemPool *mp, MeFunction *f, MeIRMap *hmap) : mp(mp), alloc(mp), func(f), meirmap(hmap) {}
  void RunSelf();

 private:
  std::string PhaseName() const {
    return "pregrename";
  }

  MemPool *mp;
  MapleAllocator alloc;
  MeFunction *func;
  MeIRMap *meirmap;
};

class MeDoPregRename : public MeFuncPhase {
 public:
  explicit MeDoPregRename(MePhaseID id) : MeFuncPhase(id) {}

  virtual ~MeDoPregRename() = default;
  AnalysisResult *Run(MeFunction *ir, MeFuncResultMgr *m, ModuleResultMgr *mrm) override;
  std::string PhaseName() const override {
    return "pregrename";
  }
};
}  // namespace maple
#endif  // MAPLEME_INCLUDE_ME_PREGRENAMER_H
