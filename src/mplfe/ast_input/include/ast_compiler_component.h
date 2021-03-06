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
#ifndef MPLFE_AST_INPUT_INCLUDE_AST_COMPILER_COMPONENT_H
#define MPLFE_AST_INPUT_INCLUDE_AST_COMPILER_COMPONENT_H
#include "fe_macros.h"
#include "mplfe_compiler_component.h"
#include "ast_input.h"

namespace maple {
class ASTCompilerComponent : public MPLFECompilerComponent {
 public:
  explicit ASTCompilerComponent(MIRModule &module);
  ~ASTCompilerComponent();

 protected:
  bool ParseInputImpl() override;
  bool PreProcessDeclImpl() override;
  void ProcessPragmaImpl() override {}
  std::unique_ptr<FEFunction> CreatFEFunctionImpl(FEInputMethodHelper *methodHelper) override;
  bool ProcessFunctionSerialImpl() override;
  std::string GetComponentNameImpl() const override {
    return "ASTCompilerComponent";
  }
  bool ParallelableImpl() const override {
    return true;
  }
  void DumpPhaseTimeTotalImpl() const override {
    INFO(kLncInfo, "[PhaseTime] ASTCompilerComponent");
    MPLFECompilerComponent::DumpPhaseTimeTotalImpl();
  }
  void ReleaseMemPoolImpl() override {
    FEUtils::DeleteMempoolPtr(mp);
  }

 private:
  MemPool *mp;
  MapleAllocator allocator;
  ASTInput astInput;
};  // class ASTCompilerComponent
}  // namespace maple
#endif  // MPLFE_AST_INPUT_INCLUDE_AST_COMPILER_COMPONENT_H
