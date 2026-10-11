// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <shader_compiler/backend/spirv/emit_spirv_instructions.h>
#include <shader_compiler/backend/spirv/spirv_emit_context.h>
#include <shader_compiler/backend/spirv/spirv_direct_ops.h>
#include <shader_compiler/frontend/ir/modifiers.h>

namespace Shader::Backend::SPIRV {
namespace {
Id Image(EmitContext& ctx, IR::TextureInstInfo info) {
    if (info.type == TextureType::Buffer) {
        const ImageBufferDefinition def{ctx.image_buffers.at(info.descriptor_index)};
        return def.id;
    } else {
        const ImageDefinition def{ctx.images.at(info.descriptor_index)};
        return def.id;
    }
}

std::pair<Id, Id> AtomicArgs(EmitContext& ctx) {
    const Id scope{ctx.Const(static_cast<u32>(spv::Scope::Device))};
    const Id semantics{ctx.u32_zero_value};
    return {scope, semantics};
}

Id ImageAtomicU32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords, Id value,
                  Direct::AtomicOperation atomic_op) {
    if (!index.IsImmediate() || index.U32() != 0) {
        // TODO: handle layers
        throw NotImplementedException("Image indexing");
    }
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const Id image{Image(ctx, info)};
    const Id pointer{ctx.OpImageTexelPointer(ctx.image_u32, image, coords, ctx.Const(0U))};
    const auto [scope, semantics]{AtomicArgs(ctx)};
    return Direct::ApplyAtomic(ctx, atomic_op, ctx.U32[1], pointer, scope, semantics, value);
}
} // Anonymous namespace

Id EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::IAdd);
}

Id EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::SMin);
}

Id EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::UMin);
}

Id EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::SMax);
}

Id EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::UMax);
}

Id EmitImageAtomicInc32(EmitContext&, IR::Inst*, const IR::Value&, Id, Id) {
    // TODO: This is not yet implemented
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitImageAtomicDec32(EmitContext&, IR::Inst*, const IR::Value&, Id, Id) {
    // TODO: This is not yet implemented
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                        Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::And);
}

Id EmitImageAtomicOr32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                       Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::Or);
}

Id EmitImageAtomicXor32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                        Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::Xor);
}

Id EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                             Id value) {
    return ImageAtomicU32(ctx, inst, index, coords, value, Direct::AtomicOperation::Exchange);
}

Id EmitBindlessImageAtomicIAdd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicSMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicUMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicSMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicUMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicInc32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicDec32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicAnd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicOr32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicXor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBindlessImageAtomicExchange32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicIAdd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicSMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicUMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicSMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicUMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicInc32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicDec32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicAnd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicOr32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicXor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBoundImageAtomicExchange32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
