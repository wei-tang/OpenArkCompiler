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
#ifndef MPL2MPL_INCLUDE_CLASS_INIT_H
#define MPL2MPL_INCLUDE_CLASS_INIT_H
#include "phase_impl.h"
#include "module_phase.h"
#include "class_hierarchy.h"

namespace maple {
class ClassInit : public FuncOptimizeImpl {
 public:
  ClassInit(MIRModule &mod, KlassHierarchy *kh, bool dump) : FuncOptimizeImpl(mod, kh, dump) {}
  ~ClassInit() = default;

  FuncOptimizeImpl *Clone() override {
    return new ClassInit(*this);
  }

  void ProcessFunc(MIRFunction *func) override;

 private:
  void GenClassInitCheckProfile(MIRFunction &func, const MIRSymbol &classInfo, StmtNode *clinit) const;
  void GenPreClassInitCheck(MIRFunction &func, const MIRSymbol &classInfo, const StmtNode *clinit) const;
  void GenPostClassInitCheck(MIRFunction &func, const MIRSymbol &classInfo, StmtNode *clinit) const;
  MIRSymbol *GetClassInfo(const std::string &classname);
  bool CanRemoveClinitCheck(const std::string &clinitClassname) const;
};

class DoClassInit : public ModulePhase {
 public:
  explicit DoClassInit(ModulePhaseID id) : ModulePhase(id) {}

  ~DoClassInit() = default;

  std::string PhaseName() const override {
    return "clinit";
  }

  AnalysisResult *Run(MIRModule *mod, ModuleResultMgr *mrm) override {
    OPT_TEMPLATE(ClassInit);
    return nullptr;
  }
};
}  // namespace maple
#endif  // MPL2MPL_INCLUDE_CLASS_INIT_H
