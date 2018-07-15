#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Intrinsics.h"

#include "llvm/IR/IRBuilder.h"
using namespace llvm;

namespace {
struct Mangle : public FunctionPass
{
	static char ID;
	Mangle() : FunctionPass(ID) {}

	std::string valName(Value* val, ModuleSlotTracker* machine)
	{
		if(val->hasName())
			return val->getName();
		else
		{
			int slot = machine->getLocalSlot(val);
			if(slot == -1)
				return "";
			else
				return "%" + std::to_string(machine->getLocalSlot(val));
		}
	}

	Module* m;
	Instruction* mangle(Value* val_unmangled, Instruction* instr_it)
	{
		// Cast the argument to an integer in order to do maths on it
		// errs() << "MANGLE\n";
		auto val_unmangled_i = CastInst::CreatePointerCast(val_unmangled, llvm::Type::getInt64Ty(instr_it->getContext()), "", instr_it);

		Function* mangle_func = cast<Function>(m->getOrInsertFunction("mangle", IntegerType::get(instr_it->getContext(), 64), IntegerType::get(instr_it->getContext(), 64)));
		ArrayRef<Value*> vals(val_unmangled_i);
		Instruction* val_mangled_i = CallInst::Create(mangle_func, vals, "", instr_it);

		auto val_mangled = CastInst::Create(CastInst::IntToPtr, val_mangled_i, val_unmangled->getType(), "", instr_it);
		return val_mangled;
	}

	Instruction* do_demangle(Value* val_mangled, Instruction* instr_it)
	{
		// errs() << "DEMANGLE\n";
		auto val_mangled_i = CastInst::CreatePointerCast(val_mangled, llvm::Type::getInt64Ty(instr_it->getContext()), "");
		val_mangled_i->insertAfter(instr_it);

		Function* demangle_func = cast<Function>(m->getOrInsertFunction("demangle", IntegerType::get(instr_it->getContext(), 64), IntegerType::get(instr_it->getContext(), 64)));		ArrayRef<Value*> vals(val_mangled_i);
		Instruction* val_demangled_i = CallInst::Create(demangle_func, vals, "");
		val_demangled_i->insertAfter(val_mangled_i);

		auto val_demangled = CastInst::Create(CastInst::IntToPtr, val_demangled_i, val_mangled->getType(), "");
		val_demangled->insertAfter(val_demangled_i);
		val_mangled->replaceAllUsesWith(val_demangled);
		val_mangled_i->setOperand(0, val_mangled);
		return val_demangled;
	}

	Instruction* try_demangle(Value* val_mangled, Instruction* instr_it)
	{
		// errs() << "TRY_DEMANGLE\n";
		int bs = val_mangled->getType()->getScalarSizeInBits();
		CastInst* val_mangled_i;
		if(val_mangled->getType()->isPointerTy())
			val_mangled_i = CastInst::Create(Instruction::PtrToInt, val_mangled, llvm::Type::getInt64Ty(instr_it->getContext()));
		else if(bs == 64)
			val_mangled_i = CastInst::Create(Instruction::BitCast, val_mangled, llvm::Type::getInt64Ty(instr_it->getContext()));
		else if(bs < 64)
			val_mangled_i = CastInst::Create(Instruction::ZExt, val_mangled, llvm::Type::getInt64Ty(instr_it->getContext()));
		else
			val_mangled_i = CastInst::Create(Instruction::Trunc, val_mangled, llvm::Type::getInt64Ty(instr_it->getContext()));
		val_mangled_i->insertAfter(instr_it);

		Function* try_demangle_func = cast<Function>(m->getOrInsertFunction("try_demangle", IntegerType::get(instr_it->getContext(), 64), IntegerType::get(instr_it->getContext(), 64)));
		ArrayRef<Value*> vals(val_mangled_i);
		Instruction* val_demangled_i = CallInst::Create(try_demangle_func, vals, "");
		val_demangled_i->insertAfter(val_mangled_i);

		CastInst* val_demangled;
		if(val_mangled->getType()->isPointerTy())
			val_demangled = CastInst::Create(Instruction::IntToPtr, val_demangled_i, val_mangled->getType());
		else if(bs == 64)
			val_demangled = CastInst::Create(Instruction::BitCast, val_demangled_i, val_mangled->getType());
		else if(bs < 64)
			val_demangled = CastInst::Create(Instruction::Trunc, val_demangled_i, val_mangled->getType());
		else
			val_demangled = CastInst::Create(Instruction::ZExt, val_demangled_i, val_mangled->getType());
		val_demangled->insertAfter(val_demangled_i);
		val_mangled->replaceAllUsesWith(val_demangled);
		val_mangled_i->setOperand(0, val_mangled);
		return val_demangled;
	}

	inline bool shallMangle(Value* val, Value* addr)
	{
		if(const GlobalValue *GV = dyn_cast<GlobalValue>(addr))
		{
			if(GV->hasExternalLinkage())
			{
				// Do not mangle external symbols, since this might lead to problems with libraries
				return false;
			}
		}
		return val->getType()->isPointerTy() && addr->stripPointerCasts()->getType()->getPointerElementType()->isPointerTy();
	}

	inline bool shallDemangle(Value* val, Value* addr)
	{
		return val->getType()->isPointerTy() || val->getType()->canLosslesslyBitCastTo(llvm::Type::getInt64Ty(addr->getContext()));
	}

	bool runOnFunction(Function &func) override
	{
		if(func.getName().equals("mangle") || func.getName().equals("demangle") || func.getName().equals("try_demangle"))
			return true;
		m = func.getParent();
		ModuleSlotTracker machine(m, true);
		machine.incorporateFunction(func);

		Function* addr_to_ret_F = Intrinsic::getDeclaration(m, llvm::Intrinsic::ID::addressofreturnaddress);

		Instruction* first_alloca = NULL;
		std::vector<Instruction*> rets;

		// errs() << "Function: ";
		errs().write_escaped(func.getName()) << '\n';
		// func.print(errs());

		for(Function::iterator bb_it = func.begin(); bb_it != func.end(); bb_it++)
		{
			for(BasicBlock::iterator instr_it = bb_it->begin(); instr_it != bb_it->end(); instr_it++)
			{
				// instr_it is an Instruction
				// http://llvm.org/doxygen/AsmWriter_8cpp_source.html#l02923
				/*
				errs() << "I: '";
				instr_it->print(errs());
				errs() << "'\n";
				*/

				Instruction* instr_it_ = (Instruction*)&*instr_it;
				
				if(instr_it->getOpcode() == Instruction::Alloca)
				{
					if(!first_alloca)
						first_alloca = instr_it_;
				}

				if(instr_it->getOpcode() == Instruction::Store)
				{
					auto op0 = instr_it->getOperand(0);
					if(shallMangle(op0, instr_it->getOperand(1)))
					{
						/* Storing a pointer => MANGLE */
						auto op0_mangled = mangle(op0, instr_it_);
						// Replace the instruction's operand with our mangled pointer
						instr_it->setOperand(0, op0_mangled);
					}
				}
				else if(instr_it->getOpcode() == Instruction::Load)
				{
					if(shallDemangle(instr_it_, instr_it->getOperand(0)))
					{
						/* Loading a pointer => DEMANGLE */
						try_demangle(instr_it_, instr_it_);
					}
				}
				else if(instr_it->getOpcode() == Instruction::Ret)
				{
					rets.push_back(instr_it_);
				}
			}

		}

		/* At function start mangle the stored return address */
		auto first_instruction = (Instruction*)&*(func.begin()->begin());
		// Get pointer to stored RIP
		ArrayRef<Value*> vals;
		Instruction* ret_addr_ptr_ = CallInst::Create(addr_to_ret_F, vals, "", (Instruction*)&*(func.begin()->begin()));
		auto ret_addr_ptr = CastInst::CreatePointerCast(ret_addr_ptr_, llvm::Type::getInt64Ty(ret_addr_ptr_->getContext())->getPointerTo()->getPointerTo(), "");
		ret_addr_ptr->insertAfter(ret_addr_ptr_);

		IntegerType* int64 = IntegerType::get(ret_addr_ptr_->getContext(), 64);
		PointerType* int64p = int64->getPointerTo();
		if(!first_alloca)
		{
			// Create dummy alloca
			// errs() << "CREATING DUMMY ALLOCA\n";
			const DataLayout &DL = m->getDataLayout();
     		first_alloca = new AllocaInst(int64, DL.getAllocaAddrSpace(), 0);
     		first_alloca->insertBefore(first_instruction);
		}
		Function* mangle_range = cast<Function>(m->getOrInsertFunction("mangle_range", int64, int64p, int64p));
		Function* demangle_range = cast<Function>(m->getOrInsertFunction("demangle_range", int64, int64p, int64p));
		std::vector<Value*> args;
		auto ret_addr_ptr_i64p = CastInst::CreatePointerCast(ret_addr_ptr, int64p, "");
		ret_addr_ptr_i64p->insertAfter(ret_addr_ptr);
		auto first_alloca_i64p = CastInst::CreatePointerCast(first_alloca, int64p, "");
		first_alloca_i64p->insertAfter(first_alloca);
		
		args.push_back(ret_addr_ptr_i64p);
		args.push_back(first_alloca_i64p);
		ArrayRef<Value*> vals2(args);
		Instruction* mangled = CallInst::Create(mangle_range, vals2, "");
		mangled->insertAfter(first_alloca_i64p);

		for(auto instr_it = rets.begin(); instr_it != rets.end(); instr_it++)
		{
			Instruction* demangled = CallInst::Create(demangle_range, vals2, "");
			demangled->insertBefore(*instr_it);
		}

		// func.print(errs());
		return true;
	}
};
}

char Mangle::ID = 0;
static RegisterPass<Mangle> X("mangle", "Pass to mangle and demangle pointers to protect against partial overwrites", false, false);

static void registerMangle(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
	PM.add(new Mangle());
}

static RegisterStandardPasses RegisterMyPassNoOpt(PassManagerBuilder::EP_EnabledOnOptLevel0, registerMangle);
static RegisterStandardPasses RegisterMyPassOpt(PassManagerBuilder::EP_OptimizerLast, registerMangle);
