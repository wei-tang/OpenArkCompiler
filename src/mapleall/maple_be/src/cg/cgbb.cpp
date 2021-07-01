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
#include "cgbb.h"
#include "cgfunc.h"

namespace maplebe {

constexpr uint32 kCondBrNum = 2;
constexpr uint32 kSwitchCaseNum = 5;

const std::string BB::bbNames[BB::kBBLast] = {
  "BB_ft",
  "BB_if",
  "BB_goto",
  "BB_ret",
  "BB_intrinsic",
  "BB_rangegoto",
  "BB_throw"
};

Insn *BB::InsertInsnBefore(Insn &existing, Insn &newInsn) {
  Insn *pre = existing.GetPrev();
  newInsn.SetPrev(pre);
  newInsn.SetNext(&existing);
  existing.SetPrev(&newInsn);
  if (pre != nullptr) {
    pre->SetNext(&newInsn);
  }
  if (&existing == firstInsn) {
    firstInsn = &newInsn;
  }
  newInsn.SetBB(this);
  return &newInsn;
}

Insn *BB::InsertInsnAfter(Insn &existing, Insn &newInsn) {
  newInsn.SetPrev(&existing);
  newInsn.SetNext(existing.GetNext());
  existing.SetNext(&newInsn);
  if (&existing == lastInsn) {
    lastInsn = &newInsn;
  } else if (newInsn.GetNext()) {
    newInsn.GetNext()->SetPrev(&newInsn);
  }
  newInsn.SetBB(this);
  internalFlag1++;
  return &newInsn;
}

void BB::ReplaceInsn(Insn &insn, Insn &newInsn) {
  if (insn.IsAccessRefField()) {
    newInsn.MarkAsAccessRefField(true);
  }
  if (insn.GetDoNotRemove()) {
    newInsn.SetDoNotRemove(true);
  }
  newInsn.SetPrev(insn.GetPrev());
  newInsn.SetNext(insn.GetNext());
  if (&insn == lastInsn) {
    lastInsn = &newInsn;
  } else if (newInsn.GetNext() != nullptr) {
    newInsn.GetNext()->SetPrev(&newInsn);
  }
  if (firstInsn == &insn) {
    firstInsn = &newInsn;
  } else if (newInsn.GetPrev() != nullptr) {
    newInsn.GetPrev()->SetNext(&newInsn);
  }
  newInsn.SetBB(this);
}

void BB::RemoveInsn(Insn &insn) {
  if ((firstInsn == &insn) && (lastInsn == &insn)) {
    firstInsn = lastInsn = nullptr;
  } else if (firstInsn == &insn) {
    firstInsn = insn.GetNext();
  } else if (lastInsn == &insn) {
    lastInsn = insn.GetPrev();
  }
  /* remove insn from lir list */
  Insn *prevInsn = insn.GetPrev();
  Insn *nextInsn = insn.GetNext();
  if (prevInsn != nullptr) {
    prevInsn->SetNext(nextInsn);
  }
  if (nextInsn != nullptr) {
    nextInsn->SetPrev(prevInsn);
  }
}

void BB::RemoveInsnPair(Insn &insn, Insn &nextInsn) {
  ASSERT(insn.GetNext() == &nextInsn, "next_insn is supposed to follow insn");
  ASSERT(nextInsn.GetPrev() == &insn, "next_insn is supposed to follow insn");
  if ((firstInsn == &insn) && (lastInsn == &nextInsn)) {
    firstInsn = lastInsn = nullptr;
  } else if (firstInsn == &insn) {
    firstInsn = nextInsn.GetNext();
  } else if (lastInsn == &nextInsn) {
    lastInsn = insn.GetPrev();
  }
  if (insn.GetPrev() != nullptr) {
    insn.GetPrev()->SetNext(nextInsn.GetNext());
  }
  if (nextInsn.GetNext() != nullptr) {
    nextInsn.GetNext()->SetPrev(insn.GetPrev());
  }
}

/* Remove insns in this bb from insn1 to insn2. */
void BB::RemoveInsnSequence(Insn &insn1, Insn &insn2) {
  ASSERT(insn1.GetBB() == this, "remove insn sequence in one bb");
  ASSERT(insn2.GetBB() == this, "remove insn sequence in one bb");
  if ((firstInsn == &insn1) && (lastInsn == &insn2)) {
    firstInsn = lastInsn = nullptr;
  } else if (firstInsn == &insn1) {
    firstInsn = insn2.GetNext();
  } else if (lastInsn == &insn2) {
    lastInsn = insn1.GetPrev();
  }

  if (insn1.GetPrev() != nullptr) {
    insn1.GetPrev()->SetNext(insn2.GetNext());
  }
  if (insn2.GetNext() != nullptr) {
    insn2.GetNext()->SetPrev(insn1.GetPrev());
  }
}

/* append all insns from bb into this bb */
void BB::AppendBBInsns(BB &bb) {
  if (firstInsn == nullptr) {
    firstInsn = bb.firstInsn;
    lastInsn = bb.lastInsn;
    if (firstInsn != nullptr) {
      FOR_BB_INSNS(i, &bb) {
        i->SetBB(this);
      }
    }
    return;
  }
  if ((bb.firstInsn == nullptr) || (bb.lastInsn == nullptr)) {
    return;
  }
  FOR_BB_INSNS_SAFE(insn, &bb, nextInsn) {
    AppendInsn(*insn);
  }
}

/* append all insns from bb into this bb */
void BB::InsertAtBeginning(BB &bb) {
  if (bb.firstInsn == nullptr) { /* nothing to add */
    return;
  }

  FOR_BB_INSNS(insn, &bb) {
    insn->SetBB(this);
  }

  if (firstInsn == nullptr) {
    firstInsn = bb.firstInsn;
    lastInsn = bb.lastInsn;
  } else {
    bb.lastInsn->SetNext(firstInsn);
    firstInsn->SetPrev(bb.lastInsn);
    firstInsn = bb.firstInsn;
  }
  bb.firstInsn = bb.lastInsn = nullptr;
}

/* Number of instructions excluding DbgInsn and comments */
int32 BB::NumInsn() const {
  int32 bbSize = 0;
  FOR_BB_INSNS_CONST(i, this) {
    if (i->IsImmaterialInsn()) {
      continue;
    }
    ++bbSize;
  }
  return bbSize;
}

void BB::Dump() const {
  LogInfo::MapleLogger() << "=== BB " << this << " <" << GetKindName();
  if (labIdx) {
    LogInfo::MapleLogger() << "[labeled with " << labIdx << "]";
    if (labelTaken) {
      LogInfo::MapleLogger() << " taken";
    }
  }
  LogInfo::MapleLogger() << "> <" << id << "> ";
  if (isCleanup) {
    LogInfo::MapleLogger() << "[is_cleanup] ";
  }
  if (unreachable) {
    LogInfo::MapleLogger() << "[unreachable] ";
  }
  LogInfo::MapleLogger() << "frequency:" << frequency << "===\n";

  Insn *insn = firstInsn;
  while (insn != nullptr) {
    insn->Dump();
    insn = insn->GetNext();
  }
}

bool BB::IsCommentBB() const {
  if (GetKind() != kBBFallthru) {
    return false;
  }
  FOR_BB_INSNS_CONST(insn, this) {
    if (insn->IsMachineInstruction()) {
      return false;
    }
  }
  return true;
}

/* return true if bb has no real insns. */
bool BB::IsEmptyOrCommentOnly() const {
  return (IsEmpty() || IsCommentBB());
}

bool BB::IsSoloGoto() const {
  if (GetKind() != kBBGoto) {
    return false;
  }
  FOR_BB_INSNS_CONST(insn, this) {
    if (!insn->IsMachineInstruction()) {
      continue;
    }
    return (insn->IsGoto());
  }
  return false;
}

bool Bfs::AllPredBBVisited(BB &bb, long &level) const {
  bool isAllPredsVisited = true;
  for (const auto *predBB : bb.GetPreds()) {
    /* See if pred bb is a loop back edge */
    bool isBackEdge = false;
    for (const auto *loopBB : predBB->GetLoopSuccs()) {
      if (loopBB == &bb) {
        isBackEdge = true;
        break;
      }
    }
    if (!isBackEdge && !visitedBBs[predBB->GetId()]) {
      isAllPredsVisited = false;
      break;
    }
    level = std::max(level, predBB->GetInternalFlag2());
  }
  for (const auto *predEhBB : bb.GetEhPreds()) {
    bool isBackEdge = false;
    for (const auto *loopBB : predEhBB->GetLoopSuccs()) {
      if (loopBB == &bb) {
        isBackEdge = true;
        break;
      }
    }
    if (!isBackEdge && !visitedBBs[predEhBB->GetId()]) {
      isAllPredsVisited = false;
      break;
    }
    level = std::max(level, predEhBB->GetInternalFlag2());
  }
  return isAllPredsVisited;
}

/*
 * During live interval construction, bb has only one predecessor and/or one
 * successor are stright line bb.  It can be considered to be a single large bb
 * for the purpose of finding live interval.  This is to prevent extending live
 * interval of registers unnecessarily when interleaving bb from other paths.
 */
BB *Bfs::MarkStraightLineBBInBFS(BB *bb) {
  while (true) {
    if ((bb->GetSuccs().size() != 1) || !bb->GetEhSuccs().empty()) {
      break;
    }
    BB *sbb = bb->GetSuccs().front();
    if (visitedBBs[sbb->GetId()]) {
      break;
    }
    if ((sbb->GetPreds().size() != 1) || !sbb->GetEhPreds().empty()) {
      break;
    }
    sortedBBs.push_back(sbb);
    visitedBBs[sbb->GetId()] = true;
    sbb->SetInternalFlag2(bb->GetInternalFlag2() + 1);
    bb = sbb;
  }
  return bb;
}

BB *Bfs::SearchForStraightLineBBs(BB &bb) {
  if ((bb.GetSuccs().size() != kCondBrNum) || bb.GetEhSuccs().empty()) {
    return &bb;
  }
  BB *sbb1 = bb.GetSuccs().front();
  BB *sbb2 = bb.GetSuccs().back();
  size_t predSz1 = sbb1->GetPreds().size();
  size_t predSz2 = sbb2->GetPreds().size();
  BB *candidateBB = nullptr;
  if ((predSz1 == 1) && (predSz2 > kSwitchCaseNum)) {
    candidateBB = sbb1;
  } else if ((predSz2 == 1) && (predSz1 > kSwitchCaseNum)) {
    candidateBB = sbb2;
  } else {
    return &bb;
  }
  ASSERT(candidateBB->GetId() < visitedBBs.size(), "index out of range in RA::SearchForStraightLineBBs");
  if (visitedBBs[candidateBB->GetId()]) {
    return &bb;
  }
  if (!candidateBB->GetEhPreds().empty()) {
    return &bb;
  }
  if (candidateBB->GetSuccs().size() != 1) {
    return &bb;
  }

  sortedBBs.push_back(candidateBB);
  visitedBBs[candidateBB->GetId()] = true;
  return MarkStraightLineBBInBFS(candidateBB);
}

void Bfs::BFS(BB &curBB) {
  std::queue<BB*> workList;
  workList.push(&curBB);
  ASSERT(curBB.GetId() < cgfunc->NumBBs(), "RA::BFS visitedBBs overflow");
  ASSERT(curBB.GetId() < visitedBBs.size(), "index out of range in RA::BFS");
  visitedBBs[curBB.GetId()] = true;
  do {
    BB *bb = workList.front();
    sortedBBs.push_back(bb);
    ASSERT(bb->GetId() < cgfunc->NumBBs(), "RA::BFS visitedBBs overflow");
    visitedBBs[bb->GetId()] = true;
    workList.pop();
    /* Look for straight line bb */
    bb = MarkStraightLineBBInBFS(bb);
    /* Look for an 'if' followed by some straight-line bb */
    bb = SearchForStraightLineBBs(*bb);
    for (auto *ibb : bb->GetSuccs()) {
      /* See if there are unvisited predecessor */
      if (visitedBBs[ibb->GetId()]) {
        continue;
      }
      long prevLevel = 0;
      if (AllPredBBVisited(*ibb, prevLevel)) {
        ibb->SetInternalFlag2(prevLevel + 1);
        workList.push(ibb);
        ASSERT(ibb->GetId() < cgfunc->NumBBs(), "GCRA::BFS visitedBBs overflow");
        visitedBBs[ibb->GetId()] = true;
      }
    }
  } while (!workList.empty());
}

void Bfs::ComputeBlockOrder() {
  visitedBBs.clear();
  sortedBBs.clear();
  visitedBBs.resize(cgfunc->NumBBs());
  for (uint32 i = 0; i < cgfunc->NumBBs(); ++i) {
    visitedBBs[i] = false;
  }
  BB *cleanupBB = nullptr;
  FOR_ALL_BB(bb, cgfunc) {
    bb->SetInternalFlag1(0);
    bb->SetInternalFlag2(1);
    if (bb->GetFirstStmt() == cgfunc->GetCleanupLabel()) {
      cleanupBB = bb;
    }
  }
  for (BB *bb = cleanupBB; bb != nullptr; bb = bb->GetNext()) {
    bb->SetInternalFlag1(1);
  }

  bool changed;
  size_t sortedCnt = 0;
  bool done = false;
  do {
    changed = false;
    FOR_ALL_BB(bb, cgfunc) {
      if (bb->GetInternalFlag1() == 1) {
        continue;
      }
      if (visitedBBs[bb->GetId()]) {
        continue;
      }
      changed = true;
      long prevLevel = 0;
      if (AllPredBBVisited(*bb, prevLevel)) {
        bb->SetInternalFlag2(prevLevel + 1);
        BFS(*bb);
      }
    }
    /* Make sure there is no infinite loop. */
    if (sortedCnt == sortedBBs.size()) {
      if (!done) {
        done = true;
      } else {
        LogInfo::MapleLogger() << "Error: RA BFS loop " << sortedCnt << " in func " << cgfunc->GetName() << "\n";
      }
    }
    sortedCnt = sortedBBs.size();
  } while (changed);

  for (BB *bb = cleanupBB; bb != nullptr; bb = bb->GetNext()) {
    sortedBBs.push_back(bb);
  }
}
}  /* namespace maplebe */
