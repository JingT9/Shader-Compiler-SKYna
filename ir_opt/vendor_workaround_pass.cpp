// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <shader_compiler/frontend/ir/basic_block.h>
#include <shader_compiler/frontend/ir/ir_emitter.h>
#include <shader_compiler/frontend/ir/value.h>
#include <shader_compiler/ir_opt/passes.h>

namespace Shader::Optimization {
namespace {

bool IsImmediate(const IR::Value& value, u32 expected) {
    return value == IR::Value{expected};
}

void RewriteByteSwapAdd(IR::Block& block, IR::Inst& add) {
    IR::Inst* const shifted_left{add.Arg(0).TryInstRecursive()};
    IR::Inst* const high_extract{add.Arg(1).TryInstRecursive()};
    if (!shifted_left || !high_extract) {
        return;
    }

    if (shifted_left->GetOpcode() != IR::Opcode::ShiftLeftLogical32 ||
        !IsImmediate(shifted_left->Arg(1), 16U)) {
        return;
    }
    if (high_extract->GetOpcode() != IR::Opcode::BitFieldUExtract ||
        !IsImmediate(high_extract->Arg(1), 16U) || !IsImmediate(high_extract->Arg(2), 16U)) {
        return;
    }

    IR::Inst* const multiply_or_extract{shifted_left->Arg(0).TryInstRecursive()};
    if (!multiply_or_extract) {
        return;
    }

    const bool multiply_is_omitted{
        multiply_or_extract->GetOpcode() == IR::Opcode::BitFieldUExtract};
    if (!multiply_is_omitted && multiply_or_extract->GetOpcode() != IR::Opcode::IMul32) {
        return;
    }

    IR::Inst* const low_extract{
        multiply_is_omitted ? multiply_or_extract : multiply_or_extract->Arg(0).TryInstRecursive()};
    if (!low_extract || low_extract->GetOpcode() != IR::Opcode::BitFieldUExtract ||
        !IsImmediate(low_extract->Arg(1), 0U) || !IsImmediate(low_extract->Arg(2), 16U)) {
        return;
    }

    IR::IREmitter ir{block, IR::Block::InstructionList::s_iterator_to(add)};
    add.ReplaceUsesWith(ir.BitwiseOr(IR::U32{add.Arg(0)}, IR::U32{add.Arg(1)}));
}

} // Anonymous namespace

void VendorWorkaroundPass(IR::Program& program) {
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::IAdd32) {
                RewriteByteSwapAdd(*block, inst);
            }
        }
    }
}

} // namespace Shader::Optimization
