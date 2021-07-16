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
#include "mir_type.h"
#include <iostream>
#include <cstring>
#include "mir_symbol.h"
#include "printing.h"
#include "namemangler.h"
#include "global_tables.h"
#include "mir_builder.h"
#include "cfg_primitive_types.h"
#if MIR_FEATURE_FULL

namespace maple {
#define LOAD_PRIMARY_TYPE_PROPERTY
#include "prim_types.def"

#define LOAD_ALGO_PRIMARY_TYPE
const PrimitiveTypeProperty &GetPrimitiveTypeProperty(PrimType pType) {
  switch (pType) {
    case PTY_begin:
      return PTProperty_begin;
#define PRIMTYPE(P) \
    case PTY_##P:   \
      return PTProperty_##P;
#include "prim_types.def"
#undef PRIMTYPE
    case PTY_end:
    default:
      return PTProperty_end;
  }
}

PrimType GetRegPrimType(PrimType primType) {
  switch (primType) {
    case PTY_i8:
    case PTY_i16:
      return PTY_i32;
    case PTY_u1:
    case PTY_u8:
    case PTY_u16:
      return PTY_u32;
    default:
      return primType;
  }
}

PrimType GetReg64PrimType(PrimType primType) {
  switch (primType) {
    case PTY_i8:
    case PTY_i16:
    case PTY_i32:
      return PTY_i64;
    case PTY_u1:
    case PTY_u8:
    case PTY_u16:
    case PTY_u32:
      return PTY_u64;
    default:
      return primType;
  }
}

bool VerifyPrimType(PrimType primType1, PrimType primType2) {
  switch (primType1) {
    case PTY_u1:
    case PTY_u8:
    case PTY_u16:
    case PTY_u32:
    case PTY_a32:
      return IsUnsignedInteger(primType2);
    case PTY_i8:
    case PTY_i16:
    case PTY_i32:
      return IsSignedInteger(primType2);
    default:
      return primType1 == primType2;
  }
}

PrimType GetDynType(PrimType primType) {
#ifdef DYNAMICLANG
  switch (primType) {
    case PTY_u1:
      return PTY_dynbool;
    case PTY_i32:
      return PTY_dyni32;
    case PTY_simplestr:
      return PTY_dynstr;
    case PTY_simpleobj:
      return PTY_dynobj;
    case PTY_f32:
      return PTY_dynf32;
    case PTY_f64:
      return PTY_dynf64;
    default:
      return primType;
  }
#else
  return primType;
#endif
}

PrimType GetNonDynType(PrimType primType) {
#ifdef DYNAMICLANG
  switch (primType) {
    case PTY_dynbool:
      return PTY_u1;
    case PTY_dyni32:
      return PTY_i32;
    case PTY_dynstr:
      return PTY_simplestr;
    case PTY_dynobj:
      return PTY_simpleobj;
    case PTY_dynf32:
      return PTY_f32;
    case PTY_dynf64:
      return PTY_f64;
    default:
      return primType;
  }
#else
  return primType;
#endif
}

bool IsNoCvtNeeded(PrimType toType, PrimType fromType) {
  if (toType == fromType) {
    return true;
  }
  switch (toType) {
    case PTY_i32:
      return fromType == PTY_i16 || fromType == PTY_i8;
    case PTY_u32:
      return fromType == PTY_u16 || fromType == PTY_u8;
    case PTY_u1:
    case PTY_u8:
    case PTY_u16:
      return fromType == PTY_u32;
    case PTY_i8:
    case PTY_i16:
      return fromType == PTY_i32;
    case PTY_u64:
      return fromType == PTY_ptr;
    case PTY_ptr:
      return fromType == PTY_u64;
    default:
      return false;
  }
}

#if TARGX86_64 || TARGAARCH64
  #define POINTER_SIZE 8
  #define POINTER_P2SIZE 3
#elif TARGX86 || TARGARM32 || TARGVM
  #define POINTER_SIZE 4
  #define POINTER_P2SIZE 2
#endif
// answer in bytes; 0 if unknown
uint32 GetPrimTypeSize(PrimType primType) {
  switch (primType) {
    case PTY_void:
    case PTY_agg:
      return 0;
    case PTY_ptr:
    case PTY_ref:
      return POINTER_SIZE;
    case PTY_u1:
    case PTY_i8:
    case PTY_u8:
      return 1;
    case PTY_i16:
    case PTY_u16:
      return 2;
    case PTY_a32:
    case PTY_f32:
    case PTY_i32:
    case PTY_u32:
    case PTY_simplestr:
    case PTY_simpleobj:
      return 4;
    case PTY_a64:
    case PTY_c64:
    case PTY_f64:
    case PTY_i64:
    case PTY_u64:
    case PTY_v2i32:
    case PTY_v4i16:
    case PTY_v8i8:
    case PTY_v2u32:
    case PTY_v4u16:
    case PTY_v8u8:
    case PTY_v2f32:
      return 8;
    case PTY_c128:
    case PTY_f128:
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
      return 16;
#ifdef DYNAMICLANG
    case PTY_dynf32:
    case PTY_dyni32:
    case PTY_dynstr:
    case PTY_dynobj:
    case PTY_dynundef:
    case PTY_dynnull:
    case PTY_dynbool:
      return 8;
    case PTY_dynany:
    case PTY_dynf64:
      return 8;
#endif
    default:
      return 0;
  }
}

// answer is n if size in byte is (1<<n) (0: 1B; 1: 2B, 2: 4B, 3: 8B, 4:16B)
uint32 GetPrimTypeP2Size(PrimType primType) {
  switch (primType) {
    case PTY_ptr:
    case PTY_ref:
      return POINTER_P2SIZE;
    case PTY_u1:
    case PTY_i8:
    case PTY_u8:
      return 0;
    case PTY_i16:
    case PTY_u16:
      return 1;
    case PTY_a32:
    case PTY_f32:
    case PTY_i32:
    case PTY_u32:
    case PTY_simplestr:
    case PTY_simpleobj:
      return 2;
    case PTY_a64:
    case PTY_c64:
    case PTY_f64:
    case PTY_i64:
    case PTY_u64:
    case PTY_v2i32:
    case PTY_v4i16:
    case PTY_v8i8:
    case PTY_v2u32:
    case PTY_v4u16:
    case PTY_v8u8:
    case PTY_v2f32:
      return 3;
    case PTY_c128:
    case PTY_f128:
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
      return 4;
#ifdef DYNAMICLANG
    case PTY_dynf32:
    case PTY_dyni32:
    case PTY_dynstr:
    case PTY_dynobj:
    case PTY_dynundef:
    case PTY_dynnull:
    case PTY_dynbool:
    case PTY_dynany:
    case PTY_dynf64:
      return 3;
#endif
    default:
      ASSERT(false, "Power-of-2 size only applicable to sizes of 1, 2, 4, 8 or 16 bytes.");
      return 10;
  }
}

uint32 GetPrimTypeLanes(PrimType pty) {
  switch (pty) {
    case PTY_v2i32:
    case PTY_v2u32:
    case PTY_v2f32:
    case PTY_v2i64:
    case PTY_v2u64:
    case PTY_v2f64:
      return 2;
    case PTY_v4i16:
    case PTY_v4u16:
    case PTY_v4i32:
    case PTY_v4u32:
    case PTY_v4f32:
      return 4;
    case PTY_v8i8:
    case PTY_v8u8:
    case PTY_v8i16:
    case PTY_v8u16:
      return 8;
    case PTY_v16i8:
    case PTY_v16u8:
      return 16;
    default:
      return 0;
  }
}

// return the signed version that has the same size
PrimType GetSignedPrimType(PrimType pty) {
  switch (pty) {
    case PTY_ptr:
    case PTY_ref:
    case PTY_a64:
    case PTY_u64:
      return PTY_i64;
    case PTY_u8:
      return PTY_i8;
    case PTY_u16:
      return PTY_i16;
    case PTY_a32:
    case PTY_u32:
      return PTY_i32;
    default: ;
  }
  return pty;
}

// return the unsigned version that has the same size
PrimType GetUnsignedPrimType(PrimType pty) {
  switch (pty) {
    case PTY_i64:
      return PTY_u64;
    case PTY_i8:
      return PTY_u8;
    case PTY_i16:
      return PTY_u16;
    case PTY_i32:
      return PTY_u32;
    default: ;
  }
  return pty;
}

const char *GetPrimTypeName(PrimType primType) {
#define LOAD_ALGO_PRIMARY_TYPE
  switch (primType) {
    case kPtyInvalid:
      return "kPtyInvalid";
#define PRIMTYPE(P) \
    case PTY_##P:   \
      return #P;
#include "prim_types.def"
#undef PRIMTYPE
    case kPtyDerived:
      return "derived";  // just for test: no primitive type for derived
    default:
      return "kPtyInvalid";
  }
  // SIMD types to be defined
}

const char *GetPrimTypeJavaName(PrimType primType) {
  switch (primType) {
    case PTY_u1:
      return "Z";
    case PTY_i8:
      return "B";
    case PTY_i16:
      return "S";
    case PTY_u16:
      return "C";
    case PTY_i32:
      return "I";
    case PTY_i64:
      return "J";
    case PTY_f32:
      return "F";
    case PTY_f64:
      return "D";
    case PTY_void:
      return "V";
    case PTY_constStr:
      return kJstrTypeName.c_str();
    case kPtyInvalid:
      return "invalid";
    default:
      return "invalid";
  }
}

void TypeAttrs::DumpAttributes() const {
#define TYPE_ATTR
#define STRING(s) #s
#define ATTR(AT)          \
  if (GetAttr(ATTR_##AT)) \
    LogInfo::MapleLogger() << " " << STRING(AT);
#include "all_attributes.def"
#undef ATTR
#undef TYPE_ATTR
  if (attrAlign) {
    LogInfo::MapleLogger() << " align(" << GetAlign() << ")";
  }
}

void FieldAttrs::DumpAttributes() const {
#define FIELD_ATTR
#define STRING(s) #s
#define ATTR(AT)             \
  if (GetAttr(FLDATTR_##AT)) \
    LogInfo::MapleLogger() << " " << STRING(AT);
#include "all_attributes.def"
#undef ATTR
#undef FIELD_ATTR
  if (attrAlign) {
    LogInfo::MapleLogger() << " align(" << GetAlign() << ")";
  }
}

const std::string &MIRType::GetName() const {
  return GlobalTables::GetStrTable().GetStringFromStrIdx(nameStrIdx);
}

bool MIRType::ValidateClassOrInterface(const std::string &className, bool noWarning) const {
  if (primType == maple::PTY_agg && (typeKind == maple::kTypeClass || typeKind == maple::kTypeInterface) &&
      nameStrIdx != 0u) {
    return true;
  }
  if (!noWarning) {
    size_t len = className.size();
    constexpr int minClassNameLen = 4;
    constexpr char suffix[] = "_3B";
    size_t suffixLen = std::strlen(suffix);
    if (len > minClassNameLen && strncmp(className.c_str() + len - suffixLen, suffix, suffixLen) == 0) {
      LogInfo::MapleLogger(kLlErr) << "error: missing proper mplt file for " << className << '\n';
    } else {
      LogInfo::MapleLogger(kLlErr) << "internal error: type is not java class or interface " << className << '\n';
    }
  }
  return false;
}

bool MIRType::PointsToConstString() const {
  return (typeKind == kTypePointer) ? static_cast<const MIRPtrType*>(this)->PointsToConstString() : false;
}

std::string MIRType::GetMplTypeName() const {
  if (typeKind == kTypeScalar) {
    return GetPrimTypeName(primType);
  }
  return "";
}

std::string MIRType::GetCompactMplTypeName() const {
  if (typeKind == kTypeScalar) {
    return GetPrimTypeJavaName(primType);
  }
  return "";
}

void MIRType::Dump(int, bool) const {
  LogInfo::MapleLogger() << GetPrimTypeName(primType);
}

void MIRType::DumpAsCxx(int) const {
  switch (primType) {
    case PTY_void:
      LogInfo::MapleLogger() << "void";
      break;
    case PTY_i8:
      LogInfo::MapleLogger() << "int8 ";
      break;
    case PTY_i16:
      LogInfo::MapleLogger() << "int16";
      break;
    case PTY_i32:
      LogInfo::MapleLogger() << "int32";
      break;
    case PTY_i64:
      LogInfo::MapleLogger() << "int64";
      break;
    case PTY_u8:
      LogInfo::MapleLogger() << "uint8";
      break;
    case PTY_u16:
      LogInfo::MapleLogger() << "uint16";
      break;
    case PTY_u32:
      LogInfo::MapleLogger() << "uint32";
      break;
    case PTY_u64:
      LogInfo::MapleLogger() << "uint64";
      break;
    case PTY_u1:
      LogInfo::MapleLogger() << "bool  ";
      break;
    case PTY_ptr:
      LogInfo::MapleLogger() << "void* ";
      break;
    case PTY_ref:
      LogInfo::MapleLogger() << "void* ";
      break;
    case PTY_a32:
      LogInfo::MapleLogger() << "int32";
      break;
    case PTY_a64:
      LogInfo::MapleLogger() << "void* ";
      break;
    case PTY_f32:
      LogInfo::MapleLogger() << "float ";
      break;
    case PTY_f64:
      LogInfo::MapleLogger() << "double";
      break;
    case PTY_c64:
      LogInfo::MapleLogger() << "float complex";
      break;
    case PTY_c128:
      LogInfo::MapleLogger() << "double complex";
      break;
    default:
      ASSERT(false, "not yet implemented");
  }
}

bool MIRType::IsOfSameType(MIRType &type) {
  if (typeKind != type.typeKind) {
    return false;
  }

  if (typeKind == kTypePointer) {
    const auto &ptrType = static_cast<const MIRPtrType&>(*this);
    const auto &ptrTypeIt = static_cast<const MIRPtrType&>(type);
    if (ptrType.GetPointedTyIdx() == ptrTypeIt.GetPointedTyIdx()) {
      return true;
    } else {
      MIRType &mirTypeIt = *GlobalTables::GetTypeTable().GetTypeFromTyIdx(ptrTypeIt.GetPointedTyIdx());
      return GlobalTables::GetTypeTable().GetTypeFromTyIdx(ptrType.GetPointedTyIdx())->IsOfSameType(mirTypeIt);
    }
  } else if (typeKind == kTypeJArray) {
    auto &arrType1 = static_cast<MIRJarrayType&>(*this);
    auto &arrType2 = static_cast<MIRJarrayType&>(type);
    if (arrType1.GetDim() != arrType2.GetDim()) {
      return false;
    }
    return arrType1.GetElemType()->IsOfSameType(*arrType2.GetElemType());
  } else {
    return tyIdx == type.tyIdx;
  }
}

inline void DumpTypeName(GStrIdx strIdx, bool isLocal) {
  LogInfo::MapleLogger() << ((isLocal) ? "%" : "$") << GlobalTables::GetStrTable().GetStringFromStrIdx(strIdx);
}

static bool CheckAndDumpTypeName(GStrIdx strIdx, bool isLocal) {
  if (strIdx == 0u) {
    return false;
  }
  LogInfo::MapleLogger() << "<";
  DumpTypeName(strIdx, isLocal);
  LogInfo::MapleLogger() << ">";
  return true;
}

void MIRFuncType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  LogInfo::MapleLogger() << "<func(";
  size_t size = paramTypeList.size();
  for (size_t i = 0; i < size; ++i) {
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(paramTypeList[i])->Dump(indent + 1);
    if (size - 1 != i) {
      LogInfo::MapleLogger() << ",";
    }
  }
  if (isVarArgs) {
    LogInfo::MapleLogger() << ", ...";
  }
  LogInfo::MapleLogger() << ") ";
  GlobalTables::GetTypeTable().GetTypeFromTyIdx(retTyIdx)->Dump(indent + 1);
  LogInfo::MapleLogger() << ">";
}

static constexpr uint64 RoundUpConst(uint64 offset, uint32 align) {
  return (-align) & (offset + align - 1);
}

static inline uint64 RoundUp(uint64 offset, uint32 align) {
  if (align == 0) {
    return offset;
  }
  return RoundUpConst(offset, align);
}

static constexpr uint64 RoundDownConst(uint64 offset, uint32 align) {
  return (-align) & offset;
}

static inline uint64 RoundDown(uint64 offset, uint32 align) {
  if (align == 0) {
    return offset;
  }
  return RoundDownConst(offset, align);
}

size_t MIRArrayType::GetSize() const {
  size_t elemsize = GetElemType()->GetSize();
  if (elemsize == 0) {
    return 0;
  }
  elemsize = RoundUp(elemsize, typeAttrs.GetAlign());
  size_t numelems = sizeArray[0];
  for (uint16 i = 1; i < dim; i++) {
    numelems *= sizeArray[i];
  }
  return elemsize * numelems;
}

uint32 MIRArrayType::GetAlign() const {
  return std::max(GetElemType()->GetAlign(), typeAttrs.GetAlign());
}

void MIRArrayType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  LogInfo::MapleLogger() << "<";
  for (uint16 i = 0; i < dim; ++i) {
    LogInfo::MapleLogger() << "[" << GetSizeArrayItem(i) << "]";
  }
  LogInfo::MapleLogger() << " ";
  GlobalTables::GetTypeTable().GetTypeFromTyIdx(eTyIdx)->Dump(indent + 1);
  GetTypeAttrs().DumpAttributes();
  LogInfo::MapleLogger() << ">";
}

std::string MIRArrayType::GetCompactMplTypeName() const {
  std::stringstream ss;
  ss << "A";
  MIRType *elemType = GetElemType();
  ss << elemType->GetCompactMplTypeName();
  return ss.str();
}

void MIRFarrayType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  LogInfo::MapleLogger() << "<[] ";
  GlobalTables::GetTypeTable().GetTypeFromTyIdx(elemTyIdx)->Dump(indent + 1);
  LogInfo::MapleLogger() << ">";
}

std::string MIRFarrayType::GetCompactMplTypeName() const {
  std::stringstream ss;
  ss << "A";
  MIRType *elemType = GetElemType();
  ss << elemType->GetCompactMplTypeName();
  return ss.str();
}

const std::string &MIRJarrayType::GetJavaName() {
  if (javaNameStrIdx == 0u) {
    DetermineName();
  }
  return GlobalTables::GetStrTable().GetStringFromStrIdx(javaNameStrIdx);
}

MIRStructType *MIRJarrayType::GetParentType() {
  if (parentTyIdx == 0u) {
    GStrIdx jloStrIdx = GlobalTables::GetStrTable().GetStrIdxFromName(
        namemangler::GetInternalNameLiteral(namemangler::kJavaLangObjectStr));
    parentTyIdx = GlobalTables::GetTypeNameTable().GetTyIdxFromGStrIdx(jloStrIdx);
    ASSERT((parentTyIdx != 0u), "cannot find type for java.lang.Object");
  }
  return static_cast<MIRStructType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx));
}

void MIRJarrayType::DetermineName() {
  if (javaNameStrIdx != 0u) {
    return;  // already determined
  }
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(GetElemTyIdx());
  dim = 1;
  std::string baseName;
  while (true) {
    if (elemType->GetKind() == kTypeScalar) {
      baseName = GetPrimTypeJavaName(elemType->GetPrimType());
      fromPrimitive = true;
      break;
    } else if (elemType->GetKind() == kTypePointer) {
      auto *pType = static_cast<MIRPtrType*>(elemType)->GetPointedType();
      ASSERT(pType != nullptr, "pType is null in MIRJarrayType::DetermineName");
      if (pType->GetKind() == kTypeByName || pType->GetKind() == kTypeClass || pType->GetKind() == kTypeInterface ||
          pType->GetKind() == kTypeClassIncomplete || pType->GetKind() == kTypeInterfaceIncomplete) {
        baseName = static_cast<MIRStructType*>(pType)->GetName();
        fromPrimitive = false;
        break;
      } else if (pType->GetKind() == kTypeJArray) {
        auto *tmpPtype = static_cast<MIRJarrayType*>(pType);
        elemType = tmpPtype->GetElemType();
        ASSERT(elemType != nullptr, "elemType is null in MIRJarrayType::DetermineName");
        ++dim;
      } else {
        ASSERT(false, "unexpected type!");
      }
    } else {
      ASSERT(false, "unexpected type!");
    }
  }
  std::string name;
  for (int i = dim; i > 0; --i) {
    name += JARRAY_PREFIX_STR;
  }
  name += baseName;
  javaNameStrIdx = GlobalTables::GetStrTable().GetOrCreateStrIdxFromName(name);
}

MIRType *MIRPtrType::GetPointedType() const {
  return GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointedTyIdx);
}

bool MIRType::IsVolatile(int fieldID) const {
  if (fieldID == 0) {
    return HasVolatileField();
  }
  return static_cast<const MIRStructType*>(this)->IsFieldVolatile(fieldID);
}

bool MIRPtrType::HasTypeParam() const {
  return GetPointedType()->HasTypeParam();
}

bool MIRPtrType::PointsToConstString() const {
  GStrIdx typeNameIdx = GetPointedType()->GetNameStrIdx();
  std::string typeName = GlobalTables::GetStrTable().GetStringFromStrIdx(typeNameIdx);
  return typeName == "Ljava_2Flang_2FString_3B";
}

void MIRPtrType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  MIRType *pointedType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointedTyIdx);
  if (pointedType->GetKind() == kTypeFunction) {  // no * for function pointer
    pointedType->Dump(indent);
  } else {
    LogInfo::MapleLogger() << "<* ";
    pointedType->Dump(indent + 1);
    typeAttrs.DumpAttributes();
    LogInfo::MapleLogger() << ">";
  }
}

void MIRBitFieldType::Dump(int, bool) const {
  LogInfo::MapleLogger() << ":" << static_cast<int>(fieldSize) << " " << GetPrimTypeName(primType);
}

size_t MIRClassType::GetSize() const {
  if (parentTyIdx == 0u) {
    return MIRStructType::GetSize();
  }
  const auto *parentType = static_cast<const MIRClassType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx));
  size_t parentSize = parentType->GetSize();
  if (parentSize == 0) {
    return 0;
  }
  size_t structSize = MIRStructType::GetSize();
  if (structSize == 0) {
    return 0;
  }
  return parentSize + structSize;
}

FieldID MIRClassType::GetFirstLocalFieldID() const {
  if (!IsLocal()) {
    return 0;
  }
  if (parentTyIdx == 0u) {
    return 1;
  }

  constexpr uint8 lastFieldIDOffset = 2;
  constexpr uint8 firstLocalFieldIDOffset = 1;
  const auto *parentClassType =
      static_cast<const MIRClassType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx));
  return !parentClassType->IsLocal() ? parentClassType->GetLastFieldID() + lastFieldIDOffset
                                     : parentClassType->GetFirstLocalFieldID() + firstLocalFieldIDOffset;
}

const MIRClassType *MIRClassType::GetExceptionRootType() const {
  GStrIdx ehTypeNameIdx = GlobalTables::GetStrTable().GetStrIdxFromName(namemangler::kJavaLangObjectStr);
  const MIRClassType *subClassType = this;
  while (subClassType != nullptr && subClassType->nameStrIdx != ehTypeNameIdx) {
    subClassType =
        static_cast<const MIRClassType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(subClassType->parentTyIdx));
  }
  return subClassType;
}

MIRClassType *MIRClassType::GetExceptionRootType() {
  return const_cast<MIRClassType*>(const_cast<const MIRClassType*>(this)->GetExceptionRootType());
}

bool MIRClassType::IsExceptionType() const {
  GStrIdx ehTypeNameIdx = GlobalTables::GetStrTable().GetStrIdxFromName("Ljava_2Flang_2FThrowable_3B");
  const MIRClassType *parentClassType = this;
  while (parentClassType != nullptr && parentClassType->nameStrIdx != ehTypeNameIdx) {
    parentClassType =
        static_cast<MIRClassType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentClassType->parentTyIdx));
  }
  return parentClassType != nullptr;
}

FieldID MIRClassType::GetLastFieldID() const {
  FieldID fieldID = fields.size();
  if (parentTyIdx != 0u) {
    const auto *parentClassType =
        static_cast<const MIRClassType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx));
    if (parentClassType != nullptr) {
      fieldID += parentClassType->GetLastFieldID() + 1;
    }
  }
  return fieldID;
}

static void DumpClassOrInterfaceInfo(const MIRStructType &type, int indent) {
  const std::vector<MIRInfoPair> &info = type.GetInfo();
  std::vector<bool> infoIsString = type.GetInfoIsString();
  size_t size = info.size();
  for (size_t i = 0; i < size; ++i) {
    LogInfo::MapleLogger() << '\n';
    PrintIndentation(indent);
    LogInfo::MapleLogger() << "@" << GlobalTables::GetStrTable().GetStringFromStrIdx(info[i].first) << " ";
    if (!infoIsString[i]) {
      LogInfo::MapleLogger() << info[i].second;
    } else {
      LogInfo::MapleLogger() << "\"" << GlobalTables::GetStrTable().GetStringFromStrIdx(GStrIdx(info[i].second))
                             << "\"";
    }
    if (i != size - 1) {
      LogInfo::MapleLogger() << ",";
    }
  }
}

static uint32 GetInfoFromStrIdx(const std::vector<MIRInfoPair> &info, const GStrIdx &strIdx) {
  for (MIRInfoPair infoPair : info) {
    if (infoPair.first == strIdx) {
      return infoPair.second;
    }
  }
  return 0;
}

uint32 MIRInterfaceType::GetInfo(GStrIdx strIdx) const {
  return GetInfoFromStrIdx(info, strIdx);
}

// return class id or superclass id accroding to input string
uint32 MIRInterfaceType::GetInfo(const std::string &infoStr) const {
  GStrIdx strIdx = GlobalTables::GetStrTable().GetStrIdxFromName(infoStr);
  return GetInfo(strIdx);
}
size_t MIRInterfaceType::GetSize() const {
  if (parentsTyIdx.empty()) {
    return MIRStructType::GetSize();
  }
  size_t size = MIRStructType::GetSize();
  if (size == 0) {
    return 0;
  }
  for (size_t i = 0; i < parentsTyIdx.size(); ++i) {
    const auto *parentType =
        static_cast<const MIRInterfaceType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentsTyIdx[i]));
    size_t parentSize = parentType->GetSize();
    if (parentSize == 0) {
      return 0;
    }
    size += parentSize;
  }
  return size;
}

static void DumpStaticValue(const MIREncodedArray &staticValue, int indent) {
  if (staticValue.empty()) {
    return;
  }
  LogInfo::MapleLogger() << '\n';
  PrintIndentation(indent);
  LogInfo::MapleLogger() << "@staticvalue";
  constexpr uint32 typeLen = 5;
  constexpr uint32 typeMask = 0x1f;
  for (const auto &value : staticValue) {
    LogInfo::MapleLogger() << " [";
    uint8 valueArg = static_cast<uint32>(value.encodedValue[0]) >> typeLen;
    uint8 valueType = static_cast<uint32>(value.encodedValue[0]) & typeMask;
    // kValueNull kValueBoolean
    constexpr uint32 simpleOffset = 1;
    constexpr uint32 aggOffSet = 2;
    valueArg = (valueType == kValueNull || valueType == kValueBoolean) ? simpleOffset : valueArg + aggOffSet;
    for (uint32 k = 0; k < valueArg; ++k) {
      LogInfo::MapleLogger() << static_cast<uint32>(value.encodedValue[k]);
      if (k != static_cast<uint32>(valueArg - 1)) {
        LogInfo::MapleLogger() << " ";
      }
    }
    LogInfo::MapleLogger() << "]";
  }
}

static void DumpFields(FieldVector fields, int indent, bool otherFields = false) {
  size_t size = fields.size();
  for (size_t i = 0; i < size; ++i) {
    LogInfo::MapleLogger() << '\n';
    PrintIndentation(indent);
    LogInfo::MapleLogger() << ((!otherFields) ? "@" : "^")
                           << GlobalTables::GetStrTable().GetStringFromStrIdx(fields[i].first) << " ";
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(fields[i].second.first)->Dump(indent + 1);
    FieldAttrs &fa = fields[i].second.second;
    fa.DumpAttributes();
    if (fa.GetAttr(FLDATTR_static) && fa.GetAttr(FLDATTR_final) &&
        (fa.GetAttr(FLDATTR_public) || fa.GetAttr(FLDATTR_protected))) {
      const char *fieldName = GlobalTables::GetStrTable().GetStringFromStrIdx(fields[i].first).c_str();
      MIRSymbol *fieldVar =
          GlobalTables::GetGsymTable().GetSymbolFromStrIdx(GlobalTables::GetStrTable().GetStrIdxFromName(fieldName));
      if (fieldVar != nullptr && fieldVar->GetKonst() != nullptr &&
          fieldVar->GetKonst()->GetKind() == kConstStr16Const) {
        LogInfo::MapleLogger() << " = ";
        fieldVar->GetKonst()->Dump();
      }
    }
    if (i != size - 1) {
      LogInfo::MapleLogger() << ",";
    }
  }
}

static void DumpFieldsAsCxx(const FieldVector &fields, int indent) {
  for (auto &f : fields) {
    PrintIndentation(indent);
    const FieldAttrs &fa = f.second.second;
    if (fa.GetAttr(FLDATTR_static)) {
      LogInfo::MapleLogger() << "// ";
    }
    LogInfo::MapleLogger() << "/* ";
    fa.DumpAttributes();
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(f.second.first)->Dump(indent + 1);
    LogInfo::MapleLogger() << " */ ";
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(f.second.first)->DumpAsCxx(indent + 1);
    LogInfo::MapleLogger() << " " << GlobalTables::GetStrTable().GetStringFromStrIdx(f.first) << ";" << '\n';
  }
}

static void DumpMethods(MethodVector methods, int indent) {
  size_t size = methods.size();
  for (size_t i = 0; i < size; ++i) {
    LogInfo::MapleLogger() << '\n';
    PrintIndentation(indent);
    LogInfo::MapleLogger() << "&" << GlobalTables::GetGsymTable().GetSymbolFromStidx(methods[i].first.Idx())->GetName();
    methods[i].second.second.DumpAttributes();
    LogInfo::MapleLogger() << " (";
    auto *funcType =
        static_cast<MIRFuncType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(methods[i].second.first));
    size_t parmListSize = funcType->GetParamTypeList().size();
    for (size_t j = 0; j < parmListSize; ++j) {
      GlobalTables::GetTypeTable().GetTypeFromTyIdx(funcType->GetNthParamType(j))->Dump(indent + 1);
      if (j != parmListSize - 1) {
        LogInfo::MapleLogger() << ",";
      }
    }
    if (funcType->IsVarargs()) {
      LogInfo::MapleLogger() << ", ...";
    }
    LogInfo::MapleLogger() << ") ";
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(funcType->GetRetTyIdx())->Dump(indent + 1);
    if (i != size - 1) {
      LogInfo::MapleLogger() << ",";
    }
  }
}

static void DumpConstructorsAsCxx(MethodVector methods, int indent) {
  unsigned int i = 0;
  for (auto &m : methods) {
    FuncAttrs &fa = m.second.second;
    if (!fa.GetAttr(FUNCATTR_constructor) || !fa.GetAttr(FUNCATTR_public)) {
      continue;
    }
    auto *funcType = static_cast<MIRFuncType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(m.second.first));
    PrintIndentation(indent);
    LogInfo::MapleLogger() << "/* &" << GlobalTables::GetGsymTable().GetSymbolFromStidx(m.first.Idx())->GetName();
    fa.DumpAttributes();
    LogInfo::MapleLogger() << " (";
    unsigned int j = 0;
    size_t paramTypeListSize = funcType->GetParamTypeList().size();
    for (auto &p : funcType->GetParamTypeList()) {
      GlobalTables::GetTypeTable().GetTypeFromTyIdx(p)->Dump(indent + 1);
      if (j != paramTypeListSize - 1) {
        LogInfo::MapleLogger() << ", ";
      }
      ++j;
    }
    if (funcType->IsVarargs()) {
      LogInfo::MapleLogger() << ", ...";
    }
    LogInfo::MapleLogger() << ") ";
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(funcType->GetRetTyIdx())->Dump(indent + 1);
    LogInfo::MapleLogger() << " */" << '\n';
    PrintIndentation(indent);
    LogInfo::MapleLogger() << "/* ";
    LogInfo::MapleLogger() << namemangler::DecodeName(
        GlobalTables::GetGsymTable().GetSymbolFromStidx(m.first.Idx())->GetName());
    LogInfo::MapleLogger() << " */" << '\n';
    PrintIndentation(indent);
    LogInfo::MapleLogger() << "extern \"C\" ";
    // return type
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(funcType->GetRetTyIdx())->DumpAsCxx(0);
    LogInfo::MapleLogger() << " " << GlobalTables::GetGsymTable().GetSymbolFromStidx(m.first.Idx())->GetName() << "( ";
    j = 0;
    for (auto &p : funcType->GetParamTypeList()) {
      GlobalTables::GetTypeTable().GetTypeFromTyIdx(p)->DumpAsCxx(indent + 1);
      if (j != paramTypeListSize - 1) {
        LogInfo::MapleLogger() << ", ";
      }
      ++j;
    }
    if (funcType->IsVarargs()) {
      LogInfo::MapleLogger() << ", ...";
    }
    LogInfo::MapleLogger() << ")";
    if (methods.size() - 1 != i++) {
      LogInfo::MapleLogger() << ";" << '\n';
    }
  }
}

static void DumpInterfaces(std::vector<TyIdx> interfaces, int indent) {
  size_t size = interfaces.size();
  for (size_t i = 0; i < size; ++i) {
    LogInfo::MapleLogger() << '\n';
    PrintIndentation(indent);
    GStrIdx stridx = GlobalTables::GetTypeTable().GetTypeFromTyIdx(interfaces[i])->GetNameStrIdx();
    LogInfo::MapleLogger() << "$" << GlobalTables::GetStrTable().GetStringFromStrIdx(stridx);
    if (i != size - 1) {
      LogInfo::MapleLogger() << ",";
    }
  }
}

size_t MIRStructType::GetSize() const {
  if (typeKind == kTypeUnion) {
    if (fields.size() == 0) {
      return isCPlusPlus ? 1 : 0;
    }
    size_t maxSize = 0;
    for (size_t i = 0; i < fields.size(); ++i) {
      TyIdxFieldAttrPair tfap = GetTyidxFieldAttrPair(i);
      MIRType *fieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(tfap.first);
      size_t size = RoundUp(fieldType->GetSize(), tfap.second.GetAlign());
      if (maxSize < size) {
        maxSize = size;
      }
    }
    return maxSize;
  }
  // since there may be bitfields, perform a layout process for the fields
  size_t byteOfst = 0;
  size_t bitOfst = 0;
  constexpr uint32 shiftNum = 3;
  for (size_t i = 0; i < fields.size(); ++i) {
    TyIdxFieldAttrPair tfap = GetTyidxFieldAttrPair(i);
    MIRType *fieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(tfap.first);
    if (fieldType->GetKind() != kTypeBitField) {
      if (byteOfst * 8 < bitOfst) {
        byteOfst = (bitOfst >> shiftNum) + 1;
      }
      byteOfst = RoundUp(byteOfst, std::max(fieldType->GetAlign(), tfap.second.GetAlign()));
      byteOfst += fieldType->GetSize();
      bitOfst = byteOfst * 8;
    } else {
      MIRBitFieldType *bitfType = static_cast<MIRBitFieldType *>(fieldType);
      if (bitfType->GetFieldSize() == 0) {  // special case, for aligning purpose
        bitOfst = RoundUp(bitOfst, GetPrimTypeBitSize(bitfType->GetPrimType()));
        byteOfst = bitOfst >> shiftNum;
      } else {
        if (RoundDown(bitOfst + bitfType->GetFieldSize() - 1, GetPrimTypeBitSize(bitfType->GetPrimType())) !=
            RoundDown(bitOfst, GetPrimTypeBitSize(bitfType->GetPrimType()))) {
          bitOfst = RoundUp(bitOfst, GetPrimTypeBitSize(bitfType->GetPrimType()));
          byteOfst = bitOfst >> shiftNum;
        }
        bitOfst += bitfType->GetFieldSize();
        byteOfst = bitOfst >> shiftNum;
      }
    }
  }
  if (byteOfst * 8 < bitOfst) {
    byteOfst = (bitOfst >> shiftNum) + 1;
  }
  byteOfst = RoundUp(byteOfst, GetAlign());
  if (byteOfst == 0 && isCPlusPlus) {
    return 1;  // empty struct in C++ has size 1
  }
  return byteOfst;
}

uint32 MIRStructType::GetAlign() const {
  if (fields.size() == 0) {
    return 0;
  }
  uint32 maxAlign = 1;
  for (size_t i = 0; i < fields.size(); ++i) {
    TyIdxFieldAttrPair tfap = GetTyidxFieldAttrPair(i);
    MIRType *fieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(tfap.first);
    uint32 algn = fieldType->GetAlign();
    if (fieldType->GetKind() == kTypeBitField) {
      algn = GetPrimTypeSize(fieldType->GetPrimType());
    } else {
      algn = std::max(algn, tfap.second.GetAlign());
    }
    if (maxAlign < algn) {
      maxAlign = algn;
    }
  }
  return maxAlign;
}

void MIRStructType::DumpFieldsAndMethods(int indent, bool hasMethod) const {
  DumpFields(fields, indent);
  bool hasField = !fields.empty();
  bool hasStaticField = !staticFields.empty();
  if (hasField && hasStaticField) {
    LogInfo::MapleLogger() << ",";
  }
  DumpFields(staticFields, indent, true);
  bool hasFieldOrStaticField = hasField || hasStaticField;
  bool hasParentField = !parentFields.empty();
  if (hasFieldOrStaticField && hasParentField) {
    LogInfo::MapleLogger() << ",";
  }
  DumpFields(parentFields, indent, true);
  if ((hasFieldOrStaticField || hasParentField) && hasMethod) {
    LogInfo::MapleLogger() << ",";
  }
  DumpMethods(methods, indent);
}

void MIRStructType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  LogInfo::MapleLogger() << ((typeKind == kTypeStruct) ? "<struct {"
                                                       : ((typeKind == kTypeUnion) ? "<union {"
                                                                                   : "<structincomplete {"));
  DumpFieldsAndMethods(indent, !methods.empty());
  LogInfo::MapleLogger() << "}>";
}

uint32 MIRClassType::GetInfo(GStrIdx strIdx) const {
  return GetInfoFromStrIdx(info, strIdx);
}

// return class id or superclass id accroding to input string
uint32 MIRClassType::GetInfo(const std::string &infoStr) const {
  GStrIdx strIdx = GlobalTables::GetStrTable().GetStrIdxFromName(infoStr);
  return GetInfo(strIdx);
}

bool MIRClassType::IsFinal() const {
  if (!IsStructType()) {
    return false;
  }
  uint32 attrStrIdx = GetInfo(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName("INFO_attribute_string"));
  CHECK(attrStrIdx < GlobalTables::GetStrTable().StringTableSize(), "out of range of vector");
  const std::string &kAttrString = GlobalTables::GetStrTable().GetStringFromStrIdx(GStrIdx(attrStrIdx));
  return kAttrString.find(" final ") != std::string::npos;
}

bool MIRClassType::IsAbstract() const {
  uint32 attrStrIdx = GetInfo(GlobalTables::GetStrTable().GetOrCreateStrIdxFromName("INFO_attribute_string"));
  CHECK(attrStrIdx < GlobalTables::GetStrTable().StringTableSize(), "out of range of vector");
  const std::string &kAttrString = GlobalTables::GetStrTable().GetStringFromStrIdx(GStrIdx(attrStrIdx));
  return kAttrString.find(" abstract ") != std::string::npos;
}

bool MIRClassType::IsInner() const {
  const std::string &name = GetName();
  return name.find("_24") != std::string::npos;
}

static void DumpInfoPragmaStaticValue(const std::vector<MIRInfoPair> &info, const std::vector<MIRPragma*> &pragmaVec,
                                      const MIREncodedArray &staticValue, int indent, bool hasFieldMethodOrInterface) {
  bool hasPragma = pragmaVec.size();
  bool hasStaticValue = staticValue.size();
  if (!info.empty() && (hasPragma || hasStaticValue || hasFieldMethodOrInterface)) {
    LogInfo::MapleLogger() << ",";
  }
  size_t size = pragmaVec.size();
  for (size_t i = 0; i < size; ++i) {
    pragmaVec[i]->Dump(indent);
    if (i != size - 1) {
      LogInfo::MapleLogger() << ",";
    }
  }
  if (hasPragma && (hasStaticValue || hasFieldMethodOrInterface)) {
    LogInfo::MapleLogger() << ",";
  }
  DumpStaticValue(staticValue, indent);
  if (hasStaticValue && hasFieldMethodOrInterface) {
    LogInfo::MapleLogger() << ",";
  }
}

void MIRClassType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  LogInfo::MapleLogger() << ((typeKind == kTypeClass) ? "<class " : "<classincomplete ");
  if (parentTyIdx != 0u) {
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx)->Dump(indent + 1);
    LogInfo::MapleLogger() << " ";
  }
  LogInfo::MapleLogger() << "{";
  DumpClassOrInterfaceInfo(*this, indent);
  bool hasFieldMethodOrInterface = !(fields.empty() && parentFields.empty() && staticFields.empty() &&
                                     methods.empty() && interfacesImplemented.empty());
  DumpInfoPragmaStaticValue(info, pragmaVec, staticValue, indent, hasFieldMethodOrInterface);

  bool hasMethod = !methods.empty();
  bool hasImplementedInterface = !interfacesImplemented.empty();
  DumpFieldsAndMethods(indent, hasMethod || hasImplementedInterface);
  if (hasMethod && hasImplementedInterface) {
    LogInfo::MapleLogger() << ",";
  }
  DumpInterfaces(interfacesImplemented, indent);
  LogInfo::MapleLogger() << "}>";
}

void MIRClassType::DumpAsCxx(int indent) const {
  LogInfo::MapleLogger() << "{\n";
  DumpFieldsAsCxx(fields, indent);
  LogInfo::MapleLogger() << "};\n";
  DumpConstructorsAsCxx(methods, 0);
}

void MIRInterfaceType::Dump(int indent, bool dontUseName) const {
  if (!dontUseName && CheckAndDumpTypeName(nameStrIdx, nameIsLocal)) {
    return;
  }
  LogInfo::MapleLogger() << ((typeKind == kTypeInterface) ? "<interface " : "<interfaceincomplete ");
  for (TyIdx idx : parentsTyIdx) {
    GlobalTables::GetTypeTable().GetTypeFromTyIdx(idx)->Dump(0);
    LogInfo::MapleLogger() << " ";
  }
  LogInfo::MapleLogger() << " {";
  DumpClassOrInterfaceInfo(*this, indent);
  bool hasFieldOrMethod = !(fields.empty() && staticFields.empty() && parentFields.empty() && methods.empty());
  DumpInfoPragmaStaticValue(info, pragmaVec, staticValue, indent, hasFieldOrMethod);
  DumpFieldsAndMethods(indent, !methods.empty());
  LogInfo::MapleLogger() << "}>";
}

void MIRTypeByName::Dump(int, bool) const {
  const std::string &name = GlobalTables::GetStrTable().GetStringFromStrIdx(nameStrIdx);
  LogInfo::MapleLogger() << (nameIsLocal ? "<%" : "<$") << name << ">";
}

void MIRTypeParam::Dump(int, bool) const {
  const std::string &name = GlobalTables::GetStrTable().GetStringFromStrIdx(nameStrIdx);
  LogInfo::MapleLogger() << "<!" << name << ">";
}

void MIRInstantVectorType::Dump(int, bool) const {
  LogInfo::MapleLogger() << "{";
  for (size_t i = 0; i < instantVec.size(); ++i) {
    TypePair typePair = instantVec[i];
    if (i != 0) {
      LogInfo::MapleLogger() << ", ";
    }
    MIRType *typeParmType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(typePair.first);
    const std::string &name = GlobalTables::GetStrTable().GetStringFromStrIdx(typeParmType->GetNameStrIdx());
    LogInfo::MapleLogger() << "!" << name << "=";
    MIRType *realty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(typePair.second);
    realty->Dump(0);
  }
  LogInfo::MapleLogger() << "}";
}

void MIRGenericInstantType::Dump(int indent, bool dontUseName) const {
  LogInfo::MapleLogger() << "<";
  const MIRType *genericType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(genericTyIdx);
  DumpTypeName(genericType->GetNameStrIdx(), genericType->IsNameIsLocal());
  MIRInstantVectorType::Dump(indent, dontUseName);
  LogInfo::MapleLogger() << ">";
}

bool MIRType::EqualTo(const MIRType &mirType) const {
  return typeKind == mirType.typeKind && primType == mirType.primType;
}

bool MIRPtrType::EqualTo(const MIRType &type) const {
  if (typeKind != type.GetKind() || GetPrimType() != type.GetPrimType()) {
    return false;
  }
  const auto &pType = static_cast<const MIRPtrType&>(type);
  return pointedTyIdx == pType.GetPointedTyIdx() && typeAttrs == pType.GetTypeAttrs();
}

bool MIRArrayType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  const auto &pType = static_cast<const MIRArrayType&>(type);
  if (dim != pType.GetDim() || eTyIdx != pType.GetElemTyIdx() || typeAttrs != pType.GetTypeAttrs()) {
    return false;
  }
  for (size_t i = 0; i < dim; ++i) {
    if (GetSizeArrayItem(i) != pType.GetSizeArrayItem(i)) {
      return false;
    }
  }
  return true;
}

MIRType *MIRArrayType::GetElemType() const {
  return GlobalTables::GetTypeTable().GetTypeFromTyIdx(eTyIdx);
}

std::string MIRArrayType::GetMplTypeName() const {
  std::stringstream ss;
  ss << "<[] ";
  MIRType *elemType = GetElemType();
  ss << elemType->GetMplTypeName();
  ss << ">";
  return ss.str();
}

bool MIRArrayType::HasFields() const {
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(eTyIdx);
  return elemType->HasFields();
}

size_t MIRArrayType::NumberOfFieldIDs() const {
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(eTyIdx);
  return elemType->NumberOfFieldIDs();
}

MIRStructType *MIRArrayType::EmbeddedStructType() {
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(eTyIdx);
  return elemType->EmbeddedStructType();
}

int64 MIRArrayType::GetBitOffsetFromArrayAddress(std::vector<int64> &indexArray) {
  // for cases that num of dimension-operand is less than array's dimension
  // e.g. array 1 ptr <* <[d1][d2][d3] type>> (array_base_addr, dim1_opnd)
  // array's dimension is 3, and dimension-opnd's num is 1;
  if (indexArray.size() < dim) {
    indexArray.insert(indexArray.end(), dim - indexArray.size(), 0);
  }
  if (GetElemType()->GetKind() == kTypeArray) {
    // for cases that array's dimension is less than num of dimension-opnd
    // e.g array 1 ptr <* <[d1] <[d2] type>>> (array_base_addr, dim1_opnd, dim2_opnd)
    // array's dimension is 1 (its element type is <[d2] type>), and dimension-opnd-num is 2
    CHECK_FATAL(indexArray.size() >= dim, "dimension mismatch!");
  } else {
    CHECK_FATAL(indexArray.size() == dim, "dimension mismatch!");
  }
  int64 sum = 0; // element numbers before the specified element
  uint32 numberOfElemInLowerDim = 1;
  for (uint32 id = 1; id <= dim; ++id) {
    sum += indexArray[dim - id] * numberOfElemInLowerDim;
    numberOfElemInLowerDim *= sizeArray[dim - id];
  }
  size_t elemsize = GetElemType()->GetSize();
  if (elemsize == 0 || sum == 0) {
    return 0;
  }
  elemsize = RoundUp(elemsize, typeAttrs.GetAlign());
  constexpr int64 bitsPerByte = 8;
  int64 offset = sum * elemsize * bitsPerByte;
  if (GetElemType()->GetKind() == kTypeArray && indexArray.size() > dim) {
    std::vector<int64> subIndexArray(indexArray.begin() + dim, indexArray.end());
    offset += static_cast<MIRArrayType*>(GetElemType())->GetBitOffsetFromArrayAddress(subIndexArray);
  }
  return offset;
}

size_t MIRArrayType::ElemNumber() {
  size_t elemNum = 1;
  for (uint16 id = 0; id < dim; ++id) {
    elemNum *= sizeArray[id];
  }

  auto elemType = GetElemType();
  if (elemType->GetKind() == kTypeArray) {
    elemNum *= static_cast<MIRArrayType*>(elemType)->ElemNumber();
  }
  return elemNum;
}

MIRType *MIRFarrayType::GetElemType() const {
  return GlobalTables::GetTypeTable().GetTypeFromTyIdx(elemTyIdx);
}

bool MIRFarrayType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  const auto &pType = static_cast<const MIRFarrayType&>(type);
  return elemTyIdx == pType.GetElemTyIdx();
}

std::string MIRFarrayType::GetMplTypeName() const {
  std::stringstream ss;
  ss << "<[] ";
  MIRType *elemType = GetElemType();
  ss << elemType->GetMplTypeName();
  ss << ">";
  return ss.str();
}

bool MIRFarrayType::HasFields() const {
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(elemTyIdx);
  return elemType->HasFields();
}

size_t MIRFarrayType::NumberOfFieldIDs() const {
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(elemTyIdx);
  return elemType->NumberOfFieldIDs();
}

MIRStructType *MIRFarrayType::EmbeddedStructType() {
  MIRType *elemType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(elemTyIdx);
  return elemType->EmbeddedStructType();
}

int64 MIRFarrayType::GetBitOffsetFromArrayAddress(int64 arrayIndex) {
  size_t elemsize = GetElemType()->GetSize();
  if (elemsize == 0 || arrayIndex == 0) {
    return 0;
  }
  size_t elemAlign = GetElemType()->GetAlign();
  elemsize = RoundUp(elemsize, elemAlign);
  constexpr int64 bitsPerByte = 8;
  return arrayIndex * static_cast<int64>(elemsize) * bitsPerByte;
}

bool MIRFuncType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  const auto &pType = static_cast<const MIRFuncType&>(type);
  return (pType.retTyIdx == retTyIdx && pType.paramTypeList == paramTypeList &&
          pType.isVarArgs == isVarArgs && pType.paramAttrsList == paramAttrsList);
}

bool MIRBitFieldType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind || type.GetPrimType() != primType) {
    return false;
  }
  const auto &pType = static_cast<const MIRBitFieldType&>(type);
  return pType.fieldSize == fieldSize;
}

bool MIRStructType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  if (type.GetNameStrIdx() != nameStrIdx) {
    return false;
  }

  ASSERT(type.IsStructType(), "p is null in MIRStructType::EqualTo");
  const MIRStructType *p = static_cast<const MIRStructType*>(&type);
  if (fields != p->fields) {
    return false;
  }
  if (staticFields != p->staticFields) {
    return false;
  }
  if (parentFields != p->parentFields) {
    return false;
  }
  if (methods != p->methods) {
    return false;
  }
  return true;
}

std::string MIRStructType::GetCompactMplTypeName() const {
  return GlobalTables::GetStrTable().GetStringFromStrIdx(nameStrIdx);
}

MIRType *MIRStructType::GetElemType(uint32 n) const {
  return GlobalTables::GetTypeTable().GetTypeFromTyIdx(GetElemTyIdx(n));
}

MIRType *MIRStructType::GetFieldType(FieldID fieldID) {
  if (fieldID == 0) {
    return this;
  }
  const FieldPair &fieldPair = TraverseToFieldRef(fieldID);
  return GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldPair.second.first);
}

bool MIRStructType::IsLocal() const {
  return GlobalTables::GetGsymTable().GetStIdxFromStrIdx(nameStrIdx).Idx() != 0;
}

std::string MIRStructType::GetMplTypeName() const {
  std::stringstream ss;
  ss << "<$";
  ss << GlobalTables::GetStrTable().GetStringFromStrIdx(nameStrIdx);
  ss << ">";
  return ss.str();
}

bool MIRClassType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  bool structeq = MIRStructType::EqualTo(type);
  if (!structeq) {
    return false;
  }

  const MIRClassType &classty = static_cast<const MIRClassType &>(type);
  // classes have parent except empty/thirdparty classes
  if (parentTyIdx != classty.parentTyIdx) {
    return false;
  }

  if (interfacesImplemented != classty.interfacesImplemented) {
    return false;
  }
  if (info != classty.info) {
    return false;
  }
  if (infoIsString != classty.infoIsString) {
    return false;
  }
  return true;
}

bool MIRInterfaceType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  bool structeq = MIRStructType::EqualTo(type);
  if (!structeq) {
    return false;
  }

  const MIRInterfaceType &interfacety = static_cast<const MIRInterfaceType &>(type);
  if (parentsTyIdx != interfacety.parentsTyIdx) {
    return false;
  }
  if (info != interfacety.info) {
    return false;
  }
  if (infoIsString != interfacety.infoIsString) {
    return false;
  }
  return true;
}

bool MIRTypeByName::EqualTo(const MIRType &type) const {
  return type.GetKind() == typeKind && type.GetNameStrIdx() == nameStrIdx && type.IsNameIsLocal() == nameIsLocal;
}

bool MIRTypeParam::EqualTo(const MIRType &type) const {
  return type.GetKind() == typeKind && type.GetNameStrIdx() == nameStrIdx;
}

bool MIRInstantVectorType::EqualTo(const MIRType &type) const {
  if (type.GetKind() != typeKind) {
    return false;
  }
  const auto &pty = static_cast<const MIRInstantVectorType&>(type);
  return (instantVec == pty.GetInstantVec());
}

bool MIRGenericInstantType::EqualTo(const MIRType &type) const {
  if (!MIRInstantVectorType::EqualTo(type)) {
    return false;
  }
  const auto &pType = static_cast<const MIRGenericInstantType&>(type);
  return genericTyIdx == pType.GetGenericTyIdx();
}

// in the search, curfieldid is being decremented until it reaches 1
FieldPair MIRStructType::TraverseToFieldRef(FieldID &fieldID) const {
  if (!fields.size()) {
    return FieldPair(GStrIdx(0), TyIdxFieldAttrPair(TyIdx(0), FieldAttrs()));
  }

  uint32 fieldIdx = 0;
  FieldPair curPair = fields[0];
  while (fieldID > 1) {
    --fieldID;
    MIRType *curFieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(curPair.second.first);
    MIRStructType *subStructTy = curFieldType->EmbeddedStructType();
    if (subStructTy != nullptr) {
      curPair = subStructTy->TraverseToFieldRef(fieldID);
      if (fieldID == 1 && curPair.second.first != TyIdx(0)) {
        return curPair;
      }
    }
    ++fieldIdx;
    if (fieldIdx == fields.size()) {
      return FieldPair(GStrIdx(0), TyIdxFieldAttrPair(TyIdx(0), FieldAttrs()));
    }
    curPair = fields[fieldIdx];
  }
  return curPair;
}

FieldPair MIRStructType::TraverseToField(FieldID fieldID) const {
  if (fieldID >= 0) {
    return TraverseToFieldRef(fieldID);
  }
  // in parentfields
  uint32 parentFieldIdx = -fieldID;
  if (parentFields.empty() || parentFieldIdx > parentFields.size()) {
    return { GStrIdx(0), TyIdxFieldAttrPair(TyIdx(0), FieldAttrs()) };
  }
  return parentFields[parentFieldIdx - 1];
}

static bool TraverseToFieldInFields(const FieldVector &fields, const GStrIdx &fieldStrIdx, FieldPair &field) {
  for (auto &fp : fields) {
    if (fp.first == fieldStrIdx) {
      field = fp;
      return true;
    }
  }
  return false;
}

FieldPair MIRStructType::TraverseToField(GStrIdx fieldStrIdx) const {
  FieldPair fieldPair;
  if ((!fields.empty() && TraverseToFieldInFields(fields, fieldStrIdx, fieldPair)) ||
      (!staticFields.empty() && TraverseToFieldInFields(staticFields, fieldStrIdx, fieldPair)) ||
      TraverseToFieldInFields(parentFields, fieldStrIdx, fieldPair)) {
    return fieldPair;
  }
  return { GStrIdx(0), TyIdxFieldAttrPair(TyIdx(0), FieldAttrs()) };
}

bool MIRStructType::HasVolatileFieldInFields(const FieldVector &fieldsOfStruct) const {
  for (const auto &field : fieldsOfStruct) {
    if (field.second.second.GetAttr(FLDATTR_volatile) ||
        GlobalTables::GetTypeTable().GetTypeFromTyIdx(field.second.first)->HasVolatileField()) {
      hasVolatileField = true;
      return true;
    }
  }
  hasVolatileField = false;
  return false;
}

// go through all the fields to check if there is volatile attribute set;
bool MIRStructType::HasVolatileField() const {
  if (hasVolatileFieldSet) {
    return hasVolatileField;
  }
  hasVolatileFieldSet = true;
  return HasVolatileFieldInFields(fields) || HasVolatileFieldInFields(parentFields);
}

int64 MIRStructType::GetBitOffsetFromUnionBaseAddr(FieldID fieldID) {
  CHECK_FATAL(fieldID <= NumberOfFieldIDs(), "GetBitOffsetFromUnionBaseAddr: fieldID too large");
  if (fieldID == 0) {
    return 0;
  }

  FieldID curFieldID = 1;
  FieldVector fieldPairs = GetFields();
  // for unions, bitfields are treated as non-bitfields
  for (FieldPair field : fieldPairs) {
    TyIdx fieldTyIdx = field.second.first;
    MIRType *fieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldTyIdx);
    if (curFieldID == fieldID) {
      // target field id is found
      // offset of field in union direcly(i.e. not embedded in other struct) is zero.
      return 0;
    } else {
      MIRStructType *subStructType = fieldType->EmbeddedStructType();
      if (subStructType == nullptr) {
        // field is not a complex structure, no extra field id in it, just inc and continue to next field
        curFieldID++;
      } else {
        // if target field id is in the embedded structure, we should go into it by recursively call
        // otherwise, just add the field-id number of the embedded structure, and continue to next field
        if ((curFieldID + subStructType->NumberOfFieldIDs()) < fieldID) {
          curFieldID += subStructType->NumberOfFieldIDs() + 1; // 1 represents subStructType itself
        } else {
          return subStructType->GetBitOffsetFromBaseAddr(fieldID - curFieldID);
        }
      }
    }
  }
  CHECK_FATAL(false, "GetBitOffsetFromUnionBaseAddr() fails to find field");
  return kOffsetUnknown;
}

int64 MIRStructType::GetBitOffsetFromStructBaseAddr(FieldID fieldID) {
  CHECK_FATAL(fieldID <= NumberOfFieldIDs(), "GetBitOffsetFromUnionBaseAddr: fieldID too large");
  if (fieldID == 0) {
    return 0;
  }

  uint64 allocedSize = 0; // space need for all fields before currentField
  uint64 allocedBitSize = 0;
  FieldID curFieldID = 1;
  constexpr uint64 bitsPerByte = 8; // 8 bits per byte
  FieldVector fieldPairs = GetFields();
  // process the struct fields
  // There are 4 possible kinds of field in a MIRStructureType:
  //  case 1 : bitfield (including zero-width bitfield);
  //  case 2 : empty structure(struct/union) field;
  //  case 3 : normal (not empty) structure(struct/union) field;
  //  case 4 : primtive field;
  for (FieldPair field : fieldPairs) {
    TyIdx fieldTyIdx = field.second.first;
    MIRType *fieldType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(fieldTyIdx);
    // case 1 : bitfield (including zero-width bitfield);
    if (fieldType->GetKind() == kTypeBitField) {
      uint32 fieldBitSize = static_cast<MIRBitFieldType*>(fieldType)->GetFieldSize();
      uint32 fieldAlign = GetPrimTypeSize(fieldType->GetPrimType());
      // Is this field is crossing the align boundary of its base type?
      // for example:
      // struct Expamle {
      //      int32 fld1 : 30;
      //      int32 fld2 : 3;  // 30 + 3 > 32(= int32 align), cross the align boundary, start from next int32 boundary
      // }
      //
      // Is field a zero-width bit field?
      // Refer to C99 standard (§6.7.2.1) :
      // > As a special case, a bit-field structure member with a width of 0 indicates that no further
      // > bit-field is to be packed into the unit in which the previous bit-field, if any, was placed.
      //
      // We know that A zero-width bit field can cause the next field to be aligned on the next container
      // boundary where the container is the same size as the underlying type of the bit field.
      // for example:
      // struct Example {
      //     int32 fld1 : 5;
      //     int32      : 0; // force the next field to be aligned on int32 align boundary
      //     int32 fld3 : 4;
      // }
      if (((allocedBitSize / (fieldAlign * bitsPerByte)) !=
          ((allocedBitSize + fieldBitSize - 1u) / (fieldAlign * bitsPerByte))) ||
          fieldBitSize == 0) {
        // the field is crossing the align boundary of its base type;
        // align alloced_size_in_bits to fieldAlign
        allocedBitSize = RoundUp(allocedBitSize, fieldAlign * bitsPerByte);
      }
      // target field id is found
      if (curFieldID == fieldID) {
        return allocedBitSize;
      } else {
        ++curFieldID;
      }
      allocedBitSize += fieldBitSize;
      allocedSize = std::max(allocedSize, RoundUp(allocedBitSize, fieldAlign * bitsPerByte) / bitsPerByte);
      continue;
    } // case 1 end

    size_t fieldTypeSize = fieldType->GetSize();
    uint32 fieldAlign = fieldType->GetAlign();
    uint64 fieldAlignBits = fieldAlign * bitsPerByte;

    // case 2 : empty structure(struct/union) field;
    // if fieldTypeSize is 0, no extra space will be allocated, just return or continue to next field
    if (fieldTypeSize == 0) {
      if (curFieldID == fieldID) {
        return allocedBitSize;
      }
      ++curFieldID;
      continue;
    } // case 2 end

    uint32 fldSizeInBits = fieldTypeSize * bitsPerByte;
    bool leftOverBits = false;
    uint64 offset = 0;
    // no bit field before current field
    if (allocedBitSize == allocedSize * bitsPerByte) {
      allocedSize = RoundUp(allocedSize, fieldAlign);
      offset = allocedSize;
    } else {
      // still some leftover bits on allocated words, we calculate things based on bits then.
      if (allocedBitSize / fieldAlignBits != (allocedBitSize + fldSizeInBits - 1) / fieldAlignBits) {
        // the field is crossing the align boundary of its base type
        allocedBitSize = RoundUp(allocedBitSize, fieldAlignBits);
      }
      allocedSize = RoundUp(allocedSize, fieldAlign);
      offset = (allocedBitSize / fieldAlignBits) * fieldAlign;
      leftOverBits = true;
    }
    // target field id is found
    if (curFieldID == fieldID) {
      return offset * bitsPerByte;
    }
    // case 4 : primtive field;
    MIRStructType *subStructType = fieldType->EmbeddedStructType();
    if (subStructType == nullptr) {
      ++curFieldID;
    } else {
      // case 3 : normal (not empty) structure(struct/union) field;
      // if target field id is in the embedded structure, we should go into it by recursively call
      // otherwise, just add the field-id number of the embedded structure, and continue to next field
      if ((curFieldID + subStructType->NumberOfFieldIDs()) < fieldID) {
        curFieldID += subStructType->NumberOfFieldIDs() + 1; // 1 represents subStructType itself
      } else {
        int64 result = subStructType->GetBitOffsetFromBaseAddr(fieldID - curFieldID);
        return result + allocedSize * bitsPerByte;
      }
    }

    if (leftOverBits) {
      allocedBitSize += fldSizeInBits;
      allocedSize = std::max(allocedSize, RoundUp(allocedBitSize, fieldAlignBits) / bitsPerByte);
    } else {
      allocedSize += fieldTypeSize;
      allocedBitSize = allocedSize * bitsPerByte;
    }
  }
  CHECK_FATAL(false, "GetBitOffsetFromStructBaseAddr() fails to find field");
  return kOffsetUnknown;
}

int64 MIRStructType::GetBitOffsetFromBaseAddr(FieldID fieldID) {
  CHECK_FATAL(fieldID <= NumberOfFieldIDs(), "GetBitOffsetFromBaseAddr: fieldID too large");
  if (fieldID == 0) {
    return 0;
  }
  switch (GetKind()) {
    case kTypeClass: {
      // NYI: should know class layout, for different language, the result is different
      return kOffsetUnknown; // Invalid offset
    }
    case kTypeUnion: {
      return GetBitOffsetFromUnionBaseAddr(fieldID);
    }
    case kTypeStruct: {
      return GetBitOffsetFromStructBaseAddr(fieldID);
    }
    default: {
      CHECK_FATAL(false, "Wrong type kind for MIRStructType!");
    }
  }
  CHECK_FATAL(false, "Should never reach here!");
  return kOffsetUnknown;
}

// set hasVolatileField to true if parent type has volatile field, otherwise flase.
static bool ParentTypeHasVolatileField(const TyIdx parentTyIdx, bool &hasVolatileField) {
  hasVolatileField = (GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx)->HasVolatileField());
  return hasVolatileField;
}

// go through all the fields to check if there is volatile attribute set;
bool MIRClassType::HasVolatileField() const {
  return MIRStructType::HasVolatileField() ||
         (parentTyIdx != 0u && ParentTypeHasVolatileField(parentTyIdx, hasVolatileField));
}

// go through all the fields to check if there is volatile attribute set;
bool MIRInterfaceType::HasVolatileField() const {
  if (MIRStructType::HasVolatileField()) {
    return true;
  }
  for (TyIdx parentTypeIdx : parentsTyIdx) {
    if (ParentTypeHasVolatileField(parentTypeIdx, hasVolatileField)) {
      return true;
    }
  }
  return false;
}

bool MIRStructType::HasTypeParamInFields(const FieldVector &fieldsOfStruct) const {
  for (const FieldPair &field : fieldsOfStruct) {
    if (field.second.second.GetAttr(FLDATTR_generic)) {
      return true;
    }
  }
  return false;
}

// go through all the fields to check if there is generic attribute set;
bool MIRStructType::HasTypeParam() const {
  return HasTypeParamInFields(fields) || HasTypeParamInFields(parentFields);
}

bool MIRClassType::HasTypeParam() const {
  return MIRStructType::HasTypeParam() ||
         (parentTyIdx != 0u && GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx)->HasTypeParam());
}

bool MIRInterfaceType::HasTypeParam() const {
  if (MIRStructType::HasTypeParam()) {
    return true;
  }
  // check if the parent classes have type parameter
  for (TyIdx parentTypeIdx : parentsTyIdx) {
    if (GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTypeIdx)->HasTypeParam()) {
      return true;
    }
  }
  return false;
}

size_t MIRClassType::NumberOfFieldIDs() const {
  size_t parentFieldIDs = 0;
  if (parentTyIdx != TyIdx(0)) {
    MIRType *parentty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx);
    parentFieldIDs = parentty->NumberOfFieldIDs();
  }
  return parentFieldIDs + MIRStructType::NumberOfFieldIDs();
}

FieldPair MIRClassType::TraverseToFieldRef(FieldID &fieldID) const {
  if (parentTyIdx != 0u) {
    auto *parentClassType =
        static_cast<MIRClassType*>(GlobalTables::GetTypeTable().GetTypeFromTyIdx(parentTyIdx));
    if (parentClassType != nullptr) {
      --fieldID;
      const FieldPair &curPair = parentClassType->TraverseToFieldRef(fieldID);
      if (fieldID == 1 && curPair.second.first != 0u) {
        return curPair;
      }
    }
  }
  return MIRStructType::TraverseToFieldRef(fieldID);
}

// fields in interface are all static and are global, won't be accessed through fields
FieldPair MIRInterfaceType::TraverseToFieldRef(FieldID&) const {
  return { GStrIdx(0), TyIdxFieldAttrPair(TyIdx(0), FieldAttrs()) };
}

bool MIRPtrType::IsPointedTypeVolatile(int fieldID) const {
  if (typeAttrs.GetAttr(ATTR_volatile)) {
    return true;
  }
  MIRType *pointedTy = GlobalTables::GetTypeTable().GetTypeFromTyIdx(GetPointedTyIdx());
  return pointedTy->IsVolatile(fieldID);
}

size_t MIRPtrType::GetSize() const { return POINTER_SIZE; }
uint32 MIRPtrType::GetAlign() const { return POINTER_SIZE; }

TyIdxFieldAttrPair MIRPtrType::GetPointedTyIdxFldAttrPairWithFieldID(FieldID fldId) const {
  if (fldId == 0) {
    return TyIdxFieldAttrPair(pointedTyIdx, FieldAttrs());
  }
  MIRType *ty = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointedTyIdx);
  MIRStructType *structTy = ty->EmbeddedStructType();
  if (structTy == nullptr) {
    // this can happen due to casting in C; just return the pointed to type
    return TyIdxFieldAttrPair(pointedTyIdx, FieldAttrs());
  }
  return structTy->TraverseToField(fldId).second;
}

TyIdx MIRPtrType::GetPointedTyIdxWithFieldID(FieldID fieldID) const {
  return GetPointedTyIdxFldAttrPairWithFieldID(fieldID).first;
}

std::string MIRPtrType::GetMplTypeName() const {
  std::stringstream ss;
  ss << "<* ";
  MIRType *pointedType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointedTyIdx);
  CHECK_FATAL(pointedType != nullptr, "invalid ptr type");
  ss << pointedType->GetMplTypeName();
  ss << ">";
  return ss.str();
}

std::string MIRPtrType::GetCompactMplTypeName() const {
  MIRType *pointedType = GlobalTables::GetTypeTable().GetTypeFromTyIdx(pointedTyIdx);
  CHECK_FATAL(pointedType != nullptr, "invalid ptr type");
  return pointedType->GetCompactMplTypeName();
}

size_t MIRStructType::NumberOfFieldIDs() const {
  size_t count = 0;
  for (FieldPair curpair : fields) {
    count++;
    MIRType *curfieldtype = GlobalTables::GetTypeTable().GetTypeFromTyIdx(curpair.second.first);
    count += curfieldtype->NumberOfFieldIDs();
  }
  return count;
}

TypeAttrs FieldAttrs::ConvertToTypeAttrs() {
  TypeAttrs attr;
  constexpr uint32 maxAttrNum = 64;
  for (uint32 i = 0; i < maxAttrNum; ++i) {
    if ((attrFlag & (1ULL << i)) == 0) {
      continue;
    }
    auto attrKind = static_cast<FieldAttrKind>(i);
    switch (attrKind) {
#define FIELD_ATTR
#define ATTR(STR)               \
    case FLDATTR_##STR:         \
      attr.SetAttr(ATTR_##STR); \
      break;
#include "all_attributes.def"
#undef ATTR
#undef FIELD_ATTR
      default:
        ASSERT(false, "unknown TypeAttrs");
        break;
    }
  }
  return attr;
}

TypeAttrs GenericAttrs::ConvertToTypeAttrs() {
  TypeAttrs attr;
  constexpr uint32 maxAttrNum = 64;
  for (uint32 i = 0; i < maxAttrNum; ++i) {
    if ((attrFlag & (1ULL << i)) == 0) {
      continue;
    }
    auto tA = static_cast<GenericAttrKind>(i);
    switch (tA) {
#define TYPE_ATTR
#define ATTR(STR)               \
    case GENATTR_##STR:         \
      attr.SetAttr(ATTR_##STR); \
      break;
#include "all_attributes.def"
#undef ATTR
#undef TYPE_ATTR
      default:
        ASSERT(false, "unknown TypeAttrs");
        break;
    }
  }
  return attr;
}

FuncAttrs GenericAttrs::ConvertToFuncAttrs() {
  FuncAttrs attr;
  constexpr uint32 maxAttrNum = 64;
  for (uint32 i = 0; i < maxAttrNum; ++i) {
    if ((attrFlag & (1ULL << i)) == 0) {
      continue;
    }
    auto tA = static_cast<GenericAttrKind>(i);
    switch (tA) {
#define FUNC_ATTR
#define ATTR(STR)                   \
    case GENATTR_##STR:             \
      attr.SetAttr(FUNCATTR_##STR); \
      break;
#include "all_attributes.def"
#undef ATTR
#undef FUNC_ATTR
      default:
        ASSERT(false, "unknown FuncAttrs");
        break;
    }
  }
  return attr;
}

FieldAttrs GenericAttrs::ConvertToFieldAttrs() {
  FieldAttrs attr;
  constexpr uint32 maxAttrNum = 64;
  for (uint32 i = 0; i < maxAttrNum; ++i) {
    if ((attrFlag & (1ULL << i)) == 0) {
      continue;
    }
    auto tA = static_cast<GenericAttrKind>(i);
    switch (tA) {
#define FIELD_ATTR
#define ATTR(STR)                  \
    case GENATTR_##STR:            \
      attr.SetAttr(FLDATTR_##STR); \
      break;
#include "all_attributes.def"
#undef ATTR
#undef FIELD_ATTR
      default:
        ASSERT(false, "unknown FieldAttrs");
        break;
    }
  }
  return attr;
}
}  // namespace maple
#endif  // MIR_FEATURE_FULL
