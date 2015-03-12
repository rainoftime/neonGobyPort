#include <string>

#include "rcs/ID/IDAssigner.h"

#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::opt<std::string> TheFunctionName("func-name", cl::desc("Function name"));
static cl::opt<std::string> TheValueName("value-name", cl::desc("Value name"));
static cl::opt<unsigned> TheValueID("value-id", cl::init(rcs::IDAssigner::InvalidID), cl::desc("Value ID"));
static cl::opt<unsigned> TheInsID("ins-id", cl::init(rcs::IDAssigner::InvalidID), cl::desc("Instruction ID"));

namespace rcs {
struct IDLookUp: public ModulePass
{
  static char ID;

  IDLookUp(): ModulePass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

private:
  void lookUpValueByID(unsigned ValueID);
  void lookUpValueByInsID(unsigned InsID);
  Value *lookUpValueByName(Module &M,
                           const std::string &TheFunctionName,
                           const std::string &TheValueName);
  void lookUpIDByName(Module &M,
                      const std::string &TheFunctionName,
                      const std::string &TheValueName);
};

void IDLookUp::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<IDAssigner>();
  AU.setPreservesAll();
}

bool IDLookUp::runOnModule(Module &M)
{
  if (!(TheValueName == "" ||
          (TheValueID == IDAssigner::InvalidID &&
           TheInsID == IDAssigner::InvalidID)))
    llvm_unreachable("Cannot specify both value-name and value/ins-id");
  if (!(TheValueID == IDAssigner::InvalidID ||
          TheInsID == IDAssigner::InvalidID))
    llvm_unreachable("Cannot specify both value-id and ins-id");

  if (TheValueID != IDAssigner::InvalidID) {
    lookUpValueByID(TheValueID);
  } else if (TheInsID != IDAssigner::InvalidID) {
    lookUpValueByInsID(TheInsID);
  } else {
    lookUpIDByName(M, TheFunctionName, TheValueName);
  }
  return false;
}

void IDLookUp::lookUpValueByID(unsigned ValueID) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  if (Value *V = IDA.getValue(ValueID)) {
    IDA.printValue(errs(), V);
    errs() << "\n";
  } else {
    errs() << "Not found\n";
  }
}

void IDLookUp::lookUpValueByInsID(unsigned InsID) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  if (Value *V = IDA.getInstruction(InsID)) {
    IDA.printValue(errs(), V);
    errs() << "\n";
  } else {
    errs() << "Not found\n";
  }
}

void IDLookUp::lookUpIDByName(Module &M,
                              const std::string &TheFunctionName,
                              const std::string &TheValueName) {
  assert(TheValueName != "");

  if (Value *V = lookUpValueByName(M, TheFunctionName, TheValueName)) {
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    errs() << "Value = " << *V << "\n";
    errs() << "Value ID = " << IDA.getValueID(V) << "\n";
  } else {
    errs() << "Not found\n";
  }
}

Value *IDLookUp::lookUpValueByName(Module &M,
                                   const std::string &TheFunctionName,
                                   const std::string &TheValueName) {
  if (TheFunctionName == "") {
    // Look up a global value.
    return M.getNamedValue(TheValueName);
  }

  // Look up a local value inside <TheFunctionName>.
  if (Function *F = M.getFunction(TheFunctionName)) {
    bool IsNumber = true;
    for (unsigned long SI = 0; SI != TheValueName.length(); ++SI) {
      if (!isdigit(TheValueName[SI])) {
        IsNumber = false;
        break;
      }
    }
    if (IsNumber) {
      // Look up a local value without name.
      int NumUnnamedValues = 0;
      for (Function::arg_iterator AI = F->arg_begin();
           AI != F->arg_end(); ++AI) {
        if (!AI->hasName() && !AI->getType()->isVoidTy()) {
          if (NumUnnamedValues == atoi(TheValueName.c_str())) {
            return AI;
          }
          NumUnnamedValues++;
        }
      }
      for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
        if (!BB->hasName()) {
          NumUnnamedValues++;
        }
        for (BasicBlock::iterator Ins = BB->begin();
             Ins != BB->end(); ++Ins) {
          if (!Ins->hasName() && !Ins->getType()->isVoidTy()) {
            if (NumUnnamedValues == atoi(TheValueName.c_str())) {
              return Ins;
            }
            NumUnnamedValues++;
          }
        }
      }
    } else {
      for (Function::arg_iterator AI = F->arg_begin();
           AI != F->arg_end(); ++AI) {
        if (AI->getName() == TheValueName) {
          return AI;
        }
      }
      for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
        for (BasicBlock::iterator Ins = BB->begin();
             Ins != BB->end(); ++Ins) {
          if (Ins->getName() == TheValueName) {
            return Ins;
          }
        }
      }
    }
  }

  return NULL;
}

char IDLookUp::ID = 0;
static RegisterPass<IDLookUp> X("lookup-id", "Look up the ID of a named value or vice versa", false, true);
}
