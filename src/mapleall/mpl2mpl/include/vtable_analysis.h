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
#ifndef MPL2MPL_INCLUDE_VTABLE_ANALYSIS_H
#define MPL2MPL_INCLUDE_VTABLE_ANALYSIS_H
#include "module_phase.h"
#include "phase_impl.h"

namespace maple {
#ifdef USE_32BIT_REF
constexpr unsigned int kTabEntrySize = 4;
constexpr unsigned int kShiftCountBit = 4 * 4;  // Get the low 16bit
#else   // !USE_32BIT_REF
constexpr unsigned int kTabEntrySize = 8;
constexpr unsigned int kShiftCountBit = 8 * 4;  // Get the low 32bit
#endif  // USE_32BIT_REF

class VtableAnalysis : public FuncOptimizeImpl {
 public:
  VtableAnalysis(MIRModule &mod, KlassHierarchy *kh, bool dump);
  ~VtableAnalysis() = default;
  static std::string DecodeBaseNameWithType(const MIRFunction &func);
  static bool IsVtableCandidate(const MIRFunction &func);
  void ProcessFunc(MIRFunction *func) override;
  FuncOptimizeImpl *Clone() override {
    return new VtableAnalysis(*this);
  }

 private:
  bool CheckInterfaceSpecification(const Klass &baseKlass, const Klass &currKlass) const;
  bool CheckOverrideForCrossPackage(const MIRFunction &baseMethod, const MIRFunction &currMethod) const;
  void AddMethodToTable(MethodPtrVector &methodTable, MethodPair &methodPair);
  void GenVtableList(const Klass &klass);
  void DumpVtableList(const Klass &klass) const;
  void GenTableSymbol(const std::string &prefix, const std::string klassName, MIRAggConst &newConst) const;
  void GenVtableDefinition(const Klass &klass);
  void GenItableDefinition(const Klass &klass);
  void AddNullPointExceptionCheck(MIRFunction &func, StmtNode &stmt) const;
  BaseNode *GenVtabItabBaseAddr(BaseNode &obj, bool isVirtual);
  size_t SearchWithoutRettype(const MIRFunction &callee, const MIRStructType &structType) const;
  bool CheckInterfaceImplemented(const CallNode &stmt) const;
  void ReplaceVirtualInvoke(CallNode &stmt);
  void ReplaceInterfaceInvoke(CallNode &stmt);
  void ReplaceSuperclassInvoke(CallNode &stmt);
  void ReplacePolymorphicInvoke(CallNode &stmt);

  std::unordered_map<PUIdx, int> puidxToVtabIndex;
  MIRType *voidPtrType;
  MIRIntConst *zeroConst;
  MIRIntConst *oneConst;
};

class DoVtableAnalysis : public ModulePhase {
 public:
  explicit DoVtableAnalysis(ModulePhaseID id) : ModulePhase(id) {}

  ~DoVtableAnalysis() = default;

  std::string PhaseName() const override {
    return "vtableanalysis";
  }

  AnalysisResult *Run(MIRModule *mod, ModuleResultMgr *mrm) override {
    OPT_TEMPLATE(VtableAnalysis);
    return nullptr;
  }
};
}  // namespace maple
#endif  // MPL2MPL_INCLUDE_VTABLE_ANALYSIS_H
