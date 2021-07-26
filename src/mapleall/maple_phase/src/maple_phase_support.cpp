/*
 * Copyright (c) [2021] Huawei Technologies Co.,Ltd.All rights reserved.
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
 * See the Mulan PSL v2 for more details.
 */
#include "maple_phase_support.h"
namespace maple {
const MapleVector<MaplePhaseID> &AnalysisDep::GetRequiredPhase() const {
  return required;
}
const MapleSet<MaplePhaseID> &AnalysisDep::GetPreservedPhase() const {
  return preserved;
}
const MapleSet<MaplePhaseID> &AnalysisDep::GetPreservedExceptPhase() const {
  return preservedExcept;
}
}
