/*
 * Copyright (c) [2019] Huawei Technologies Co.,Ltd.All rights reserved.
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
#ifndef MAPLE_ME_INCLUDE_MECONSTPROP_H
#define MAPLE_ME_INCLUDE_MECONSTPROP_H
#include "me_ir.h"
#include "me_phase.h"

namespace maple {
class MeConstProp {
 public:
  MeConstProp() = default;

  virtual ~MeConstProp() = default;
  void IntraConstProp() const;
  void InterConstProp() const;
};

class MeDoIntraConstProp : public MeFuncPhase {
 public:
  explicit MeDoIntraConstProp(MePhaseID id) : MeFuncPhase(id) {}

  virtual ~MeDoIntraConstProp() = default;
  AnalysisResult *Run(MeFunction *func, MeFuncResultMgr *frm, ModuleResultMgr *mrm) override;
  std::string PhaseName() const override {
    return "intraconstantpropagation";
  }
};

class MeDoInterConstProp : public MeFuncPhase {
 public:
  explicit MeDoInterConstProp(MePhaseID id) : MeFuncPhase(id) {}

  virtual ~MeDoInterConstProp() = default;
  AnalysisResult *Run(MeFunction *func, MeFuncResultMgr *frm, ModuleResultMgr *mrm) override;
  std::string PhaseName() const override {
    return "interconstantpropagation";
  }
};
}  // namespace maple
#endif  // MAPLE_ME_INCLUDE_MECONSTPROP_H
