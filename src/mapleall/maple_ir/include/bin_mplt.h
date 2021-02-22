/*
 * Copyright (c) [2020] Huawei Technologies Co.,Ltd.All rights reserved.
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
#ifndef MAPLE_IR_INCLUDE_BIN_MPLT_H
#define MAPLE_IR_INCLUDE_BIN_MPLT_H
#include "mir_module.h"
#include "mir_nodes.h"
#include "mir_preg.h"
#include "parser_opt.h"
#include "bin_mpl_export.h"
#include "bin_mpl_import.h"

namespace maple {
class BinaryMplt {
 public:
  std::vector<std::string> seUsage = {
    "out/soong/.intermediates/vendor/huawei/maple/Lib/core/libmaplecore-all/android_arm64_armv8-a_core_shared/obj/classes.mpl",
    "out/soong/.intermediates/vendor/huawei/maple/Lib/services/libmapleservices/android_arm64_armv8-a_core_shared/obj/classes.mrg.mpl",
    "out/target/product/generic_a15/obj/SHARED_LIBRARIES/libmaplehwServices_intermediates/classes.mpl",
    "out/soong/.intermediates/vendor/huawei/maple/Lib/frameworks/libmapleframework/android_arm64_armv8-a_core_shared/obj/classes.mrg.mpl",
    "libcore-all.mpl",
    "./libcore-all.mpl"
  };

  explicit BinaryMplt(MIRModule &md) : mirModule(md), binImport(md), binExport(md) {}

  virtual ~BinaryMplt() = default;

  void Export(const std::string &suffix) {
    binExport.Export(suffix);
  }

  bool Import(const std::string &modID, bool readCG = false, bool readSE = false) {
    bool found = true;
    for (size_t i = 0; i < seUsage.size(); ++i) {
      if (seUsage[i] == mirModule.GetFileName()) {
        found = true;
        break;
      }
    }
    readSE = readSE && found;
#ifdef MPLT_DEBUG
    if (readCG) {
      LogInfo::MapleLogger() << "READING CG mpl file : " << _mod.fileName << '\n';
    }
    if (readSE) {
      LogInfo::MapleLogger() << "READING SE mpl file : " << _mod.fileName << '\n';
    }
#endif
    importFileName = modID;
    return binImport.Import(modID, readCG, readSE);
  }

  const MIRModule &GetMod() const {
    return mirModule;
  }

  BinaryMplImport &GetBinImport() {
    return binImport;
  }

  BinaryMplExport &GetBinExport() {
    return binExport;
  }

  const std::string &GetImportFileName() const {
    return importFileName;
  }

 private:
  MIRModule &mirModule;
  BinaryMplImport binImport;
  BinaryMplExport binExport;
  std::string importFileName;
};
}  // namespace maple
#endif  // MAPLE_IR_INCLUDE_BIN_MPLT_H
