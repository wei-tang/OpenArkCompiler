/*
 * Copyright (c) [2019-2020] Huawei Technologies Co.,Ltd.All rights reserved.
 *
 * OpenArkCompiler is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *
 *     http://license.coscl.org.cn/MulanPSL
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
 * FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v1 for more details.
 */
#ifndef MAPLE_IR_INCLUDE_METADATA_LAYOUT_H
#define MAPLE_IR_INCLUDE_METADATA_LAYOUT_H
#include <cstdlib>

// metadata layout is shared between maple compiler and runtime, thus not in namespace maplert
// some of the reference field of metadata is stored as relative offset
// for example, declaring class of Fields/Methods
// which can be negative
#ifdef USE_32BIT_REF
using MetaRef = uint32_t;      // consistent with reffield_t in address.h
#else
using MetaRef = uintptr_t;     // consistent iwth reffield_t in address.h
#endif // USE_32BIT_REF

// DataRefOffset aims to represent a reference to data in maple file, which is already an offset.
// DataRefOffset is meant to have pointer size and aligned to at least 4 bytes.
// All Xx32 data types defined in this file aim to use 32 bits to save 64-bit address, and thus are
// specific for 64-bit platforms.
struct DataRefOffset32 {
  int32_t refOffset;
  template<typename T>
  inline void SetDataRef(T ref);
  template<typename T>
  inline T GetDataRef() const;
  inline int32_t GetRawValue() const;
  inline void SetRawValue(int64_t value);
};

struct DataRefOffset64 {
  intptr_t refOffset;
  template<typename T>
  inline void SetDataRef(T ref);
  template<typename T>
  inline T GetDataRef() const;
  inline bool IsCompact() const;
  template<typename T>
  inline T GetCompactData() const;
  inline int64_t GetRawValue() const;
  inline void SetRawValue(int64_t value);
};

struct DataRefOffset {
#ifdef USE_32BIT_REF
  DataRefOffset32 refOffset;
#else
  DataRefOffset64 refOffset;
#endif
  template<typename T>
  inline void SetDataRef(T ref);
  template<typename T>
  inline T GetDataRef() const;
  inline int64_t GetRawValue() const;
  inline void SetRawValue(int64_t value);
};
/* DataRef aims for reference to data in maple file (generated by maple compiler) and is aligned to at least 4 bytes.
   Perhaps MDataRef is more fit, still DataRef is chosen to make it common.
   DataRef allows 4 formats of value:
   0. "label_name" for direct reference
   1. "label_name - . + 1" for historical compact metadata reference
   2. "label_name - . + 2" for reference in offset format
   3. "indirect.label_name - . + 3" for indirect reference
      this format aims to support lld which does not support expression "global_symbol - ."
   DataRef is self-decoded by also encoding the format and is defined for binary compatibility.
   If no compatibility problem is involved, DataRefOffset is preferred.
 */
enum DataRefFormat {
  kDataRefIsDirect   = 0, // must be 0
  kDataRefIsCompact  = 1, // read-only
  kDataRefIsOffset   = 2,
  kDataRefIsIndirect = 3, // read-only
  kDataRefBitMask    = 3,
};

struct DataRef32 {
  // be careful when *refVal* is treated as an offset which is a signed integer actually.
  uint32_t refVal;
  template<typename T>
  inline T GetDataRef() const;
  template<typename T>
  inline void SetDataRef(T ref, DataRefFormat format = kDataRefIsDirect);
  template<typename T>
  inline T GetRawValue() const;
};

struct DataRef {
  uintptr_t refVal;
  template<typename T>
  inline T GetDataRef() const;
  template<typename T>
  inline void SetDataRef(const T ref, const DataRefFormat format = kDataRefIsDirect);
  template<typename T>
  inline T GetRawValue() const;
};

/* GctibRef aims to represent a reference to gctib in maple file, which is an offset by default.
   GctibRef is meant to have pointer size and aligned to at least 4 bytes.
   GctibRef allows 2 formats of value:
   0. "label_name - . + 0" for reference in offset format
   1. "indirect.label_name - . + 1" for indirect reference
      this format aims to support lld which does not support expression "global_symbol - ."
   GctibRef is self-decoded by also encoding the format and is defined for binary compatibility.
   If no compatibility problem is involved, DataRef is preferred.
 */
enum GctibRefFormat {
  kGctibRefIsOffset = 0, // default
  kGctibRefIsIndirect = 1,
  kGctibRefBitMask = 3
};

struct GctibRef32 {
  // be careful when *refVal* is treated as an offset which is a signed integer actually.
  uint32_t refVal;
  template<typename T>
  inline T GetGctibRef() const;
  template<typename T>
  inline void SetGctibRef(T ref, GctibRefFormat format = kGctibRefIsOffset);
};

struct GctibRef {
  uintptr_t refVal;
  template<typename T>
  inline T GetGctibRef() const;
  template<typename T>
  inline void SetGctibRef(const T ref, const GctibRefFormat format = kGctibRefIsOffset);
};

// MByteRef is meant to represent a reference to data defined in maple file. It is a direct reference or an offset.
// MByteRef is self-encoded/decoded and aligned to 1 byte.
// Unlike DataRef, the format of MByteRef is determined by its value.
struct MByteRef {
  uintptr_t refVal; // initializer prefers this field to be a pointer

#if defined(__arm__) || defined(USE_ARM32_MACRO)
  // assume address range 0 ~ 256MB is unused in arm runtime
  // OffsetMin ~ OffsetMax is the value range of encoded offset
  static constexpr intptr_t FarthestOffset = 128 * 1024 * 1024;
  static constexpr intptr_t PositiveOffsetBias = 128 * 1024 * 1024;
  static constexpr intptr_t OffsetMin = PositiveOffsetBias + 0;
  static constexpr intptr_t OffsetMax = PositiveOffsetBias + FarthestOffset;
#else
  enum {
    kBiasBitPosition = sizeof(refVal) * 8 - 4, // the most significant 4 bits
  };

  static constexpr uintptr_t FarthestOffset = 256 * 1024 * 1024; // according to kDsoLoadedAddessEnd = 0xF0000000

  static constexpr uintptr_t PositiveOffsetBias = static_cast<uintptr_t>(6) << kBiasBitPosition;
  static constexpr uintptr_t PositiveOffsetMin = 0 + PositiveOffsetBias;
  static constexpr uintptr_t PositiveOffsetMax = FarthestOffset + PositiveOffsetBias;
#endif

  template<typename T>
  inline T GetRef() const;
  template<typename T>
  inline void SetRef(const T ref);
  inline bool IsOffset() const;
};

struct MByteRef32 {
  uint32_t refVal;
  static constexpr uint32_t FarthestOffset = 256 * 1024 * 1024; // according to kDsoLoadedAddessEnd = 0xF0000000

  static constexpr uint32_t PositiveOffsetBias = 0x60000000; // the most significant 4 bits 0110
  static constexpr uint32_t PositiveOffsetMin = 0 + PositiveOffsetBias;
  static constexpr uint32_t PositiveOffsetMax = FarthestOffset + PositiveOffsetBias;

  static constexpr uint32_t DirectRefMin = 0xC0000000; // according to kDsoLoadedAddessStart = 0xC0000000
  static constexpr uint32_t DirectRefMax = 0xF0000000; // according to kDsoLoadedAddessEnd = 0xF0000000

  static constexpr int32_t NegativeOffsetMin = -FarthestOffset;
  static constexpr int32_t NegativeOffsetMax = 0;

  template<typename T>
  inline T GetRef() const;
  template<typename T>
  inline void SetRef(T ref);
  inline bool IsOffset() const;
  inline bool IsPositiveOffset() const;
  inline bool IsNegativeOffset() const;
};

// MethodMeta defined in MethodMeta.h
// FieldMeta  defined in FieldMeta.h
// MethodDesc contains MethodMetadata and stack map
struct MethodDesc {
  // relative offset for method metadata relative to current PC.
  // method metadata is in compact format if this offset is odd.
  uint32_t metadataOffset;

  uint16_t localRefOffset;
  uint16_t localRefNumber;

  // stack map for a methed might be placed here
};

// Note: class init in maplebe and cg is highly dependent on this type.
// update aarch64rtsupport.h if you modify this definition.
struct ClassMetadataRO {
  MByteRef className;
  DataRefOffset64 fields;  // point to info of fields
  DataRefOffset64 methods; // point to info of methods
  union {  // Element classinfo of array, others parent classinfo
    DataRef superclass;
    DataRef componentClass;
  };

  uint16_t numOfFields;
  uint16_t numOfMethods;

#ifndef USE_32BIT_REF
  uint16_t flag;
  uint16_t numOfSuperclasses;
  uint32_t padding;
#endif // !USE_32BIT_REF

  uint32_t mod;
  DataRefOffset32 annotation;
  DataRefOffset32 clinitAddr;
};

static constexpr size_t PageSize = 4096;

// according to kSpaceAnchor and kFireBreak defined in bpallocator.cpp
// the address of this readable page is set as kClassInitialized for java class
static constexpr uintptr_t kClassInitializedState = 0xC0000000 - (1u << 20) * 2;

extern "C" uint8_t classInitProtectRegion[];

// Note there is no state to indicate a class is already initialized.
// Any state beyond listed below is treated as initialized.
enum ClassInitState {
  kClassInitStateMin   = 0,
  kClassUninitialized  = 1,
  kClassInitializing   = 2,
  kClassInitFailed     = 3,
  kClassInitialized    = 4,
  kClassInitStateMax   = 4,
};

enum SEGVAddr {
  kSEGVAddrRangeStart            = PageSize + 0,

  // Note any readable address is treated as Initialized.
  kSEGVAddrForClassInitStateMin  = kSEGVAddrRangeStart + kClassInitStateMin,
  kSEGVAddrForClassUninitialized = kSEGVAddrForClassInitStateMin + kClassUninitialized,
  kSEGVAddrForClassInitializing  = kSEGVAddrForClassInitStateMin + kClassInitializing,
  kSEGVAddrForClassInitFailed    = kSEGVAddrForClassInitStateMin + kClassInitFailed,
  kSEGVAddrFoClassInitStateMax   = kSEGVAddrForClassInitStateMin + kClassInitStateMax,

  kSEGVAddrRangeEnd,
};

struct ClassMetadata {
  // object common fields
  MetaRef shadow;  // point to classinfo of java/lang/Class
  int32_t monitor;

  // other fields
  uint16_t clIndex; // 8bit ClassLoader index, used for querying the address of related ClassLoader instance.
  union {
    uint16_t objSize;
    uint16_t componentSize;
  } sizeInfo;

#ifdef USE_32BIT_REF // for alignment purpose
  uint16_t flag;
  uint16_t numOfSuperclasses;
#endif // USE_32BIT_REF

  DataRef iTable;  // iTable of current class, used for virtual call, will insert the content into classinfo
  DataRef vTable;  // vTable of current class, used for interface call, will insert the content into classinfo
  GctibRef gctib;  // for rc

#ifdef USE_32BIT_REF
  DataRef32 classInfoRo;
  DataRef32 cacheFalseClass;
#else
  DataRef classInfoRo;
#endif

  union {
    uintptr_t initState; // a readable address for initState means initialized
    DataRef cacheTrueClass;
  };

 public:
  static inline intptr_t OffsetOfInitState() {
    ClassMetadata *base = nullptr;
    return reinterpret_cast<intptr_t>(&(base->initState));
  }

  uintptr_t GetInitStateRawValue() const {
    return __atomic_load_n(&initState, __ATOMIC_ACQUIRE);
  }

  template<typename T>
  void SetInitStateRawValue(T val) {
    __atomic_store_n(&initState, reinterpret_cast<uintptr_t>(val), __ATOMIC_RELEASE);
  }
};

// function to set Class/Field/Method metadata's shadow field to avoid type conversion
// Note 1: here we don't do NULL-check and type-compatibility check
// NOte 2: C should be of jclass/ClassMetata* type
template<typename M, typename C>
static inline void MRTSetMetadataShadow(M *meta, C cls) {
  meta->shadow = (MetaRef)(uintptr_t)cls;
}

#endif // METADATA_LAYOUT_H
