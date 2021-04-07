/*
 * Copyright (c) [2019-2020] Huawei Technologies Co.,Ltd.All rights reserved.
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
#ifndef MAPLE_DRIVER_INCLUDE_COMPILER_H
#define MAPLE_DRIVER_INCLUDE_COMPILER_H
#include <map>
#include <unordered_set>
#include "error_code.h"
#include "mpl_options.h"
#include "cg_option.h"
#include "me_option.h"
#include "option.h"
#include "mir_module.h"
#include "mir_parser.h"
#include "driver_runner.h"
#include "bin_mplt.h"

namespace maple {
const std::string kBinNameNone = "";
const std::string kBinNameJbc2mpl = "jbc2mpl";
const std::string kBinNameMe = "me";
const std::string kBinNameMpl2mpl = "mpl2mpl";
const std::string kBinNameMplcg = "mplcg";
const std::string kBinNameMapleComb = "maplecomb";

class Compiler {
 public:
  explicit Compiler(const std::string &name) : name(name) {}

  virtual ~Compiler() = default;

  virtual ErrorCode Compile(MplOptions &options, std::unique_ptr<MIRModule> &theModule);

  virtual void GetTmpFilesToDelete(const MplOptions&, std::vector<std::string>&) const {}

  virtual std::unordered_set<std::string> GetFinalOutputs(const MplOptions&) const {
    return std::unordered_set<std::string>();
  }

  virtual void PrintCommand(const MplOptions&) const {}

 protected:
  virtual std::string GetBinPath(const MplOptions &mplOptions) const;
  virtual const std::string &GetBinName() const {
    return kBinNameNone;
  }

  virtual std::string GetInputFileName(const MplOptions &options) const {
    std::ostringstream stream;
    for (const auto &inputFile : options.GetSplitsInputFiles()) {
      stream << " " << inputFile;
    }
    return stream.str();
  }

  virtual DefaultOption GetDefaultOptions(const MplOptions&) const {
    return DefaultOption();
  }

 private:
  const std::string name;
  std::string MakeOption(const MplOptions &options) const;
  void AppendDefaultOptions(std::map<std::string, MplOption> &finalOptions,
                            const std::map<std::string, MplOption> &defaultOptions,
                            std::ostringstream &strOption, bool isDebug) const;
  void AppendOptions(std::map<std::string, MplOption> &finalOptions, const std::string &key,
                     const std::string &value) const;
  void AppendExtraOptions(std::map<std::string, MplOption> &finalOptions,
                          const MplOptions &options,
                          std::ostringstream &strOption, bool isDebug) const;
  std::map<std::string, MplOption> MakeDefaultOptions(const MplOptions &options) const;
  int Exe(const MplOptions &mplOptions, const std::string &options) const;
  const std::string &GetName() const {
    return name;
  }
};

class Jbc2MplCompiler : public Compiler {
 public:
  explicit Jbc2MplCompiler(const std::string &name) : Compiler(name) {}

  ~Jbc2MplCompiler() = default;

 private:
  const std::string &GetBinName() const override;
  DefaultOption GetDefaultOptions(const MplOptions &options) const override;
  void GetTmpFilesToDelete(const MplOptions &mplOptions, std::vector<std::string> &tempFiles) const override;
  std::unordered_set<std::string> GetFinalOutputs(const MplOptions &mplOptions) const override;
};

class MapleCombCompiler : public Compiler {
 public:
  explicit MapleCombCompiler(const std::string &name) : Compiler(name), realRunningExe("") {}

  ~MapleCombCompiler() = default;

  ErrorCode Compile(MplOptions &options, std::unique_ptr<MIRModule> &theModule) override;
  void PrintCommand(const MplOptions &options) const override;
  std::string GetInputFileName(const MplOptions &options) const override;

 private:
  std::string realRunningExe;
  std::unordered_set<std::string> GetFinalOutputs(const MplOptions &mplOptions) const override;
  void GetTmpFilesToDelete(const MplOptions &mplOptions, std::vector<std::string> &tempFiles) const override;
  ErrorCode MakeMeOptions(const MplOptions &options, DriverRunner &runner);
  ErrorCode MakeMpl2MplOptions(const MplOptions &options, DriverRunner &runner);
  std::string DecideOutExe(const MplOptions &options);
};

class MplcgCompiler : public Compiler {
 public:
  explicit MplcgCompiler(const std::string &name) : Compiler(name) {}

  ~MplcgCompiler() = default;
  ErrorCode Compile(MplOptions &options, std::unique_ptr<MIRModule> &theModule) override;
  void PrintMplcgCommand(const MplOptions &options, const MIRModule &md) const;
  void SetOutputFileName(const MplOptions &options, const MIRModule &md);
  std::string GetInputFile(const MplOptions &options, const MIRModule &md) const;
 private:
  DefaultOption GetDefaultOptions(const MplOptions &options) const override;
  ErrorCode MakeCGOptions(const MplOptions &options);
  const std::string &GetBinName() const override;
  std::string baseName;
  std::string outputFile;
};
}  // namespace maple
#endif  // MAPLE_DRIVER_INCLUDE_COMPILER_H
