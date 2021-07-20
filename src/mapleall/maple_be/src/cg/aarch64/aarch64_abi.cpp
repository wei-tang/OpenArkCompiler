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
#include "aarch64_cgfunc.h"
#include "becommon.h"

namespace maplebe {
using namespace maple;

namespace {
constexpr int kMaxRegCount = 4;

int32 ProcessNonStructAndNonArrayWhenClassifyAggregate(const MIRType &mirType,
                                                       AArch64ArgumentClass classes[kMaxRegCount],
                                                       size_t classesLength) {
  CHECK_FATAL(classesLength > 0, "classLength must > 0");
  /* scalar type */
  switch (mirType.GetPrimType()) {
    case PTY_u1:
    case PTY_u8:
    case PTY_i8:
    case PTY_u16:
    case PTY_i16:
    case PTY_a32:
    case PTY_u32:
    case PTY_i32:
    case PTY_a64:
    case PTY_ptr:
    case PTY_ref:
    case PTY_u64:
    case PTY_i64:
      classes[0] = kAArch64IntegerClass;
      return 1;
    case PTY_f32:
    case PTY_f64:
    case PTY_c64:
    case PTY_c128:
      classes[0] = kAArch64FloatClass;
      return 1;
    default:
      CHECK_FATAL(false, "NYI");
  }

  /* should not reach to this point */
  return 0;
}

PrimType TraverseStructFieldsForFp(MIRType *ty, uint32 &numRegs) {
  if (ty->GetKind() == kTypeArray) {
    MIRArrayType *arrtype = static_cast<MIRArrayType *>(ty);
    MIRType *pty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(arrtype->GetElemTyIdx());
    if (pty->GetKind() == kTypeArray || pty->GetKind() == kTypeStruct) {
      return TraverseStructFieldsForFp(pty, numRegs);
    }
    for (uint32 i = 0; i < arrtype->GetDim(); ++i) {
      numRegs += arrtype->GetSizeArrayItem(i);
    }
    return pty->GetPrimType();
  } else if (ty->GetKind() == kTypeStruct) {
    MIRStructType *sttype = static_cast<MIRStructType *>(ty);
    FieldVector fields = sttype->GetFields();
    PrimType oldtype = PTY_void;
    for (uint32 fcnt = 0; fcnt < fields.size(); ++fcnt) {
      TyIdx fieldtyidx = fields[fcnt].second.first;
      MIRType *fieldty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldtyidx);
      PrimType ptype = TraverseStructFieldsForFp(fieldty, numRegs);
      if (oldtype != PTY_void && oldtype != ptype) {
        return PTY_void;
      } else {
        oldtype = ptype;
      }
    }
    return oldtype;
  } else {
    numRegs++;
    return ty->GetPrimType();
  }
}

int32 ClassifyAggregate(const BECommon &be, MIRType &mirType, AArch64ArgumentClass classes[kMaxRegCount],
                        size_t classesLength, uint32 &fpSize);

uint32 ProcessStructWhenClassifyAggregate(const BECommon &be, MIRStructType &structType,
                                          AArch64ArgumentClass classes[kMaxRegCount],
                                          size_t classesLength, uint32 &fpSize) {
  CHECK_FATAL(classesLength > 0, "classLength must > 0");
  uint32 sizeOfTyInDwords = RoundUp(be.GetTypeSize(structType.GetTypeIndex()), k8ByteSize) >> k8BitShift;
  bool isF32 = false;
  bool isF64 = false;
  uint32 numRegs = 0;
  for (uint32 f = 0; f < structType.GetFieldsSize(); ++f) {
    TyIdx fieldTyIdx = structType.GetFieldsElemt(f).second.first;
    MIRType *fieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldTyIdx);
    PrimType pType = TraverseStructFieldsForFp(fieldType, numRegs);
    if (pType == PTY_f32) {
      if (isF64) {
        isF64 = false;
        break;
      }
      isF32 = true;
    } else if (pType == PTY_f64) {
      if (isF32) {
        isF32 = false;
        break;
      }
      isF64 = true;
    } else if (IsPrimitiveVector(pType)) {
      isF64 = true;
      break;
    } else {
      isF32 = isF64 = false;
      break;
    }
  }
  if (isF32 || isF64) {
    for (uint32 i = 0; i < numRegs; ++i) {
      classes[i] = kAArch64FloatClass;
    }
    fpSize = isF32 ? k4ByteSize : k8ByteSize;
    return numRegs;
  }

  classes[0] = kAArch64IntegerClass;
  if (sizeOfTyInDwords == kDwordSizeTwo) {
    classes[1] = kAArch64IntegerClass;
  }
  return sizeOfTyInDwords;
}

/*
 * Analyze the given aggregate using the rules given by the ARM 64-bit ABI and
 * return the number of doublewords to be passed in registers; the classes of
 * the doublewords are returned in parameter "classes"; if 0 is returned, it
 * means the whole aggregate is passed in memory.
 */
int32 ClassifyAggregate(const BECommon &be, MIRType &mirType, AArch64ArgumentClass classes[kMaxRegCount],
                        size_t classesLength, uint32 &fpSize) {
  CHECK_FATAL(classesLength > 0, "invalid index");
  uint64 sizeOfTy = be.GetTypeSize(mirType.GetTypeIndex());
  /* Rule B.3.
   * If the argument type is a Composite Type that is larger than 16 bytes
   * then the argument is copied to memory allocated by the caller and
   * the argument is replaced by a pointer to the copy.
   */
  if ((sizeOfTy > k16ByteSize) || (sizeOfTy == 0)) {
    return 0;
  }

  /*
   * An argument of any Integer class takes up an integer register
   * which is a single double-word.
   * Rule B.4. The size of an argument of composite type is rounded up to the nearest
   * multiple of 8 bytes.
   */
  int32 sizeOfTyInDwords = RoundUp(sizeOfTy, k8ByteSize) >> k8BitShift;
  ASSERT(sizeOfTyInDwords > 0, "sizeOfTyInDwords should be sizeOfTyInDwords > 0");
  ASSERT(sizeOfTyInDwords <= kTwoRegister, "sizeOfTyInDwords should be <= 2");
  int32 i;
  for (i = 0; i < sizeOfTyInDwords; ++i) {
    classes[i] = kAArch64NoClass;
  }
  if ((mirType.GetKind() != kTypeStruct) && (mirType.GetKind() != kTypeArray) && (mirType.GetKind() != kTypeUnion)) {
    return ProcessNonStructAndNonArrayWhenClassifyAggregate(mirType, classes, classesLength);
  }
  if (mirType.GetKind() == kTypeStruct) {
    MIRStructType &structType = static_cast<MIRStructType&>(mirType);
    return ProcessStructWhenClassifyAggregate(be, structType, classes, classesLength, fpSize);
  }
  /* post merger clean-up */
  for (i = 0; i < sizeOfTyInDwords; ++i) {
    if (classes[i] == kAArch64MemoryClass) {
      return 0;
    }
  }
  return sizeOfTyInDwords;
}
}

namespace AArch64Abi {
bool IsAvailableReg(AArch64reg reg) {
  switch (reg) {
/* integer registers */
#define INT_REG(ID, PREF32, PREF64, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case R##ID:                                                                                  \
      return canBeAssigned;
#define INT_REG_ALIAS(ALIAS, ID, PREF32, PREF64)
#include "aarch64_int_regs.def"
#undef INT_REG
#undef INT_REG_ALIAS
/* fp-simd registers */
#define FP_SIMD_REG(ID, PV, P8, P16, P32, P64, P128, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case V##ID:                                                                                                   \
      return canBeAssigned;
#define FP_SIMD_REG_ALIAS(ID)
#include "aarch64_fp_simd_regs.def"
#undef FP_SIMD_REG
#undef FP_SIMD_REG_ALIAS
    default:
      return false;
  }
}

bool IsCalleeSavedReg(AArch64reg reg) {
  switch (reg) {
/* integer registers */
#define INT_REG(ID, PREF32, PREF64, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case R##ID:                                                                                  \
      return isCalleeSave;
#define INT_REG_ALIAS(ALIAS, ID, PREF32, PREF64)
#include "aarch64_int_regs.def"
#undef INT_REG
#undef INT_REG_ALIAS
/* fp-simd registers */
#define FP_SIMD_REG(ID, PV, P8, P16, P32, P64, P128, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case V##ID:                                                                                                   \
      return isCalleeSave;
#define FP_SIMD_REG_ALIAS(ID)
#include "aarch64_fp_simd_regs.def"
#undef FP_SIMD_REG
#undef FP_SIMD_REG_ALIAS
    default:
      return false;
  }
}

bool IsParamReg(AArch64reg reg) {
  switch (reg) {
/* integer registers */
#define INT_REG(ID, PREF32, PREF64, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case R##ID:                                                                                  \
      return isParam;
#define INT_REG_ALIAS(ALIAS, ID, PREF32, PREF64)
#include "aarch64_int_regs.def"
#undef INT_REG
#undef INT_REG_ALIAS
/* fp-simd registers */
#define FP_SIMD_REG(ID, PV, P8, P16, P32, P64, P128, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case V##ID:                                                                                                   \
      return isParam;
#define FP_SIMD_REG_ALIAS(ID)
#include "aarch64_fp_simd_regs.def"
#undef FP_SIMD_REG
#undef FP_SIMD_REG_ALIAS
    default:
      return false;
  }
}

bool IsSpillReg(AArch64reg reg) {
  switch (reg) {
/* integer registers */
#define INT_REG(ID, PREF32, PREF64, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case R##ID:                                                                                  \
      return isSpill;
#define INT_REG_ALIAS(ALIAS, ID, PREF32, PREF64)
#include "aarch64_int_regs.def"
#undef INT_REG
#undef INT_REG_ALIAS
/* fp-simd registers */
#define FP_SIMD_REG(ID, PV, P8, P16, P32, P64, P128, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case V##ID:                                                                                                   \
      return isSpill;
#define FP_SIMD_REG_ALIAS(ID)
#include "aarch64_fp_simd_regs.def"
#undef FP_SIMD_REG
#undef FP_SIMD_REG_ALIAS
    default:
      return false;
  }
}

bool IsExtraSpillReg(AArch64reg reg) {
  switch (reg) {
/* integer registers */
#define INT_REG(ID, PREF32, PREF64, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case R##ID:                                                                                  \
      return isExtraSpill;
#define INT_REG_ALIAS(ALIAS, ID, PREF32, PREF64)
#include "aarch64_int_regs.def"
#undef INT_REG
#undef INT_REG_ALIAS
/* fp-simd registers */
#define FP_SIMD_REG(ID, PV, P8, P16, P32, P64, P128, canBeAssigned, isCalleeSave, isParam, isSpill, isExtraSpill) \
    case V##ID:                                                                                                   \
      return isExtraSpill;
#define FP_SIMD_REG_ALIAS(ID)
#include "aarch64_fp_simd_regs.def"
#undef FP_SIMD_REG
#undef FP_SIMD_REG_ALIAS
    default:
      return false;
  }
}

bool IsSpillRegInRA(AArch64reg regNO, bool has3RegOpnd) {
  /* if has 3 RegOpnd, previous reg used to spill. */
  if (has3RegOpnd) {
    return AArch64Abi::IsSpillReg(regNO) || AArch64Abi::IsExtraSpillReg(regNO);
  }
  return AArch64Abi::IsSpillReg(regNO);
}

PrimType IsVectorArrayType(MIRType *ty, int &arraySize) {
  if (ty->GetKind() == kTypeStruct) {
    MIRStructType *structTy = static_cast<MIRStructType *>(ty);
    if (structTy->GetFields().size() == 1) {
      auto fieldPair = structTy->GetFields()[0];
      MIRType *fieldTy = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldPair.second.first);
      if (fieldTy->GetKind() == kTypeArray) {
        MIRArrayType *arrayTy = static_cast<MIRArrayType *>(fieldTy);
        MIRType *arrayElemTy = arrayTy->GetElemType();
        arraySize = arrayTy->GetSizeArrayItem(0);
        if (arrayTy->GetDim() == k1BitSize && arraySize <= k4BitSize &&
            IsPrimitiveVector(arrayElemTy->GetPrimType())) {
          return arrayElemTy->GetPrimType();
        }
      }
    }
  }
  return PTY_void;
}
}  /* namespace AArch64Abi */

void ParmLocator::InitPLocInfo(PLocInfo &pLoc) const {
  pLoc.reg0 = kRinvalid;
  pLoc.reg1 = kRinvalid;
  pLoc.reg2 = kRinvalid;
  pLoc.reg3 = kRinvalid;
  pLoc.memOffset = nextStackArgAdress;
  pLoc.fpSize = 0;
  pLoc.numFpPureRegs = 0;
}

int32 ParmLocator::LocateRetVal(MIRType &retType, PLocInfo &pLoc) {
  InitPLocInfo(pLoc);
  uint32 retSize = beCommon.GetTypeSize(retType.GetTypeIndex().GetIdx());
  if (retSize == 0) {
    return 0;    /* size 0 ret val */
  }
  if (retSize <= k16ByteSize) {
    /* For return struct size less or equal to 16 bytes, the values */
    /* are returned in register pairs. */
    AArch64ArgumentClass classes[kMaxRegCount] = { kAArch64NoClass }; /* Max of four floats. */
    uint32 fpSize;
    uint32 numRegs = static_cast<uint32>(ClassifyAggregate(beCommon, retType, classes, sizeof(classes), fpSize));
    if (classes[0] == kAArch64FloatClass) {
      CHECK_FATAL(numRegs <= kMaxRegCount, "LocateNextParm: illegal number of regs");
      AllocateNSIMDFPRegisters(pLoc, numRegs);
      pLoc.numFpPureRegs = numRegs;
      pLoc.fpSize = fpSize;
      return 0;
    } else {
      CHECK_FATAL(numRegs <= kTwoRegister, "LocateNextParm: illegal number of regs");
      if (numRegs == kOneRegister) {
        pLoc.reg0 = AllocateGPRegister();
      } else {
        AllocateTwoGPRegisters(pLoc);
      }
      return 0;
    }
  } else {
    /* For return struct size > 16 bytes the pointer returns in x8. */
    pLoc.reg0 = R8;
    return kSizeOfPtr;
  }
}

/*
 * Refer to ARM IHI 0055C_beta: Procedure Call Standard for
 * the ARM 64-bit Architecture. $5.4.2
 *
 * For internal only functions, we may want to implement
 * our own rules as Apple IOS has done. Maybe we want to
 * generate two versions for each of externally visible functions,
 * one conforming to the ARM standard ABI, and the other for
 * internal only use.
 *
 * LocateNextParm should be called with each parameter in the parameter list
 * starting from the beginning, one call per parameter in sequence; it returns
 * the information on how each parameter is passed in pLoc
 */
int32 ParmLocator::LocateNextParm(MIRType &mirType, PLocInfo &pLoc, bool isFirst) {
  InitPLocInfo(pLoc);

  if (isFirst) {
    MIRFunction *func = const_cast<MIRFunction *>(beCommon.GetMIRModule().CurFunction());
    if (beCommon.HasFuncReturnType(*func)) {
      uint32 size = beCommon.GetTypeSize(beCommon.GetFuncReturnType(*func));
      if (size == 0) {
        /* For return struct size 0 there is no return value. */
        return 0;
      } else if (size > k16ByteSize) {
        /* For return struct size > 16 bytes the pointer returns in x8. */
        pLoc.reg0 = R8;
        return kSizeOfPtr;
      }
      /* For return struct size less or equal to 16 bytes, the values
       * are returned in register pairs.
       * Check for pure float struct.
       */
      AArch64ArgumentClass classes[kMaxRegCount] = { kAArch64NoClass };
      uint32 fpSize;
      MIRType *retType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(beCommon.GetFuncReturnType(*func));
      uint32 numRegs = static_cast<uint32>(ClassifyAggregate(beCommon, *retType, classes, sizeof(classes), fpSize));
      if (classes[0] == kAArch64FloatClass) {
        CHECK_FATAL(numRegs <= kMaxRegCount, "LocateNextParm: illegal number of regs");
        AllocateNSIMDFPRegisters(pLoc, numRegs);
        pLoc.numFpPureRegs = numRegs;
        pLoc.fpSize = fpSize;
        return 0;
      } else {
        CHECK_FATAL(numRegs <= kTwoRegister, "LocateNextParm: illegal number of regs");
        if (numRegs == kOneRegister) {
          pLoc.reg0 = AllocateGPRegister();
        } else {
          AllocateTwoGPRegisters(pLoc);
        }
        return 0;
      }
    }
  }
  uint64 typeSize = beCommon.GetTypeSize(mirType.GetTypeIndex());
  if (typeSize == 0) {
    return 0;
  }
  int32 typeAlign = beCommon.GetTypeAlign(mirType.GetTypeIndex());
  /*
   * Rule C.12 states that we do round nextStackArgAdress up before we use its value
   * according to the alignment requirement of the argument being processed.
   * We do the rounding up at the end of LocateNextParm(),
   * so we want to make sure our rounding up is correct.
   */
  ASSERT((nextStackArgAdress & (std::max(typeAlign, static_cast<int32>(k8ByteSize)) - 1)) == 0,
         "C.12 alignment requirement is violated");
  pLoc.memSize = static_cast<int32>(typeSize);
  ++paramNum;

  int32 aggCopySize = 0;
  switch (mirType.GetPrimType()) {
    case PTY_u1:
    case PTY_u8:
    case PTY_i8:
    case PTY_u16:
    case PTY_i16:
    case PTY_a32:
    case PTY_u32:
    case PTY_i32:
    case PTY_ptr:
    case PTY_ref:
    case PTY_a64:
    case PTY_u64:
    case PTY_i64:
      /* Rule C.7 */
      typeSize = k8ByteSize;
      pLoc.reg0 = AllocateGPRegister();
      ASSERT(nextGeneralRegNO <= AArch64Abi::kNumIntParmRegs, "RegNo should be pramRegNO");
      break;
    /*
     * for c64 complex numbers, we assume
     * - callers marshall the two f32 numbers into one f64 register
     * - callees de-marshall one f64 value into the real and the imaginery part
     */
    case PTY_f32:
    case PTY_f64:
    case PTY_c64:
    case PTY_v2i32:
    case PTY_v4i16:
    case PTY_v8i8:
    case PTY_v2u32:
    case PTY_v4u16:
    case PTY_v8u8:
    case PTY_v2f32:
      /* Rule C.1 */
      ASSERT(GetPrimTypeSize(PTY_f64) == k8ByteSize, "unexpected type size");
      typeSize = k8ByteSize;
      pLoc.reg0 = AllocateSIMDFPRegister();
      break;
    /*
     * for c128 complex numbers, we assume
     * - callers marshall the two f64 numbers into one f128 register
     * - callees de-marshall one f128 value into the real and the imaginery part
     */
    case PTY_c128:
    case PTY_v2i64:
    case PTY_v4i32:
    case PTY_v8i16:
    case PTY_v16i8:
    case PTY_v2u64:
    case PTY_v4u32:
    case PTY_v8u16:
    case PTY_v16u8:
    case PTY_v2f64:
    case PTY_v4f32:
      /* SIMD-FP registers have 128-bits. */
      pLoc.reg0 = AllocateSIMDFPRegister();
      ASSERT(nextFloatRegNO <= AArch64Abi::kNumFloatParmRegs, "regNO should not be greater than kNumFloatParmRegs");
      ASSERT(typeSize == k16ByteSize, "unexpected type size");
      break;
    /*
     * case of quad-word integer:
     * we don't support java yet.
     * if (has-16-byte-alignment-requirement)
     * nextGeneralRegNO = (nextGeneralRegNO+1) & ~1; // C.8 round it up to the next even number
     * try allocate two consecutive registers at once.
     */
    /* case PTY_agg */
    case PTY_agg: {
      aggCopySize = ProcessPtyAggWhenLocateNextParm(mirType, pLoc, typeSize, typeAlign);
      break;
    }
    default:
      CHECK_FATAL(false, "NYI");
  }

  /* Rule C.12 */
  if (pLoc.reg0 == kRinvalid) {
    /* being passed in memory */
    nextStackArgAdress = pLoc.memOffset + typeSize;
  }
  return aggCopySize;
}

int32 ParmLocator::ProcessPtyAggWhenLocateNextParm(MIRType &mirType, PLocInfo &pLoc, uint64 &typeSize,
                                                   int32 typeAlign) {
  /*
   * In AArch64, integer-float or float-integer
   * argument passing is not allowed. All should go through
   * integer-integer.
   * In the case where a struct is homogeneous composed of one of the fp types,
   * either all single fp or all double fp, then it can be passed by float-float.
   */
  AArch64ArgumentClass classes[kMaxRegCount] = { kAArch64NoClass };
  typeSize = beCommon.GetTypeSize(mirType.GetTypeIndex().GetIdx());
  int32 aggCopySize = 0;
  if (typeSize > k16ByteSize) {
    aggCopySize = RoundUp(typeSize, kSizeOfPtr);
  }
  /*
   * alignment requirement
   * Note. This is one of a few things iOS diverges from
   * the ARM 64-bit standard. They don't observe the round-up requirement.
   */
  if (typeAlign == k16ByteSize) {
    RoundNGRNUpToNextEven();
  }

  uint32 fpSize;
  uint32 numRegs = static_cast<uint32>(
      ClassifyAggregate(beCommon, mirType, classes, sizeof(classes) / sizeof(AArch64ArgumentClass), fpSize));
  if (classes[0] == kAArch64FloatClass) {
    CHECK_FATAL(numRegs <= kMaxRegCount, "LocateNextParm: illegal number of regs");
    typeSize = k8ByteSize;
    AllocateNSIMDFPRegisters(pLoc, numRegs);
    pLoc.numFpPureRegs = numRegs;
    pLoc.fpSize = fpSize;
  } else if (numRegs == 1) {
    /* passing in registers */
    typeSize = k8ByteSize;
    if (classes[0] == kAArch64FloatClass) {
      CHECK_FATAL(false, "param passing in FP reg not allowed here");
    } else {
      pLoc.reg0 = AllocateGPRegister();
      /* Rule C.11 */
      ASSERT((pLoc.reg0 != kRinvalid) || (nextGeneralRegNO == AArch64Abi::kNumIntParmRegs),
             "reg0 should not be kRinvalid or nextGeneralRegNO should equal kNumIntParmRegs");
    }
  } else if (numRegs == kTwoRegister) {
    /* Other aggregates with 8 < size <= 16 bytes can be allocated in reg pair */
    ASSERT(classes[0] == kAArch64IntegerClass || classes[0] == kAArch64NoClass,
           "classes[0] must be either integer class or no class");
    ASSERT(classes[1] == kAArch64IntegerClass || classes[1] == kAArch64NoClass,
           "classes[1] must be either integer class or no class");
    AllocateTwoGPRegisters(pLoc);
    /* Rule C.11 */
    if (pLoc.reg0 == kRinvalid) {
      nextGeneralRegNO = AArch64Abi::kNumIntParmRegs;
    }
  } else {
    /*
     * 0 returned from ClassifyAggregate(). This means the whole data
     * is passed thru memory.
     * Rule B.3.
     *  If the argument type is a Composite Type that is larger than 16
     *  bytes then the argument is copied to memory allocated by the
     *  caller and the argument is replaced by a pointer to the copy.
     *
     * Try to allocate an integer register
     */
    typeSize = k8ByteSize;
    pLoc.reg0 = AllocateGPRegister();
    pLoc.memSize = k8ByteSize;  /* byte size of a pointer in AArch64 */
    if (pLoc.reg0 != kRinvalid) {
      numRegs = 1;
    }
  }
  /* compute rightpad */
  if ((numRegs == 0) || (pLoc.reg0 == kRinvalid)) {
    /* passed in memory */
    typeSize = RoundUp(pLoc.memSize, k8ByteSize);
  }
  return aggCopySize;
}

/*
 * instantiated with the type of the function return value, it describes how
 * the return value is to be passed back to the caller
 *
 *  Refer to ARM IHI 0055C_beta: Procedure Call Standard for
 *  the ARM 64-bit Architecture. $5.5
 *  "If the type, T, of the result of a function is such that
 *     void func(T arg)
 *   would require that 'arg' be passed as a value in a register
 *   (or set of registers) according to the rules in $5.4 Parameter
 *   Passing, then the result is returned in the same registers
 *   as would be used for such an argument.
 */
ReturnMechanism::ReturnMechanism(MIRType &retTy, const BECommon &be)
    : regCount(0), reg0(kRinvalid), reg1(kRinvalid), primTypeOfReg0(kPtyInvalid), primTypeOfReg1(kPtyInvalid) {
  PrimType pType = retTy.GetPrimType();
  switch (pType) {
    case PTY_void:
      break;
    case PTY_u1:
    case PTY_u8:
    case PTY_i8:
    case PTY_u16:
    case PTY_i16:
    case PTY_a32:
    case PTY_u32:
    case PTY_i32:
      regCount = 1;
      reg0 = AArch64Abi::intReturnRegs[0];
      primTypeOfReg0 = IsSignedInteger(pType) ? PTY_i32 : PTY_u32;  /* promote the type */
      return;

    case PTY_ptr:
    case PTY_ref:
      CHECK_FATAL(false, "PTY_ptr should have been lowered");
      return;

    case PTY_a64:
    case PTY_u64:
    case PTY_i64:
      regCount = 1;
      reg0 = AArch64Abi::intReturnRegs[0];
      primTypeOfReg0 = IsSignedInteger(pType) ? PTY_i64 : PTY_u64;  /* promote the type */
      return;

    /*
     * for c64 complex numbers, we assume
     * - callers marshall the two f32 numbers into one f64 register
     * - callees de-marshall one f64 value into the real and the imaginery part
     */
    case PTY_f32:
    case PTY_f64:
    case PTY_c64:
    case PTY_v2i32:
    case PTY_v4i16:
    case PTY_v8i8:
    case PTY_v2u32:
    case PTY_v4u16:
    case PTY_v8u8:
    case PTY_v2f32:

    /*
     * for c128 complex numbers, we assume
     * - callers marshall the two f64 numbers into one f128 register
     * - callees de-marshall one f128 value into the real and the imaginery part
     */
    case PTY_c128:
    case PTY_v2i64:
    case PTY_v4i32:
    case PTY_v8i16:
    case PTY_v16i8:
    case PTY_v2u64:
    case PTY_v4u32:
    case PTY_v8u16:
    case PTY_v16u8:
    case PTY_v2f64:
    case PTY_v4f32:
      regCount = 1;
      reg0 = AArch64Abi::floatReturnRegs[0];
      primTypeOfReg0 = pType;
      return;

    /*
     * Refer to ARM IHI 0055C_beta: Procedure Call Standard for
     * the ARM 64-bit Architecture. $5.5
     * "Otherwise, the caller shall reserve a block of memory of
     * sufficient size and alignment to hold the result. The
     * address of the memory block shall be passed as an additional
     * argument to the function in x8. The callee may modify the
     * result memory block at any point during the execution of the
     * subroutine (there is no requirement for the callee to preserve
     * the value stored in x8)."
     */
    case PTY_agg: {
      uint64 size = be.GetTypeSize(retTy.GetTypeIndex());
      if ((size > k16ByteSize) || (size == 0)) {
        /*
         * The return value is returned via memory.
         * The address is in X8 and passed by the caller.
         */
        SetupToReturnThroughMemory();
        return;
      }
      uint32 fpSize;
      AArch64ArgumentClass classes[kMaxRegCount] = { kAArch64NoClass };
      regCount = static_cast<uint8>(ClassifyAggregate(be, retTy, classes,
                                                      sizeof(classes) / sizeof(AArch64ArgumentClass), fpSize));
      if (classes[0] == kAArch64FloatClass) {
        switch (regCount) {
          case kFourRegister:
            reg3 = AArch64Abi::floatReturnRegs[3];
            break;
          case kThreeRegister:
            reg2 = AArch64Abi::floatReturnRegs[2];
            break;
          case kTwoRegister:
            reg1 = AArch64Abi::floatReturnRegs[1];
            break;
          case kOneRegister:
            reg0 = AArch64Abi::floatReturnRegs[0];
            break;
          default:
            CHECK_FATAL(0, "ReturnMechanism: unsupported");
        }
        if (fpSize == k4ByteSize) {
          primTypeOfReg0 = primTypeOfReg1 = PTY_f32;
        } else {
          primTypeOfReg0 = primTypeOfReg1 = PTY_f64;
        }
        return;
      } else if (regCount == 0) {
        SetupToReturnThroughMemory();
        return;
      } else {
        if (regCount == 1) {
          /* passing in registers */
          if (classes[0] == kAArch64FloatClass) {
            reg0 = AArch64Abi::floatReturnRegs[0];
            primTypeOfReg0 = PTY_f64;
          } else {
            reg0 = AArch64Abi::intReturnRegs[0];
            primTypeOfReg0 = PTY_i64;
          }
        } else {
          ASSERT(regCount == kMaxRegCount, "reg count from ClassifyAggregate() should be 0, 1, or 2");
          ASSERT(classes[0] == kAArch64IntegerClass, "error val :classes[0]");
          ASSERT(classes[1] == kAArch64IntegerClass, "error val :classes[1]");
          reg0 = AArch64Abi::intReturnRegs[0];
          primTypeOfReg0 = PTY_i64;
          reg1 = AArch64Abi::intReturnRegs[1];
          primTypeOfReg1 = PTY_i64;
        }
        return;
      }
    }
    default:
      CHECK_FATAL(false, "NYI");
  }
}

void ReturnMechanism::SetupSecondRetReg(const MIRType &retTy2) {
  ASSERT(reg1 == kRinvalid, "make sure reg1 equal kRinvalid");
  PrimType pType = retTy2.GetPrimType();
  switch (pType) {
    case PTY_void:
      break;
    case PTY_u1:
    case PTY_u8:
    case PTY_i8:
    case PTY_u16:
    case PTY_i16:
    case PTY_a32:
    case PTY_u32:
    case PTY_i32:
    case PTY_ptr:
    case PTY_ref:
    case PTY_a64:
    case PTY_u64:
    case PTY_i64:
      reg1 = AArch64Abi::intReturnRegs[1];
      primTypeOfReg1 = IsSignedInteger(pType) ? PTY_i64 : PTY_u64;  /* promote the type */
      break;
    default:
      CHECK_FATAL(false, "NYI");
  }
}

/*
 * From "ARM Procedure Call Standard for ARM 64-bit Architecture"
 *     ARM IHI 0055C_beta, 6th November 2013
 * $ 5.1 machine Registers
 * $ 5.1.1 General-Purpose Registers
 *  <Table 2>                Note
 *  SP       Stack Pointer
 *  R30/LR   Link register   Stores the return address.
 *                           We push it into stack along with FP on function
 *                           entry using STP and restore it on function exit
 *                           using LDP even if the function is a leaf (i.e.,
 *                           it does not call any other function) because it
 *                           is free (we have to store FP anyway).  So, if a
 *                           function is a leaf, we may use it as a temporary
 *                           register.
 *  R29/FP   Frame Pointer
 *  R19-R28  Callee-saved
 *           registers
 *  R18      Platform reg    Can we use it as a temporary register?
 *  R16,R17  IP0,IP1         Maybe used as temporary registers. Should be
 *                           given lower priorities. (i.e., we push them
 *                           into the free register stack before the others)
 *  R9-R15                   Temporary registers, caller-saved
 *  Note:
 *  R16 and R17 may be used by a linker as a scratch register between
 *  a routine and any subroutine it calls. They can also be used within a
 *  routine to hold intermediate values between subroutine calls.
 *
 *  The role of R18 is platform specific. If a platform ABI has need of
 *  a dedicated general purpose register to carry inter-procedural state
 *  (for example, the thread context) then it should use this register for
 *  that purpose. If the platform ABI has no such requirements, then it should
 *  use R18 as an additional temporary register. The platform ABI specification
 *  must document the usage for this register.
 *
 *  A subroutine invocation must preserve the contents of the registers R19-R29
 *  and SP. All 64 bits of each value stored in R19-R29 must be preserved, even
 *  when using the ILP32 data model.
 *
 *  $ 5.1.2 SIMD and Floating-Point Registers
 *
 *  The first eight registers, V0-V7, are used to pass argument values into
 *  a subroutine and to return result values from a function. They may also
 *  be used to hold intermediate values within a routine.
 *
 *  V8-V15 must be preserved by a callee across subroutine calls; the
 *  remaining registers do not need to be preserved( or caller-saved).
 *  Additionally, only the bottom 64 bits of each value stored in V8-
 *  V15 need to be preserved.
 */
}  /* namespace maplebe */
