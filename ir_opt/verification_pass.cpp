// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <set>

#include <shader_compiler/exception.h>
#include <shader_compiler/frontend/ir/basic_block.h>
#include <shader_compiler/frontend/ir/value.h>
#include <shader_compiler/ir_opt/passes.h>

namespace Shader::Optimization {

static void ValidateTypes(const IR::Program& program) {
    for (const auto& block : program.blocks) {
        for (const IR::Inst& inst : *block) {
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                // Skip validation on phi nodes
                continue;
            }
            const size_t num_args{inst.NumArgs()};
            for (size_t i = 0; i < num_args; ++i) {
                const IR::Value type_arg{inst.Arg(i)};
                const IR::Type t1{type_arg.Type()};
                const IR::Type t2{IR::ArgTypeOf(inst.GetOpcode(), i)};
                if (!IR::AreTypesCompatible(t1, t2)) {
                    const auto diagnostic{IR::DumpBlock(*block)};
                    throw LogicError("Invalid types in block:\n{}", diagnostic);
                }
            }
        }
    }
}

static void ValidateUses(const IR::Program& program) {
    std::map<IR::Inst*, int> actual_uses;
    for (const auto& block : program.blocks) {
        for (const IR::Inst& inst : *block) {
            const size_t num_args{inst.NumArgs()};
            for (size_t i = 0; i < num_args; ++i) {
                const IR::Value arg{inst.Arg(i)};
                if (!arg.IsImmediate()) {
                    ++actual_uses[arg.Inst()];
                }
            }
        }
    }
    for (const auto& entry : actual_uses) {
        IR::Inst* const inst{entry.first};
        const int uses{entry.second};
        if (inst->UseCount() != uses) {
            const auto diagnostic{IR::DumpProgram(program)};
            throw LogicError("Invalid uses in block: {}", diagnostic);
        }
    }
}

static void ValidateForwardDeclarations(const IR::Program& program) {
    std::set<const IR::Inst*> definitions;
    for (const IR::Block* const block : program.blocks) {
        for (const IR::Inst& inst : *block) {
            definitions.emplace(&inst);
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                // Phi nodes can have forward declarations
                continue;
            }
            const size_t num_args{inst.NumArgs()};
            for (size_t arg = 0; arg < num_args; ++arg) {
                const IR::Value forward_arg{inst.Arg(arg)};
                if (forward_arg.IsImmediate()) {
                    continue;
                }
                if (!definitions.contains(forward_arg.Inst())) {
                    const auto diagnostic{IR::DumpBlock(*block)};
                    throw LogicError("Forward declaration in block: {}", diagnostic);
                }
            }
        }
    }
}

static void ValidatePhiNodes(const IR::Program& program) {
    for (const IR::Block* const block : program.blocks) {
        bool no_more_phis{false};
        for (const IR::Inst& inst : *block) {
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                if (no_more_phis) {
                    const auto diagnostic{IR::DumpBlock(*block)};
                    throw LogicError("Interleaved phi nodes: {}", diagnostic);
                }
            } else {
                no_more_phis = true;
            }
        }
    }
}

void VerificationPass(const IR::Program& program) {
    ValidateTypes(program);
    ValidateUses(program);
    ValidateForwardDeclarations(program);
    ValidatePhiNodes(program);
}

} // namespace Shader::Optimization
