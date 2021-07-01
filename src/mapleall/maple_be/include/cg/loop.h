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
#ifndef MAPLEBE_INCLUDE_CG_LOOP_H
#define MAPLEBE_INCLUDE_CG_LOOP_H

#include "cg_phase.h"
#include "cgbb.h"
#include "insn.h"

namespace maplebe {
class LoopHierarchy {
 public:
  struct HeadIDCmp {
    bool operator()(const LoopHierarchy *loopHierarchy1, const LoopHierarchy *loopHierarchy2) const {
      CHECK_NULL_FATAL(loopHierarchy1);
      CHECK_NULL_FATAL(loopHierarchy2);
      return (loopHierarchy1->GetHeader()->GetId() < loopHierarchy2->GetHeader()->GetId());
    }
  };

  explicit LoopHierarchy(MemPool &memPool)
      : loopMemPool(&memPool),
        otherLoopEntries(loopMemPool.Adapter()),
        loopMembers(loopMemPool.Adapter()),
        backedge(loopMemPool.Adapter()),
        exits(loopMemPool.Adapter()),
        innerLoops(loopMemPool.Adapter()) {}

  virtual ~LoopHierarchy() = default;

  const BB *GetHeader() const {
    return header;
  }
  const MapleSet<BB*, BBIdCmp> &GetLoopMembers() const {
    return loopMembers;
  }
  const MapleSet<BB*, BBIdCmp> &GetBackedge() const {
    return backedge;
  }
  const MapleSet<BB*, BBIdCmp> &GetExits() const {
    return exits;
  }
  const MapleSet<LoopHierarchy*, HeadIDCmp> &GetInnerLoops() const {
    return innerLoops;
  }
  const LoopHierarchy *GetOuterLoop() const {
    return outerLoop;
  }
  LoopHierarchy *GetPrev() {
    return prev;
  }
  LoopHierarchy *GetNext() {
    return next;
  }

  MapleSet<BB*, BBIdCmp>::iterator EraseLoopMembers(MapleSet<BB*, BBIdCmp>::iterator it) {
    return loopMembers.erase(it);
  }
  void InsertLoopMembers(BB &bb) {
    (void)loopMembers.insert(&bb);
  }
  void InsertBackedge(BB &bb) {
    (void)backedge.insert(&bb);
  }
  void InsertExit(BB &bb) {
    (void)exits.insert(&bb);
  }
  void InsertInnerLoops(LoopHierarchy &loop) {
    (void)innerLoops.insert(&loop);
  }
  void SetHeader(BB &bb) {
    header = &bb;
  }
  void SetOuterLoop(LoopHierarchy &loop) {
    outerLoop = &loop;
  }
  void SetPrev(LoopHierarchy *loop) {
    prev = loop;
  }
  void SetNext(LoopHierarchy *loop) {
    next = loop;
  }
  void PrintLoops(const std::string &name) const;

 protected:
  LoopHierarchy *prev = nullptr;
  LoopHierarchy *next = nullptr;

 private:
  MapleAllocator loopMemPool;
  BB *header = nullptr;
 public:
  MapleSet<BB*, BBIdCmp> otherLoopEntries;
  MapleSet<BB*, BBIdCmp> loopMembers;
  MapleSet<BB*, BBIdCmp> backedge;
  MapleSet<BB*, BBIdCmp> exits;
  MapleSet<LoopHierarchy*, HeadIDCmp> innerLoops;
  LoopHierarchy *outerLoop = nullptr;
};

class LoopFinder : public AnalysisResult {
 public:
  LoopFinder(CGFunc &func, MemPool &mem)
      : AnalysisResult(&mem),
        cgFunc(&func),
        memPool(&mem),
        loopMemPool(memPool),
        stack(loopMemPool.Adapter()),
        candidate(loopMemPool.Adapter()),
        visitedBBs(loopMemPool.Adapter()),
        sortedBBs(loopMemPool.Adapter()),
        dfsBBs(loopMemPool.Adapter()),
        recurseVisited(loopMemPool.Adapter())
        {}

  ~LoopFinder() override = default;

  void DetectLoop(BB *header, BB *back);
  bool DetectLoopSub(BB *header, BB *back);
  void Insert(BB *bb, BB *header, std::set<BB *> &extraHeader);
  void FindBackedge();
  void PushBackedge(BB &bb, std::stack<BB*> &succs, bool &childPushed);
  void MergeLoops();
  void SortLoops();
  void UpdateOuterForInnerLoop(BB *bb, LoopHierarchy *outer);
  void UpdateOuterLoop(LoopHierarchy *outer);
  void CreateInnerLoop(LoopHierarchy &inner, LoopHierarchy &outer);
  void DetectInnerLoop();
  void UpdateCGFunc();
  void FormLoopHierarchy();

 private:
  CGFunc *cgFunc;
  MemPool *memPool;
  MapleAllocator loopMemPool;
  MapleStack<BB *> stack;
  MapleList<BB*> candidate;  /* loop candidate */
  MapleVector<bool> visitedBBs;
  MapleVector<BB*> sortedBBs;
  MapleStack<BB*> dfsBBs;
  MapleVector<bool> recurseVisited;
  LoopHierarchy *loops = nullptr;
};

class CGFuncLoops {
 public:
  explicit CGFuncLoops(MemPool &memPool)
      : loopMemPool(&memPool),
        multiEntries(loopMemPool.Adapter()),
        loopMembers(loopMemPool.Adapter()),
        backedge(loopMemPool.Adapter()),
        exits(loopMemPool.Adapter()),
        innerLoops(loopMemPool.Adapter()) {}

  ~CGFuncLoops() = default;

  void CheckOverlappingInnerLoops(const MapleVector<CGFuncLoops*> &innerLoops,
                                  const MapleVector<BB*> &loopMembers) const;
  void CheckLoops() const;
  void PrintLoops(const CGFuncLoops &loops) const;

  const BB *GetHeader() const {
    return header;
  }
  const MapleVector<BB*> &GetMultiEntries() const {
    return multiEntries;
  }
  const MapleVector<BB*> &GetLoopMembers() const {
    return loopMembers;
  }
  const MapleVector<BB*> &GetBackedge() const {
    return backedge;
  }
  const MapleVector<BB*> &GetExits() const {
    return exits;
  }
  const MapleVector<CGFuncLoops*> &GetInnerLoops() const {
    return innerLoops;
  }
  const CGFuncLoops *GetOuterLoop() const {
    return outerLoop;
  }
  uint32 GetLoopLevel() const {
    return loopLevel;
  }

  void AddMultiEntries(BB &bb) {
    multiEntries.emplace_back(&bb);
  }
  void AddLoopMembers(BB &bb) {
    loopMembers.emplace_back(&bb);
  }
  void AddBackedge(BB &bb) {
    backedge.emplace_back(&bb);
  }
  void AddExit(BB &bb) {
    exits.emplace_back(&bb);
  }
  void AddInnerLoops(CGFuncLoops &loop) {
    innerLoops.emplace_back(&loop);
  }
  void SetHeader(BB &bb) {
    header = &bb;
  }
  void SetOuterLoop(CGFuncLoops &loop) {
    outerLoop = &loop;
  }
  void SetLoopLevel(uint32 val) {
    loopLevel = val;
  }

 private:
  MapleAllocator loopMemPool;
  BB *header = nullptr;
  MapleVector<BB*> multiEntries;
  MapleVector<BB*> loopMembers;
  MapleVector<BB*> backedge;
  MapleVector<BB*> exits;
  MapleVector<CGFuncLoops*> innerLoops;
  CGFuncLoops *outerLoop = nullptr;
  uint32 loopLevel = 0;
};

struct CGFuncLoopCmp {
  bool operator()(const CGFuncLoops *lhs, const CGFuncLoops *rhs) const {
    CHECK_NULL_FATAL(lhs);
    CHECK_NULL_FATAL(rhs);
    return lhs->GetHeader()->GetId() < rhs->GetHeader()->GetId();
  }
};

CGFUNCPHASE(CgDoLoopAnalysis, "loopanalysis")
}  /* namespace maplebe */

#endif  /* MAPLEBE_INCLUDE_CG_LOOP_H */
