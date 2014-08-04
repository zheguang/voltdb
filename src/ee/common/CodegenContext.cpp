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
        , m_executionEngine()
        , m_passManager()
        , m_errorString()
    {
        // This really only needs to be called once for the whole process.
        llvm::InitializeNativeTarget();

        m_llvmContext.reset(new llvm::LLVMContext());

        llvm::Module *module = new llvm::Module("voltdb_generated_code", *m_llvmContext);

        llvm::ExecutionEngine *engine = llvm::EngineBuilder(module).setErrorStr(&m_errorString).create();

        if (! engine) {
            // throwing in a constructor is bad
            // all of this should be in an init method
            // should also release module in this case.
            throw std::exception();
        }

        m_executionEngine.reset(engine);

        m_passManager.reset(new llvm::FunctionPassManager(module));

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
    }

    CodegenContext::~CodegenContext() {
    }
}
