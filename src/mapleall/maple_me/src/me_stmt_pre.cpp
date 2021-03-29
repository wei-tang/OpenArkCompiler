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
#include "utils.h"
#include "me_dominance.h"
#include "me_option.h"
#include "me_ssa_update.h"
#include "me_stmt_pre.h"

// Note: after the movement of assignments, some phi nodes that used to be dead
// can become live.  Before we run another round of dead store elimination, we
// should NOT trust the isLive flag in phi nodes.
// accumulate the BBs that are in the iterated dominance frontiers of bb in
// the set dfset, visiting each BB only once
namespace maple {
static bool IsParaAndRetTypeRefOrPtr(const CallMeStmt &stmt) {
  const MIRFunction &func = stmt.GetTargetFunction();
  if (func.GetName() == "MCC_GetOrInsertLiteral") {
    return false;
  }
  for (auto it : stmt.GetOpnds()) {
    if (it->GetPrimType() == PTY_ref || it->GetPrimType() == PTY_ptr) {
      return true;
    }
  }
  const MeExpr *expr = stmt.GetAssignedLHS();
  if (expr != nullptr && (expr->GetPrimType() == PTY_ref || expr->GetPrimType() == PTY_ptr)) {
    return true;
  }
  return false;
}

void MeStmtPre::GetIterDomFrontier(const BB &bb, MapleSet<uint32> &dfSet, std::vector<bool> &visitedMap) const {
  CHECK_FATAL(bb.GetBBId() < visitedMap.size(), "index out of range in MeStmtPre::GetIterDomFrontier");
  if (visitedMap[bb.GetBBId()]) {
    return;
  }
  visitedMap[bb.GetBBId()] = true;
  for (BBId frontierBBId : dom->GetDomFrontier(bb.GetBBId())) {
    (void)dfSet.insert(dom->GetDtDfnItem(frontierBBId));
    BB *frontierBB = GetBB(frontierBBId);
    GetIterDomFrontier(*frontierBB, dfSet, visitedMap);
  }
}

void MeStmtPre::CodeMotion() {
  for (MeOccur *occ : allOccs) {
    switch (occ->GetOccType()) {
      case kOccReal: {
        auto *realOcc = static_cast<MeRealOcc*>(occ);
        if (realOcc->IsSave()) {
          CHECK_FATAL(!realOcc->IsReload(), "should be false");
        } else if (realOcc->IsReload()) {
          realOcc->GetBB()->RemoveMeStmt(realOcc->GetMeStmt());
          if (realOcc->GetOpcodeOfMeStmt() == OP_dassign) {
            auto *dass = static_cast<DassignMeStmt*>(realOcc->GetMeStmt());
            OStIdx ostIdx = dass->GetVarLHS()->GetOstIdx();
            if (candsForSSAUpdate.find(ostIdx) == candsForSSAUpdate.end()) {
              candsForSSAUpdate[ostIdx] =
                  ssaPreMemPool->New<MapleSet<BBId>>(std::less<BBId>(), ssaPreAllocator.Adapter());
            }
          } else if (realOcc->GetOpcodeOfMeStmt() == OP_callassigned) {
            auto *call = static_cast<CallMeStmt*>(realOcc->GetMeStmt());
            if (call->GetAssignedLHS() != nullptr) {
              CHECK_FATAL(call->GetAssignedLHS()->GetMeOp() == kMeOpVar, "should be var");
              auto *var = static_cast<VarMeExpr*>(call->GetAssignedLHS());
              OStIdx ostIdx = var->GetOstIdx();
              if (candsForSSAUpdate.find(ostIdx) == candsForSSAUpdate.end()) {
                candsForSSAUpdate[ostIdx] =
                    ssaPreMemPool->New<MapleSet<BBId>>(std::less<BBId>(), ssaPreAllocator.Adapter());
              }
            }
          }
        }
        break;
      }
      case kOccPhiopnd: {
        auto *phiOpnd = static_cast<MePhiOpndOcc*>(occ);
        if (!phiOpnd->GetDefPhiOcc()->IsWillBeAvail()) {
          break;
        }
        MeOccur *defOcc = phiOpnd->GetDef();
        if (defOcc->GetOccType() == kOccInserted) {
          if (!phiOpnd->IsPhiOpndReload()) {
            auto *insertedOcc = static_cast<MeInsertedOcc*>(defOcc);
            if (insertedOcc->GetOpcodeOfMeStmt() == OP_dassign) {
              auto *dass = static_cast<DassignMeStmt*>(insertedOcc->GetMeStmt());
              OStIdx ostIdx = dass->GetVarLHS()->GetOstIdx();
              if (candsForSSAUpdate.find(ostIdx) == candsForSSAUpdate.end()) {
                MapleSet<BBId> *bbSet =
                    ssaPreMemPool->New<MapleSet<BBId>>(std::less<BBId>(), ssaPreAllocator.Adapter());
                (void)bbSet->insert(occ->GetBB()->GetBBId());
                candsForSSAUpdate[ostIdx] = bbSet;
              } else {
                (void)candsForSSAUpdate[ostIdx]->insert(occ->GetBB()->GetBBId());
              }
              // create a new LHS for the dassign in insertedOcc->GetMeStmt()
              VarMeExpr *newVarVersion = irMap->CreateVarMeExprVersion(*dass->GetVarLHS());
              dass->UpdateLhs(newVarVersion);
            } else if (insertedOcc->GetOpcodeOfMeStmt() == OP_callassigned) {
              auto *call = static_cast<CallMeStmt*>(insertedOcc->GetMeStmt());
              if (call->GetAssignedLHS() != nullptr) {
                CHECK_FATAL(call->GetAssignedLHS()->GetMeOp() == kMeOpVar, "should be var");
                auto *var = static_cast<VarMeExpr*>(call->GetAssignedLHS());
                ASSERT_NOT_NULL(var);
                OStIdx ostIdx = var->GetOstIdx();
                if (candsForSSAUpdate.find(ostIdx) == candsForSSAUpdate.end()) {
                  MapleSet<BBId> *bbSet =
                      ssaPreMemPool->New<MapleSet<BBId>>(std::less<BBId>(), ssaPreAllocator.Adapter());
                  (void)bbSet->insert(occ->GetBB()->GetBBId());
                  candsForSSAUpdate[ostIdx] = bbSet;
                } else {
                  (void)candsForSSAUpdate[ostIdx]->insert(occ->GetBB()->GetBBId());
                }
                VarMeExpr *newVarVersion = irMap->CreateVarMeExprVersion(*var);
                auto &mustDef = call->GetMustDefList()->front();
                mustDef.UpdateLHS(*newVarVersion);
                newVarVersion->SetDefBy(kDefByMustDef);
                newVarVersion->SetDefMustDef(mustDef);
              }
            }
            if (insertedOcc->GetOpcodeOfMeStmt() == OP_intrinsiccallwithtype &&
                (static_cast<IntrinsiccallMeStmt*>(insertedOcc->GetMeStmt())->GetIntrinsic() ==
                 INTRN_JAVA_CLINIT_CHECK)) {
              BB *insertBB = insertedOcc->GetBB();
              // insert at earlist point in BB, but after statements required
              // to be first statement in BB
              MeStmt *curStmt = to_ptr(insertBB->GetMeStmts().begin());
              while (curStmt != nullptr && (curStmt->GetOp() == OP_catch || curStmt->GetOp() == OP_try ||
                                            curStmt->GetOp() == OP_comment)) {
                curStmt = curStmt->GetNext();
              }
              if (curStmt == nullptr) {
                insertBB->AddMeStmtLast(insertedOcc->GetMeStmt());
              } else {
                insertBB->InsertMeStmtBefore(curStmt, insertedOcc->GetMeStmt());
              }
            } else {
              insertedOcc->GetBB()->InsertMeStmtLastBr(insertedOcc->GetMeStmt());
            }
          }
        }
        break;
      }
      case kOccPhiocc:
      case kOccExit:
      case kOccUse:
      case kOccMembar:
        break;
      default:
        ASSERT(false, "should not be here");
        break;
    }
  }
}

void MeStmtPre::Finalize1() {
  PreWorkCand *workCand = GetWorkCand();
  std::vector<MeOccur*> availDefVec(classCount, nullptr);
  // traversal in preoder DT
  for (MeOccur *occ : allOccs) {
    uint32 classId = static_cast<uint32>(occ->GetClassID());
    switch (occ->GetOccType()) {
      case kOccPhiocc: {
        auto *phiOcc = static_cast<MePhiOcc*>(occ);
        if (phiOcc->IsWillBeAvail()) {
          availDefVec[classId] = phiOcc;
        }
        break;
      }
      case kOccReal: {
        MeOccur *availDef = availDefVec[classId];
        auto *realOcc = static_cast<MeRealOcc*>(occ);
        if (availDef == nullptr || !availDef->IsDominate(*dom, *occ)) {
          realOcc->SetIsReload(false);
          availDefVec[classId] = realOcc;
        } else {
          realOcc->SetIsReload(true);
          ASSERT(!realOcc->IsSave(), "real occ with isSave cannot be set isReload");
          realOcc->SetDef(availDefVec[classId]);
          CHECK_NULL_FATAL(realOcc->GetDef());
        }
        break;
      }
      case kOccPhiopnd: {
        // we assume one phiOpnd has only one phiOcc use because critical edge split the blocks
        auto *phiOpnd = static_cast<MePhiOpndOcc*>(occ);
        MePhiOcc *phiOcc = phiOpnd->GetDefPhiOcc();
        if (phiOcc->IsWillBeAvail()) {
          if (phiOpnd->IsOkToInsert()) {
            // insert the current expression at the end of the block containing phiOpnd
            if (phiOpnd->GetBB()->GetSucc().size() > 1) {
              CHECK_FATAL(!workCand->Redo2HandleCritEdges(), "Finalize1: insertion at critical edge; aborting");
              workCand->SetRedo2HandleCritEdges(true);
              if (GetSSAPreDebug()) {
                mirModule->GetOut() << "<<<<< Re-doing this candidate due to existence of critical edge >>>>>\n";
              }
              return;
            }
            MeStmt *insertedStmt = phiOpnd->GetCurrentMeStmt();
            ASSERT(insertedStmt != nullptr, "NYI");
            MeInsertedOcc *insertedOcc =
                perCandMemPool->New<MeInsertedOcc>(static_cast<MeExpr*>(nullptr), insertedStmt, *phiOpnd->GetBB());
            insertedOcc->SetClassID(classCount++);
            phiOpnd->SetDef(insertedOcc);
            phiOpnd->SetClassID(insertedOcc->GetClassID());
            if (workCand->GetPUIdx() != GetPUIdx()) {
              ASSERT(!workCand->HasLocalOpnd(), "candidate with local opnd cannot be inserted outside its PU");
              workCand->SetPUIdx(0);
            }
            phiOpnd->SetIsInsertedOcc(true);
          } else {
            phiOpnd->SetDef(availDefVec[classId]);
          }
        }
        break;
      }
      case kOccExit:
      case kOccUse:
      case kOccMembar:
        break;
      default:
        ASSERT(false, "should not be here");
        break;
    }
  }
  if (GetSSAPreDebug()) {
    PreWorkCand *curCand = workCand;
    mirModule->GetOut() << "========ssapre candidate " << curCand->GetIndex() <<
        " after Finalize1===================\n";
    for (auto it = phiOccs.begin(); it != phiOccs.end(); ++it) {
      MePhiOcc *phiOcc = *it;
      if (phiOcc->IsWillBeAvail()) {
        for (MePhiOpndOcc *phiOpnd : phiOcc->GetPhiOpnds()) {
          ASSERT(phiOpnd->GetDef() != nullptr, "EPhiFinalizer::DumpFinalize1: phiopndocc cannot have no def");
          MeOccur *defOcc = phiOpnd->GetDef();
          if (defOcc->GetOccType() == kOccInserted) {
            auto *realDefOcc = static_cast<MeInsertedOcc*>(defOcc);
            phiOpnd->Dump(*irMap);
            mirModule->GetOut() << " was inserted by ";
            realDefOcc->Dump(*irMap);
            mirModule->GetOut() << "\n";
          }
        }
      }
    }
  }
}

bool MeStmtPre::AllVarsSameVersion(const MeRealOcc &realOcc1, const MeRealOcc &realOcc2) const {
  const MeStmt *stmt1 = realOcc1.GetMeStmt();
  Opcode op = stmt1->GetOp();
  if (op == OP_intrinsiccallwithtype) {
    return true;
  }
  if ((op == OP_dassign || op == OP_callassigned) && realOcc1.GetMeExpr() != realOcc2.GetMeExpr()) {
    return false;
  }
  const MeStmt *stmt2 = realOcc2.GetMeStmt();
  for (size_t i = 0; i < stmt1->NumMeStmtOpnds(); ++i) {
    if (stmt1->GetOpnd(i) != stmt2->GetOpnd(i)) {
      return false;
    }
  }
  return true;
}

// collect meExpr's variables and put them into varvec;
// varvec can only store RegMeExpr and VarMeExpr
void MeStmtPre::CollectVarForMeStmt(const MeStmt &meStmt, MeExpr *meExpr, std::vector<MeExpr*> &varVec) const {
  switch (meStmt.GetOp()) {
    case OP_assertnonnull: {
      auto *unaryStmt = static_cast<const UnaryMeStmt*>(&meStmt);
      if (unaryStmt->GetOpnd()->GetMeOp() == kMeOpVar || unaryStmt->GetOpnd()->GetMeOp() == kMeOpReg) {
        varVec.push_back(unaryStmt->GetOpnd());
      }
      break;
    }
    case OP_dassign: {
      auto *dassMeStmt = static_cast<const DassignMeStmt*>(&meStmt);
      if (dassMeStmt->GetRHS()->GetMeOp() == kMeOpVar || dassMeStmt->GetRHS()->GetMeOp() == kMeOpReg) {
        varVec.push_back(dassMeStmt->GetRHS());
      }
      if (meExpr != nullptr) {
        CHECK_FATAL(meExpr->GetMeOp() == kMeOpVar, "CollectVarForMeStmt:bad meExpr field in realocc node");
        varVec.push_back(meExpr);
      }
      break;
    }
    case OP_intrinsiccallwithtype:
    case OP_intrinsiccall:
    case OP_callassigned: {
      auto *nStmt = static_cast<const NaryMeStmt*>(&meStmt);
      for (size_t i = 0; i < nStmt->NumMeStmtOpnds(); ++i)
        if (nStmt->GetOpnd(i)->GetMeOp() == kMeOpVar || nStmt->GetOpnd(i)->GetMeOp() == kMeOpReg) {
          varVec.push_back(nStmt->GetOpnd(i));
        }
      if (meExpr != nullptr) {
        CHECK_FATAL(meExpr->GetMeOp() == kMeOpVar, "CollectVarForMeStmt:bad meExpr field in realocc node");
        varVec.push_back(meExpr);
      }
      break;
    }
    default:
      CHECK_FATAL(false, "MeStmtEPre::CollectVarForCand: NYI");
  }
}

void MeStmtPre::CollectVarForCand(MeRealOcc &realOcc, std::vector<MeExpr*> &varVec) const {
  CollectVarForMeStmt(*realOcc.GetMeStmt(), realOcc.GetMeExpr(), varVec);
}

MeStmt *MeStmtPre::CopyMeStmt(const MeStmt &meStmt) const {
  switch (meStmt.GetOp()) {
    case OP_assertnonnull: {
      auto *unaryStmt = static_cast<const UnaryMeStmt*>(&meStmt);
      UnaryMeStmt *newUnaryStmt = irMap->New<UnaryMeStmt>(unaryStmt);
      return newUnaryStmt;
    }
    case OP_dassign: {
      auto *dass = static_cast<const DassignMeStmt*>(&meStmt);
      DassignMeStmt *newDass = irMap->New<DassignMeStmt>(&irMap->GetIRMapAlloc(), dass);
      return newDass;
    }
    case OP_intrinsiccall:
    case OP_intrinsiccallwithtype: {
      auto *intrnStmt = static_cast<const IntrinsiccallMeStmt*>(&meStmt);
      IntrinsiccallMeStmt *newIntrnStmt = irMap->NewInPool<IntrinsiccallMeStmt>(intrnStmt);
      return newIntrnStmt;
    }
    case OP_callassigned: {
      auto *callAss = static_cast<const CallMeStmt*>(&meStmt);
      CallMeStmt *newCallAss = irMap->NewInPool<CallMeStmt>(callAss);
      return newCallAss;
    }
    default:
      CHECK_FATAL(false, "MeStmtEPre::CopyMeStmt: NYI");
  }
}

// for each variable in realz that is defined by a phi, replace it by the jth
// phi opnd; the tagged lhs is returned in the reference parameter lhsvar
MeStmt *MeStmtPre::PhiOpndFromRes4Stmt(MeRealOcc &realZ, size_t j, MeExpr *&lhsVar) const {
  MeOccur *defZ = realZ.GetDef();
  CHECK_FATAL(defZ != nullptr, "must be def by phiocc");
  CHECK_FATAL(defZ->GetOccType() == kOccPhiocc, "must be def by phiocc");
  MeStmt *stmtQ = CopyMeStmt(utils::ToRef(realZ.GetMeStmt()));
  lhsVar = realZ.GetMeExpr();
  BB *phiBB = defZ->GetBB();
  CHECK_FATAL(stmtQ != nullptr, "nullptr check");
  switch (stmtQ->GetOp()) {
    case OP_assertnonnull: {
      auto *unaryStmtQ = static_cast<UnaryMeStmt*>(stmtQ);
      MeExpr *retOpnd = GetReplaceMeExpr(*unaryStmtQ->GetOpnd(), *phiBB, j);
      if (retOpnd != nullptr) {
        unaryStmtQ->SetMeStmtOpndValue(retOpnd);
      }
      break;
    }
    case OP_dassign: {
      auto *dassQ = static_cast<DassignMeStmt*>(stmtQ);
      MeExpr *retOpnd = GetReplaceMeExpr(*dassQ->GetRHS(), *phiBB, j);
      if (retOpnd != nullptr) {
        dassQ->SetRHS(retOpnd);
      }
      break;
    }
    case OP_intrinsiccall:
    case OP_intrinsiccallwithtype:
    case OP_callassigned: {
      auto *nStmtQ = static_cast<NaryMeStmt*>(stmtQ);
      for (size_t i = 0; i < nStmtQ->NumMeStmtOpnds(); ++i) {
        MeExpr *retOpnd = GetReplaceMeExpr(*nStmtQ->GetOpnd(i), *phiBB, j);
        if (retOpnd != nullptr) {
          nStmtQ->SetOpnd(i, retOpnd);
        }
      }
      break;
    }
    default:
      ASSERT(false, "MeStmtPre::PhiOpndFromRes4Stmt: NYI");
      break;
  }
  if ((stmtQ->GetOp() == OP_dassign || stmtQ->GetOp() == OP_callassigned) && realZ.GetMeExpr() != nullptr) {
    MeExpr *retOpnd = GetReplaceMeExpr(*realZ.GetMeExpr(), *phiBB, j);
    if (retOpnd != nullptr) {
      lhsVar = retOpnd;
    }
  }
  return stmtQ;
}

void MeStmtPre::Rename2() {
  PreWorkCand *workCand = GetWorkCand();
  while (!rename2Set.empty()) {
    MapleSet<uint32>::iterator it = rename2Set.begin();
    MeRealOcc *realOcc = workCand->GetRealOcc(*it);
    (void)rename2Set.erase(it);
    MeOccur *defOcc = realOcc->GetDef();
    CHECK_FATAL(defOcc != nullptr, "should be def by phiocc");
    CHECK_FATAL(defOcc->GetOccType() == kOccPhiocc, "should be def by phiocc");
    auto *defPhiOcc = static_cast<MePhiOcc*>(defOcc);
    MapleVector<MePhiOpndOcc*> &phiOpnds = defPhiOcc->GetPhiOpnds();
    for (size_t i = 0; i < phiOpnds.size(); ++i) {
      MePhiOpndOcc *phiOccOpnd = phiOpnds[i];
      if (!phiOccOpnd->IsProcessed()) {
        phiOccOpnd->SetIsProcessed(true);
        MeExpr *varY = nullptr;
        MeStmt *stmtY = PhiOpndFromRes4Stmt(*realOcc, i, varY);
        stmtY->SetBB(phiOccOpnd->GetBB());
        phiOccOpnd->SetCurrentMeStmt(*stmtY);  // stmtY might be inserted at the end of the block
        MeOccur *defX = phiOccOpnd->GetDef();
        if (defX == nullptr) {
          continue;
        }
        if (defX->GetOccType() == kOccReal) {
          auto *realDefX = static_cast<MeRealOcc*>(defX);
          std::vector<MeExpr*> varVecX;
          std::vector<MeExpr*> varVecY;
          CollectVarForCand(*realDefX, varVecX);
          CollectVarForMeStmt(*stmtY, varY, varVecY);
          CHECK_FATAL(varVecX.size() == varVecY.size(), "vector size should be the same");
          bool hasSameVersion = true;
          size_t checkLimit = varVecY.size();
          if (varY != nullptr) {
            if (varVecY[checkLimit - 1] == realDefX->GetMeStmt()->GetVarLHS()) {
              --checkLimit;
            }
          }
          for (size_t j = 0; j < checkLimit; ++j) {
            if (varVecX[j] != varVecY[j]) {
              hasSameVersion = false;
            }
          }
          if (!hasSameVersion) {
            phiOccOpnd->SetDef(nullptr);
            phiOccOpnd->SetHasRealUse(false);
          }
        } else if (defX->GetOccType() == kOccPhiocc) {
          std::vector<MeExpr*> varVecY;
          bool allDom = true;
          CollectVarForMeStmt(*stmtY, varY, varVecY);
          size_t checkLimit = varVecY.size();
          if (varY != nullptr) {
            if (varVecY[checkLimit - 1] == stmtY->GetVarLHS()) {
              --checkLimit;
            }
          }
          for (size_t k = 0; k < checkLimit; ++k) {
            if (!DefVarDominateOcc(varVecY[k], *defX)) {
              allDom = false;
            }
          }
          if (allDom) {
            // create a realOcc and add to rename2 set
            MeRealOcc *occY = perCandMemPool->New<MeRealOcc>(stmtY, 0, varY);
            occY->SetPosition(workCand->GetRealOccs().size());
            workCand->GetRealOccs().push_back(occY);
            occY->SetDef(defX);
            occY->SetClassID(defX->GetClassID());
            (void)rename2Set.insert(occY->GetPosition());
            if (GetSSAPreDebug()) {
              mirModule->GetOut() << "--- rename2 adds to rename2Set manufactured ";
              occY->Dump(*irMap);
              mirModule->GetOut() << '\n';
            }
          } else {
            phiOccOpnd->SetDef(nullptr);
            phiOccOpnd->SetHasRealUse(false);
            auto *phiDefX = static_cast<MePhiOcc*>(defX);
            phiDefX->SetIsDownSafe(false);
          }
        }
      }
    }
  }
  if (GetSSAPreDebug()) {
    PreWorkCand *curCand = workCand;
    mirModule->GetOut() << "========ssapre candidate " << curCand->GetIndex() << " after rename2===================\n";
    for (MeOccur *occ : allOccs) {
      occ->Dump(*irMap);
      mirModule->GetOut() << '\n';
    }
  }
}

// Df phis are computed into the df_phis set; Var Phis in the var_phis set
void MeStmtPre::ComputeVarAndDfPhis() {
  varPhiDfns.clear();
  dfPhiDfns.clear();
  PreWorkCand *workCand = GetWorkCand();
  const MapleVector<MeRealOcc*> &realOccList = workCand->GetRealOccs();
  CHECK_FATAL(!dom->IsBBVecEmpty(), "size to be allocated is 0");
  for (auto it = realOccList.begin(); it != realOccList.end(); ++it) {
    MeRealOcc *realOcc = *it;
    if (realOcc->GetOccType() == kOccMembar) {
      continue;
    }
    BB *defBB = realOcc->GetMeStmt()->GetBB();
    std::vector<bool> visitedMap(dom->GetBBVecSize(), false);
    GetIterDomFrontier(*defBB, dfPhiDfns, visitedMap);
    MeStmt *stmt = realOcc->GetMeStmt();
    switch (stmt->GetOp()) {
      case OP_assertnonnull: {
        auto *unaryStmt = static_cast<UnaryMeStmt*>(stmt);
        SetVarPhis(*unaryStmt->GetOpnd());
        break;
      }
      case OP_dassign: {
        auto *dassMeStmt = static_cast<DassignMeStmt*>(stmt);
        SetVarPhis(*dassMeStmt->GetRHS());
        SetVarPhis(*realOcc->GetMeExpr());
        break;
      }
      case OP_intrinsiccall:
      case OP_intrinsiccallwithtype:
      case OP_callassigned: {
        auto *nStmt = static_cast<NaryMeStmt*>(stmt);
        for (size_t i = 0; i < nStmt->NumMeStmtOpnds(); ++i) {
          SetVarPhis(*nStmt->GetOpnd(i));
        }
        if (realOcc->GetMeExpr() != nullptr) {
          SetVarPhis(*realOcc->GetMeExpr());
        }
        break;
      }
      default:
        CHECK_FATAL(false, "NYI");
    }
  }
}

// Based on ssapre->workCand's realOccs and dfPhiDfns (which will privides all
// the inserted phis) and the mapped set in useOccurMap,
// create the phi, phiopnd occ nodes and use_occ nodes; link them all
// up in order of dt_preorder in ssapre->allOccs; the phi occ nodes are in
// addition provided in order of dt_preorder in ssapre->phiOccs.
// When a real_occ has uses before it in its bb, ignore (do not insert)
// the real_occ.
void MeStmtPre::CreateSortedOccs() {
  // get set of bb dfns that contain uses if candidate is dassign or callassigned
  MapleSet<uint32> *useDfns;
  PreWorkCand *workCand = GetWorkCand();
  auto *stmtWkCand = static_cast<PreStmtWorkCand*>(workCand);
  if ((stmtWkCand->GetTheMeStmt()->GetOp() == OP_dassign || stmtWkCand->GetTheMeStmt()->GetOp() == OP_callassigned) &&
       stmtWkCand->GetTheMeStmt()->GetVarLHS() != nullptr && !stmtWkCand->LHSIsFinal()) {
    VarMeExpr *lhsVar = stmtWkCand->GetTheMeStmt()->GetVarLHS();
    OStIdx ostIdx = lhsVar->GetOstIdx();
    MapleMap<OStIdx, MapleSet<uint32>*>::iterator uMapIt = useOccurMap.find(ostIdx);
    CHECK_FATAL(uMapIt != useOccurMap.end(), "MeStmtPre::CreateSortedOccs: missing entry in useOccurMap");
    useDfns = uMapIt->second;
  } else {
    // create empty MapleSet<uint32> to be pointed to by use_dfns
    useDfns = perCandMemPool->New<MapleSet<uint32>>(perCandAllocator.Adapter());
  }
  // merge varPhiDfns to dfPhiDfns
  dfPhiDfns.insert(varPhiDfns.begin(), varPhiDfns.end());
  // form phiopnd_dfns
  std::multiset<uint32> phiOpndDfns;
  for (uint32 dfn : dfPhiDfns) {
    BBId bbId = dom->GetDtPreOrderItem(dfn);
    BB *bb = GetBB(bbId);
    CHECK_FATAL(bb != nullptr, "GetBB error");
    for (BB *pred : bb->GetPred()) {
      (void)phiOpndDfns.insert(dom->GetDtDfnItem(pred->GetBBId()));
    }
  }
  std::unordered_map<BBId, std::forward_list<MePhiOpndOcc*>> bb2PhiOpndMap;
  MapleVector<MeRealOcc*>::iterator realOccIt = workCand->GetRealOccs().begin();
  MapleVector<MeOccur*>::iterator exitOccIt = exitOccs.begin();
  auto phiDfnIt = dfPhiDfns.begin();
  auto phiOpndDfnIt = phiOpndDfns.begin();
  auto useDfnIt = useDfns->begin();
  MeOccur *nextUseOcc = nullptr;
  if (useDfnIt != useDfns->end()) {
    nextUseOcc = perCandMemPool->New<MeOccur>(kOccUse, 0, *GetBB(dom->GetDtPreOrderItem(*useDfnIt)), nullptr);
  }
  MeRealOcc *nextRealOcc = nullptr;
  if (realOccIt != workCand->GetRealOccs().end()) {
    nextRealOcc = *realOccIt;
  }
  MeOccur *nextExitOcc = nullptr;
  if (exitOccIt != exitOccs.end()) {
    nextExitOcc = *exitOccIt;
  }
  MePhiOcc *nextPhiOcc = nullptr;
  if (phiDfnIt != dfPhiDfns.end()) {
    nextPhiOcc = perCandMemPool->New<MePhiOcc>(*GetBB(dom->GetDtPreOrderItem(*phiDfnIt)), perCandAllocator);
  }
  MePhiOpndOcc *nextPhiOpndOcc = nullptr;
  if (phiOpndDfnIt != phiOpndDfns.end()) {
    nextPhiOpndOcc = perCandMemPool->New<MePhiOpndOcc>(*GetBB(dom->GetDtPreOrderItem(*phiOpndDfnIt)));
    std::unordered_map<BBId, std::forward_list<MePhiOpndOcc*>>::iterator it =
        bb2PhiOpndMap.find(dom->GetDtPreOrderItem(*phiOpndDfnIt));
    if (it == bb2PhiOpndMap.end()) {
      std::forward_list<MePhiOpndOcc*> newList = { nextPhiOpndOcc };
      CHECK(*phiOpndDfnIt < dom->GetDtPreOrderSize(), "index out of range in SSAPre::CreateSortedOccs");
      bb2PhiOpndMap[dom->GetDtPreOrderItem(*phiOpndDfnIt)] = newList;
    } else {
      it->second.push_front(nextPhiOpndOcc);
    }
  }
  bool realOccInserted = false;
  MeOccur *pickedOcc = nullptr;  // the next picked occ in order of preorder traveral of dominator tree
  do {
    pickedOcc = nullptr;
    // the 5 kinds of occ must be checked in this order, so it will be right
    // if more than 1 has the same dfn
    if (nextPhiOcc != nullptr) {
      pickedOcc = nextPhiOcc;
    }
    if (nextRealOcc != nullptr && (pickedOcc == nullptr ||
        dom->GetDtDfnItem(nextRealOcc->GetBB()->GetBBId()) < dom->GetDtDfnItem(pickedOcc->GetBB()->GetBBId()))) {
      pickedOcc = nextRealOcc;
    }
    if (nextUseOcc != nullptr && (pickedOcc == nullptr ||
        dom->GetDtDfnItem(nextUseOcc->GetBB()->GetBBId()) < dom->GetDtDfnItem(pickedOcc->GetBB()->GetBBId()))) {
      pickedOcc = nextUseOcc;
    }
    if (nextExitOcc != nullptr && (pickedOcc == nullptr ||
        dom->GetDtDfnItem(nextExitOcc->GetBB()->GetBBId()) < dom->GetDtDfnItem(pickedOcc->GetBB()->GetBBId()))) {
      pickedOcc = nextExitOcc;
    }
    if (nextPhiOpndOcc != nullptr && (pickedOcc == nullptr ||
        *phiOpndDfnIt < dom->GetDtDfnItem(pickedOcc->GetBB()->GetBBId()))) {
      pickedOcc = nextPhiOpndOcc;
    }
    if (pickedOcc != nullptr) {
      allOccs.push_back(pickedOcc);
      switch (pickedOcc->GetOccType()) {
        case kOccUse: {
          // get the next use occ
          ++useDfnIt;
          if (useDfnIt != useDfns->end()) {
            CHECK_NULL_FATAL(GetBB(dom->GetDtPreOrderItem(*useDfnIt)));
            nextUseOcc = perCandMemPool->New<MeOccur>(kOccUse, 0, *GetBB(dom->GetDtPreOrderItem(*useDfnIt)), nullptr);
          } else {
            nextUseOcc = nullptr;
          }
          break;
        }
        case kOccReal:
        case kOccMembar:
          // get the next real occ
          ++realOccIt;
          if (realOccIt != workCand->GetRealOccs().end()) {
            nextRealOcc = *realOccIt;
          } else {
            nextRealOcc = nullptr;
          }
          realOccInserted = true;
          break;
        case kOccExit:
          ++exitOccIt;
          if (exitOccIt != exitOccs.end()) {
            nextExitOcc = *exitOccIt;
          } else {
            nextExitOcc = nullptr;
          }
          break;
        case kOccPhiocc:
          phiOccs.push_back(static_cast<MePhiOcc*>(pickedOcc));
          ++phiDfnIt;
          if (phiDfnIt != dfPhiDfns.end()) {
            nextPhiOcc = perCandMemPool->New<MePhiOcc>(*GetBB(dom->GetDtPreOrderItem(*phiDfnIt)), perCandAllocator);
          } else {
            nextPhiOcc = nullptr;
          }
          break;
        case kOccPhiopnd:
          ++phiOpndDfnIt;
          if (phiOpndDfnIt != phiOpndDfns.end()) {
            nextPhiOpndOcc = perCandMemPool->New<MePhiOpndOcc>(*GetBB(dom->GetDtPreOrderItem(*phiOpndDfnIt)));
            std::unordered_map<BBId, std::forward_list<MePhiOpndOcc*>>::iterator it =
                bb2PhiOpndMap.find(dom->GetDtPreOrderItem(*phiOpndDfnIt));
            if (it == bb2PhiOpndMap.end()) {
              std::forward_list<MePhiOpndOcc*> newList = { nextPhiOpndOcc };
              bb2PhiOpndMap[dom->GetDtPreOrderItem(*phiOpndDfnIt)] = newList;
            } else {
              it->second.push_front(nextPhiOpndOcc);
            }
          } else {
            nextPhiOpndOcc = nullptr;
          }
          break;
        default:
          ASSERT(false, "CreateSortedOccs: unexpected occty");
          break;
      }
    }
  } while (pickedOcc != nullptr);
  // if no real occ inserted, no more work for this workCand
  if (!realOccInserted) {
    workCand->GetRealOccs().clear();
  }
  // initialize phiOpnds vector in each MePhiOcc node and defPhiOcc field in
  // each MePhiOpndOcc node
  for (MePhiOcc *phiOcc : phiOccs)
    for (BB *pred : phiOcc->GetBB()->GetPred()) {
      MePhiOpndOcc *phiOpndOcc = bb2PhiOpndMap[pred->GetBBId()].front();
      phiOcc->AddPhiOpnd(*phiOpndOcc);
      phiOpndOcc->SetDefPhiOcc(*phiOcc);
      bb2PhiOpndMap[pred->GetBBId()].pop_front();
    }
  if (GetSSAPreDebug()) {
    mirModule->GetOut() << "========ssapre candidate " << workCand->GetIndex() <<
        " after phi insert===================\n";
    for (MeOccur *occ : allOccs) {
      occ->Dump(*irMap);
      mirModule->GetOut() << '\n';
    }
  }
}

void MeStmtPre::ConstructUseOccurMapExpr(uint32 bbDfn, const MeExpr &meExpr) {
  if (meExpr.GetMeOp() == kMeOpVar) {
    OStIdx ostIdx = static_cast<const VarMeExpr*>(&meExpr)->GetOstIdx();
    MapleMap<OStIdx, MapleSet<uint32>*>::iterator mapIt;
    mapIt = useOccurMap.find(ostIdx);
    if (mapIt == useOccurMap.end()) {
      return;
    }
    MapleSet<uint32> *bbDfnSet = mapIt->second;
    (void)bbDfnSet->insert(bbDfn);
    return;
  }
  for (uint8 i = 0; i < meExpr.GetNumOpnds(); ++i) {
    ConstructUseOccurMapExpr(bbDfn, *meExpr.GetOpnd(i));
  }
}

void MeStmtPre::ConstructUseOccurMap() {
  for (PreWorkCand *wkCand : workList) {
    auto *stmtWkCand = static_cast<PreStmtWorkCand*>(wkCand);
    if (stmtWkCand->GetTheMeStmt()->GetOp() != OP_dassign && stmtWkCand->GetTheMeStmt()->GetOp() != OP_callassigned) {
      continue;
    }
    if (stmtWkCand->LHSIsFinal()) {
      continue;
    }
    VarMeExpr *lhsVar = stmtWkCand->GetTheMeStmt()->GetVarLHS();
    if (lhsVar == nullptr) {
      continue;
    }
    OStIdx ostIdx = lhsVar->GetOstIdx();
    if (useOccurMap.find(ostIdx) == useOccurMap.end()) {
      // add an entry for ostIdx
      useOccurMap[ostIdx] = ssaPreMemPool->New<MapleSet<uint32>>(ssaPreAllocator.Adapter());
    }
  }
  // do a pass over the program
  const MapleVector<BBId> &preOrderDt = dom->GetDtPreOrder();
  for (size_t i = 0; i < preOrderDt.size(); ++i) {
    BB *bb = func->GetAllBBs().at(preOrderDt[i]);
    for (auto &stmt : bb->GetMeStmts()) {
      for (size_t j = 0; j < stmt.NumMeStmtOpnds(); ++j) {
        ConstructUseOccurMapExpr(static_cast<uint32>(i), *stmt.GetOpnd(j));
      }
    }
  }
}

// create a new realOcc based on meStmt
PreStmtWorkCand *MeStmtPre::CreateStmtRealOcc(MeStmt &meStmt, int seqStmt) {
  uint32 hashIdx = PreWorkCandHashTable::ComputeStmtWorkCandHashIndex(meStmt);
  auto *wkCand = static_cast<PreStmtWorkCand*>(preWorkCandHashTable.GetWorkcandFromIndex(hashIdx));
  while (wkCand != nullptr) {
    MeStmt *currStmt = wkCand->GetTheMeStmt();
    ASSERT(currStmt != nullptr, "CreateStmtRealOcc: found workcand with theMeStmt as nullptr");
    if (currStmt->IsTheSameWorkcand(meStmt)) {
      break;
    }
    wkCand = static_cast<PreStmtWorkCand*>(wkCand->GetNext());
  }
  MeExpr *meExpr = nullptr;
  if ((meStmt.GetOp() == OP_dassign || meStmt.GetOp() == OP_callassigned) && meStmt.GetVarLHS() != nullptr) {
    MapleStack<ScalarMeExpr*> *pStack = versionStackVec.at(meStmt.GetVarLHS()->GetOstIdx());
    meExpr = pStack->top();
  }
  MeRealOcc *newOcc = ssaPreMemPool->New<MeRealOcc>(&meStmt, seqStmt, meExpr);
  if (wkCand != nullptr) {
    wkCand->AddRealOccAsLast(*newOcc, GetPUIdx());
    return wkCand;
  }
  // workCand not yet created; create a new one and add to workList
  wkCand = ssaPreMemPool->New<PreStmtWorkCand>(ssaPreAllocator, workList.size(), meStmt, GetPUIdx());
  wkCand->SetHasLocalOpnd(true);  // dummy
  workList.push_back(wkCand);
  wkCand->AddRealOccAsLast(*newOcc, GetPUIdx());
  // add to bucket at workcandHashTable[hashIdx]
  wkCand->SetNext(*preWorkCandHashTable.GetWorkcandFromIndex(hashIdx));
  preWorkCandHashTable.SetWorkCandAt(hashIdx, *wkCand);
  return wkCand;
}

void MeStmtPre::VersionStackChiListUpdate(const MapleMap<OStIdx, ChiMeNode*> &chiList) {
  for (auto it = chiList.begin(); it != chiList.end(); ++it) {
    const OriginalSt *ost = it->second->GetLHS()->GetOst();
    if (!ost->IsSymbolOst() || ost->GetIndirectLev() != 0) {
      continue;
    }
    MapleStack<ScalarMeExpr*> *pStack = versionStackVec.at(it->second->GetLHS()->GetOstIdx());
    pStack->push(it->second->GetLHS());
  }
}

// verify that there is no prior use of lhsVar before stmt in its BB
static bool NoPriorUseInBB(const VarMeExpr *lhsVar, MeStmt *defStmt) {
  for (MeStmt *stmt = defStmt->GetPrev(); stmt != nullptr; stmt = stmt->GetPrev()) {
    for (size_t i = 0; i < stmt->NumMeStmtOpnds(); ++i) {
      CHECK_FATAL(stmt->GetOpnd(i) != nullptr, "null ptr check");
      if (stmt->GetOpnd(i)->SymAppears(lhsVar->GetOstIdx())) {
        return false;
      }
    }
  }
  return true;
}

void MeStmtPre::BuildWorkListBB(BB *bb) {
  if (bb == nullptr) {
    return;
  }
  // record stack size for variable versions, used for stack pop up at return
  std::vector<size_t> curStackSizeVec;
  curStackSizeVec.resize(versionStackVec.size());
  for (size_t i = 1; i < versionStackVec.size(); ++i) {
    if (versionStackVec[i] == nullptr) {
      continue;
    }
    curStackSizeVec[i] = versionStackVec[i]->size();
  }
  // traverse var phi nodes to update versionStack
  MapleMap<OStIdx, MePhiNode*> &mePhiList = bb->GetMePhiList();
  for (auto it = mePhiList.begin(); it != mePhiList.end(); ++it) {
    MePhiNode *phiMeNode = it->second;
    const OriginalSt *ost = phiMeNode->GetLHS()->GetOst();
    if (!ost->IsSymbolOst() || ost->GetIndirectLev() != 0) {
      continue;
    }
    MapleStack<ScalarMeExpr*> *pStack = versionStackVec.at(phiMeNode->GetLHS()->GetOstIdx());
    pStack->push(phiMeNode->GetLHS());
  }
  // traverse statements
  uint32 seqStmt = 0;
  for (auto &stmt : bb->GetMeStmts()) {
    ++seqStmt;
    switch (stmt.GetOp()) {
      case OP_jstry:
      case OP_jscatch:
      case OP_finally:
      case OP_endtry:
      case OP_cleanuptry:
      case OP_try:
      case OP_catch:
      case OP_goto:
      case OP_comment:
      case OP_brtrue:
      case OP_brfalse:
      case OP_switch:
      case OP_igoto:
        break;
      case OP_membaracquire:
      case OP_membarrelease:
      case OP_membarstoreload:
      case OP_membarstorestore:
        CreateMembarOcc(stmt, seqStmt);
        break;
      case OP_gosub:
      case OP_retsub:
      case OP_throw:
      case OP_return:
        break;
      case OP_iassign: {
        auto &iass = static_cast<IassignMeStmt&>(stmt);
        VersionStackChiListUpdate(*iass.GetChiList());
        break;
      }
      case OP_maydassign: {
        auto &maydStmt = static_cast<MaydassignMeStmt&>(stmt);
        VersionStackChiListUpdate(*maydStmt.GetChiList());
        break;
      }
      case OP_regassign:
      case OP_eval:
      case OP_decref:
      case OP_decrefreset:
      case OP_incref:
      case OP_free:
      case OP_syncenter:
      case OP_syncexit:
      case OP_assertlt:
      case OP_assertge:
        break;
      case OP_call:
      case OP_virtualcall:
      case OP_virtualicall:
      case OP_superclasscall:
      case OP_interfacecall:
      case OP_interfaceicall:
      case OP_customcall:
      case OP_polymorphiccall:
      case OP_virtualcallassigned:
      case OP_virtualicallassigned:
      case OP_superclasscallassigned:
      case OP_interfacecallassigned:
      case OP_interfaceicallassigned:
      case OP_customcallassigned:
      case OP_polymorphiccallassigned: {
        auto &callMeStmt = static_cast<CallMeStmt&>(stmt);
        VersionStackChiListUpdate(*callMeStmt.GetChiList());
        break;
      }
      case OP_icall:
      case OP_icallassigned: {
        auto &icallMeStmt = static_cast<IcallMeStmt&>(stmt);
        VersionStackChiListUpdate(*icallMeStmt.GetChiList());
        break;
      }
      case OP_xintrinsiccall:
      case OP_intrinsiccallassigned:
      case OP_xintrinsiccallassigned:
      case OP_intrinsiccallwithtypeassigned: {
        auto &intrinStmt = static_cast<IntrinsiccallMeStmt&>(stmt);
        VersionStackChiListUpdate(*intrinStmt.GetChiList());
        break;
      }
      case OP_assertnonnull: {
        auto &unaryStmt = static_cast<UnaryMeStmt&>(stmt);
        if (!unaryStmt.GetOpnd()->IsLeaf()) {
          break;
        }
        (void)CreateStmtRealOcc(stmt, static_cast<int>(seqStmt));
        break;
      }
      case OP_dassign: {
        auto &dassMeStmt = static_cast<DassignMeStmt&>(stmt);
        if (!MeOption::dassignPre || !dassMeStmt.GetRHS()->IsLeaf() || !dassMeStmt.GetChiList()->empty() ||
            dassMeStmt.NeedIncref() ||
            (dassMeStmt.GetRHS()->GetOp() == OP_regread &&
             static_cast<RegMeExpr*>(dassMeStmt.GetRHS())->GetRegIdx() == -kSregThrownval)) {
          // update version stacks
          MapleStack<ScalarMeExpr*> *pStack = versionStackVec.at(dassMeStmt.GetVarLHS()->GetOstIdx());
          pStack->push(dassMeStmt.GetVarLHS());
          VersionStackChiListUpdate(*dassMeStmt.GetChiList());
          break;
        }
        VarMeExpr *varMeExpr = dassMeStmt.GetVarLHS();
        const OriginalSt *ost = varMeExpr->GetOst();
        if (ost->IsFinal()) {
          PreStmtWorkCand *stmtWkCand = CreateStmtRealOcc(stmt, seqStmt);
          stmtWkCand->SetLHSIsFinal(true);
        } else if (!dassMeStmt.GetRHS()->SymAppears(varMeExpr->GetOstIdx()) && dassMeStmt.GetRHS()->Pure()) {
          if (NoPriorUseInBB(dassMeStmt.GetVarLHS(), &stmt)) {
            (void)CreateStmtRealOcc(stmt, static_cast<int>(seqStmt));
          }
        } else if (dassMeStmt.GetLHS() != nullptr && dassMeStmt.GetLHS()->IsUseSameSymbol(*dassMeStmt.GetRHS())) {
          RemoveUnnecessaryDassign(dassMeStmt);
        }
        // update version stacks
        MapleStack<ScalarMeExpr*> *pStack = versionStackVec.at(dassMeStmt.GetVarLHS()->GetOstIdx());
        pStack->push(dassMeStmt.GetVarLHS());
        VersionStackChiListUpdate(*dassMeStmt.GetChiList());
        break;
      }
      case OP_intrinsiccallwithtype: {
        auto &intrnStmt = static_cast<IntrinsiccallMeStmt&>(stmt);
        VersionStackChiListUpdate(*intrnStmt.GetChiList());
        if (!MeOption::clinitPre) {
          VersionStackChiListUpdate(*intrnStmt.GetChiList());
          break;
        }
        if (intrnStmt.GetIntrinsic() == INTRN_JAVA_CLINIT_CHECK) {
          (void)CreateStmtRealOcc(stmt, static_cast<int>(seqStmt));
        }
        break;
      }
      case OP_intrinsiccall: {
        auto &intrnStmt = static_cast<IntrinsiccallMeStmt&>(stmt);
        bool allOperandsAreLeaf = true;
        for (size_t i = 0; i < intrnStmt.NumMeStmtOpnds(); ++i) {
          if (!intrnStmt.GetOpnd(i)->IsLeaf()) {
            allOperandsAreLeaf = false;
            break;
          }
        }
        if (!allOperandsAreLeaf) {
          break;
        }
        if (intrnStmt.GetIntrinsic() == INTRN_MPL_BOUNDARY_CHECK) {
          (void)CreateStmtRealOcc(stmt, static_cast<int>(seqStmt));
        }
        VersionStackChiListUpdate(*intrnStmt.GetChiList());
        break;
      }
      case OP_callassigned: {
        auto &callAss = static_cast<CallMeStmt&>(stmt);
        VersionStackChiListUpdate(*callAss.GetChiList());
        MIRFunction &callee = callAss.GetTargetFunction();
        if (!callAss.GetChiList()->empty() || !callee.IsPure() || !callee.IsNoThrowException()) {
          break;
        }
        if (IsParaAndRetTypeRefOrPtr(callAss)) {
          break;
        }

        bool allOperandsAreLeafAndPure = true;
        for (MeExpr *o : callAss.GetOpnds()) {
          if (!o->IsLeaf() || !o->Pure()) {
            allOperandsAreLeafAndPure = false;
            break;
          }
        }
        if (!allOperandsAreLeafAndPure) {
          break;
        }
        if (callAss.GetMustDefList()->empty()) {
          (void)CreateStmtRealOcc(stmt, static_cast<int>(seqStmt));
          break;
        }
        VarMeExpr &varMeExpr = utils::ToRef(safe_cast<VarMeExpr>(callAss.GetMustDefList()->front().GetLHS()));
        const OriginalSt *ost = ssaTab->GetOriginalStFromID(varMeExpr.GetOstIdx());
        if (ost->IsFinal()) {
          PreStmtWorkCand *stmtWkCand = CreateStmtRealOcc(stmt, seqStmt);
          stmtWkCand->SetLHSIsFinal(true);
        } else {
          bool allOperandFeasible = true;
          for (MeExpr *o : callAss.GetOpnds()) {
            if (o->SymAppears(varMeExpr.GetOstIdx())) {
              allOperandFeasible = false;
              break;
            }
          }
          if (allOperandFeasible && NoPriorUseInBB(&varMeExpr, &stmt)) {
            (void)CreateStmtRealOcc(stmt, static_cast<int>(seqStmt));
          }
        }
        break;
      }
      default:
        ASSERT(stmt.GetOp() == OP_comment, "must be OP_comment");
        break;
    }
    if (kOpcodeInfo.IsCallAssigned(stmt.GetOp())) {
      // update version stacks
      MapleVector<MustDefMeNode> *mustDefList = stmt.GetMustDefList();
      if (!mustDefList->empty()) {
        MeExpr *meLHS = mustDefList->front().GetLHS();
        if (meLHS->GetMeOp() == kMeOpVar) {
          auto *lhsVar = static_cast<VarMeExpr*>(meLHS);
          MapleStack<ScalarMeExpr*> *pStack = versionStackVec.at(lhsVar->GetOstIdx());
          pStack->push(lhsVar);
        }
      }
    }
  }
  if (bb->GetAttributes(kBBAttrIsExit) || bb->GetAttributes(kBBAttrWontExit)) {
    CreateExitOcc(*bb);
  }
  // recurse on child BBs in dominator tree
  const MapleSet<BBId> &domChildren = dom->GetDomChildren(bb->GetBBId());
  for (auto bbIt = domChildren.begin(); bbIt != domChildren.end(); ++bbIt) {
    BBId childBBId = *bbIt;
    BuildWorkListBB(GetBB(childBBId));
  }
  // pop the stacks back to their levels on entry
  for (size_t i = 1; i < versionStackVec.size(); ++i) {
    MapleStack<ScalarMeExpr*> *pStack = versionStackVec[i];
    if (pStack == nullptr) {
      continue;
    }
    size_t curSize = curStackSizeVec[i];
    while (pStack->size() > curSize) {
      pStack->pop();
    }
  }
}

void MeStmtPre::BuildWorkList() {
  // initialize version stack
  const MapleVector<OriginalSt*> &originalStVec = ssaTab->GetOriginalStTable().GetOriginalStVector();
  for (size_t i = 1; i < originalStVec.size(); ++i) {
    OriginalSt *ost = originalStVec[i];
    if (!ost->IsSymbolOst() || ost->GetIndirectLev() != 0) {
      continue;
    }
    MapleStack<ScalarMeExpr*> *varStack = ssaPreMemPool->New<MapleStack<ScalarMeExpr*>>(ssaPreAllocator.Adapter());
    varStack->push(static_cast<VarMeExpr*>(irMap->GetOrCreateZeroVersionVarMeExpr(*ost)));
    versionStackVec[ost->GetIndex()] = varStack;
  }
  BuildWorkListBB(func->GetCommonEntryBB());
}

void MeStmtPre::RemoveUnnecessaryDassign(DassignMeStmt &dssMeStmt) {
  BB *bb = dssMeStmt.GetBB();
  bb->RemoveMeStmt(&dssMeStmt);
  OStIdx ostIdx = dssMeStmt.GetVarLHS()->GetOstIdx();
  MapleSet<BBId> *bbSet = nullptr;
  if (candsForSSAUpdate.find(ostIdx) == candsForSSAUpdate.end()) {
    bbSet = ssaPreMemPool->New<MapleSet<BBId>>(std::less<BBId>(), ssaPreAllocator.Adapter());
    candsForSSAUpdate[ostIdx] = bbSet;
  } else {
    bbSet = candsForSSAUpdate[ostIdx];
  }
  (void)bbSet->insert(bb->GetBBId());
}

AnalysisResult *MeDoStmtPre::Run(MeFunction *func, MeFuncResultMgr *m, ModuleResultMgr*) {
  auto *dom = static_cast<Dominance*>(m->GetAnalysisResult(MeFuncPhase_DOMINANCE, func));
  ASSERT(dom != nullptr, "dominance phase has problem");
  auto *irMap = static_cast<MeIRMap*>(m->GetAnalysisResult(MeFuncPhase_IRMAPBUILD, func));
  ASSERT(irMap != nullptr, "irMap phase has problem");
  MeStmtPre ssaPre(*func, *irMap, *dom, *NewMemPool(), *NewMemPool(), MeOption::stmtprePULimit);
  if (DEBUGFUNC(func)) {
    ssaPre.SetSSAPreDebug(true);
  }
  ssaPre.ApplySSAPRE();
  if (!ssaPre.GetCandsForSSAUpdate().empty()) {
    MemPool *memPool = NewMemPool();
    CHECK_FATAL(memPool != nullptr, "must be");
    MeSSAUpdate ssaUpdate(*func, *func->GetMeSSATab(), *dom, ssaPre.GetCandsForSSAUpdate(), *memPool);
    ssaUpdate.Run();
  }
  if (DEBUGFUNC(func)) {
    LogInfo::MapleLogger() << "\n============== STMTPRE =============" << '\n';
    func->Dump(false);
  }
  return nullptr;
}
}  // namespace maple
