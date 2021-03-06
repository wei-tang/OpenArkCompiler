/*
 * Copyright (c) [2019-2020] Huawei Technologies Co.,Ltd.All rights reserved.
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
#ifndef MAPLE_IPA_INCLUDE_MODULE_PHASE_H
#define MAPLE_IPA_INCLUDE_MODULE_PHASE_H
#include "mir_module.h"
#include "phase.h"

namespace maple {
enum ModulePhaseID {
  kMoPhaseDoNothing,
#define MODAPHASE(MODPHASEID, CLASSNAME) MODPHASEID,
#define MODTPHASE(MODPHASEID, CLASSNAME) MODPHASEID,
#include "module_phases.def"
#undef MODAPHASE
#undef MODTPHASE
  kMoPhaseMax
};

class ModulePhase;  // circular dependency exists, no other choice
using ModuleResultMgr = AnalysisResultManager<MIRModule, ModulePhaseID, ModulePhase>;
class ModulePhase : public Phase {
 public:
  explicit ModulePhase(ModulePhaseID id) : Phase(), phaseID(id) {}

  virtual ~ModulePhase() = default;

  virtual AnalysisResult *Run(MIRModule *module, ModuleResultMgr *mrm) = 0;

  ModulePhaseID GetPhaseID() const {
    return phaseID;
  }

  virtual std::string PhaseName() const = 0;

 private:
  ModulePhaseID phaseID;
};
}  // namespace maple
#endif  // MAPLE_IPA_INCLUDE_MODULE_PHASE_H
