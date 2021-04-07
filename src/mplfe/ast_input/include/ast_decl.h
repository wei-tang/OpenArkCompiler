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
#ifndef MPLFE_AST_INPUT_INCLUDE_AST_DECL_H
#define MPLFE_AST_INPUT_INCLUDE_AST_DECL_H
#include <string>
#include <list>
#include <vector>
#include "types_def.h"
#include "ast_stmt.h"
#include "feir_var.h"
#include "fe_function.h"

namespace maple {
class ASTDecl {
 public:
  ASTDecl(const std::string &srcFile, const std::string &nameIn, const std::vector<MIRType*> &typeDescIn)
      : srcFileName(srcFile), name(nameIn), typeDesc(typeDescIn) {}
  virtual ~ASTDecl() = default;
  const std::string &GetSrcFileName() const;
  const std::string &GetName() const;
  const std::vector<MIRType*> &GetTypeDesc() const;
  GenericAttrs GetGenericAttrs() const {
    return genAttrs;
  }

 protected:
  const std::string srcFileName;
  std::string name;
  std::vector<MIRType*> typeDesc;
  GenericAttrs genAttrs;
};

class ASTField : public ASTDecl {
 public:
  ASTField(const std::string &srcFile, const std::string &nameIn, const std::vector<MIRType*> &typeDescIn,
           const GenericAttrs &genAttrsIn)
      : ASTDecl(srcFile, nameIn, typeDescIn) {
    genAttrs = genAttrsIn;
  }
  ~ASTField() = default;
};

class ASTFunc : public ASTDecl {
 public:
  ASTFunc(const std::string &srcFile, const std::string &nameIn, const std::vector<MIRType*> &typeDescIn,
          const GenericAttrs &genAttrsIn, const std::vector<std::string> &parmNamesIn)
      : ASTDecl(srcFile, nameIn, typeDescIn), compound(nullptr), parmNames(parmNamesIn) {
    genAttrs = genAttrsIn;
  }
  ~ASTFunc() {
    compound = nullptr;
  }
  void SetCompoundStmt(ASTStmt*);
  const ASTStmt *GetCompoundStmt() const;
  const std::vector<std::string> &GetParmNames() const {
    return parmNames;
  }
  std::vector<std::unique_ptr<FEIRVar>> GenArgVarList() const;
  std::list<UniqueFEIRStmt> EmitASTStmtToFEIR() const;

 private:
  // typeDesc format: [retType, arg0, arg1 ... argN]
  ASTStmt *compound;  // func body
  std::vector<std::string> parmNames;
};

class ASTStruct : public ASTDecl {
 public:
  ASTStruct(const std::string &srcFile, const std::string &nameIn, const std::vector<MIRType*> &typeDescIn,
            const GenericAttrs &genAttrsIn)
      : ASTDecl(srcFile, nameIn, typeDescIn), isUnion(false) {
    genAttrs = genAttrsIn;
  }
  ~ASTStruct() = default;

  std::string GetStructName(bool mapled) const;

  void SetField(ASTField *f) {
    fields.emplace_back(f);
  }

  const std::list<ASTField*> &GetFields() const {
    return fields;
  }

  void SetIsUnion() {
    isUnion = true;
  }

  bool IsUnion() const {
    return isUnion;
  }

 private:
  bool isUnion;
  std::list<ASTField*> fields;
  std::list<ASTFunc*> methods;
};

union VarValue {
  bool b;
  uint8 u8;
  int8 i8;
  uint16 u16;
  int16 i16;
  uint32 u32;
  int32 i32;
  float f32;
  uint64 u64 = 0;
  int64 i64;
  double d;
};

class ASTVar : public ASTDecl {
 public:
  ASTVar(const std::string &srcFile, const std::string &nameIn, const std::vector<MIRType*> &typeDescIn,
         const GenericAttrs &genAttrsIn)
      : ASTDecl(srcFile, nameIn, typeDescIn) {
    genAttrs = genAttrsIn;
  }
  virtual ~ASTVar() = default;
};

class ASTPrimitiveVar : public ASTVar {
 public:
  ASTPrimitiveVar(const std::string &srcFile, const std::string &varName, const std::vector<MIRType*> &typeDescIn,
                  VarValue value, const GenericAttrs &genAttrsIn)
      : ASTVar(srcFile, varName, typeDescIn, genAttrsIn) {
    val = value;
  }
  ~ASTPrimitiveVar() = default;

  VarValue GetVal() const {
    return val;
  }

 private:
  VarValue val;
};
}  // namespace maple
#endif // MPLFE_AST_INPUT_INCLUDE_AST_DECL_H
