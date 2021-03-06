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
#include "irmap.h"
#include <queue>
#include "ssa.h"
#include "mir_builder.h"
#include "constantfold.h"

namespace maple {
// For controlling cast elimination
static constexpr bool simplifyCastExpr = true;
static constexpr bool simplifyCastAssign = true;
static constexpr bool printSimplifyCastLog = false;
static constexpr auto kNumCastKinds = static_cast<uint32>(CAST_unknown);

static const char *GetCastKindName(CastKind castKind) {
  static const char* castKindNameArr[] = { "intTrunc", "zext", "sext", "int2fp", "fp2int",
                                           "fpTrunc", "fpExt", "retype", "unknown" };
  auto index = static_cast<uint32>(castKind);
  CHECK_FATAL(index <= kNumCastKinds, "invalid castKind value");
  return castKindNameArr[index];
}

struct CastInfo {
  CastKind kind = CAST_unknown;
  PrimType srcType = PTY_begin;
  PrimType dstType = PTY_end;
  void Dump() const {
    LogInfo::MapleLogger() << GetCastKindName(kind) << " " <<
        GetPrimTypeName(dstType) << " " << GetPrimTypeName(srcType);
  }
};

static uint32 GetPrimTypeActualBitSize(PrimType primType) {
  // GetPrimTypeSize(PTY_u1) will return 1, so we take it as a special case
  if (primType == PTY_u1) {
    return 1;
  }
  // 1 byte = 8 bits = 2^3 bits
  return GetPrimTypeSize(primType) << 3;
}

static bool IsCompareMeExpr(const MeExpr &expr) {
  Opcode op = expr.GetOp();
  if (op == OP_eq || op == OP_ne || op == OP_ge || op == OP_gt || op == OP_le || op == OP_lt) {
    return true;
  }
  return false;
}

// This interface is conservative, which means that some op are explicit type cast but
// the interface returns false.
static bool IsCastMeExprExplicit(const MeExpr &expr) {
  Opcode op = expr.GetOp();
  // Maybe we can add more explicit cast op later such as `trunc`, `extractbits` etc.
  if (op == OP_cvt || op == OP_retype) {
    return true;
  }
  if (op == OP_zext || op == OP_sext ) {
    // Not all zext/sext is type cast
    auto bitSize = static_cast<const OpMeExpr&>(expr).GetBitsSize();
    if (bitSize == 1 || bitSize == 8 || bitSize == 16 || bitSize == 32 || bitSize == 64) {
      return true;
    }
    return false; // This is not type cast but bit operation
  }
  return false;
}

// This interface is conservative, which means that some op are implicit type cast but
// the interface returns false.
static bool IsCastMeExprImplicit(const MeExpr &expr) {
  Opcode op = expr.GetOp();
  // MEIR varMeExpr has no implicit cast, so dread is not included
  if (op == OP_regread) {
    PrimType dstType = expr.GetPrimType();
    if (!IsPrimitiveInteger(dstType)) {
      return false;
    }
    const auto &scalarExpr = static_cast<const ScalarMeExpr&>(expr);
    PrimType srcType = scalarExpr.GetOst()->GetType()->GetPrimType();
    // Only consider regread with implicit integer extension
    if (GetPrimTypeActualBitSize(srcType) < GetPrimTypeActualBitSize(dstType)) {
      // To delete: never be here?
      CHECK_FATAL(false, "never be here?");
      return true;
    }
  } else if (op == OP_iread) {
    PrimType dstType = expr.GetPrimType();
    if (!IsPrimitiveInteger(dstType)) {
      return false;
    }
    const auto &ivarExpr = static_cast<const IvarMeExpr&>(expr);
    PrimType srcType = ivarExpr.GetType()->GetPrimType();
    // Only consider iread with implicit integer extension
    if (GetPrimTypeActualBitSize(srcType) < GetPrimTypeActualBitSize(dstType)) {
      return true;
    }
  }
  return false;
}

static CastKind GetCastKindByTwoType(PrimType fromType, PrimType toType) {
  // This is a workaround, we don't optimize `cvt u1 xx <expr>` because it will be converted to
  // `ne u1 xx (<expr>, constval xx 0)`. There is no `cvt u1 xx <expr>` in the future.
  if (toType == PTY_u1 && fromType != PTY_u1) {
    return CAST_unknown;
  }
  const uint32 fromTypeBitSize = GetPrimTypeActualBitSize(fromType);
  const uint32 toTypeBitSize = GetPrimTypeActualBitSize(toType);
  // Both integer, ptr/ref/a64/u64... are also integer
  if (IsPrimitiveInteger(fromType) && IsPrimitiveInteger(toType)) {
    if (toTypeBitSize == fromTypeBitSize) {
      return CAST_retype;
    } else if (toTypeBitSize < fromTypeBitSize) {
      return CAST_intTrunc;
    } else {
      return IsSignedInteger(fromType) ? CAST_sext : CAST_zext;
    }
  }
  // Both fp
  if (IsPrimitiveFloat(fromType) && IsPrimitiveFloat(toType)) {
    if (toTypeBitSize == fromTypeBitSize) {
      return CAST_retype;
    } else if (toTypeBitSize < fromTypeBitSize) {
      return CAST_fpTrunc;
    } else {
      return CAST_fpExt;
    }
  }
  // int2fp
  if (IsPrimitiveInteger(fromType) && IsPrimitiveFloat(toType)) {
    return CAST_int2fp;
  }
  // fp2int
  if (IsPrimitiveFloat(fromType) && IsPrimitiveInteger(toType)) {
    return CAST_fp2int;
  }
  return CAST_unknown;
}

static PrimType GetIntegerPrimTypeBySizeAndSign(size_t sizeBit, bool isSign) {
  switch (sizeBit) {
    case 1: {
      CHECK_FATAL(!isSign, "no i1 type");
      return PTY_u1;
    }
    case 8: {
      return isSign ? PTY_i8 : PTY_u8;
    }
    case 16: {
      return isSign ? PTY_i16 : PTY_u16;
    }
    case 32: {
      return isSign ? PTY_i32 : PTY_u32;
    }
    case 64: {
      return isSign ? PTY_i64 : PTY_u64;
    }
    default: {
      LogInfo::MapleLogger() << sizeBit << ", isSign = " << isSign << std::endl;
      CHECK_FATAL(false, "not supported");
    }
  }
}

// If the computed castInfo.castKind is CAST_unknown, the computed castInfo is invalid
static void ComputeCastInfoForExpr(const MeExpr &expr, CastInfo &castInfo) {
  Opcode op = expr.GetOp();
  PrimType dstType = expr.GetPrimType();
  PrimType srcType = PTY_begin;
  CastKind castKind = CAST_unknown;
  switch (op) {
    case OP_zext:
    case OP_sext: {
      size_t sizeBit = static_cast<const OpMeExpr&>(expr).GetBitsSize();
      // The code can be improved
      // exclude: sext ixx 1 <expr> because there is no i1 type
      if (sizeBit == 1 && op == OP_sext) {
        break;
      }
      if (sizeBit == 1 || sizeBit == 8 || sizeBit == 16 || sizeBit == 32 || sizeBit == 64) {
        srcType = GetIntegerPrimTypeBySizeAndSign(sizeBit, op == OP_sext);
        castKind = (op == OP_sext ? CAST_sext : CAST_zext);
      }
      break;
    }
    case OP_retype: {
      srcType = static_cast<const OpMeExpr&>(expr).GetOpndType();
      castKind = CAST_retype;
      break;
    }
    case OP_cvt: {
      srcType = static_cast<const OpMeExpr&>(expr).GetOpndType();
      if (srcType == PTY_u1 && dstType != PTY_u1) {
        srcType = PTY_u8;  // From the codegen view, `cvt xx u1` is always same as `cvt xx u8`
      }
      castKind = GetCastKindByTwoType(srcType, dstType);
      break;
    }
    case OP_regread: {
      const auto &scalarExpr = static_cast<const ScalarMeExpr&>(expr);
      srcType = scalarExpr.GetOst()->GetType()->GetPrimType();
      // Only consider dread/regread with implicit integer extension
      if (IsPrimitiveInteger(srcType) && GetPrimTypeActualBitSize(srcType) < GetPrimTypeActualBitSize(dstType)) {
        castKind = (IsSignedInteger(srcType) ? CAST_sext : CAST_zext);
      }
      break;
    }
    case OP_iread: {
      const auto &ivarExpr = static_cast<const IvarMeExpr&>(expr);
      srcType = ivarExpr.GetType()->GetPrimType();
      // Only consider iread with implicit integer extension
      if (IsPrimitiveInteger(srcType) && GetPrimTypeActualBitSize(srcType) < GetPrimTypeActualBitSize(dstType)) {
        castKind = (IsSignedInteger(srcType) ? CAST_sext : CAST_zext);
      }
      break;
    }
    default:
      break;
  }
  castInfo.kind = castKind;
  castInfo.srcType = srcType;
  castInfo.dstType = dstType;
}

static const uint8 castMatrix[kNumCastKinds][kNumCastKinds] = {
    // i        i  f  f     r  -+
    // t        n  p  t  f  e   |
    // r  z  s  t  2  r  p  t   +- secondCastKind
    // u  e  e  2  i  u  e  y   |
    // n  x  x  f  n  n  x  p   |
    // c  t  t  p  t  c  t  e  -+
    {  1, 0, 0, 0,99,99,99, 3 },  // intTrunc   -+
    {  8, 9, 9,10,99,99,99, 3 },  // zext        |
    {  8, 0, 9, 0,99,99,99, 3 },  // sext        |
    { 99,99,99,99, 0, 0, 0, 4 },  // int2fp      |
    {  0, 0, 0, 0,99,99,99, 3 },  // fp2int      +- firstCastKind
    { 99,99,99,99, 0, 0, 0, 4 },  // fpTrunc     |
    { 99,99,99,99, 2, 8, 2, 4 },  // fpExt       |
    {  5, 7, 7, 5, 6, 6, 6, 1 },  // retype     -+
};

// This function determines whether to eliminate a cast pair according to castMatrix
// Input is a cast pair like this:
//   secondCastKind dstType midType2 (firstCastKind midType1 srcType)
// If the function returns a valid resultCastKind, the cast pair can be optimized to:
//   resultCastKind dstType srcType
// If the cast pair can NOT be eliminated, -1 will be returned.
// ATTENTION: This function may modify srcType
static int IsEliminableCastPair(CastKind firstCastKind, CastKind secondCastKind,
                         PrimType dstType, PrimType midType2, PrimType midType1, PrimType &srcType) {
  int castCase = castMatrix[firstCastKind][secondCastKind];
  uint32 srcSize = GetPrimTypeActualBitSize(srcType);
  uint32 midSize1 = GetPrimTypeActualBitSize(midType1);
  uint32 midSize2 = GetPrimTypeActualBitSize(midType2);
  uint32 dstSize = GetPrimTypeActualBitSize(dstType);

  switch (castCase) {
    case 0: {
      // Not allowed
      return -1;
    }
    case 1: {
      // first intTrunc, then intTrunc
      // Example: cvt u16 u32 (cvt u32 u64)  ==>  cvt u16 u64
      // first retype, then retype
      // Example: retype i64 u64 (retype u64 ptr)  ==>  retype i64 ptr
      return firstCastKind;
    }
    case 2: {
      // first fpExt, then fpExt
      // Example: cvt f128 f64 (cvt f64 f32)  ==>  cvt f128 f32
      // first fpExt, then fp2int
      // Example: cvt i64 f64 (cvt f64 f32)  ==>  cvt i64 f32
      return secondCastKind;
    }
    case 3: {
      if (IsPrimitiveInteger(dstType)) {
        return firstCastKind;
      }
      return -1;
    }
    case 4: {
      if (IsPrimitiveFloat(dstType)) {
        return firstCastKind;
      }
      return -1;
    }
    case 5: {
      if (IsPrimitiveInteger(srcType)) {
        return secondCastKind;
      }
      return -1;
    }
    case 6: {
      if (IsPrimitiveFloat(srcType)) {
        return secondCastKind;
      }
      return -1;
    }
    case 7: {
      // first integer retype, then sext/zext
      if (IsPrimitiveInteger(srcType) && dstSize >= midSize1) {
        CHECK_FATAL(srcSize == midSize1, "must be");
        if (midSize2 >= srcSize) {
          return secondCastKind;
        }
        // Example: zext u64 8 (retype u32 i32)  ==>  zext u64 8
        srcType = midType2;
        return secondCastKind;
      }
      return -1;
    }
    case 8: {
      if (srcSize == dstSize) {
        return CAST_retype;
      } else if (srcSize < dstSize) {
        return firstCastKind;
      } else {
        return secondCastKind;
      }
    }
    // For integer extension pair
    case 9: {
      // first zext, then sext
      // Extreme example: sext i32 16 (zext u64 8)  ==> zext i32 8
      if (firstCastKind != secondCastKind && midSize2 <= midSize1) {
        if (midSize2 > srcSize) {
          // The first extension works. After the first zext, the most significant bit must be 0, so the second sext
          // is actually a zext.
          // Example: sext i64 16 (zext u32 8)  ==> zext i64 8
          return firstCastKind;
        }
        // midSize2 <= srcSize
        // The first extension didn't work
        // Example: sext i64 8 (zext u32 16)  ==> sext i64 8
        // Example: sext i16 8 (zext u32 16)  ==> sext i16 8
        srcType = midType2;
        return secondCastKind;
      }

      // first zext, then zext
      // first sext, then sext
      // Example: sext i32 8 (sext i32 8)  ==>  sext i32 8
      // Example: zext u16 1 (zext u32 8)  ==>  zext u16 1    it's ok
      // midSize2 < srcSize:
      // Example: zext u64 8 (zext u32 16)  ==> zext u64 8
      // Example: sext i64 8 (sext i32 16)  ==> sext i64 8
      // Example: zext i32 1 (zext u32 8)  ==> zext i32 1
      // Wrong example (midSize2 > midSize1): zext u64 32 (zext u16 8)  =[x]=>  zext u64 8
      if (firstCastKind == secondCastKind && midSize2 <= midSize1) {
        if (midSize2 < srcSize) {
          srcType = midType2;
        }
        return secondCastKind;
      }
      return -1;
    }
    case 10: {
      // first zext, then int2fp
      if (IsSignedInteger(midType2)) {
        return secondCastKind;
      }
      // To improved: consider unsigned
      return -1;
    }
    case 99: {
      CHECK_FATAL(false, "invalid cast pair");
    }
    default: {
      CHECK_FATAL(false, "can not be here, is castMatrix wrong?");
    }
  }
}

MeExpr *IRMap::CreateMeExprByCastKind(CastKind castKind, PrimType fromType, PrimType toType, MeExpr *opnd) {
  if (castKind == CAST_zext) {
    return CreateMeExprExt(OP_zext, toType, GetPrimTypeActualBitSize(fromType), *opnd);
  } else if (castKind == CAST_sext) {
    return CreateMeExprExt(OP_sext, toType, GetPrimTypeActualBitSize(fromType), *opnd);
  } else if (castKind == CAST_retype) {
    // Maybe we can create more concrete expr
    return CreateMeExprTypeCvt(toType, fromType, *opnd);
  } else {
    return CreateMeExprTypeCvt(toType, fromType, *opnd);
  }
}

// The input castExpr must be a explicit cast expr
MeExpr *IRMap::SimplifyCastSingle(MeExpr *castExpr) {
  MeExpr *opnd = castExpr->GetOpnd(0);
  if (opnd == nullptr) {
    return nullptr;
  }
  CastInfo castInfo;
  ComputeCastInfoForExpr(*castExpr, castInfo);
  // cast to integer + compare  ==>  compare
  if (castInfo.kind != CAST_unknown && IsPrimitiveInteger(castInfo.dstType) && IsCompareMeExpr(*opnd)) {
    // exclude the following castExpr:
    //   sext xx 1 <expr>
    bool excluded = (castExpr->GetOp() == OP_sext && static_cast<OpMeExpr*>(castExpr)->GetBitsSize() == 1);
    if (!excluded) {
      opnd->SetPtyp(castExpr->GetPrimType());
      return HashMeExpr(*opnd);
    }
  }
  if (opnd->GetOp() == OP_dread && (castInfo.kind == CAST_zext || castInfo.kind == CAST_sext)) {
    auto *varExpr = static_cast<VarMeExpr*>(opnd);
    if (varExpr->GetPrimType() == PTY_u1) {
      // zext/sext + dread u1 %var ==>  dread u1 %var
      return opnd;
    }
    if (varExpr->GetDefBy() == kDefByStmt) {
      MeStmt *defStmt = varExpr->GetDefByMeStmt();
      if (defStmt->GetOp() == OP_dassign && IsCompareMeExpr(*static_cast<DassignMeStmt*>(defStmt)->GetRHS())) {
        // zext/sext + dread non-u1 %var (%var is defined by compare op)  ==>  dread non-u1 %var
        return opnd;
      }
    }
  }
  if (castInfo.dstType == opnd->GetPrimType() &&
      GetPrimTypeActualBitSize(castInfo.srcType) >= GetPrimTypeActualBitSize(opnd->GetPrimType())) {
    return opnd;
  }
  return nullptr;
}

// The firstCastExpr may be a implicit cast expr
// The secondCastExpr must be a explicit cast expr
MeExpr *IRMap::SimplifyCastPair(MeExpr *firstCastExpr, MeExpr *secondCastExpr, bool isFirstCastImplicit) {
  CastInfo firstCastInfo;
  CastInfo secondCastInfo;
  ComputeCastInfoForExpr(*firstCastExpr, firstCastInfo);
  if (firstCastInfo.kind == CAST_unknown) {
    // We can NOT eliminate the first cast, try to simplify the second cast individually
    return SimplifyCastSingle(secondCastExpr);
  }
  ComputeCastInfoForExpr(*secondCastExpr, secondCastInfo);
  if (secondCastInfo.kind == CAST_unknown) {
    return nullptr;
  }
  PrimType srcType = firstCastInfo.srcType;
  PrimType origSrcType = srcType;
  PrimType midType1 = firstCastInfo.dstType;
  PrimType midType2 = secondCastInfo.srcType;
  PrimType dstType = secondCastInfo.dstType;
  uint32 result = IsEliminableCastPair(firstCastInfo.kind, secondCastInfo.kind, dstType, midType2, midType1, srcType);
  if (result == -1) {
    return SimplifyCastSingle(secondCastExpr);
  }
  auto resultCastKind = CastKind(result);
  MeExpr *toCastExpr = firstCastExpr->GetOpnd(0);

  // To improved: do more powerful optimization for firstCastImplicit
  if (isFirstCastImplicit) {
    // Wrong example: zext u32 u8 (iread u32 <* u16>)  =[x]=>  iread u32 <* u16>
    // srcType may be modified, we should use origSrcType
    if (resultCastKind != CAST_unknown && dstType == midType1 &&
        GetPrimTypeActualBitSize(midType2) >= GetPrimTypeActualBitSize(origSrcType)) {
      return firstCastExpr;
    } else {
      return nullptr;
    }
  }

  if (resultCastKind == CAST_retype && srcType == dstType) {
    return toCastExpr;
  }

  if (printSimplifyCastLog) {
    CastInfo resInfo = { resultCastKind, srcType, dstType };
    LogInfo::MapleLogger() << "[input ] ";
    secondCastInfo.Dump();
    LogInfo::MapleLogger() << " (";
    firstCastInfo.Dump();
    LogInfo::MapleLogger() << " [ caseId = " <<
        (int)castMatrix[firstCastInfo.kind][secondCastInfo.kind] << " ]" <<  std::endl;
    LogInfo::MapleLogger() << "[output] ";
    resInfo.Dump();
    LogInfo::MapleLogger() << std::endl;
  }

  return CreateMeExprByCastKind(resultCastKind, srcType, dstType, toCastExpr);
}

// Return a simplified expr if succeed, return nullptr if fail
MeExpr *IRMap::SimplifyCast(MeExpr *expr) {
  if (!IsCastMeExprExplicit(*expr)) {
    return nullptr;
  }
  MeExpr *opnd = expr->GetOpnd(0);
  if (opnd == nullptr) {
    return nullptr;
  }
  // Convert `cvt u1 xx <expr>` to `ne u1 xx (<expr>, constval xx 0)`
  if (expr->GetOp() == OP_cvt && expr->GetPrimType() == PTY_u1) {
    auto *opExpr = static_cast<OpMeExpr*>(expr);
    PrimType fromType = opExpr->GetOpndType();
    if (fromType != PTY_u1) {  // No need to convert `cvt u1 u1 <expr>`
      return CreateMeExprCompare(OP_ne, PTY_u1, fromType, *opnd, *CreateIntConstMeExpr(0, fromType));
    }
  }
  // If the opnd is a iread/regread, it's OK because it may be a implicit zext or sext
  bool isFirstCastImplicit = IsCastMeExprImplicit(*opnd);
  if (!IsCastMeExprExplicit(*opnd) && !isFirstCastImplicit) {
    // only 1 cast
    // Exmaple: cvt i32 i64 (add i32)  ==>  add i32
    return SimplifyCastSingle(expr);
  }
  MeExpr *simplified1 = SimplifyCastPair(opnd, expr, isFirstCastImplicit);
  MeExpr *simplified2 = nullptr;
  if (simplified1 != nullptr && IsCastMeExprExplicit(*simplified1)) {
    // Simplify cast further
    simplified2 = SimplifyCastSingle(simplified1);
  }
  return (simplified2 != nullptr ? simplified2 : simplified1);
}

// Try remove redundant intTrunc for dassgin and iassign
void IRMap::SimplifyCastForAssign(MeStmt *assignStmt) {
  if (!simplifyCastAssign) {
    return;
  }
  Opcode stmtOp = assignStmt->GetOp();
  PrimType expectedType = PTY_begin;
  MeExpr *rhsExpr = nullptr;
  if (stmtOp == OP_dassign) {
    auto *dassign = static_cast<DassignMeStmt*>(assignStmt);
    expectedType = dassign->GetLHS()->GetPrimType();
    rhsExpr = dassign->GetRHS();
  } else if (stmtOp == OP_iassign) {
    auto *iassign = static_cast<IassignMeStmt*>(assignStmt);
    expectedType = iassign->GetLHSVal()->GetType()->GetPrimType();
    rhsExpr = iassign->GetRHS();
  } else if (stmtOp == OP_regassign) {
    auto *regassign = static_cast<AssignMeStmt*>(assignStmt);
    expectedType = assignStmt->GetLHS()->GetPrimType();
    rhsExpr = regassign->GetRHS();
  }
  if (rhsExpr == nullptr || !IsPrimitiveInteger(expectedType) || !IsCastMeExprExplicit(*rhsExpr)) {
    return;
  }
  MeExpr *cur = rhsExpr;
  CastInfo castInfo;
  do {
    ComputeCastInfoForExpr(*cur, castInfo);
    // Only consider intTrunc
    if (castInfo.kind != CAST_intTrunc) {
      break;
    }
    cur = cur->GetOpnd(0);
  } while (true);
  if (cur != rhsExpr) {
    if (stmtOp == OP_dassign) {
      auto *dassign = static_cast<DassignMeStmt*>(assignStmt);
      dassign->SetRHS(cur);
    } else if (stmtOp == OP_iassign) {
      auto *iassign = static_cast<IassignMeStmt*>(assignStmt);
      iassign->SetRHS(cur);
    }
  }
}

VarMeExpr *IRMap::CreateVarMeExprVersion(OriginalSt *ost) {
  VarMeExpr *varMeExpr = New<VarMeExpr>(exprID++, ost, verst2MeExprTable.size(),
      GlobalTables::GetTypeTable().GetTypeFromTyIdx(ost->GetTyIdx())->GetPrimType());
  ost->PushbackVersionsIndices(verst2MeExprTable.size());
  verst2MeExprTable.push_back(varMeExpr);
  return varMeExpr;
}

MeExpr *IRMap::CreateAddrofMeExpr(MeExpr &expr) {
  if (expr.GetMeOp() == kMeOpVar) {
    auto &varMeExpr = static_cast<VarMeExpr&>(expr);
    AddrofMeExpr addrofme(-1, PTY_ptr, varMeExpr.GetOst()->GetIndex());
    return HashMeExpr(addrofme);
  } else {
    ASSERT(expr.GetMeOp() == kMeOpIvar, "expecting IVarMeExpr");
    auto &ivarExpr = static_cast<IvarMeExpr&>(expr);
    OpMeExpr opMeExpr(kInvalidExprID, OP_iaddrof, PTY_ptr, 1);
    opMeExpr.SetFieldID(ivarExpr.GetFieldID());
    opMeExpr.SetTyIdx(ivarExpr.GetTyIdx());
    opMeExpr.SetOpnd(0, ivarExpr.GetBase());
    return HashMeExpr(opMeExpr);
  }
}

MeExpr *IRMap::CreateAddroffuncMeExpr(PUIdx puIdx) {
  AddroffuncMeExpr addroffuncMeExpr(-1, puIdx);
  addroffuncMeExpr.SetOp(OP_addroffunc);
  addroffuncMeExpr.SetPtyp(PTY_ptr);
  addroffuncMeExpr.SetNumOpnds(0);
  return HashMeExpr(addroffuncMeExpr);
}

MeExpr *IRMap::CreateIaddrofMeExpr(FieldID fieldId, TyIdx tyIdx, MeExpr *base) {
  OpMeExpr opMeExpr(kInvalidExprID, OP_iaddrof, PTY_ptr, 1);
  opMeExpr.SetFieldID(fieldId);
  opMeExpr.SetTyIdx(tyIdx);
  opMeExpr.SetOpnd(0, base);
  return HashMeExpr(opMeExpr);
}

MeExpr *IRMap::CreateIvarMeExpr(MeExpr &expr, TyIdx tyIdx, MeExpr &base) {
  ASSERT(expr.GetMeOp() == kMeOpVar, "expecting IVarMeExpr");
  auto &varMeExpr = static_cast<VarMeExpr&>(expr);
  IvarMeExpr ivarMeExpr(-1, varMeExpr.GetPrimType(), tyIdx, varMeExpr.GetOst()->GetFieldID());
  ivarMeExpr.SetBase(&base);
  ivarMeExpr.SetMuVal(&varMeExpr);
  return HashMeExpr(ivarMeExpr);
}

NaryMeExpr *IRMap::CreateNaryMeExpr(const NaryMeExpr &nMeExpr) {
  NaryMeExpr tmpNaryMeExpr(&(irMapAlloc), kInvalidExprID, nMeExpr);
  tmpNaryMeExpr.SetBoundCheck(false);
  MeExpr *newNaryMeExpr = HashMeExpr(tmpNaryMeExpr);
  return static_cast<NaryMeExpr*>(newNaryMeExpr);
}

VarMeExpr *IRMap::CreateNewVar(GStrIdx strIdx, PrimType pType, bool isGlobal) {
  MIRSymbol *st =
      mirModule.GetMIRBuilder()->CreateSymbol((TyIdx)pType, strIdx, kStVar,
                                              isGlobal ? kScGlobal : kScAuto,
                                              isGlobal ? nullptr : mirModule.CurFunction(),
                                              isGlobal ? kScopeGlobal : kScopeLocal);
  if (isGlobal) {
    st->SetIsTmp(true);
  }
  OriginalSt *oSt = ssaTab.CreateSymbolOriginalSt(*st, isGlobal ? 0 : mirModule.CurFunction()->GetPuidx(), 0);
  oSt->SetZeroVersionIndex(verst2MeExprTable.size());
  verst2MeExprTable.push_back(nullptr);
  oSt->PushbackVersionsIndices(oSt->GetZeroVersionIndex());

  VarMeExpr *varx = New<VarMeExpr>(exprID++, oSt, verst2MeExprTable.size(), pType);
  verst2MeExprTable.push_back(varx);
  return varx;
}

VarMeExpr *IRMap::CreateNewLocalRefVarTmp(GStrIdx strIdx, TyIdx tIdx) {
  MIRSymbol *st =
      mirModule.GetMIRBuilder()->CreateSymbol(tIdx, strIdx, kStVar, kScAuto, mirModule.CurFunction(), kScopeLocal);
  st->SetInstrumented();
  OriginalSt *oSt = ssaTab.CreateSymbolOriginalSt(*st, mirModule.CurFunction()->GetPuidx(), 0);
  oSt->SetZeroVersionIndex(verst2MeExprTable.size());
  verst2MeExprTable.push_back(nullptr);
  oSt->PushbackVersionsIndices(oSt->GetZeroVersionIndex());
  auto *newLocalRefVar = New<VarMeExpr>(exprID++, oSt, verst2MeExprTable.size(), PTY_ref);
  verst2MeExprTable.push_back(newLocalRefVar);
  return newLocalRefVar;
}

RegMeExpr *IRMap::CreateRegMeExprVersion(OriginalSt &pregOSt) {
  RegMeExpr *regReadExpr =
      New<RegMeExpr>(exprID++, &pregOSt, verst2MeExprTable.size(), kMeOpReg,
                     OP_regread, pregOSt.GetMIRPreg()->GetPrimType());
  pregOSt.PushbackVersionsIndices(verst2MeExprTable.size());
  verst2MeExprTable.push_back(regReadExpr);
  return regReadExpr;
}

RegMeExpr *IRMap::CreateRegMeExpr(PrimType pType) {
  MIRFunction *mirFunc = mirModule.CurFunction();
  PregIdx regIdx = mirFunc->GetPregTab()->CreatePreg(pType);
  ASSERT(regIdx <= 0xffff, "register oversized");
  OriginalSt *ost = ssaTab.GetOriginalStTable().CreatePregOriginalSt(regIdx, mirFunc->GetPuidx());
  return CreateRegMeExprVersion(*ost);
}

RegMeExpr *IRMap::CreateRegMeExpr(MIRType &mirType) {
  if (mirType.GetPrimType() != PTY_ref && mirType.GetPrimType() != PTY_ptr) {
    return CreateRegMeExpr(mirType.GetPrimType());
  }
  if (mirType.GetPrimType() == PTY_ptr) {
    return CreateRegMeExpr(mirType.GetPrimType());
  }
  MIRFunction *mirFunc = mirModule.CurFunction();
  PregIdx regIdx = mirFunc->GetPregTab()->CreatePreg(PTY_ref, &mirType);
  ASSERT(regIdx <= 0xffff, "register oversized");
  OriginalSt *ost = ssaTab.GetOriginalStTable().CreatePregOriginalSt(regIdx, mirFunc->GetPuidx());
  return CreateRegMeExprVersion(*ost);
}

RegMeExpr *IRMap::CreateRegRefMeExpr(const MeExpr &meExpr) {
  MIRType *mirType = nullptr;
  switch (meExpr.GetMeOp()) {
    case kMeOpVar: {
      auto &varMeExpr = static_cast<const VarMeExpr&>(meExpr);
      const OriginalSt *ost = varMeExpr.GetOst();
      ASSERT(ost->GetTyIdx() != 0u, "expect ost->tyIdx to be initialized");
      mirType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(ost->GetTyIdx());
      break;
    }
    case kMeOpIvar: {
      auto &ivarMeExpr = static_cast<const IvarMeExpr&>(meExpr);
      MIRType *ptrMirType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(ivarMeExpr.GetTyIdx());
      ASSERT(ptrMirType->GetKind() == kTypePointer, "must be point type for ivar");
      auto *realMirType = static_cast<MIRPtrType*>(ptrMirType);
      FieldID fieldID = ivarMeExpr.GetFieldID();
      if (fieldID > 0) {
        mirType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(realMirType->GetPointedTyIdxWithFieldID(fieldID));
      } else {
        mirType = realMirType->GetPointedType();
      }
      ASSERT(mirType->GetPrimType() == meExpr.GetPrimType() ||
             !(IsAddress(mirType->GetPrimType()) && IsAddress(meExpr.GetPrimType())),
             "inconsistent type");
      ASSERT(mirType->GetPrimType() == PTY_ref, "CreateRegRefMeExpr: only ref type expected");
      break;
    }
    case kMeOpOp:
      if (meExpr.GetOp() == OP_retype) {
        auto &opMeExpr = static_cast<const OpMeExpr&>(meExpr);
        ASSERT(opMeExpr.GetTyIdx() != 0u, "expect opMeExpr.tyIdx to be initialized");
        mirType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(opMeExpr.GetTyIdx());
        break;
      }
      // fall thru
      [[clang::fallthrough]];
    default:
      return CreateRegMeExpr(PTY_ptr);
  }
  return CreateRegMeExpr(*mirType);
}

VarMeExpr *IRMap::GetOrCreateZeroVersionVarMeExpr(OriginalSt &ost) {
  ASSERT(ost.GetZeroVersionIndex() != 0,
         "GetOrCreateZeroVersionVarMeExpr: version index of osym's kInitVersion not set");
  ASSERT(ost.GetZeroVersionIndex() < verst2MeExprTable.size(),
         "GetOrCreateZeroVersionVarMeExpr: version index of osym's kInitVersion out of range");
  if (verst2MeExprTable[ost.GetZeroVersionIndex()] == nullptr) {
    auto *varMeExpr = New<VarMeExpr>(exprID++, &ost, ost.GetZeroVersionIndex(),
        GlobalTables::GetTypeTable().GetTypeFromTyIdx(ost.GetTyIdx())->GetPrimType());
    ASSERT(!GlobalTables::GetTypeTable().GetTypeTable().empty(), "container check");
    verst2MeExprTable[ost.GetZeroVersionIndex()] = varMeExpr;
    return varMeExpr;
  }
  return static_cast<VarMeExpr*>(verst2MeExprTable[ost.GetZeroVersionIndex()]);
}

IvarMeExpr *IRMap::BuildLHSIvar(MeExpr &baseAddr, PrimType primType, const TyIdx &tyIdx, FieldID fieldID) {
  auto *meDef = New<IvarMeExpr>(exprID++, primType, tyIdx, fieldID);
  meDef->SetBase(&baseAddr);
  PutToBucket(meDef->GetHashIndex() % mapHashLength, *meDef);
  return meDef;
}

static std::pair<MeExpr*, OffsetType> SimplifyBaseAddressOfIvar(MeExpr *base) {
  if (base->GetOp() == OP_add || base->GetOp() == OP_sub) {
    auto offsetNode = base->GetOpnd(1);
    if (offsetNode->GetOp() == OP_constval) {
      // get offset value
      auto *mirConst = static_cast<ConstMeExpr*>(offsetNode)->GetConstVal();
      CHECK_FATAL(mirConst->GetKind() == kConstInt, "must be integer const");
      auto offsetInByte = static_cast<MIRIntConst*>(mirConst)->GetValue();
      OffsetType offset(kOffsetUnknown);
      offset.Set(base->GetOp() == OP_add ? offsetInByte : -offsetInByte);
      if (offset.IsInvalid()) {
        return std::make_pair(base, offset);
      }

      const auto &baseNodeAndOffset = SimplifyBaseAddressOfIvar(base->GetOpnd(0));
      auto newOffset = offset + baseNodeAndOffset.second;
      if (newOffset.IsInvalid()) {
        return std::make_pair(base->GetOpnd(0), offset);
      }
      return std::make_pair(baseNodeAndOffset.first, newOffset);
    }
  }
  return std::make_pair(base, OffsetType::InvalidOffset());
}

void IRMap::SimplifyIvar(IvarMeExpr *ivar) {
  const auto &newBaseAndOffset = SimplifyBaseAddressOfIvar(ivar->GetBase());
  if (newBaseAndOffset.second.IsInvalid()) {
    return;
  }

  auto newOffset = newBaseAndOffset.second + ivar->GetOffset();
  if (newOffset.IsInvalid()) {
    return;
  }

  ivar->SetBase(newBaseAndOffset.first);
  if (newOffset.val != 0) {
    ivar->SetOp(OP_ireadoff);
  } else {
    ivar->SetOp(OP_iread);
  }
  ivar->SetOffset(newOffset.val);
}

IvarMeExpr *IRMap::BuildLHSIvar(MeExpr &baseAddr, IassignMeStmt &iassignMeStmt, FieldID fieldID) {
  MIRType *ptrMIRType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(iassignMeStmt.GetTyIdx());
  auto *realMIRType = static_cast<MIRPtrType*>(ptrMIRType);
  MIRType *ty = nullptr;
  if (fieldID > 0) {
    ty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(realMIRType->GetPointedTyIdxWithFieldID(fieldID));
  } else {
    ty = realMIRType->GetPointedType();
  }
  auto *meDef = New<IvarMeExpr>(exprID++, ty->GetPrimType(), iassignMeStmt.GetTyIdx(), fieldID);
  if (iassignMeStmt.GetLHSVal() != nullptr && iassignMeStmt.GetLHSVal()->GetOffset() != 0) {
    meDef->SetOp(OP_ireadoff);
    meDef->SetOffset(iassignMeStmt.GetLHSVal()->GetOffset());
  }
  meDef->SetBase(&baseAddr);
  meDef->SetDefStmt(&iassignMeStmt);
  SimplifyIvar(meDef);
  PutToBucket(meDef->GetHashIndex() % mapHashLength, *meDef);
  return meDef;
}

IvarMeExpr *IRMap::BuildLHSIvarFromIassMeStmt(IassignMeStmt &iassignMeStmt) {
  IvarMeExpr *ivarx = BuildLHSIvar(*iassignMeStmt.GetLHSVal()->GetBase(), iassignMeStmt,
                                   iassignMeStmt.GetLHSVal()->GetFieldID());
  ivarx->SetVolatileFromBaseSymbol(iassignMeStmt.GetLHSVal()->GetVolatileFromBaseSymbol());
  return ivarx;
}

void IRMap::PutToBucket(uint32 hashIdx, MeExpr &meExpr) {
  MeExpr *headExpr = hashTable[hashIdx];
  if (headExpr != nullptr) {
    meExpr.SetNext(headExpr);
  }
  hashTable[hashIdx] = &meExpr;
}

MeExpr *IRMap::HashMeExpr(MeExpr &meExpr) {
  MeExpr *resultExpr = nullptr;
  uint32 hashIdx = meExpr.GetHashIndex() % mapHashLength;
  MeExpr *hashedExpr = hashTable[hashIdx];

  if (hashedExpr != nullptr && meExpr.GetMeOp() != kMeOpGcmalloc) {
    resultExpr = meExpr.GetIdenticalExpr(*hashedExpr, mirModule.CurFunction()->IsConstructor());
  }

  if (resultExpr == nullptr) {
    switch (meExpr.GetMeOp()) {
      case kMeOpIvar:
        resultExpr = New<IvarMeExpr>(exprID, static_cast<IvarMeExpr&>(meExpr));
        break;
      case kMeOpOp:
        resultExpr = New<OpMeExpr>(static_cast<OpMeExpr&>(meExpr), exprID);
        break;
      case kMeOpConst:
        resultExpr = New<ConstMeExpr>(exprID, static_cast<ConstMeExpr&>(meExpr).GetConstVal(), meExpr.GetPrimType());
        break;
      case kMeOpConststr:
        resultExpr =
            New<ConststrMeExpr>(exprID, static_cast<ConststrMeExpr&>(meExpr).GetStrIdx(), meExpr.GetPrimType());
        break;
      case kMeOpConststr16:
        resultExpr =
            New<Conststr16MeExpr>(exprID, static_cast<Conststr16MeExpr&>(meExpr).GetStrIdx(), meExpr.GetPrimType());
        break;
      case kMeOpSizeoftype:
        resultExpr =
            New<SizeoftypeMeExpr>(exprID, meExpr.GetPrimType(), static_cast<SizeoftypeMeExpr&>(meExpr).GetTyIdx());
        break;
      case kMeOpFieldsDist: {
        auto &expr = static_cast<FieldsDistMeExpr&>(meExpr);
        resultExpr = New<FieldsDistMeExpr>(exprID, meExpr.GetPrimType(), expr.GetTyIdx(),
                                           expr.GetFieldID1(), expr.GetFieldID2());
        break;
      }
      case kMeOpAddrof:
        resultExpr = New<AddrofMeExpr>(exprID, meExpr.GetPrimType(), static_cast<AddrofMeExpr&>(meExpr).GetOstIdx());
        static_cast<AddrofMeExpr*>(resultExpr)->SetFieldID(static_cast<AddrofMeExpr&>(meExpr).GetFieldID());
        break;
      case kMeOpNary:
        resultExpr = NewInPool<NaryMeExpr>(exprID, static_cast<NaryMeExpr&>(meExpr));
        break;
      case kMeOpAddroffunc:
        resultExpr = New<AddroffuncMeExpr>(exprID, static_cast<AddroffuncMeExpr&>(meExpr).GetPuIdx());
        break;
      case kMeOpAddroflabel:
        resultExpr = New<AddroflabelMeExpr>(exprID, static_cast<AddroflabelMeExpr&>(meExpr).labelIdx);
        break;
      case kMeOpGcmalloc:
        resultExpr = New<GcmallocMeExpr>(exprID, meExpr.GetOp(), meExpr.GetPrimType(),
                                         static_cast<GcmallocMeExpr&>(meExpr).GetTyIdx());
        break;
      default:
        CHECK_FATAL(false, "not yet implement");
    }
    ++exprID;
    PutToBucket(hashIdx, *resultExpr);
  }
  return resultExpr;
}

MeExpr *IRMap::ReplaceMeExprExpr(MeExpr &origExpr, MeExpr &newExpr, size_t opndsSize,
                                 const MeExpr &meExpr, MeExpr &repExpr) {
  bool needRehash = false;

  for (size_t i = 0; i < opndsSize; ++i) {
    MeExpr *origOpnd = origExpr.GetOpnd(i);
    if (origOpnd == nullptr) {
      continue;
    }

    if (origOpnd == &meExpr) {
      needRehash = true;
      newExpr.SetOpnd(i, &repExpr);
    } else if (!origOpnd->IsLeaf()) {
      newExpr.SetOpnd(i, ReplaceMeExprExpr(*newExpr.GetOpnd(i), meExpr, repExpr));
      if (newExpr.GetOpnd(i) != origOpnd) {
        needRehash = true;
      }
    }
  }
  if (needRehash) {
    auto simplifiedExpr = SimplifyMeExpr(&newExpr);
    return simplifiedExpr != &newExpr ? simplifiedExpr : HashMeExpr(newExpr);
  }
  return &origExpr;
}

// replace meExpr with repexpr. meExpr must be a kid of origexpr
// return repexpr's parent if replaced, otherwise return nullptr
MeExpr *IRMap::ReplaceMeExprExpr(MeExpr &origExpr, const MeExpr &meExpr, MeExpr &repExpr) {
  if (origExpr.IsLeaf()) {
    return &origExpr;
  }

  switch (origExpr.GetMeOp()) {
    case kMeOpOp: {
      auto &opMeExpr = static_cast<OpMeExpr&>(origExpr);
      OpMeExpr newMeExpr(opMeExpr, kInvalidExprID);
      return ReplaceMeExprExpr(opMeExpr, newMeExpr, kOperandNumTernary, meExpr, repExpr);
    }
    case kMeOpNary: {
      auto &naryMeExpr = static_cast<NaryMeExpr&>(origExpr);
      NaryMeExpr newMeExpr(&irMapAlloc, kInvalidExprID, naryMeExpr);
      return ReplaceMeExprExpr(naryMeExpr, newMeExpr, naryMeExpr.GetOpnds().size(), meExpr, repExpr);
    }
    case kMeOpIvar: {
      auto &ivarExpr = static_cast<IvarMeExpr&>(origExpr);
      IvarMeExpr newMeExpr(kInvalidExprID, ivarExpr);
      bool needRehash = false;
      if (ivarExpr.GetBase() == &meExpr) {
        newMeExpr.SetBase(&repExpr);
        needRehash = true;
      } else if (!ivarExpr.GetBase()->IsLeaf()) {
        newMeExpr.SetBase(ReplaceMeExprExpr(*newMeExpr.GetBase(), meExpr, repExpr));
        if (newMeExpr.GetBase() != ivarExpr.GetBase()) {
          needRehash = true;
        }
      }
      return needRehash ? HashMeExpr(newMeExpr) : &origExpr;
    }
    default:
      ASSERT(false, "NYI");
      return nullptr;
  }
}

bool IRMap::ReplaceMeExprStmtOpnd(uint32 opndID, MeStmt &meStmt, const MeExpr &meExpr, MeExpr &repExpr) {
  MeExpr *opnd = meStmt.GetOpnd(opndID);

  if (opnd == &meExpr) {
    meStmt.SetOpnd(opndID, &repExpr);
    SimplifyCastForAssign(&meStmt);
    return true;
  } else if (!opnd->IsLeaf()) {
    meStmt.SetOpnd(opndID, ReplaceMeExprExpr(*opnd, meExpr, repExpr));
    SimplifyCastForAssign(&meStmt);
    return meStmt.GetOpnd(opndID) != opnd;
  }

  return false;
}

// replace meExpr in meStmt with repexpr
bool IRMap::ReplaceMeExprStmt(MeStmt &meStmt, const MeExpr &meExpr, MeExpr &repexpr) {
  bool isReplaced = false;
  Opcode op = meStmt.GetOp();

  for (size_t i = 0; i < meStmt.NumMeStmtOpnds(); ++i) {
    if (op == OP_intrinsiccall || op == OP_xintrinsiccall || op == OP_intrinsiccallwithtype ||
        op == OP_intrinsiccallassigned || op == OP_xintrinsiccallassigned ||
        op == OP_intrinsiccallwithtypeassigned) {
      MeExpr *opnd = meStmt.GetOpnd(i);
      if (opnd->IsLeaf() && opnd->GetMeOp() == kMeOpVar) {
        auto *varMeExpr = static_cast<VarMeExpr*>(opnd);
        const OriginalSt *ost = varMeExpr->GetOst();
        if (ost->IsSymbolOst() && ost->GetMIRSymbol()->GetAttr(ATTR_static)) {
          // its address may be taken
          continue;
        }
      }
    }

    bool curOpndReplaced = false;
    if (i == 0 && op == OP_iassign) {
      auto &ivarStmt = static_cast<IassignMeStmt&>(meStmt);
      MeExpr *oldBase = ivarStmt.GetLHSVal()->GetBase();
      MeExpr *newBase = nullptr;
      if (oldBase == &meExpr) {
        newBase = &repexpr;
        curOpndReplaced = true;
      } else if (!oldBase->IsLeaf()) {
        newBase = ReplaceMeExprExpr(*oldBase, meExpr, repexpr);
        curOpndReplaced = (newBase != oldBase);
      }
      if (curOpndReplaced) {
        ASSERT_NOT_NULL(newBase);
        auto *newLHS = BuildLHSIvar(*newBase, ivarStmt, ivarStmt.GetLHSVal()->GetFieldID());
        newLHS->SetVolatileFromBaseSymbol(ivarStmt.GetLHSVal()->GetVolatileFromBaseSymbol());
        ivarStmt.SetLHSVal(newLHS);
      }
    } else {
      curOpndReplaced = ReplaceMeExprStmtOpnd(i, meStmt, meExpr, repexpr);
    }
    isReplaced = isReplaced || curOpndReplaced;
  }

  if (isReplaced) {
    SimplifyCastForAssign(&meStmt);
  }

  return isReplaced;
}

MePhiNode *IRMap::CreateMePhi(ScalarMeExpr &meExpr) {
  auto *phiMeVar = NewInPool<MePhiNode>();
  phiMeVar->UpdateLHS(meExpr);
  return phiMeVar;
}

IassignMeStmt *IRMap::CreateIassignMeStmt(TyIdx tyIdx, IvarMeExpr &lhs, MeExpr &rhs,
                                          const MapleMap<OStIdx, ChiMeNode*> &clist) {
  return NewInPool<IassignMeStmt>(tyIdx, &lhs, &rhs, &clist);
}

AssignMeStmt *IRMap::CreateAssignMeStmt(ScalarMeExpr &lhs, MeExpr &rhs, BB &currBB) {
  AssignMeStmt *meStmt = nullptr;
  if (lhs.GetMeOp() == kMeOpReg) {
    meStmt = New<AssignMeStmt>(OP_regassign, &lhs, &rhs);
  } else {
    meStmt = NewInPool<DassignMeStmt>(&static_cast<VarMeExpr &>(lhs), &rhs);
  }
  lhs.SetDefBy(kDefByStmt);
  lhs.SetDefStmt(meStmt);
  meStmt->SetBB(&currBB);
  return meStmt;
}

// get the false goto bb, if condgoto is brtrue, take the other bb of brture @lable
// otherwise, take the bb of @lable
BB *IRMap::GetFalseBrBB(const CondGotoMeStmt &condgoto) {
  LabelIdx lblIdx = (LabelIdx)condgoto.GetOffset();
  BB *gotoBB = GetBBForLabIdx(lblIdx);
  BB *bb = condgoto.GetBB();
  ASSERT(bb->GetSucc().size() == kBBVectorInitialSize, "array size error");
  if (condgoto.GetOp() == OP_brfalse) {
    return gotoBB;
  } else {
    return gotoBB == bb->GetSucc(0) ? bb->GetSucc(1) : bb->GetSucc(0);
  }
}

MeExpr *IRMap::CreateConstMeExpr(PrimType pType, MIRConst &mirConst) {
  ConstMeExpr constMeExpr(kInvalidExprID, &mirConst, pType);
  return HashMeExpr(constMeExpr);
}

MeExpr *IRMap::CreateIntConstMeExpr(int64 value, PrimType pType) {
  auto *intConst =
      GlobalTables::GetIntConstTable().GetOrCreateIntConst(value, *GlobalTables::GetTypeTable().GetPrimType(pType));
  return CreateConstMeExpr(pType, *intConst);
}

MeExpr *IRMap::CreateMeExprUnary(Opcode op, PrimType pType, MeExpr &expr0) {
  OpMeExpr opMeExpr(kInvalidExprID, op, pType, kOperandNumUnary);
  opMeExpr.SetOpnd(0, &expr0);
  return HashMeExpr(opMeExpr);
}

MeExpr *IRMap::CreateMeExprBinary(Opcode op, PrimType pType, MeExpr &expr0, MeExpr &expr1) {
  OpMeExpr opMeExpr(kInvalidExprID, op, pType, kOperandNumBinary);
  opMeExpr.SetOpnd(0, &expr0);
  opMeExpr.SetOpnd(1, &expr1);
  return HashMeExpr(opMeExpr);
}

MeExpr *IRMap::CreateMeExprSelect(PrimType pType, MeExpr &expr0, MeExpr &expr1, MeExpr &expr2) {
  OpMeExpr opMeExpr(kInvalidExprID, OP_select, pType, kOperandNumTernary);
  opMeExpr.SetOpnd(0, &expr0);
  opMeExpr.SetOpnd(1, &expr1);
  opMeExpr.SetOpnd(2, &expr2);
  return HashMeExpr(opMeExpr);
}

MeExpr *IRMap::CreateMeExprCompare(Opcode op, PrimType resptyp, PrimType opndptyp, MeExpr &opnd0, MeExpr &opnd1) {
  OpMeExpr opMeExpr(kInvalidExprID, op, resptyp, kOperandNumBinary);
  opMeExpr.SetOpnd(0, &opnd0);
  opMeExpr.SetOpnd(1, &opnd1);
  opMeExpr.SetOpndType(opndptyp);
  MeExpr *retMeExpr = HashMeExpr(opMeExpr);
  static_cast<OpMeExpr*>(retMeExpr)->SetOpndType(opndptyp);
  return retMeExpr;
}

MeExpr *IRMap::CreateMeExprTypeCvt(PrimType pType, PrimType opndptyp, MeExpr &opnd0) {
  OpMeExpr opMeExpr(kInvalidExprID, OP_cvt, pType, kOperandNumUnary);
  opMeExpr.SetOpnd(0, &opnd0);
  opMeExpr.SetOpndType(opndptyp);
  return HashMeExpr(opMeExpr);
}

MeExpr *IRMap::CreateMeExprExt(Opcode op, PrimType pType, uint32 bitsSize, MeExpr &opnd) {
  ASSERT(op == OP_zext || op == OP_sext, "must be");
  OpMeExpr opMeExpr(kInvalidExprID, op, pType, kOperandNumUnary);
  opMeExpr.SetOpnd(0, &opnd);
  opMeExpr.SetBitsSize(bitsSize);
  return HashMeExpr(opMeExpr);
}

UnaryMeStmt *IRMap::CreateUnaryMeStmt(Opcode op, MeExpr *opnd) {
  UnaryMeStmt *unaryMeStmt = New<UnaryMeStmt>(op);
  unaryMeStmt->SetMeStmtOpndValue(opnd);
  return unaryMeStmt;
}

UnaryMeStmt *IRMap::CreateUnaryMeStmt(Opcode op, MeExpr *opnd, BB *bb, const SrcPosition *src) {
  UnaryMeStmt *unaryMeStmt = CreateUnaryMeStmt(op, opnd);
  unaryMeStmt->SetBB(bb);
  unaryMeStmt->SetSrcPos(*src);
  return unaryMeStmt;
}

IntrinsiccallMeStmt *IRMap::CreateIntrinsicCallMeStmt(MIRIntrinsicID idx, std::vector<MeExpr*> &opnds, TyIdx tyIdx) {
  auto *meStmt =
      NewInPool<IntrinsiccallMeStmt>(tyIdx == 0u ? OP_intrinsiccall : OP_intrinsiccallwithtype, idx, tyIdx);
  for (MeExpr *opnd : opnds) {
    meStmt->PushBackOpnd(opnd);
  }
  return meStmt;
}

IntrinsiccallMeStmt *IRMap::CreateIntrinsicCallAssignedMeStmt(MIRIntrinsicID idx, std::vector<MeExpr*> &opnds,
                                                              ScalarMeExpr *ret, TyIdx tyIdx) {
  auto *meStmt = NewInPool<IntrinsiccallMeStmt>(
      tyIdx == 0u ? OP_intrinsiccallassigned : OP_intrinsiccallwithtypeassigned, idx, tyIdx);
  for (MeExpr *opnd : opnds) {
    meStmt->PushBackOpnd(opnd);
  }
  if (ret != nullptr) {
    auto *mustDef = New<MustDefMeNode>(ret, meStmt);
    meStmt->GetMustDefList()->push_back(*mustDef);
  }
  return meStmt;
}

MeExpr *IRMap::CreateAddrofMeExprFromSymbol(MIRSymbol &st, PUIdx puIdx) {
  OriginalSt *baseOst = ssaTab.FindOrCreateSymbolOriginalSt(st, puIdx, 0);
  if (baseOst->GetZeroVersionIndex() == 0) {
    baseOst->SetZeroVersionIndex(verst2MeExprTable.size());
    verst2MeExprTable.push_back(nullptr);
    baseOst->PushbackVersionsIndices(baseOst->GetZeroVersionIndex());
  }

  AddrofMeExpr addrOfMe(kInvalidExprID, PTY_ptr, baseOst->GetIndex());
  return HashMeExpr(addrOfMe);
}

// (typeA -> typeB -> typeC) => (typeA -> typeC)
static bool IgnoreInnerTypeCvt(PrimType typeA, PrimType typeB, PrimType typeC) {
  if (IsPrimitiveInteger(typeA)) {
    if (IsPrimitiveInteger(typeB)) {
      if (IsPrimitiveInteger(typeC)) {
        return (GetPrimTypeSize(typeB) >= GetPrimTypeSize(typeA) || GetPrimTypeSize(typeB) >= GetPrimTypeSize(typeC));
      } else if (IsPrimitiveFloat(typeC)) {
        return GetPrimTypeSize(typeB) >= GetPrimTypeSize(typeA) && IsSignedInteger(typeB) == IsSignedInteger(typeA);
      }
    } else if (IsPrimitiveFloat(typeB)) {
      if (IsPrimitiveFloat(typeC)) {
        return GetPrimTypeSize(typeB) >= GetPrimTypeSize(typeC);
      }
    }
  } else if (IsPrimitiveFloat(typeA)) {
    if (IsPrimitiveFloat(typeB) && IsPrimitiveFloat(typeC)) {
      return GetPrimTypeSize(typeB) >= GetPrimTypeSize(typeA) || GetPrimTypeSize(typeB) >= GetPrimTypeSize(typeC);
    }
  }
  return false;
}

// return nullptr if the result is unknow
MeExpr *IRMap::SimplifyCompareSameExpr(OpMeExpr *opmeexpr) {
  CHECK_FATAL(opmeexpr->GetOpnd(0) == opmeexpr->GetOpnd(1), "must be");
  Opcode opop = opmeexpr->GetOp();
  int64 val = 0;
  switch (opop) {
    case OP_eq:
    case OP_le:
    case OP_ge: {
      val = 1;
      break;
    }
    case OP_ne:
    case OP_lt:
    case OP_gt:
    case OP_cmp: {
      val = 0;
      break;
    }
    case OP_cmpl:
    case OP_cmpg: {
      // cmpl/cmpg is special for cases that any of opnd is NaN
      auto opndType = opmeexpr->GetOpndType();
      if (IsPrimitiveFloat(opndType) || IsPrimitiveDynFloat(opndType)) {
        if (opmeexpr->GetOpnd(0)->GetMeOp() == kMeOpConst) {
          double dVal =
              static_cast<MIRFloatConst*>(static_cast<ConstMeExpr *>(opmeexpr->GetOpnd(0))->GetConstVal())->GetValue();
          if (isnan(dVal)) {
            val = (opop == OP_cmpl) ? -1 : 1;
          } else {
            val = 0;
          }
          break;
        }
        // other case, return nullptr because it is not sure whether any of opnd is nan.
        return nullptr;
      }
      val = 0;
      break;
    }
    default:
      CHECK_FATAL(false, "must be compare op!");
  }
  return CreateIntConstMeExpr(val, opmeexpr->GetPrimType());
}

// opA (opB (opndA, opndB), opndC)
MeExpr *IRMap::CreateCanonicalizedMeExpr(PrimType primType, Opcode opA,  Opcode opB,
                                         MeExpr *opndA, MeExpr *opndB, MeExpr *opndC) {
  if (primType != opndC->GetPrimType() && !IsNoCvtNeeded(primType, opndC->GetPrimType())) {
    opndC = CreateMeExprTypeCvt(primType, opndC->GetPrimType(), *opndC);
  }

  if (opndA->GetMeOp() == kMeOpConst && opndB->GetMeOp() == kMeOpConst) {
    auto *constOpnd0 = FoldConstExpr(primType, opB, static_cast<ConstMeExpr*>(opndA), static_cast<ConstMeExpr*>(opndB));
    if (constOpnd0 != nullptr) {
      return CreateMeExprBinary(opA, primType, *constOpnd0, *opndC);
    }
  }

  if (primType != opndA->GetPrimType() && !IsNoCvtNeeded(primType, opndA->GetPrimType())) {
    opndA = CreateMeExprTypeCvt(primType, opndA->GetPrimType(), *opndA);
  }
  if (primType != opndB->GetPrimType() && !IsNoCvtNeeded(primType, opndB->GetPrimType())) {
    opndB = CreateMeExprTypeCvt(primType, opndB->GetPrimType(), *opndB);
  }

  auto *opnd0 = CreateMeExprBinary(opB, primType, *opndA, *opndB);
  return CreateMeExprBinary(opA, primType, *opnd0, *opndC);
}

// opA (opndA, opB (opndB, opndC))
MeExpr *IRMap::CreateCanonicalizedMeExpr(PrimType primType, Opcode opA, MeExpr *opndA,
                                         Opcode opB, MeExpr *opndB, MeExpr *opndC) {
  if (primType != opndA->GetPrimType() && !IsNoCvtNeeded(primType, opndA->GetPrimType())) {
    opndA = CreateMeExprTypeCvt(primType, opndA->GetPrimType(), *opndA);
  }

  if (opndB->GetMeOp() == kMeOpConst && opndC->GetMeOp() == kMeOpConst) {
    auto *constOpnd1 = FoldConstExpr(primType, opB, static_cast<ConstMeExpr*>(opndB), static_cast<ConstMeExpr*>(opndC));
    if (constOpnd1 != nullptr) {
      return CreateMeExprBinary(opA, primType, *opndA, *constOpnd1);
    }
  }

  if (primType != opndB->GetPrimType() && !IsNoCvtNeeded(primType, opndB->GetPrimType())) {
    opndB = CreateMeExprTypeCvt(primType, opndB->GetPrimType(), *opndB);
  }
  if (primType != opndC->GetPrimType() && !IsNoCvtNeeded(primType, opndC->GetPrimType())) {
    opndC = CreateMeExprTypeCvt(primType, opndC->GetPrimType(), *opndC);
  }
  auto *opnd1 = CreateMeExprBinary(opB, primType, *opndB, *opndC);
  return CreateMeExprBinary(opA, primType, *opndA, *opnd1);
}

// opA (opB (opndA, opndB), opC (opndC, opndD))
MeExpr *IRMap::CreateCanonicalizedMeExpr(PrimType primType, Opcode opA, Opcode opB, MeExpr *opndA, MeExpr *opndB,
                                         Opcode opC, MeExpr *opndC, MeExpr *opndD) {
  MeExpr *newOpnd0 = nullptr;
  if (opndA->GetMeOp() == kMeOpConst && opndB->GetMeOp() == kMeOpConst) {
    newOpnd0 = FoldConstExpr(primType, opB, static_cast<ConstMeExpr*>(opndA), static_cast<ConstMeExpr*>(opndB));
  }
  if (newOpnd0 == nullptr) {
    if (primType != opndA->GetPrimType() && !IsNoCvtNeeded(primType, opndA->GetPrimType())) {
      opndA = CreateMeExprTypeCvt(primType, opndA->GetPrimType(), *opndA);
    }
    if (primType != opndB->GetPrimType() && !IsNoCvtNeeded(primType, opndB->GetPrimType())) {
      opndB = CreateMeExprTypeCvt(primType, opndB->GetPrimType(), *opndB);
    }
    newOpnd0 = CreateMeExprBinary(opB, primType, *opndA, *opndB);
  }

  MeExpr *newOpnd1 = nullptr;
  if (opndC->GetMeOp() == kMeOpConst && opndD->GetMeOp() == kMeOpConst) {
    newOpnd1 = FoldConstExpr(primType, opC, static_cast<ConstMeExpr*>(opndC), static_cast<ConstMeExpr*>(opndD));
  }
  if (newOpnd1 == nullptr) {
    if (primType != opndC->GetPrimType() && !IsNoCvtNeeded(primType, opndC->GetPrimType())) {
      opndC = CreateMeExprTypeCvt(primType, opndC->GetPrimType(), *opndC);
    }
    if (primType != opndD->GetPrimType() && !IsNoCvtNeeded(primType, opndD->GetPrimType())) {
      opndD = CreateMeExprTypeCvt(primType, opndD->GetPrimType(), *opndD);
    }
    newOpnd1 = CreateMeExprBinary(opC, primType, *opndC, *opndD);
  }
  return CreateMeExprBinary(opA, primType, *newOpnd0, *newOpnd1);
}

MeExpr *IRMap::FoldConstExpr(PrimType primType, Opcode op, ConstMeExpr *opndA, ConstMeExpr *opndB) {
  if (!IsPrimitiveInteger(primType)) {
    return nullptr;
  }

  maple::ConstantFold cf(mirModule);
  auto *constA = static_cast<MIRIntConst*>(opndA->GetConstVal());
  auto *constB = static_cast<MIRIntConst*>(opndB->GetConstVal());
  if ((op == OP_div || op == OP_rem)) {
    if (constB->GetValue() == 0 ||
        (constB->GetValue() == -1 && ((primType == PTY_i32 && constA->GetValue() == INT32_MIN) ||
                                      (primType == PTY_i64 && constA->GetValue() == INT64_MIN)))) {
      return nullptr;
    }
  }
  MIRConst *resconst = cf.FoldIntConstBinaryMIRConst(op, primType, constA, constB);
  return CreateConstMeExpr(primType, *resconst);
}

MeExpr *IRMap::SimplifyAddExpr(OpMeExpr *addExpr) {
  if (addExpr->GetOp() != OP_add) {
    return nullptr;
  }

  auto *opnd0 = addExpr->GetOpnd(0);
  auto *opnd1 = addExpr->GetOpnd(1);
  if (opnd0->IsLeaf() && opnd1->IsLeaf()) {
    if (opnd0->GetMeOp() == kMeOpConst && opnd1->GetMeOp() == kMeOpConst) {
      return FoldConstExpr(addExpr->GetPrimType(), addExpr->GetOp(),
                           static_cast<ConstMeExpr*>(opnd0), static_cast<ConstMeExpr*>(opnd1));
    }
    return nullptr;
  }

  if (!opnd0->IsLeaf() && !opnd1->IsLeaf()) {
    return nullptr;
  }

  if (!opnd1->IsLeaf()) {
    auto *tmp = opnd1;
    opnd1 = opnd0;
    opnd0 = tmp;
  }

  if (opnd1->IsLeaf()) {
    if (opnd0->GetOp() == OP_cvt) {
      opnd0 = opnd0->GetOpnd(0);
    }

    if (opnd0->GetOp() == OP_add) {
      auto *opndA = opnd0->GetOpnd(0);
      auto *opndB = opnd0->GetOpnd(1);
      if (!opndA->IsLeaf() || !opndB->IsLeaf()) {
        return nullptr;
      }
      if (opndA->GetMeOp() == kMeOpConst) {
        // (constA + constB) + opnd1
        if (opndB->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, opnd1, OP_add, opndA, opndB);
        }
        // (constA + a) + constB --> a + (constA + constB)
        if (opnd1->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, opndB, OP_add, opndA, opnd1);
        }
        // (const + a) + b --> (a + b) + const
        return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, OP_add, opndB, opnd1, opndA);
      }

      if (opndB->GetMeOp() == kMeOpConst) {
        // (a + constA) + constB --> a + (constA + constB)
        if (opnd1->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, opndA, OP_add, opndB, opnd1);
        }
        // (a + const) + b --> (a + b) + const
        return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, OP_add, opndA, opnd1, opndB);
      }
    }

    if (opnd0->GetOp() == OP_sub) {
      auto *opndA = opnd0->GetOpnd(0);
      auto *opndB = opnd0->GetOpnd(1);
      if (!opndA->IsLeaf() || !opndB->IsLeaf()) {
        return nullptr;
      }
      if (opndA->GetMeOp() == kMeOpConst) {
        // (constA - constB) + opnd1
        if (opndB->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, opnd1, OP_sub, opndA, opndB);
        }
        // (constA - a) + constB --> (constA + constB) - a
        if (opnd1->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_sub, OP_add, opndA, opnd1, opndB);
        }
        // (const - a) + b --> (b - a) + const
        return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, OP_sub, opnd1, opndB, opndA);
      }

      if (opndB->GetMeOp() == kMeOpConst) {
        // (a - constA) + constB --> a + (constB - constA)
        if (opnd1->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_add, opndA, OP_sub, opnd1, opndB);
        }
        // (a - const) + b --> (a + b) - const
        return CreateCanonicalizedMeExpr(addExpr->GetPrimType(), OP_sub, OP_add, opndA, opnd1, opndB);
      }
    }
  }
  return nullptr;
}

MeExpr *IRMap::SimplifyMulExpr(OpMeExpr *mulExpr) {
  if (mulExpr->GetOp() != OP_mul) {
    return nullptr;
  }

  auto *opnd0 = mulExpr->GetOpnd(0);
  auto *opnd1 = mulExpr->GetOpnd(1);
  if (opnd0->IsLeaf() && opnd1->IsLeaf()) {
    if (opnd0->GetMeOp() == kMeOpConst && opnd1->GetMeOp() == kMeOpConst) {
      return FoldConstExpr(mulExpr->GetPrimType(), mulExpr->GetOp(),
                           static_cast<ConstMeExpr*>(opnd0), static_cast<ConstMeExpr*>(opnd1));
    }
    return nullptr;
  }

  if (!opnd0->IsLeaf() && !opnd1->IsLeaf()) {
    return nullptr;
  }

  if (!opnd1->IsLeaf()) {
    auto *tmp = opnd1;
    opnd1 = opnd0;
    opnd0 = tmp;
  }

  if (opnd1->IsLeaf()) {
    if (opnd0->GetOp() == OP_cvt) {
      opnd0 = opnd0->GetOpnd(0);
    }

    if (opnd0->GetOp() == OP_add) {
      auto *opndA = opnd0->GetOpnd(0);
      auto *opndB = opnd0->GetOpnd(1);
      if (!opndA->IsLeaf() || !opndB->IsLeaf()) {
        return nullptr;
      }
      if (opndA->GetMeOp() == kMeOpConst) {
        // (constA + constB) * opnd1
        if (opndB->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(mulExpr->GetPrimType(), OP_mul, opnd1, OP_add, opndA, opndB);
        }
        // (constA + a) * constB --> a * constB + (constA * constB)
        if (opnd1->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(
              mulExpr->GetPrimType(), OP_add, OP_mul, opndB, opnd1, OP_mul, opndA, opnd1);
        }
        return nullptr;
      }

      // (a + constA) * constB --> a * constB + (constA * constB)
      if (opndB->GetMeOp() == kMeOpConst && opnd1->GetMeOp() == kMeOpConst) {
        return CreateCanonicalizedMeExpr(
            mulExpr->GetPrimType(), OP_add, OP_mul, opndA, opnd1, OP_mul, opndB, opnd1);
      }
      return nullptr;
    }

    if (opnd0->GetOp() == OP_sub) {
      auto *opndA = opnd0->GetOpnd(0);
      auto *opndB = opnd0->GetOpnd(1);
      if (!opndA->IsLeaf() || !opndB->IsLeaf()) {
        return nullptr;
      }
      if (opndA->GetMeOp() == kMeOpConst) {
        // (constA - constB) * opnd1
        if (opndB->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(mulExpr->GetPrimType(), OP_mul, opnd1, OP_sub, opndA, opndB);
        }
        // (constA - a) * constB --> a * constB + (constA * constB)
        if (opnd1->GetMeOp() == kMeOpConst) {
          return CreateCanonicalizedMeExpr(
              mulExpr->GetPrimType(), OP_sub, OP_mul, opndA, opnd1, OP_mul, opndB, opnd1);
        }
        return nullptr;
      }

      // (a - constA) * constB --> a * constB - (constA * constB)
      if (opndB->GetMeOp() == kMeOpConst && opnd1->GetMeOp() == kMeOpConst) {
        return CreateCanonicalizedMeExpr(
            mulExpr->GetPrimType(), OP_sub, OP_mul, opndA, opnd1, OP_mul, opndB, opnd1);
      }
      return nullptr;
    }
  }
  return nullptr;
}

MeExpr *IRMap::SimplifyOpMeExpr(OpMeExpr *opmeexpr) {
  Opcode opop = opmeexpr->GetOp();
  if (simplifyCastExpr && IsCastMeExprExplicit(*opmeexpr)) {
    MeExpr *simpleCast = SimplifyCast(opmeexpr);
    if (simpleCast != nullptr) {
      return simpleCast;
    }
  }
  switch (opop) {
    case OP_cvt: {
      OpMeExpr *cvtmeexpr = static_cast<OpMeExpr *>(opmeexpr);
      MeExpr *opnd0 = cvtmeexpr->GetOpnd(0);
      if (opnd0->GetMeOp() == kMeOpConst) {
        ConstantFold cf(mirModule);
        MIRConst *tocvt =
          cf.FoldTypeCvtMIRConst(*static_cast<ConstMeExpr *>(opnd0)->GetConstVal(),
                                 cvtmeexpr->GetOpndType(), cvtmeexpr->GetPrimType());
        if (tocvt) {
          return CreateConstMeExpr(cvtmeexpr->GetPrimType(), *tocvt);
        }
      }
      // If typeA, typeB, and typeC{fieldId} are integer, and sizeof(typeA) == sizeof(typeB).
      // Simplfy expr: cvt typeA typeB (iread typeB <* typeC> fieldId address)
      // to: iread typeA <* typeC> fieldId address
      if (opnd0->GetMeOp() == kMeOpIvar && cvtmeexpr->GetOpndType() == opnd0->GetPrimType() &&
          IsPrimitiveInteger(opnd0->GetPrimType()) && IsPrimitiveInteger(cvtmeexpr->GetPrimType()) &&
          GetPrimTypeSize(opnd0->GetPrimType()) == GetPrimTypeSize(cvtmeexpr->GetPrimType())) {
        IvarMeExpr *ivar = static_cast<IvarMeExpr*>(opnd0);
        auto resultPrimType = ivar->GetType()->GetPrimType();
        if (IsPrimitiveInteger(resultPrimType)) {
          IvarMeExpr newIvar(kInvalidExprID, *ivar);
          newIvar.SetPtyp(cvtmeexpr->GetPrimType());
          return HashMeExpr(newIvar);
        }
        return nullptr;
      }
      if (opnd0->GetOp() == OP_cvt) {
        OpMeExpr *cvtopnd0 = static_cast<OpMeExpr *>(opnd0);
        // cvtopnd0 should have tha same type as cvtopnd0->GetOpnd(0) or cvtmeexpr,
        // and the type size of cvtopnd0 should be ge(>=) one of them.
        // Otherwise, deleting the cvt of cvtopnd0 may result in information loss.
        if (maple::IsSignedInteger(cvtmeexpr->GetOpndType()) != maple::IsSignedInteger(cvtopnd0->GetOpndType())) {
          return nullptr;
        }
        if (maple::GetPrimTypeSize(cvtopnd0->GetPrimType()) >=
            maple::GetPrimTypeSize(cvtopnd0->GetOpnd(0)->GetPrimType())) {
          if ((maple::IsPrimitiveInteger(cvtopnd0->GetPrimType()) &&
               maple::IsPrimitiveInteger(cvtopnd0->GetOpnd(0)->GetPrimType()) &&
               !maple::IsPrimitiveFloat(cvtmeexpr->GetPrimType())) ||
              (maple::IsPrimitiveFloat(cvtopnd0->GetPrimType()) &&
               maple::IsPrimitiveFloat(cvtopnd0->GetOpnd(0)->GetPrimType()))) {
            if (cvtmeexpr->GetPrimType() == cvtopnd0->GetOpndType()) {
              return cvtopnd0->GetOpnd(0);
            } else {
              if (maple::IsUnsignedInteger(cvtopnd0->GetOpndType()) ||
                  maple::GetPrimTypeSize(cvtopnd0->GetPrimType()) >= maple::GetPrimTypeSize(cvtmeexpr->GetPrimType())) {
                return CreateMeExprTypeCvt(cvtmeexpr->GetPrimType(), cvtopnd0->GetOpndType(), *cvtopnd0->GetOpnd(0));
              }
              return nullptr;
            }
          }
        }
        if (maple::GetPrimTypeSize(cvtopnd0->GetPrimType()) >= maple::GetPrimTypeSize(cvtmeexpr->GetPrimType())) {
          if ((maple::IsPrimitiveInteger(cvtopnd0->GetPrimType()) &&
               maple::IsPrimitiveInteger(cvtmeexpr->GetPrimType()) &&
               !maple::IsPrimitiveFloat(cvtopnd0->GetOpnd(0)->GetPrimType())) ||
              (maple::IsPrimitiveFloat(cvtopnd0->GetPrimType()) &&
               maple::IsPrimitiveFloat(cvtmeexpr->GetPrimType()))) {
            return CreateMeExprTypeCvt(cvtmeexpr->GetPrimType(), cvtopnd0->GetOpndType(), *cvtopnd0->GetOpnd(0));
          }
        }
        // simplify "cvt type1 type2 (cvt type2 type3 (expr ))" to "cvt type1 type3 expr"
        auto typeA = cvtopnd0->GetOpnd(0)->GetPrimType();
        auto typeB = cvtopnd0->GetPrimType();
        auto typeC = opmeexpr->GetPrimType();
        if (IgnoreInnerTypeCvt(typeA, typeB, typeC)) {
          return CreateMeExprTypeCvt(typeC, typeA, utils::ToRef(cvtopnd0->GetOpnd(0)));
        }
      }
      return nullptr;
    }
    case OP_add: {
      if (!IsPrimitiveInteger(opmeexpr->GetPrimType())) {
        return nullptr;
      }
      return SimplifyAddExpr(opmeexpr);
    }
    case OP_mul: {
      if (!IsPrimitiveInteger(opmeexpr->GetPrimType())) {
        return nullptr;
      }
      return SimplifyMulExpr(opmeexpr);
    }
    case OP_sub:
    case OP_div:
    case OP_rem:
    case OP_ashr:
    case OP_lshr:
    case OP_shl:
    case OP_max:
    case OP_min:
    case OP_band:
    case OP_bior:
    case OP_bxor:
    case OP_cand:
    case OP_land:
    case OP_cior:
    case OP_lior:
    case OP_depositbits: {
      if (!IsPrimitiveInteger(opmeexpr->GetPrimType())) {
        return nullptr;
      }
      MeExpr *opnd0 = opmeexpr->GetOpnd(0);
      MeExpr *opnd1 = opmeexpr->GetOpnd(1);
      if (opop == OP_sub && opnd0 == opnd1) {
        return CreateIntConstMeExpr(0, opmeexpr->GetPrimType());
      }
      if (opnd0->GetMeOp() != kMeOpConst || opnd1->GetMeOp() != kMeOpConst) {
        return nullptr;
      }
      maple::ConstantFold cf(mirModule);
      MIRIntConst *opnd0const = static_cast<MIRIntConst *>(static_cast<ConstMeExpr *>(opnd0)->GetConstVal());
      MIRIntConst *opnd1const = static_cast<MIRIntConst *>(static_cast<ConstMeExpr *>(opnd1)->GetConstVal());
      if ((opop == OP_div || opop == OP_rem)) {
        int64 opnd0constValue = opnd0const->GetValue();
        int64 opnd1constValue = opnd1const->GetValue();
        PrimType resPtyp = opmeexpr->GetPrimType();
        if (opnd1constValue == 0 ||
            (opnd1constValue == -1 && ((resPtyp == PTY_i32 && opnd0constValue == INT32_MIN) ||
                                       (resPtyp == PTY_i64 && opnd0constValue == INT64_MIN)))) {
          return nullptr;
        }
      }
      MIRConst *resconst = cf.FoldIntConstBinaryMIRConst(opmeexpr->GetOp(),
          opmeexpr->GetPrimType(), opnd0const, opnd1const);
      return CreateConstMeExpr(opmeexpr->GetPrimType(), *resconst);
    }
    case OP_ne:
    case OP_eq:
    case OP_lt:
    case OP_le:
    case OP_ge:
    case OP_gt:
    case OP_cmp:
    case OP_cmpl:
    case OP_cmpg: {
      MeExpr *opnd0 = opmeexpr->GetOpnd(0);
      MeExpr *opnd1 = opmeexpr->GetOpnd(1);
      if (opnd0 == opnd1) {
        // node compared with itself
        auto *cmpExpr = SimplifyCompareSameExpr(opmeexpr);
        if (cmpExpr != nullptr) {
          return cmpExpr;
        }
      }
      bool isneeq = (opop == OP_ne || opop == OP_eq);
      if (opnd0->GetMeOp() == kMeOpConst && opnd1->GetMeOp() == kMeOpConst) {
        maple::ConstantFold cf(mirModule);
        MIRConst *opnd0const = static_cast<ConstMeExpr *>(opnd0)->GetConstVal();
        MIRConst *opnd1const = static_cast<ConstMeExpr *>(opnd1)->GetConstVal();
        MIRConst *resconst = cf.FoldConstComparisonMIRConst(opmeexpr->GetOp(), opmeexpr->GetPrimType(),
                                                            opmeexpr->GetOpndType(), *opnd0const, *opnd1const);
        return CreateConstMeExpr(opmeexpr->GetPrimType(), *resconst);
      } else if (isneeq && ((opnd0->GetMeOp() == kMeOpAddrof && opnd1->GetMeOp() == kMeOpConst) ||
                            (opnd0->GetMeOp() == kMeOpConst && opnd1->GetMeOp() == kMeOpAddrof))) {
        MIRConst *resconst = nullptr;
        if (opnd0->GetMeOp() == kMeOpAddrof) {
          MIRConst *constopnd1 = static_cast<ConstMeExpr *>(opnd1)->GetConstVal();
          if (constopnd1->IsZero()) {
            // addrof will not be zero, so this comparison can be replaced with a constant
            resconst = mirModule.GetMemPool()->New<MIRIntConst>(
                (opop == OP_ne), *GlobalTables::GetTypeTable().GetTypeTable()[PTY_u1]);
          }
        } else {
          MIRConst *constopnd0 = static_cast<ConstMeExpr *>(opnd0)->GetConstVal();
          if (constopnd0->IsZero()) {
            // addrof will not be zero, so this comparison can be replaced with a constant
            resconst = mirModule.GetMemPool()->New<MIRIntConst>(
                (opop == OP_ne), *GlobalTables::GetTypeTable().GetTypeTable()[PTY_u1]);
          }
        }
        if (resconst) {
          return CreateConstMeExpr(opmeexpr->GetPrimType(), *resconst);
        }
      } else if (isneeq && opnd0->GetOp() == OP_select &&
                 (opnd1->GetMeOp() == kMeOpConst && IsPrimitivePureScalar(opnd1->GetPrimType()))) {
        OpMeExpr *opmeopnd0 = static_cast<OpMeExpr *>(opnd0);
        if (opmeopnd0->GetOp() == OP_select) {
          MeExpr *opnd01 = opmeopnd0->GetOpnd(1);
          MeExpr *opnd02 = opmeopnd0->GetOpnd(2);
          if (opnd01->GetMeOp() == kMeOpConst && IsPrimitivePureScalar(opnd01->GetPrimType()) &&
              opnd02->GetMeOp() == kMeOpConst && IsPrimitivePureScalar(opnd02->GetPrimType())) {
            MIRConst *constopnd1 = static_cast<ConstMeExpr *>(opnd1)->GetConstVal();
            MIRConst *constopnd01 = static_cast<ConstMeExpr *>(opnd01)->GetConstVal();
            MIRConst *constopnd02 = static_cast<ConstMeExpr *>(opnd02)->GetConstVal();
            bool needswapopnd = false;
            bool canbereplaced = false;
            bool isne = opmeexpr->GetOp() == OP_ne;
            if (isne && constopnd1->IsZero() && constopnd01->IsOne() && constopnd02->IsZero()) {
              canbereplaced = true;
            } else if (isne && constopnd1->IsZero() && constopnd01->IsZero() && constopnd02->IsOne()) {
              canbereplaced = true;
            } else if (isne && constopnd1->IsOne() && constopnd01->IsOne() && constopnd02->IsZero()) {
              needswapopnd = true;
              canbereplaced = true;
            } else if (isne && constopnd1->IsOne() && constopnd01->IsZero() && constopnd02->IsOne()) {
              needswapopnd = true;
              canbereplaced = true;
            } else if (!isne && constopnd1->IsZero() && constopnd01->IsOne() && constopnd02->IsZero()) {
              needswapopnd = true;
              canbereplaced = true;
            } else if (!isne && constopnd1->IsZero() && constopnd01->IsZero() && constopnd02->IsOne()) {
              needswapopnd = true;
              canbereplaced = true;
            } else if (!isne && constopnd1->IsOne() && constopnd01->IsOne() && constopnd02->IsZero()) {
              canbereplaced = true;
            } else if (!isne && constopnd1->IsOne() && constopnd01->IsZero() && constopnd02->IsOne()) {
              canbereplaced = true;
            }
            if (canbereplaced) {
              constexpr uint8 numOfOpnds = 3;
              OpMeExpr newopmeexpr(kInvalidExprID, OP_select, PTY_u1, numOfOpnds);
              newopmeexpr.SetOpnd(0, opmeopnd0->GetOpnd(0));
              ConstMeExpr xnewopnd01(kInvalidExprID, constopnd01, PTY_u1);
              MeExpr *newopnd01 = HashMeExpr(xnewopnd01);
              ConstMeExpr xnewopnd02(kInvalidExprID, constopnd02, PTY_u1);
              MeExpr *newopnd02 = HashMeExpr(xnewopnd02);
              if (needswapopnd) {
                newopmeexpr.SetOpnd(1, newopnd02);
                newopmeexpr.SetOpnd(2, newopnd01);
              } else {
                newopmeexpr.SetOpnd(1, newopnd01);
                newopmeexpr.SetOpnd(2, newopnd02);
              }
              return HashMeExpr(newopmeexpr);
            }
          }
        }
      } else if (isneeq && opnd0->GetOp() == OP_cmp && opnd1->GetMeOp() == kMeOpConst) {
        auto *constVal = static_cast<ConstMeExpr*>(opnd1)->GetConstVal();
        if (constVal->GetKind() == kConstInt && constVal->IsZero()) {
          auto *subOpnd0 = opnd0->GetOpnd(0);
          auto *subOpnd1 = opnd0->GetOpnd(1);
          return CreateMeExprCompare(opop, PTY_u1, subOpnd0->GetPrimType(), *subOpnd0, *subOpnd1);
        }
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

MeExpr *IRMap::SimplifyMeExpr(MeExpr *x) {
  switch (x->GetMeOp()) {
    case kMeOpAddrof:
    case kMeOpAddroffunc:
    case kMeOpConst:
    case kMeOpConststr:
    case kMeOpConststr16:
    case kMeOpSizeoftype:
    case kMeOpFieldsDist:
    case kMeOpVar:
    case kMeOpReg:
    case kMeOpIvar: return x;
    case kMeOpOp: {
      OpMeExpr *opexp = static_cast<OpMeExpr *>(x);
      opexp->SetOpnd(0, SimplifyMeExpr(opexp->GetOpnd(0)));
      if (opexp->GetNumOpnds() > 1) {
        opexp->SetOpnd(1, SimplifyMeExpr(opexp->GetOpnd(1)));
        if (opexp->GetNumOpnds() > 2) {
          opexp->SetOpnd(2, SimplifyMeExpr(opexp->GetOpnd(2)));
        }
      }
      MeExpr *newexp = SimplifyOpMeExpr(opexp);
      if (newexp != nullptr) {
        return newexp;
      }
      return opexp;
    }
    case kMeOpNary: {
      NaryMeExpr *opexp = static_cast<NaryMeExpr *>(x);
      for (uint32 i = 0; i < opexp->GetNumOpnds(); i++) {
        opexp->SetOpnd(i, SimplifyMeExpr(opexp->GetOpnd(i)));
      }
      // TODO do the simplification of this op
      return opexp;
    }
    default:
      break;
  }
  return x;
}
}  // namespace maple
