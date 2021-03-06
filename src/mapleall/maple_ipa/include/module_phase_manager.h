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
#ifndef MAPLE_IPA_INCLUDE_MODULE_PHASE_MANAGER_H
#define MAPLE_IPA_INCLUDE_MODULE_PHASE_MANAGER_H
#include "me_phase_manager.h"

namespace maple {
class ModulePhaseManager : public PhaseManager {
 public:
  ModulePhaseManager(MemPool *memPool, MIRModule &mod, ModuleResultMgr *mrm = nullptr)
      : PhaseManager(*memPool, "modulephase"), mirModule(mod) {
    if (mrm != nullptr) {
      arModuleMgr = mrm;
    } else {
      arModuleMgr = memPool->New<ModuleResultMgr>(GetMemAllocator());
    }
  }

  ~ModulePhaseManager() = default;

  // Register all module phases defined in module_phases.def
  void RegisterModulePhases();
  // Add module phases which are going to be run
  void AddModulePhases(const std::vector<std::string> &phases);
  ModuleResultMgr *GetModResultMgr() override {
    return arModuleMgr;
  }

  void SetTimePhases(bool val) {
    timePhases = val;
  }

  void Run() override;
  void Emit(const std::string &passName);

 private:
  bool timePhases = false;
  MIRModule &mirModule;
  ModuleResultMgr *arModuleMgr; // module level analysis result
};
}  // namespace maple
#endif  // MAPLE_IPA_INCLUDE_MODULE_PHASE_MANAGER_H
