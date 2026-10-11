// SPDX-FileCopyrightText: Copyright 2026 SKYmu contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <shader_compiler/backend/spirv/spirv_emit_context.h>
#include <shader_compiler/exception.h>

// Native direct calls preserve the real Sirit::Module subobject. No member
// pointer ABI, callback lifetime, or Rust layout reinterpretation is assumed.
namespace Shader::Backend::SPIRV::Direct {

enum class AtomicOperation { IAdd, SMin, UMin, SMax, UMax, And, Or, Xor, Exchange };

inline Id ApplyAtomic(EmitContext& ctx, AtomicOperation operation, Id type, Id pointer, Id scope, Id semantics, Id value) {
    switch (operation) {
    case AtomicOperation::IAdd:
        return ctx.OpAtomicIAdd(type, pointer, scope, semantics, value);
    case AtomicOperation::SMin:
        return ctx.OpAtomicSMin(type, pointer, scope, semantics, value);
    case AtomicOperation::UMin:
        return ctx.OpAtomicUMin(type, pointer, scope, semantics, value);
    case AtomicOperation::SMax:
        return ctx.OpAtomicSMax(type, pointer, scope, semantics, value);
    case AtomicOperation::UMax:
        return ctx.OpAtomicUMax(type, pointer, scope, semantics, value);
    case AtomicOperation::And:
        return ctx.OpAtomicAnd(type, pointer, scope, semantics, value);
    case AtomicOperation::Or:
        return ctx.OpAtomicOr(type, pointer, scope, semantics, value);
    case AtomicOperation::Xor:
        return ctx.OpAtomicXor(type, pointer, scope, semantics, value);
    case AtomicOperation::Exchange:
        return ctx.OpAtomicExchange(type, pointer, scope, semantics, value);
    }
    throw LogicError("Invalid SPIR-V AtomicOperation");
}

enum class BinaryOperation { IAdd, SMin, UMin, SMax, UMax, BitwiseAnd, BitwiseOr, BitwiseXor };

inline Id ApplyBinary(EmitContext& ctx, BinaryOperation operation, Id type, Id lhs, Id rhs) {
    switch (operation) {
    case BinaryOperation::IAdd:
        return ctx.OpIAdd(type, lhs, rhs);
    case BinaryOperation::SMin:
        return ctx.OpSMin(type, lhs, rhs);
    case BinaryOperation::UMin:
        return ctx.OpUMin(type, lhs, rhs);
    case BinaryOperation::SMax:
        return ctx.OpSMax(type, lhs, rhs);
    case BinaryOperation::UMax:
        return ctx.OpUMax(type, lhs, rhs);
    case BinaryOperation::BitwiseAnd:
        return ctx.OpBitwiseAnd(type, lhs, rhs);
    case BinaryOperation::BitwiseOr:
        return ctx.OpBitwiseOr(type, lhs, rhs);
    case BinaryOperation::BitwiseXor:
        return ctx.OpBitwiseXor(type, lhs, rhs);
    }
    throw LogicError("Invalid SPIR-V BinaryOperation");
}

enum class FloatCompareOperation { Equal, LessThan, GreaterThan, LessThanEqual, GreaterThanEqual };

inline Id ApplyFloatCompare(EmitContext& ctx, FloatCompareOperation operation, Id type, Id lhs, Id rhs) {
    switch (operation) {
    case FloatCompareOperation::Equal:
        return ctx.OpFUnordEqual(type, lhs, rhs);
    case FloatCompareOperation::LessThan:
        return ctx.OpFUnordLessThan(type, lhs, rhs);
    case FloatCompareOperation::GreaterThan:
        return ctx.OpFUnordGreaterThan(type, lhs, rhs);
    case FloatCompareOperation::LessThanEqual:
        return ctx.OpFUnordLessThanEqual(type, lhs, rhs);
    case FloatCompareOperation::GreaterThanEqual:
        return ctx.OpFUnordGreaterThanEqual(type, lhs, rhs);
    }
    throw LogicError("Invalid SPIR-V FloatCompareOperation");
}

enum class StorageElement { U32, U64, U32x2 };

inline Id GetStorageBuffer(const StorageDefinitions& definitions, StorageElement element) {
    switch (element) {
    case StorageElement::U32:
        return definitions.U32;
    case StorageElement::U64:
        return definitions.U64;
    case StorageElement::U32x2:
        return definitions.U32x2;
    }
    throw LogicError("Invalid SPIR-V storage element");
}

} // namespace Shader::Backend::SPIRV::Direct
