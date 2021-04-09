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
#include "ast_struct2fe_helper.h"
#include "fe_manager.h"
#include "fe_utils_ast.h"
#include "ast_util.h"

namespace maple {
// ---------- ASTStruct2FEHelper ----------
bool ASTStruct2FEHelper::ProcessDeclImpl() {
  if (isSkipped) {
    return true;
  }
  if (mirStructType == nullptr) {
    return false;
  }
  // Process Fields
  InitFieldHelpers();
  ProcessFieldDef();
  // Process Methods
  InitMethodHelpers();
  ProcessMethodDef();
  return true;
}

void ASTStruct2FEHelper::InitFieldHelpersImpl() {
  MemPool *mp = allocator.GetMemPool();
  ASSERT(mp != nullptr, "mem pool is nullptr");
  for (const ASTField *field : astStruct.GetFields()) {
    ASSERT(field != nullptr, "field is nullptr");
    ASTStructField2FEHelper *fieldHelper = mp->New<ASTStructField2FEHelper>(allocator, *field);
    fieldHelpers.push_back(fieldHelper);
  }
}

void ASTStruct2FEHelper::InitMethodHelpersImpl() {
}

TypeAttrs ASTStruct2FEHelper::GetStructAttributeFromInputImpl() const {
  GenericAttrs attrs = astStruct.GetGenericAttrs();
  return attrs.ConvertToTypeAttrs();
}

ASTStruct2FEHelper::ASTStruct2FEHelper(MapleAllocator &allocator, const ASTStruct &structIn)
    : FEInputStructHelper(allocator), astStruct(structIn) {
  srcLang = kSrcLangC;
}

std::string ASTStruct2FEHelper::GetStructNameOrinImpl() const {
  return astStruct.GetStructName(false);
}

std::string ASTStruct2FEHelper::GetStructNameMplImpl() const {
  return astStruct.GetStructName(true);
}

std::list<std::string> ASTStruct2FEHelper::GetSuperClassNamesImpl() const {
  CHECK_FATAL(false, "NIY");
  return std::list<std::string> {};
}

std::vector<std::string> ASTStruct2FEHelper::GetInterfaceNamesImpl() const {
  CHECK_FATAL(false, "NIY");
  return std::vector<std::string> {};
}

std::string ASTStruct2FEHelper::GetSourceFileNameImpl() const {
  return astStruct.GetSrcFileName();
}

std::string ASTStruct2FEHelper::GetSrcFileNameImpl() const {
  return astStruct.GetSrcFileName();
}

MIRStructType *ASTStruct2FEHelper::CreateMIRStructTypeImpl(bool &error) const {
  std::string name = GetStructNameOrinImpl();
  if (name.empty()) {
    error = true;
    ERR(kLncErr, "class name is empty");
    return nullptr;
  }
  MIRStructType *type = FEManager::GetTypeManager().GetOrCreateStructType(name);
  error = false;
  if (astStruct.IsUnion()) {
    type->SetMIRTypeKind(kTypeUnion);
  } else {
    type->SetMIRTypeKind(kTypeStruct);
  }
  return type;
}

uint64 ASTStruct2FEHelper::GetRawAccessFlagsImpl() const {
  CHECK_FATAL(false, "NIY");
  return 0;
}

GStrIdx ASTStruct2FEHelper::GetIRSrcFileSigIdxImpl() const {
  // Not implemented, just return a invalid value
  return GStrIdx(0);
}

bool ASTStruct2FEHelper::IsMultiDefImpl() const {
  // Not implemented, alway return false
  return false;
}

// ---------- ASTGlobalVar2FEHelper ---------
bool ASTStructField2FEHelper::ProcessDeclImpl(MapleAllocator &allocator) {
  CHECK_FATAL(false, "should not run here");
  return false;
}

bool ASTStructField2FEHelper::ProcessDeclWithContainerImpl(MapleAllocator &allocator) {
  std::string fieldName = field.GetName();
  GStrIdx idx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(fieldName);
  FieldAttrs attrs = field.GetGenericAttrs().ConvertToFieldAttrs();
  MIRType *fieldType = field.GetTypeDesc().front();
  ASSERT(fieldType != nullptr, "nullptr check for fieldType");
  mirFieldPair.first = idx;
  mirFieldPair.second.first = fieldType->GetTypeIndex();
  mirFieldPair.second.second = attrs;
  return true;
}

// ---------- ASTGlobalVar2FEHelper ---------
bool ASTGlobalVar2FEHelper::ProcessDeclImpl(MapleAllocator &allocator) {
  const std::string varName = astVar.GetName();
  MIRType *type = astVar.GetTypeDesc().front();
  MIRSymbol *mirSymbol = FEManager::GetMIRBuilder().GetOrCreateGlobalDecl(varName, *type);
  if (mirSymbol == nullptr) {
    return false;
  }
  PrimType primType = type->GetPrimType();
  if (primType != PTY_ref && primType != PTY_agg && primType != PTY_ptr) {
    MIRConst *cst = nullptr;
    switch (primType) {
      case PTY_i32:
      case PTY_u32:
      case PTY_i64:
      case PTY_u64:
        cst = allocator.GetMemPool()->New<MIRIntConst>(static_cast<int64>(astVar.GetVal().u64), *type);
        break;
      case PTY_f32:
        cst = allocator.GetMemPool()->New<MIRFloatConst>(astVar.GetVal().f32, *type);
        break;
      case PTY_f64:
        cst = allocator.GetMemPool()->New<MIRDoubleConst>(astVar.GetVal().d, *type);
        break;
      default:
        CHECK_FATAL(false, "unsupported type.");
        break;
    }
    mirSymbol->SetKonst(cst);
  }
  mirSymbol->SetAttrs(astVar.GetGenericAttrs().ConvertToTypeAttrs());
  return true;
}

// ---------- ASTFunc2FEHelper ----------
bool ASTFunc2FEHelper::ProcessDeclImpl(MapleAllocator &allocator) {
  MPLFE_PARALLEL_FORBIDDEN();
  ASSERT(srcLang != kSrcLangUnknown, "src lang not set");
  std::string methodShortName = GetMethodName(false, false);
  CHECK_FATAL(!methodShortName.empty(), "error: method name is empty");
  if (methodShortName.compare("main") == 0) {
    FEManager::GetMIRBuilder().GetMirModule().SetEntryFuncName(methodShortName);
  }
  methodNameIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(methodShortName);
  if (!ASTUtil::InsertFuncSet(methodNameIdx)) {
    return true;
  }
  SolveReturnAndArgTypes(allocator);
  FuncAttrs attrs = GetAttrs();
  bool isStatic = IsStatic();
  bool isVarg = IsVarg();
  CHECK_FATAL(retMIRType != nullptr, "function must have return type");
  std::vector<TyIdx> argsTypeIdx;
  for (auto *type : argMIRTypes) {
    argsTypeIdx.push_back(type->GetTypeIndex());
  }
  mirFunc = FEManager::GetTypeManager().CreateFunction(methodNameIdx, retMIRType->GetTypeIndex(),
                                                       argsTypeIdx, isVarg, isStatic);
#ifndef USE_OPS
  for (uint32 i = 0; i < func.GetParmNames().size(); ++i) {
    MIRSymbol *sym = SymbolBuilder::Instance().GetOrCreateLocalSymbol(
        *argMIRTypes[i], func.GetParmNames()[i], *mirFunc);
    sym->SetStorageClass(kScFormal);
    sym->SetSKind(kStVar);
    mirFunc->AddFormal(sym);
  }
  mirMethodPair.first = mirFunc;
#else
  for (uint32 i = 0; i < func.GetParmNames().size(); ++i) {
    MIRSymbol *sym = FEManager::GetMIRBuilder().GetOrCreateDeclInFunc(
        func.GetParmNames()[i], *argMIRTypes[i], *mirFunc);
    sym->SetStorageClass(kScFormal);
    sym->SetSKind(kStVar);
    mirFunc->AddArgument(sym);
  }
  mirMethodPair.first = mirFunc->GetStIdx();
#endif
  mirMethodPair.second.first = mirFunc->GetMIRFuncType()->GetTypeIndex();
  mirMethodPair.second.second = attrs;
  mirFunc->SetFuncAttrs(attrs);
  return true;
}

const std::string &ASTFunc2FEHelper::GetSrcFileName() const {
  return func.GetSrcFileName();
}

void ASTFunc2FEHelper::SolveReturnAndArgTypesImpl(MapleAllocator &allocator) {
  const std::vector<MIRType*> &returnAndArgTypeNames = func.GetTypeDesc();
  retMIRType = returnAndArgTypeNames[0];
  argMIRTypes.insert(argMIRTypes.begin(), returnAndArgTypeNames.begin() + 1, returnAndArgTypeNames.end());
}

std::string ASTFunc2FEHelper::GetMethodNameImpl(bool inMpl, bool full) const {
  std::string funcName = func.GetName();
  if (!full) {
    return inMpl ? namemangler::EncodeName(funcName) : funcName;
  }
  // fullName not implemented yet
  return funcName;
}

bool ASTFunc2FEHelper::IsVargImpl() const {
  return false;
}

bool ASTFunc2FEHelper::HasThisImpl() const {
  CHECK_FATAL(false, "NIY");
  return false;
}

MIRType *ASTFunc2FEHelper::GetTypeForThisImpl() const {
  CHECK_FATAL(false, "NIY");
  return nullptr;
}

FuncAttrs ASTFunc2FEHelper::GetAttrsImpl() const {
  return func.GetGenericAttrs().ConvertToFuncAttrs();
}

bool ASTFunc2FEHelper::IsStaticImpl() const {
  return false;
}

bool ASTFunc2FEHelper::IsVirtualImpl() const {
  CHECK_FATAL(false, "NIY");
  return false;
}
bool ASTFunc2FEHelper::IsNativeImpl() const {
  CHECK_FATAL(false, "NIY");
  return false;
}

bool ASTFunc2FEHelper::HasCodeImpl() const {
  if (func.GetCompoundStmt() == nullptr) {
    return false;
  }
  return true;
}
}  // namespace maple