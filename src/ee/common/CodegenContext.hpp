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

#ifndef _CODEGENCONTEXT_HPP_
#define _CODEGENCONTEXT_HPP_

#include "boost/scoped_ptr.hpp"
#include "common/types.h"

#include "llvm/IR/IRBuilder.h"

#include <string>

namespace llvm {
class ExecutionEngine;
    namespace legacy {
class FunctionPassManager;
    }
class LLVMContext;
class Module;
class Value;
class Type;
class IntegerType;
}

namespace voltdb {

    class TupleSchema;
    class AbstractExpression;

    class CodegenContext {
    public:
        CodegenContext();

        PredFunction compilePredicate(const std::string& fnName,
                                      const TupleSchema* tupleSchema,
                                      const AbstractExpression* expr);

        llvm::Module* getModule();
        llvm::LLVMContext& getLlvmContext();

        llvm::Type* getLlvmType(ValueType voltType);

        // returns an llvm integer type that can store an pointer on the jit's target
        llvm::IntegerType* getIntPtrType();

        ~CodegenContext();

        static void shutdownLlvm();

    private:
        boost::scoped_ptr<llvm::LLVMContext> m_llvmContext;
        llvm::Module* m_module;
        boost::scoped_ptr<llvm::ExecutionEngine> m_executionEngine;
        boost::scoped_ptr<llvm::legacy::FunctionPassManager> m_passManager;

        std::string m_errorString;
   };

}

#endif
