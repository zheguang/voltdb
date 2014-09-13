/* This file is part of VoltDB.
 * Copyright (C) 2008-2014 VoltDB Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/CodegenContext.hpp"
#include "common/TupleSchema.h"
#include "common/types.h"
#include "common/value_defs.h"
#include "expressions/abstractexpression.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

namespace voltdb {
    CodegenContext::CodegenContext()
        : m_llvmContext()
        , m_module(NULL)
        , m_executionEngine()
        , m_passManager()
        , m_errorString()
        , m_builder()
        , m_tupleArg(NULL)
    {
        // This really only needs to be called once for the whole process.
        llvm::InitializeNativeTarget();

        m_llvmContext.reset(new llvm::LLVMContext());

        m_module = new llvm::Module("voltdb_generated_code", *m_llvmContext);

        llvm::ExecutionEngine *engine = llvm::EngineBuilder(m_module).setErrorStr(&m_errorString).create();

        if (! engine) {
            // throwing in a constructor is bad
            // all of this should be in an init method
            // should also release module in this case.
            throw std::exception();
        }

        // m_module now owned by the engine.

        m_executionEngine.reset(engine);

        m_passManager.reset(new llvm::FunctionPassManager(m_module));

        m_passManager->add(new llvm::DataLayout(*m_executionEngine->getDataLayout()));

        // Provide basic AliasAnalysis support for GVN.
        m_passManager->add(llvm::createBasicAliasAnalysisPass());
        // Do simple "peephole" optimizations and bit-twiddling optzns.
        m_passManager->add(llvm::createInstructionCombiningPass());
        // Reassociate expressions.
        m_passManager->add(llvm::createReassociatePass());
        // Eliminate Common SubExpressions.
        m_passManager->add(llvm::createGVNPass());
        // Simplify the control flow graph (deleting unreachable blocks, etc).
        m_passManager->add(llvm::createCFGSimplificationPass());

        m_passManager->doInitialization();

        m_builder.reset(new llvm::IRBuilder<>(*m_llvmContext));
    }

    CodegenContext::~CodegenContext() {
    }

    llvm::LLVMContext&
    CodegenContext::getLlvmContext() {
        return *m_llvmContext;
    }

    llvm::IRBuilder<>&
    CodegenContext::builder() {
        return *m_builder;
    }

    llvm::Value*
    CodegenContext::getTupleArg() {
        assert(m_tupleArg != NULL);
        return m_tupleArg;
    }

    llvm::Type*
    CodegenContext::getLlvmType(ValueType voltType) {
        llvm::LLVMContext &ctx = *m_llvmContext;
        switch (voltType) {
        case VALUE_TYPE_TINYINT:
            return llvm::Type::getInt8Ty(ctx);
        case VALUE_TYPE_SMALLINT:
            return llvm::Type::getInt16Ty(ctx);
        case VALUE_TYPE_INTEGER:
            return llvm::Type::getInt32Ty(ctx);
        case VALUE_TYPE_BIGINT:
            return llvm::Type::getInt32Ty(ctx);
        case VALUE_TYPE_DOUBLE:
            return llvm::Type::getDoubleTy(ctx);
        case VALUE_TYPE_TIMESTAMP:
            return llvm::Type::getInt64Ty(ctx);
        case VALUE_TYPE_BOOLEAN:
            return llvm::Type::getInt8Ty(ctx);
        default:
            throw std::exception();
        }
    }

    PredFunction
    CodegenContext::compilePredicate(const TupleSchema* tupleSchema,
                                     const AbstractExpression* expr) {
        // Create the type for our function:
        // It accepts a tuple, and returns a signed, 8-bit int.
        // (to represent true, false, and unknown)
        llvm::LLVMContext &ctx = *m_llvmContext;

        std::vector<llvm::Type*> argType(1, llvm::Type::getInt8PtrTy(ctx));
        llvm::Type* retType = getLlvmType(VALUE_TYPE_BOOLEAN);
        llvm::FunctionType* ft = llvm::FunctionType::get(retType, argType, false);
        llvm::Function* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "pred", m_module);

        m_tupleArg = fn->arg_begin();
        m_tupleArg->setName("tuple");

        llvm::BasicBlock *bb = llvm::BasicBlock::Create(ctx, "entry", fn);
        m_builder->SetInsertPoint(bb);

        // Here is where code is generated:
        std::pair<llvm::Value*, bool> answerPair = expr->codegen(*this, tupleSchema);

        m_tupleArg = NULL;

        m_builder->CreateRet(answerPair.first);

        m_module->dump();

        // This will throw an exception if we did anything wonky in LLVM IR
        llvm::verifyFunction(*fn);

        // This will optimize the function
        m_passManager->run(*fn);

        m_module->dump();

        return (PredFunction)m_executionEngine->getPointerToFunction(fn);
    }

    static llvm::Value* getNullValueForType(llvm::Type* ty) {
        if (!ty->isIntegerTy()) {
            throw std::exception();
        }

        llvm::IntegerType* intTy = static_cast<llvm::IntegerType*>(ty);
        switch (intTy->getBitWidth()) {
        case 8:
            return llvm::ConstantInt::get(ty, INT8_NULL);
        case 16:
            return llvm::ConstantInt::get(ty, INT16_NULL);
        case 32:
            return llvm::ConstantInt::get(ty, INT32_NULL);
        case 64:
            return llvm::ConstantInt::get(ty, INT64_NULL);
        default:
            throw std::exception();
        }
    }

    // Implement early return if a null value is encountered
    void CodegenContext::emitReturnIfNull(llvm::Value* value, const std::string& labelPrefix) {
        llvm::Function* curFn = builder().GetInsertBlock()->getParent();
        std::ostringstream labelStr;
        labelStr << labelPrefix << "_is_not_null";
        llvm::BasicBlock *isNotNullLabel = llvm::BasicBlock::Create(*m_llvmContext,
                                                                    labelStr.str(),
                                                                    curFn);

        llvm::BasicBlock *isNullLabel = llvm::BasicBlock::Create(*m_llvmContext,
                                                                 "return_null", 
                                                                 curFn);

        llvm::Value* isNull = builder().CreateICmpEQ(value, getNullValueForType(value->getType()));
        builder().CreateCondBr(isNull, isNullLabel, isNotNullLabel);
        
        builder().SetInsertPoint(isNullLabel);
        builder().CreateRet(getNullValueForType(getLlvmType(VALUE_TYPE_BOOLEAN)));

        // Resume code generation in the is-not-null branch
        builder().SetInsertPoint(isNotNullLabel);
    }


    llvm::IntegerType* CodegenContext::getIntPtrType() {
        return m_executionEngine->getDataLayout()->getIntPtrType(*m_llvmContext);
    }
}
