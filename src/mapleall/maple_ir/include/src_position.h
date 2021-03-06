/*
 * Copyright (c) [2021] Huawei Technologies Co.,Ltd.All rights reserved.
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
#ifndef MAPLE_IR_INCLUDE_SRC_POSITION_H
#define MAPLE_IR_INCLUDE_SRC_POSITION_H

namespace maple {
// to store source position information
class SrcPosition {
 public:
  SrcPosition() : lineNum(0), mplLineNum(0) {
    u.word0 = 0;
  }

  virtual ~SrcPosition() = default;

  uint32 RawData() const {
    return u.word0;
  }

  uint32 FileNum() const {
    return u.fileColumn.fileNum;
  }

  uint32 Column() const {
    return u.fileColumn.column;
  }

  uint32 LineNum() const {
    return lineNum;
  }

  uint32 MplLineNum() const {
    return mplLineNum;
  }

  void SetFileNum(uint16 n) {
    u.fileColumn.fileNum = n;
  }

  void SetColumn(uint16 n) {
    u.fileColumn.column = n;
  }

  void SetLineNum(uint32 n) {
    lineNum = n;
  }

  void SetRawData(uint32 n) {
    u.word0 = n;
  }

  void SetMplLineNum(uint32 n) {
    mplLineNum = n;
  }

  void CondSetLineNum(uint32 n) {
    lineNum = lineNum ? lineNum : n;
  }

  void CondSetFileNum(uint16 n) {
    uint16 i = u.fileColumn.fileNum;
    u.fileColumn.fileNum = i ? i : n;
  }

 private:
  union {
    struct {
      uint16 fileNum;
      uint16 column : 12;
      uint16 stmtBegin : 1;
      uint16 bbBegin : 1;
      uint16 unused : 2;
    } fileColumn;
    uint32 word0;
  } u;
  uint32 lineNum;     // line number of original src file, like foo.java
  uint32 mplLineNum;  // line number of mpl file
};
}  // namespace maple
#endif  // MAPLE_IR_INCLUDE_SRC_POSITION_H