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
#include "common/SQLException.h"
#include "common/TupleSchema.h"
#include "common/types.h"
#include "common/value_defs.h"
#include "expressions/abstractexpression.h"
#include "expressions/comparisonexpression.h"

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

    namespace {
        // This is thrown if we encounter something we can't yet generate code for.
        // In this case, we can always fall back to interpreting the expression.
        class UnsupportedForCodegenException {
        public:
            UnsupportedForCodegenException(const std::string& message)
                : m_message(message)
            {}
            
            std::string getMessage() const {
                std::ostringstream oss;
                oss << "Unsupported for codegen: " << m_message;
                return oss.str();
            }

        private:
            const std::string m_message;
        };
    }

    static llvm::Value* getNullValueForType(llvm::Type* ty) {
        if (!ty->isIntegerTy()) {
            throw UnsupportedForCodegenException("attempt to get null value of non-integer type");
        }

        // These could also be named global constant variables
        // Easier to read!

        llvm::IntegerType* intTy = static_cast<llvm::IntegerType*>(ty);
        unsigned bitWidth = intTy->getBitWidth();
        switch (bitWidth) {
        case 8:
            return llvm::ConstantInt::get(intTy, INT8_NULL);
        case 16:
            return llvm::ConstantInt::get(intTy, INT16_NULL);
        case 32:
            return llvm::ConstantInt::get(intTy, INT32_NULL);
        default:
            assert (bitWidth == 64);
            return llvm::ConstantInt::get(intTy, INT64_NULL);
        }
    }

    namespace {

        // maintains the current state of the LLVM function being generated
        class FnCtx {
        public:
            FnCtx(CodegenContext* codegenContext,
                  llvm::Function* function)
                : m_codegenContext(codegenContext)
                , m_function(function)
                , m_builder(new llvm::IRBuilder<>(&(function->getEntryBlock())))
                , m_returnNullBasicBlock(NULL)
            {
            }

            FnCtx(CodegenContext* codegenContext)
            : m_codegenContext(codegenContext)
            , m_function(NULL)
            , m_builder()
            , m_returnNullBasicBlock(NULL)
            {
                init();
            }

            void codegen(const TupleSchema* tupleSchema, 
                         const AbstractExpression* expr) {
                std::pair<llvm::Value*, bool> answerPair = codegenExpr(tupleSchema, expr);
                builder().CreateRet(answerPair.first);
            }

            llvm::Function* getFunction() {
                return m_function;
            }

        private:
            
            void init() {
                llvm::LLVMContext &ctx = getLlvmContext();

                std::vector<llvm::Type*> argType(1, llvm::Type::getInt8PtrTy(ctx));
                llvm::Type* retType = getLlvmType(VALUE_TYPE_BOOLEAN);
                llvm::FunctionType* ft = llvm::FunctionType::get(retType, argType, false);
                m_function = llvm::Function::Create(ft, 
                                                    llvm::Function::ExternalLinkage, 
                                                    "pred", 
                                                    m_codegenContext->getModule());
                
                m_function->arg_begin()->setName("tuple");

                llvm::BasicBlock *bb = llvm::BasicBlock::Create(ctx, "entry", m_function);
                m_builder.reset(new llvm::IRBuilder<>(bb));
            }


            llvm::IRBuilder<>& builder() {
                return *m_builder;
            }

            llvm::Type* getIntPtrType() {
                return m_codegenContext->getIntPtrType();
            }

            llvm::Type* getLlvmType(ValueType voltType) {
                return m_codegenContext->getLlvmType(voltType);
            }

            llvm::Value* getTupleArg() {
                return m_function->arg_begin();
            }

            void generateReturnIfNull(llvm::Value* value, const std::string& labelPrefix) {
                std::ostringstream labelStr;
                labelStr << labelPrefix << "_is_not_null";
                llvm::BasicBlock *isNotNullLabel = llvm::BasicBlock::Create(getLlvmContext(),
                                                                            labelStr.str(),
                                                                            m_function);
                
                llvm::Value* isNull = builder().CreateICmpEQ(value, getNullValueForType(value->getType()));
                builder().CreateCondBr(isNull, getReturnNullBasicBlock(), isNotNullLabel);
                
                // Resume code generation in the is-not-null branch
                builder().SetInsertPoint(isNotNullLabel);
            }

            llvm::LLVMContext& getLlvmContext() {
                return m_codegenContext->getLlvmContext();
            }

            llvm::BasicBlock* getReturnNullBasicBlock() {
                if (m_returnNullBasicBlock == NULL) {
                    llvm::BasicBlock* orig = builder().GetInsertBlock();
                    m_returnNullBasicBlock = llvm::BasicBlock::Create(getLlvmContext(),
                                                                      "return_null", 
                                                                      m_function);
                    builder().SetInsertPoint(m_returnNullBasicBlock);
                    builder().CreateRet(getNullValueForType(m_codegenContext->getLlvmType(VALUE_TYPE_BOOLEAN)));
                    builder().SetInsertPoint(orig);
                }
                return m_returnNullBasicBlock;
            }

            std::pair<llvm::Value*, bool> 
            codegenParameterValueExpr(const TupleSchema*,
                                      const ParameterValueExpression* expr) {
                llvm::Constant* nvalueAddrAsInt = llvm::ConstantInt::get(getIntPtrType(),
                                                                         (uintptr_t)expr->getParamValue());

                // cast the pointer to the nvalue as a pointer to the value.
                // Since the first member of NValue is the 16-byte m_data
                // array, this is okay for all the numeric types.  But if
                // NValue ever changes, this code will break.
                llvm::PointerType* ptrTy = llvm::PointerType::getUnqual(getLlvmType(expr->getValueType()));
                llvm::Value* castedAddr = builder().CreateIntToPtr(nvalueAddrAsInt, ptrTy);

                std::ostringstream varName;
                varName << "param_" << expr->getValueIdx();
                return std::make_pair(builder().CreateLoad(castedAddr, varName.str().c_str()),
                                      true); // true means value may be null

            }

            std::pair<llvm::Value*, bool> 
            codegenTupleValueExpr(const TupleSchema* schema,
                                  const TupleValueExpression* expr) {
                // find the offset of the field in the record
                const TupleSchema::ColumnInfo *columnInfo = schema->getColumnInfo(expr->getColumnId());
                uint32_t intOffset = TUPLE_HEADER_SIZE + columnInfo->offset;
                llvm::Value* offset = llvm::ConstantInt::get(getLlvmType(VALUE_TYPE_INTEGER), intOffset);

                // emit instruction that computes the address of the value
                // GEP is getelementptr, which amounts here to a pointer add.
                llvm::Value* addr = builder().CreateGEP(getTupleArg(),
                                                        offset);

                // Cast addr from char* to the appropriate pointer type
                // An LLVM IR instruction is created but it will be a no-op on target
                llvm::Type* ptrTy = llvm::PointerType::getUnqual(getLlvmType(expr->getValueType()));
                llvm::Value* castedAddr = builder().CreateBitCast(addr,
                                                                  ptrTy);
                std::ostringstream varName;
                varName << "field_" << expr->getColumnId();
                return std::make_pair(builder().CreateLoad(castedAddr, varName.str().c_str()),
                                      columnInfo->allowNull);
            }
            
            llvm::Value* 
            codegenCmpOp(ExpressionType exprType, 
                         llvm::Value* lhs, 
                         llvm::Value* rhs) {
                // For floating point types, we would CreateFCmp* here instead...
        
                llvm::Value* cmp = NULL;
                switch (exprType) {
                case EXPRESSION_TYPE_COMPARE_EQUAL:
                    cmp = builder().CreateICmpEQ(lhs, rhs);
                    break;
                case EXPRESSION_TYPE_COMPARE_NOTEQUAL:
                    cmp = builder().CreateICmpNE(lhs, rhs);
                    break;
                case EXPRESSION_TYPE_COMPARE_LESSTHAN:
                    cmp = builder().CreateICmpSLT(lhs, rhs);
                    break;
                case EXPRESSION_TYPE_COMPARE_GREATERTHAN:
                    cmp = builder().CreateICmpSGT(lhs, rhs);
                    break;
                case EXPRESSION_TYPE_COMPARE_LESSTHANOREQUALTO:
                    cmp = builder().CreateICmpSLE(lhs, rhs);
                    break;
                case EXPRESSION_TYPE_COMPARE_GREATERTHANOREQUALTO:
                    cmp = builder().CreateICmpSGE(lhs, rhs);
                    break;
                default:
                    throw UnsupportedForCodegenException(expressionToString(exprType));
                }
         
                // LLVM icmp and fcmp instructions produce a 1-bit integer
                // Zero-extend to 8 bits.
                return builder().CreateZExt(cmp, getLlvmType(VALUE_TYPE_BOOLEAN));
            }
    
            std::pair<llvm::Value*, bool> 
            codegenComparisonExpr(const TupleSchema* tupleSchema,
                                  const AbstractExpression* expr) {
                std::pair<llvm::Value*, bool> leftPair = codegenExpr(tupleSchema,
                                                                     expr->getLeft());
                if (leftPair.second) { // value produced on LHS may be null
                    generateReturnIfNull(leftPair.first, "cmp_lhs");
                }
            
                std::pair<llvm::Value*, bool> rightPair = codegenExpr(tupleSchema,
                                                                      expr->getRight());
                if (rightPair.second) { // value produced on RHS may be null
                    generateReturnIfNull(rightPair.first, "cmp_rhs");
                }
            
                llvm::Value* cmp = codegenCmpOp(expr->getExpressionType(), leftPair.first, rightPair.first);
            
                return std::make_pair(cmp,
                                      false); // output of cmp will never be null, because of early return.
            }

            std::pair<llvm::Value*, bool>
            codegenExpr(const TupleSchema* tupleSchema,
                        const AbstractExpression* expr) {
                ExpressionType exprType = expr->getExpressionType();
                switch (expr->getExpressionType()) {
                case EXPRESSION_TYPE_COMPARE_EQUAL:
                case EXPRESSION_TYPE_COMPARE_NOTEQUAL:
                case EXPRESSION_TYPE_COMPARE_LESSTHAN:
                case EXPRESSION_TYPE_COMPARE_GREATERTHAN:
                case EXPRESSION_TYPE_COMPARE_LESSTHANOREQUALTO:
                case EXPRESSION_TYPE_COMPARE_GREATERTHANOREQUALTO:
                case EXPRESSION_TYPE_COMPARE_LIKE:
                case EXPRESSION_TYPE_COMPARE_IN:
                    return codegenComparisonExpr(tupleSchema, expr);
                case EXPRESSION_TYPE_VALUE_TUPLE:
                    return codegenTupleValueExpr(tupleSchema, static_cast<const TupleValueExpression*>(expr));
                case EXPRESSION_TYPE_VALUE_PARAMETER:
                    return codegenParameterValueExpr(tupleSchema, static_cast<const ParameterValueExpression*>(expr));
                default:
                    throw UnsupportedForCodegenException(expressionToString(exprType));
                }
            }



            CodegenContext* m_codegenContext;
            llvm::Function* m_function;
            boost::scoped_ptr<llvm::IRBuilder<> > m_builder;
            llvm::BasicBlock* m_returnNullBasicBlock;
        };
    }

    CodegenContext::CodegenContext()
        : m_llvmContext()
        , m_module(NULL)
        , m_executionEngine()
        , m_passManager()
        , m_errorString()
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
    }

    CodegenContext::~CodegenContext() {
    }

    llvm::LLVMContext&
    CodegenContext::getLlvmContext() {
        return *m_llvmContext;
    }

    llvm::Module*
    CodegenContext::getModule() {
        return m_module;
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
        default: {
            std::ostringstream oss;
            oss << "expression with type " << valueToString(voltType);
            throw UnsupportedForCodegenException(oss.str());
        }
        }
    }

    llvm::IntegerType* CodegenContext::getIntPtrType() {
        return m_executionEngine->getDataLayout()->getIntPtrType(*m_llvmContext);
    }

    PredFunction
    CodegenContext::compilePredicate(const TupleSchema* tupleSchema,
                                     const AbstractExpression* expr) {
        FnCtx fnCtx(this);
        fnCtx.codegen(tupleSchema, expr);

        // Dump the unoptimized fn
        m_module->dump();

        llvm::Function* fn = fnCtx.getFunction();

        // This will throw an exception if we did anything wonky in LLVM IR
        llvm::verifyFunction(*fn);

        // This will optimize the function
        m_passManager->run(*fn);

        // Dump optimized LLVM IR
        m_module->dump();

        return (PredFunction)m_executionEngine->getPointerToFunction(fn);
    }

}
