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
#include "lfo_dep_test.h"
#include "me_function.h"

namespace maple {
void LfoDepInfo::CreateDoloopInfo(BlockNode *block, DoloopInfo *parent) {
  StmtNode *stmt = block->GetFirst();
  while (stmt != nullptr) {
    switch (stmt->GetOpCode()) {
      case OP_doloop: {
        DoloopNode *doloop = static_cast<DoloopNode *>(stmt);
        DoloopInfo *doloopInfo = memPool->New<DoloopInfo>(&alloc, this, doloop, parent);
        doloopInfoMap.insert(std::pair<DoloopNode *, DoloopInfo *>(doloop, doloopInfo));
        if (parent) {
          parent->children.push_back(doloopInfo);
        } else {
          outermostDoloopInfoVec.push_back(doloopInfo);
        }
        LfoPart *lfopart = preEmit->GetLfoStmtPart(doloop->GetStmtID());
        MeStmt *meStmt = lfopart->GetMeStmt();
        doloopInfo->doloopBB = meStmt->GetBB();
        CreateDoloopInfo(doloop->GetDoBody(), doloopInfo);
        break;
      }
      case OP_block: {
        CreateDoloopInfo(static_cast<BlockNode *>(stmt), parent);
        break;
      }
      case OP_if: {
        IfStmtNode *ifstmtnode = static_cast<IfStmtNode *>(stmt);
        if (ifstmtnode->GetThenPart()) {
          CreateDoloopInfo(ifstmtnode->GetThenPart(), parent);
        }
        if (ifstmtnode->GetElsePart()) {
          CreateDoloopInfo(ifstmtnode->GetElsePart(), parent);
        }
        break;
      }
      case OP_dowhile:
      case OP_while: {
        if (parent) {
          parent->hasOtherCtrlFlow = true;
        }
        CreateDoloopInfo(static_cast<WhileStmtNode *>(stmt)->GetBody(), parent);
        break;
      }
      case OP_goto:
      case OP_igoto:
      case OP_brtrue:
      case OP_brfalse:
      case OP_switch:
      case OP_call:
      case OP_callassigned:
      case OP_icall:
      case OP_icallassigned:
      case OP_return:
      case OP_throw:
        if (parent) {
          parent->hasOtherCtrlFlow = true;
        }
        break;
      default:
        break;
    }
    stmt = stmt->GetNext();
  }
}

// check if x is loop-invariant w.r.t. the doloop
bool DoloopInfo::IsLoopInvariant(MeExpr *x) {
  if (x == nullptr) {
    return true;
  }
  switch (x->GetMeOp()) {
    case kMeOpAddrof:
    case kMeOpAddroffunc:
    case kMeOpConst:
    case kMeOpConststr:
    case kMeOpConststr16:
    case kMeOpSizeoftype:
    case kMeOpFieldsDist: return true;
    case kMeOpVar:
    case kMeOpReg: {
      ScalarMeExpr *scalar = static_cast<ScalarMeExpr *>(x);
      BB *defBB = scalar->DefByBB();
      return defBB == nullptr || (defBB != doloopBB && depInfo->dom->Dominate(*defBB, *doloopBB));
    }
    case kMeOpIvar: {
      IvarMeExpr *ivar = static_cast<IvarMeExpr *>(x);
      if (!IsLoopInvariant(ivar->GetBase())) {
        return false;
      }
      BB *defBB = ivar->GetMu()->DefByBB();
      return defBB == nullptr || (defBB != doloopBB && depInfo->dom->Dominate(*defBB, *doloopBB));
    }
    case kMeOpOp: {
      OpMeExpr *opexp = static_cast<OpMeExpr *>(x);
      return IsLoopInvariant(opexp->GetOpnd(0)) &&
             IsLoopInvariant(opexp->GetOpnd(1)) &&
             IsLoopInvariant(opexp->GetOpnd(2));
    }
    case kMeOpNary: {
      NaryMeExpr *opexp = static_cast<NaryMeExpr *>(x);
      for (uint32 i = 0; i < opexp->GetNumOpnds(); i++) {
        if (!IsLoopInvariant(opexp->GetOpnd(i))) {
          return false;
        }
      }
      return true;
    }
    default:
      break;
  }
  return false;
}

SubscriptDesc *DoloopInfo::BuildOneSubscriptDesc(BaseNode *subsX) {
  LfoPart *lfopart = depInfo->preEmit->GetLfoExprPart(subsX);
  MeExpr *meExpr = lfopart->GetMeExpr();
  SubscriptDesc *subsDesc = alloc->GetMemPool()->New<SubscriptDesc>(meExpr);
  if (IsLoopInvariant(meExpr)) {
    subsDesc->loopInvariant = true;
    return subsDesc;
  }
  Opcode op = subsX->GetOpCode();
  BaseNode *mainTerm = nullptr;
  if (op != OP_add && op != OP_sub) {
    mainTerm = subsX;
  } else {  // get addtiveConst
    BinaryNode *binnode = static_cast<BinaryNode *>(subsX);
    BaseNode *opnd0 = binnode->Opnd(0);
    BaseNode *opnd1 = binnode->Opnd(1);
    if (opnd1->op != OP_constval) {
      subsDesc->tooMessy = true;
      return subsDesc;
    }
    MIRConst *mirconst = static_cast<ConstvalNode *>(opnd1)->GetConstVal();
    if (mirconst->GetKind() != kConstInt) {
      subsDesc->tooMessy = true;
      return subsDesc;
    }
    subsDesc->additiveConst = static_cast<MIRIntConst *>(mirconst)->GetValue();
    if (op == OP_sub) {
      subsDesc->additiveConst = - subsDesc->additiveConst;
    }
    mainTerm = opnd0;
  }
  // process mainTerm
  BaseNode *varNode = nullptr;
  if (op != OP_mul) {
    varNode = mainTerm;
  } else {
    BinaryNode *mulbinnode = static_cast<BinaryNode *>(mainTerm);
    BaseNode *mulopnd0 = mulbinnode->Opnd(0);
    BaseNode *mulopnd1 = mulbinnode->Opnd(1);
    if (mulopnd0->GetOpCode() != OP_dread) {
      subsDesc->tooMessy = true;
      return subsDesc;
    }
    varNode = mulopnd0;
    if (mulopnd1->GetOpCode() != OP_constval) {
      subsDesc->tooMessy = true;
      return subsDesc;
    }
    MIRConst *mirconst = static_cast<ConstvalNode *>(mulopnd1)->GetConstVal();
    if (mirconst->GetKind() != kConstInt) {
      subsDesc->tooMessy = true;
      return subsDesc;
    }
    subsDesc->coeff = static_cast<MIRIntConst *>(mirconst)->GetValue();
    if (subsDesc->coeff < 0) {
      subsDesc->tooMessy = true;
      return subsDesc;
    }
  }
  // process varNode
  if (varNode->GetOpCode() == OP_dread) {
    DreadNode *dnode = static_cast<DreadNode *>(varNode);
    if (dnode->GetStIdx() == doloop->GetDoVarStIdx()) {
      subsDesc->iv = dnode;
      return subsDesc;
    }
  }
  subsDesc->tooMessy = true;
  return subsDesc;
}

ArrayAccessDesc *DoloopInfo::BuildOneArrayAccessDesc(ArrayNode *arr, bool isRHS) {
#if 0
  MIRType *atype =  arr->GetArrayType(GlobalTables::GetTypeTable());
  ASSERT(atype->GetKind() == kTypeArray, "type was wrong");
  MIRArrayType *arryty = static_cast<MIRArrayType *>(atype);
  size_t dim = arryty->GetDim();
  CHECK_FATAL(dim == arr->NumOpnds() - 1, "BuildOneArrayAccessDesc: inconsistent array dimension");
#else
  size_t dim = arr->NumOpnds() - 1;
#endif
  // determine arrayOst
  LfoPart *lfopart = depInfo->preEmit->GetLfoExprPart(arr);
  OpMeExpr *arrayMeExpr = static_cast<OpMeExpr *>(lfopart->GetMeExpr());
  OriginalSt *arryOst = nullptr;
  if (arrayMeExpr->GetOpnd(0)->GetMeOp() == kMeOpAddrof) {
    AddrofMeExpr *addrof = static_cast<AddrofMeExpr *>(arrayMeExpr->GetOpnd(0));
    arryOst = depInfo->lfoFunc->meFunc->GetMeSSATab()->GetOriginalStFromID(addrof->GetOstIdx());
  } else {
    ScalarMeExpr *scalar = dynamic_cast<ScalarMeExpr *>(arrayMeExpr->GetOpnd(0));
    if (scalar) {
      arryOst = scalar->GetOst();
    } else {
      hasPtrAccess = true;
      return nullptr;
    }
  }
  ArrayAccessDesc *arrDesc = alloc->GetMemPool()->New<ArrayAccessDesc>(alloc, arr, arryOst);
  if (isRHS) {
    rhsArrays.push_back(arrDesc);
  } else {
    lhsArrays.push_back(arrDesc);
  }
  for (size_t i = 0; i < dim; i++) {
    SubscriptDesc *subs = BuildOneSubscriptDesc(arr->GetIndex(i));
    arrDesc->subscriptVec.push_back(subs);
  }
  return arrDesc;
}

void DoloopInfo::CreateRHSArrayAccessDesc(BaseNode *x) {
  if (x->GetOpCode() == OP_array) {
    BuildOneArrayAccessDesc(static_cast<ArrayNode *>(x), true /* isRHS */);
  }
  for (size_t i = 0; i < x->NumOpnds(); i++) {
    CreateRHSArrayAccessDesc(x->Opnd(i));
  }
}

void DoloopInfo::CreateArrayAccessDesc(BlockNode *block) {
  StmtNode *stmt = block->GetFirst();
  while (stmt) {
    switch (stmt->GetOpCode()) {
      case OP_doloop: {
        CHECK_FATAL(false, "CreateArrayAccessDesc: non-innermost doloop NYI");
        break;
      }
      case OP_block: {
        CreateArrayAccessDesc(static_cast<BlockNode *>(stmt));
        break;
      }
      case OP_if: {
        CreateRHSArrayAccessDesc(stmt->Opnd(0));
        IfStmtNode *ifstmtnode = static_cast<IfStmtNode *>(stmt);
        if (ifstmtnode->GetThenPart())
          CreateArrayAccessDesc(ifstmtnode->GetThenPart());
        if (ifstmtnode->GetElsePart())
          CreateArrayAccessDesc(ifstmtnode->GetElsePart());
        break;
      }
      case OP_dowhile:
      case OP_while: {
        CreateRHSArrayAccessDesc(stmt->Opnd(0));
        CreateArrayAccessDesc(static_cast<WhileStmtNode *>(stmt)->GetBody());
        break;
      }
      case OP_iassign: {
        IassignNode *iass = static_cast<IassignNode *>(stmt);
        if (iass->addrExpr->GetOpCode() == OP_array) {
          ArrayAccessDesc *adesc = BuildOneArrayAccessDesc(static_cast<ArrayNode *>(iass->addrExpr), false /* isRHS */);
          if (adesc == nullptr) {
            hasMayDef = true;
          } else {
            // check if the chi list has only the same array
            LfoPart *lfopart = depInfo->preEmit->GetLfoStmtPart(iass->GetStmtID());
            IassignMeStmt *iassMeStmt = static_cast<IassignMeStmt *>(lfopart->GetMeStmt());
            MapleMap<OStIdx, ChiMeNode*> *chilist = iassMeStmt->GetChiList();
            MapleMap<OStIdx, ChiMeNode*>::iterator chiit = chilist->begin();
            for (; chiit != chilist->end(); chiit++) {
              OriginalSt *chiOst = depInfo->lfoFunc->meFunc->GetMeSSATab()->GetOriginalStFromID(chiit->first);
              if (!chiOst->IsSameSymOrPreg(adesc->arrayOst)) {
                hasMayDef = true;
                break;
              }
            }
          }
        } else {
          hasPtrAccess = true;
        }
        CreateRHSArrayAccessDesc(iass->rhs);
        break;
      }
      case OP_dassign:
      case OP_regassign: {
        hasScalarAssign = true;
        CreateRHSArrayAccessDesc(stmt->Opnd(0));
        break;
      }
      [[clang::fallthrough]];
      default: {
        for (size_t i = 0; i < stmt->NumOpnds(); i++) {
          CreateRHSArrayAccessDesc(stmt->Opnd(i));
        }
        break;
      }
    }
    stmt = stmt->GetNext();
  }
}

void DoloopInfo::CreateDepTestLists() {
  size_t i, j;
  for (i = 0; i < lhsArrays.size(); i++) {
    for (j = i + 1; j < lhsArrays.size(); j++) {
      if (lhsArrays[i]->arrayOst->IsSameSymOrPreg(lhsArrays[j]->arrayOst)) {
        outputDepTestList.push_back(DepTestPair(i, j));
      }
    }
  }
  for (i = 0; i < lhsArrays.size(); i++) {
    for (j = 0; j < rhsArrays.size(); j++) {
      if (lhsArrays[i]->arrayOst->IsSameSymOrPreg(rhsArrays[j]->arrayOst)) {
        flowDepTestList.push_back(DepTestPair(i, j));
      }
    }
  }
}

static int64 Gcd(int64 a, int64 b) {
  CHECK_FATAL(a > 0 && b >= 0, "Gcd: NYI");
  if (b == 0) {
    return a;
  }
  return Gcd(b, a % b);
}

void DoloopInfo::TestDependences(MapleVector<DepTestPair> *depTestList, bool bothLHS) {
  size_t i, j;
  for (i = 0; i < depTestList->size(); i++) {
    DepTestPair *testPair = &(*depTestList)[i];
    ArrayAccessDesc *arrDesc1 = lhsArrays[testPair->depTestPair.first];
    ArrayAccessDesc *arrDesc2 = nullptr;
    if (bothLHS) {
      arrDesc2 = lhsArrays[testPair->depTestPair.second];
    } else {
      arrDesc2 = rhsArrays[testPair->depTestPair.second];
    }
    CHECK_FATAL(arrDesc1->subscriptVec.size() == arrDesc2->subscriptVec.size(),
                "TestDependences: inconsistent array dimension");
    for (j = 0; j < arrDesc1->subscriptVec.size(); j++) {
      SubscriptDesc *subs1 = arrDesc1->subscriptVec[j];
      SubscriptDesc *subs2 = arrDesc2->subscriptVec[j];
      if (subs1->tooMessy || subs2->tooMessy) {
        testPair->dependent = true;
        testPair->unknownDist = true;
        break;
      }
      if (subs1->loopInvariant || subs2->loopInvariant) {
        if (subs1->subscriptX == subs2->subscriptX) {
          continue;
        } else {
          testPair->dependent = true;
          testPair->unknownDist = true;
          break;
        }
      }
      if (subs1->coeff == subs2->coeff) { // lamport test
        if (((subs1->additiveConst - subs2->additiveConst) % subs1->coeff) != 0) {
          continue;
        }
        testPair->dependent = true;
        int64 dist = (subs1->additiveConst - subs2->additiveConst) / subs1->coeff;
        if (dist != 0) {
          testPair->depDist = dist;
        }
        continue;
      }
      // gcd test
      if ((subs1->additiveConst - subs2->additiveConst) % Gcd(subs1->coeff, subs2->coeff) == 0) {
        testPair->dependent = true;
        testPair->unknownDist = true;
        break;
      }
    }
  }
}

ArrayAccessDesc* DoloopInfo::GetArrayAccessDesc(ArrayNode *node, bool isRHS) {
  MapleVector<ArrayAccessDesc *>* arrayDescptr = isRHS ? &rhsArrays : &lhsArrays;
  for (auto it = arrayDescptr->begin(); it != arrayDescptr->end(); it++) {
    if ((*it)->theArray == node) {
      return (*it);
    }
  }
  return nullptr;
}

bool DoloopInfo::Parallelizable() {
  if (hasPtrAccess || hasOtherCtrlFlow || hasScalarAssign || hasMayDef) {
    return false;
  }
  for (size_t i = 0; i < outputDepTestList.size(); i++) {
    DepTestPair *testPair = &outputDepTestList[i];
    if (testPair->dependent && (testPair->unknownDist || testPair->depDist != 0)) {
      return false;
    }
  }
  for (size_t i = 0; i < flowDepTestList.size(); i++) {
    DepTestPair *testPair = &flowDepTestList[i];
    if (testPair->dependent && (testPair->unknownDist || testPair->depDist != 0)) {
      return false;
    }
  }
  return true;
}

void LfoDepInfo::PerformDepTest() {
  size_t i;
  MapleMap<DoloopNode *, DoloopInfo *>::iterator mapit = doloopInfoMap.begin();
  for (; mapit != doloopInfoMap.end(); mapit++) {
    DoloopInfo *doloopInfo = mapit->second;
    if (!doloopInfo->children.empty()) {
      continue;  // only handling innermost doloops
    }
    doloopInfo->CreateArrayAccessDesc(doloopInfo->doloop->GetDoBody());
    if (DEBUGFUNC(lfoFunc->meFunc)) {
      LogInfo::MapleLogger() << "Innermost Doloop:";
      if (doloopInfo->hasPtrAccess) {
        LogInfo::MapleLogger() << " hasPtrAccess";
      }
      if (doloopInfo->hasOtherCtrlFlow) {
        LogInfo::MapleLogger() << " hasOtherCtrlFlow";
      }
      if (doloopInfo->hasScalarAssign) {
        LogInfo::MapleLogger() << " hasScalarAssign";
      }
      if (doloopInfo->hasMayDef) {
        LogInfo::MapleLogger() << " hasMayDef";
      }
      LogInfo::MapleLogger() << std::endl;
      doloopInfo->doloop->Dump(0);
      LogInfo::MapleLogger() << "LHS arrays:\n";
      for (i = 0; i < doloopInfo->lhsArrays.size(); i++) {
        ArrayAccessDesc *arrAcc = doloopInfo->lhsArrays[i];
        LogInfo::MapleLogger() << "(L" << i << ") ";
        arrAcc->arrayOst->Dump();
        LogInfo::MapleLogger() << " subscripts:";
        for (SubscriptDesc *subs : arrAcc->subscriptVec) {
          if (subs->loopInvariant) {
            LogInfo::MapleLogger() << " [loopinvariant]";
          } else if (subs->tooMessy) {
            LogInfo::MapleLogger() << " [messy]";
          } else {
            LogInfo::MapleLogger() << " [" << subs->coeff << "*";
            LfoPart *lfopart = preEmit->GetLfoExprPart(subs->iv);
            ScalarMeExpr *scalar = static_cast<ScalarMeExpr *>(lfopart->GetMeExpr());
            scalar->GetOst()->Dump();
            LogInfo::MapleLogger() << "+" << subs->additiveConst << "]";
          }
        }
        LogInfo::MapleLogger() << std::endl;
      }
      LogInfo::MapleLogger() << "RHS arrays:\n";
      for (i = 0; i < doloopInfo->rhsArrays.size(); i++) {
        ArrayAccessDesc *arrAcc = doloopInfo->rhsArrays[i];
        LogInfo::MapleLogger() << "(R" << i << ") ";
        arrAcc->arrayOst->Dump();
        LogInfo::MapleLogger() << " subscripts:";
        for (SubscriptDesc *subs : arrAcc->subscriptVec) {
          if (subs->loopInvariant) {
            LogInfo::MapleLogger() << " [loopinvariant]";
          } else if (subs->tooMessy) {
            LogInfo::MapleLogger() << " [messy]";
          } else {
            LogInfo::MapleLogger() << " [" << subs->coeff << "*";
            LfoPart *lfopart = preEmit->GetLfoExprPart(subs->iv);
            ScalarMeExpr *scalar = static_cast<ScalarMeExpr *>(lfopart->GetMeExpr());
            scalar->GetOst()->Dump();
            LogInfo::MapleLogger() << "+" << subs->additiveConst << "]";
          }
        }
        LogInfo::MapleLogger() << std::endl;
      }
      LogInfo::MapleLogger() << std::endl;
    }
    doloopInfo->CreateDepTestLists();
    doloopInfo->TestDependences(&doloopInfo->outputDepTestList, true);
    doloopInfo->TestDependences(&doloopInfo->flowDepTestList, false);
    if (DEBUGFUNC(lfoFunc->meFunc)) {
      for (DepTestPair item : doloopInfo->outputDepTestList) {
        LogInfo::MapleLogger() << "Dep between L" << item.depTestPair.first << " and L" << item.depTestPair.second;
        if (!item.dependent) {
          LogInfo::MapleLogger() << " independent";
        } else {
          LogInfo::MapleLogger() << " dependent";
          if (item.unknownDist) {
            LogInfo::MapleLogger() << " unknownDist";
          } else {
            LogInfo::MapleLogger() << " distance: " << item.depDist;
          }
        }
        LogInfo::MapleLogger() << std::endl;
      }
      for (DepTestPair item : doloopInfo->flowDepTestList) {
        LogInfo::MapleLogger() << "Dep between L" << item.depTestPair.first << " and R" << item.depTestPair.second;
        if (!item.dependent) {
          LogInfo::MapleLogger() << " independent";
        } else {
          LogInfo::MapleLogger() << " dependent";
          if (item.unknownDist) {
            LogInfo::MapleLogger() << " unknownDist";
          } else {
            LogInfo::MapleLogger() << " distance: " << item.depDist;
          }
        }
        LogInfo::MapleLogger() << std::endl;
      }
      if (doloopInfo->Parallelizable()) {
        LogInfo::MapleLogger() << "LOOP CAN BE VECTORIZED\n";
      }
    }
  }
}

AnalysisResult *DoLfoDepTest::Run(MeFunction *func, MeFuncResultMgr *m, ModuleResultMgr*) {
  Dominance *dom = static_cast<Dominance *>(m->GetAnalysisResult(MeFuncPhase_DOMINANCE, func));
  ASSERT(dom != nullptr, "dominance phase has problem");
  LfoPreEmitter *preEmit = static_cast<LfoPreEmitter *>(m->GetAnalysisResult(MeFuncPhase_LFOPREEMIT, func));
  ASSERT(preEmit != nullptr, "lfo preemit phase has problem");
  LfoFunction *lfoFunc = func->GetLfoFunc();
  MemPool *depTestMp = NewMemPool();
  LfoDepInfo *depInfo = depTestMp->New<LfoDepInfo>(depTestMp, lfoFunc, dom, preEmit);
  if (DEBUGFUNC(func)) {
    LogInfo::MapleLogger() << "\n============== LFO_DEP_TEST =============" << '\n';
  }
  depInfo->CreateDoloopInfo(func->GetMirFunc()->GetBody(), nullptr);
  depInfo->PerformDepTest();
  if (DEBUGFUNC(func)) {
    LogInfo::MapleLogger() << "________________" << std::endl;
    lfoFunc->meFunc->GetMirFunc()->Dump();
  }
  return depInfo;
}
}  // namespace maple
