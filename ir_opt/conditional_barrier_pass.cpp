// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <shader_compiler/common/assert.h>
#include <shader_compiler/common/log.h>
#include <shader_compiler/frontend/ir/program.h>
#include <shader_compiler/ir_opt/passes.h>

namespace Shader::Optimization {

void ConditionalBarrierPass(IR::Program& program) {
    s32 control_flow_depth{};
    s32 conditional_exit_count{};

    for (IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::If:
        case IR::AbstractSyntaxNode::Type::Loop:
            ++control_flow_depth;
            break;
        case IR::AbstractSyntaxNode::Type::EndIf:
        case IR::AbstractSyntaxNode::Type::Repeat:
            --control_flow_depth;
            break;
        case IR::AbstractSyntaxNode::Type::Unreachable:
        case IR::AbstractSyntaxNode::Type::Return:
            if (control_flow_depth > 0) {
                ++conditional_exit_count;
            }
            break;
        case IR::AbstractSyntaxNode::Type::Block:
            if (control_flow_depth <= 0 && conditional_exit_count <= 0) {
                break;
            }
            for (IR::Inst& inst : node.data.block->Instructions()) {
                if (inst.GetOpcode() == IR::Opcode::Barrier) {
                    LOG_WARNING(Shader, "Removing barrier from conditional control flow");
                    inst.ReplaceOpcode(IR::Opcode::Identity);
                }
            }
            break;
        default:
            break;
        }
    }

    DEBUG_ASSERT(control_flow_depth == 0);
}

} // namespace Shader::Optimization
