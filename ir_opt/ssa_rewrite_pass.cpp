// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This file implements the SSA rewriting algorithm proposed in
//
//      Simple and Efficient Construction of Static Single Assignment Form.
//      Braun M., Buchwald S., Hack S., Leiba R., Mallon C., Zwinkau A. (2013)
//      In: Jhala R., De Bosschere K. (eds)
//      Compiler Construction. CC 2013.
//      Lecture Notes in Computer Science, vol 7791.
//      Springer, Berlin, Heidelberg
//
//      https://link.springer.com/chapter/10.1007/978-3-642-37051-9_6
//

#include <array>
#include <deque>
#include <map>
#include <span>
#include <unordered_map>
#include <vector>

#include <boost/container/small_vector.hpp>

#include <shader_compiler/frontend/ir/basic_block.h>
#include <shader_compiler/frontend/ir/opcodes.h>
#include <shader_compiler/frontend/ir/pred.h>
#include <shader_compiler/frontend/ir/reg.h>
#include <shader_compiler/frontend/ir/value.h>
#include <shader_compiler/ir_opt/passes.h>

namespace Shader::Optimization {
namespace {
// Match the original std::variant alternative order. The scalar key below
// preserves that order without variant visitation or inherited tag comparisons.
enum class VariableKind : u32 {
    Register,
    Predicate,
    ZeroFlag,
    SignFlag,
    CarryFlag,
    OverflowFlag,
    Goto,
    IndirectBranch,
};

struct Variable {
    VariableKind kind;
    u32 index{};
};

u64 VariableKey(Variable variable) noexcept {
    return (static_cast<u64>(variable.kind) << 32) | variable.index;
}

Variable VariableFromKey(u64 key) noexcept {
    return Variable{static_cast<VariableKind>(key >> 32), static_cast<u32>(key)};
}

using ValueMap = std::unordered_map<IR::Block*, IR::Value>;

struct DefTable {
    const IR::Value& Def(IR::Block* block, Variable variable) {
        switch (variable.kind) {
        case VariableKind::Register:
            return block->SsaRegValue(static_cast<IR::Reg>(variable.index));
        case VariableKind::Predicate:
            return preds[variable.index][block];
        case VariableKind::ZeroFlag:
            return zero_flag[block];
        case VariableKind::SignFlag:
            return sign_flag[block];
        case VariableKind::CarryFlag:
            return carry_flag[block];
        case VariableKind::OverflowFlag:
            return overflow_flag[block];
        case VariableKind::Goto:
            return goto_vars[variable.index][block];
        case VariableKind::IndirectBranch:
            return indirect_branch_var[block];
        }
        throw LogicError("Invalid SSA variable kind");
    }

    void SetDef(IR::Block* block, Variable variable, const IR::Value& value) {
        switch (variable.kind) {
        case VariableKind::Register:
            block->SetSsaRegValue(static_cast<IR::Reg>(variable.index), value);
            break;
        case VariableKind::Predicate:
            preds[variable.index].insert_or_assign(block, value);
            break;
        case VariableKind::ZeroFlag:
            zero_flag.insert_or_assign(block, value);
            break;
        case VariableKind::SignFlag:
            sign_flag.insert_or_assign(block, value);
            break;
        case VariableKind::CarryFlag:
            carry_flag.insert_or_assign(block, value);
            break;
        case VariableKind::OverflowFlag:
            overflow_flag.insert_or_assign(block, value);
            break;
        case VariableKind::Goto:
            goto_vars[variable.index].insert_or_assign(block, value);
            break;
        case VariableKind::IndirectBranch:
            indirect_branch_var.insert_or_assign(block, value);
            break;
        }
    }

    std::array<ValueMap, IR::NUM_USER_PREDS> preds;
    std::unordered_map<u32, ValueMap> goto_vars;
    ValueMap indirect_branch_var;
    ValueMap zero_flag;
    ValueMap sign_flag;
    ValueMap carry_flag;
    ValueMap overflow_flag;
};

IR::Opcode UndefOpcode(Variable variable) noexcept {
    if (variable.kind == VariableKind::Register ||
        variable.kind == VariableKind::IndirectBranch) {
        return IR::Opcode::UndefU32;
    }
    // GotoVariable inherited FlagTag in the original implementation: U1.
    return IR::Opcode::UndefU1;
}

enum class Status {
    Start,
    SetValue,
    PreparePhiArgument,
    PushPhiArgument,
};

struct ReadState {
    ReadState(IR::Block* block_) : block{block_} {}
    ReadState() = default;

    IR::Block* block{};
    IR::Value result{};
    IR::Inst* phi{};
    IR::Block* const* pred_it{};
    IR::Block* const* pred_end{};
    Status pc{Status::Start};
};

using ReadStack = boost::container::small_vector<ReadState, 64>;

class Pass {
public:
    void WriteVariable(Variable variable, IR::Block* block, const IR::Value& value) {
        current_def.SetDef(block, variable, value);
    }

    IR::Value ReadVariable(Variable variable, IR::Block* root_block) {
        ReadStack stack{
            ReadState(nullptr),
            ReadState(root_block),
        };
        do {
            IR::Block* const block{stack.back().block};
            switch (stack.back().pc) {
            case Status::Start: {
                if (const IR::Value& def = current_def.Def(block, variable); !def.IsEmpty()) {
                    stack.back().result = def;
                } else if (!block->IsSsaSealed()) {
                    // Incomplete CFG
                    IR::Inst* phi{&*block->PrependNewInst(block->begin(), IR::Opcode::Phi)};
                    phi->SetFlags(IR::TypeOf(UndefOpcode(variable)));

                    incomplete_phis[block].insert_or_assign(VariableKey(variable), phi);
                    stack.back().result = IR::Value{&*phi};
                } else if (const std::span imm_preds = block->ImmPredecessors();
                           imm_preds.size() == 1) {
                    // Optimize the common case of one predecessor: no phi needed
                    stack.back().pc = Status::SetValue;
                    stack.emplace_back(imm_preds.front());
                    break;
                } else {
                    // Break potential cycles with operandless phi
                    IR::Inst* const phi{&*block->PrependNewInst(block->begin(), IR::Opcode::Phi)};
                    phi->SetFlags(IR::TypeOf(UndefOpcode(variable)));

                    WriteVariable(variable, block, IR::Value{phi});

                    stack.back().phi = phi;
                    stack.back().pred_it = imm_preds.data();
                    stack.back().pred_end = imm_preds.data() + imm_preds.size();
                    PreparePhiOperand(variable, stack);
                    break;
                }
            }
                [[fallthrough]];
            case Status::SetValue: {
                const IR::Value result{stack.back().result};
                WriteVariable(variable, block, result);
                stack.pop_back();
                stack.back().result = result;
                break;
            }
            case Status::PushPhiArgument: {
                IR::Inst* const phi{stack.back().phi};
                phi->AddPhiOperand(*stack.back().pred_it, stack.back().result);
                ++stack.back().pred_it;
            }
                [[fallthrough]];
            case Status::PreparePhiArgument:
                PreparePhiOperand(variable, stack);
                break;
            }
        } while (stack.size() > 1);
        return stack.back().result;
    }

    void SealBlock(IR::Block* block) {
        const auto it{incomplete_phis.find(block)};
        if (it != incomplete_phis.end()) {
            for (auto& pair : it->second) {
                AddPhiOperands(VariableFromKey(pair.first), *pair.second, block);
            }
        }
        block->SsaSeal();
    }

private:
    void PreparePhiOperand(Variable variable, ReadStack& stack) {
        if (stack.back().pred_it == stack.back().pred_end) {
            IR::Inst* const phi{stack.back().phi};
            IR::Block* const block{stack.back().block};
            const IR::Value result{TryRemoveTrivialPhi(*phi, block, UndefOpcode(variable))};
            stack.pop_back();
            stack.back().result = result;
            WriteVariable(variable, block, result);
        } else {
            IR::Block* const imm_pred{*stack.back().pred_it};
            stack.back().pc = Status::PushPhiArgument;
            stack.emplace_back(imm_pred);
        }
    }

    IR::Value AddPhiOperands(Variable variable, IR::Inst& phi, IR::Block* block) {
        for (IR::Block* const imm_pred : block->ImmPredecessors()) {
            phi.AddPhiOperand(imm_pred, ReadVariable(variable, imm_pred));
        }
        return TryRemoveTrivialPhi(phi, block, UndefOpcode(variable));
    }

    IR::Value TryRemoveTrivialPhi(IR::Inst& phi, IR::Block* block, IR::Opcode undef_opcode) {
        IR::Value same;
        const size_t num_args{phi.NumArgs()};
        for (size_t arg_index = 0; arg_index < num_args; ++arg_index) {
            const IR::Value& op{phi.Arg(arg_index)};
            if (op.Resolve() == same.Resolve() || op == IR::Value{&phi}) {
                // Unique value or self-reference
                continue;
            }
            if (!same.IsEmpty()) {
                // The phi merges at least two values: not trivial
                return IR::Value{&phi};
            }
            same = op;
        }
        // Remove the phi node from the block, it will be reinserted
        IR::Block::InstructionList& list{block->Instructions()};
        list.erase(IR::Block::InstructionList::s_iterator_to(phi));

        // Find the first non-phi instruction and use it as an insertion point
        IR::Block::iterator reinsert_point{list.begin()};
        while (reinsert_point != list.end() && IR::IsPhi(*reinsert_point)) {
            ++reinsert_point;
        }
        if (same.IsEmpty()) {
            // The phi is unreachable or in the start block
            // Insert an undefined instruction and make it the phi node replacement
            // The "phi" node reinsertion point is specified after this instruction
            reinsert_point = block->PrependNewInst(reinsert_point, undef_opcode);
            same = IR::Value{&*reinsert_point};
            ++reinsert_point;
        }
        // Reinsert the phi node and reroute all its uses to the "same" value
        list.insert(reinsert_point, phi);
        phi.ReplaceUsesWith(same);
        // TODO: Try to recursively remove all phi users, which might have become trivial
        return same;
    }

    std::unordered_map<IR::Block*, std::map<u64, IR::Inst*>> incomplete_phis;
    DefTable current_def;
};

void VisitInst(Pass& pass, IR::Block* block, IR::Inst& inst) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::SetRegister:
        if (const IR::Reg reg{inst.Arg(0).Reg()}; reg != IR::Reg::RZ) {
            pass.WriteVariable(Variable{VariableKind::Register, static_cast<u32>(reg)}, block,
                               inst.Arg(1));
        }
        break;
    case IR::Opcode::SetPred:
        if (const IR::Pred pred{inst.Arg(0).Pred()}; pred != IR::Pred::PT) {
            pass.WriteVariable(Variable{VariableKind::Predicate, static_cast<u32>(pred)}, block,
                               inst.Arg(1));
        }
        break;
    case IR::Opcode::SetGotoVariable:
        pass.WriteVariable(Variable{VariableKind::Goto, inst.Arg(0).U32()}, block, inst.Arg(1));
        break;
    case IR::Opcode::SetIndirectBranchVariable:
        pass.WriteVariable(Variable{VariableKind::IndirectBranch}, block, inst.Arg(0));
        break;
    case IR::Opcode::SetZFlag:
        pass.WriteVariable(Variable{VariableKind::ZeroFlag}, block, inst.Arg(0));
        break;
    case IR::Opcode::SetSFlag:
        pass.WriteVariable(Variable{VariableKind::SignFlag}, block, inst.Arg(0));
        break;
    case IR::Opcode::SetCFlag:
        pass.WriteVariable(Variable{VariableKind::CarryFlag}, block, inst.Arg(0));
        break;
    case IR::Opcode::SetOFlag:
        pass.WriteVariable(Variable{VariableKind::OverflowFlag}, block, inst.Arg(0));
        break;
    case IR::Opcode::GetRegister:
        if (const IR::Reg reg{inst.Arg(0).Reg()}; reg != IR::Reg::RZ) {
            inst.ReplaceUsesWith(
                pass.ReadVariable(Variable{VariableKind::Register, static_cast<u32>(reg)}, block));
        }
        break;
    case IR::Opcode::GetPred:
        if (const IR::Pred pred{inst.Arg(0).Pred()}; pred != IR::Pred::PT) {
            inst.ReplaceUsesWith(
                pass.ReadVariable(Variable{VariableKind::Predicate, static_cast<u32>(pred)}, block));
        }
        break;
    case IR::Opcode::GetGotoVariable:
        inst.ReplaceUsesWith(pass.ReadVariable(Variable{VariableKind::Goto, inst.Arg(0).U32()}, block));
        break;
    case IR::Opcode::GetIndirectBranchVariable:
        inst.ReplaceUsesWith(pass.ReadVariable(Variable{VariableKind::IndirectBranch}, block));
        break;
    case IR::Opcode::GetZFlag:
        inst.ReplaceUsesWith(pass.ReadVariable(Variable{VariableKind::ZeroFlag}, block));
        break;
    case IR::Opcode::GetSFlag:
        inst.ReplaceUsesWith(pass.ReadVariable(Variable{VariableKind::SignFlag}, block));
        break;
    case IR::Opcode::GetCFlag:
        inst.ReplaceUsesWith(pass.ReadVariable(Variable{VariableKind::CarryFlag}, block));
        break;
    case IR::Opcode::GetOFlag:
        inst.ReplaceUsesWith(pass.ReadVariable(Variable{VariableKind::OverflowFlag}, block));
        break;
    default:
        break;
    }
}

void VisitBlock(Pass& pass, IR::Block* block) {
    for (IR::Inst& inst : block->Instructions()) {
        VisitInst(pass, block, inst);
    }
    pass.SealBlock(block);
}

IR::Type GetConcreteType(IR::Inst* inst) {
    std::deque<IR::Inst*> queue;
    queue.push_back(inst);
    while (!queue.empty()) {
        IR::Inst* current = queue.front();
        queue.pop_front();
        const size_t num_args{current->NumArgs()};
        for (size_t i = 0; i < num_args; ++i) {
            const auto set_type = current->Arg(i).Type();
            if (set_type != IR::Type::Opaque) {
                return set_type;
            }
            if (!current->Arg(i).IsImmediate()) {
                queue.push_back(current->Arg(i).Inst());
            }
        }
    }
    return IR::Type::Opaque;
}
} // Anonymous namespace

void SsaRewritePass(IR::Program& program) {
    Pass pass;
    const auto end{program.post_order_blocks.rend()};
    for (auto block = program.post_order_blocks.rbegin(); block != end; ++block) {
        VisitBlock(pass, *block);
    }
    for (auto block = program.post_order_blocks.rbegin(); block != end; ++block) {
        for (IR::Inst& inst : (*block)->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                if (inst.Type() == IR::Type::Opaque) {
                    inst.SetFlags(GetConcreteType(&inst));
                }
                inst.OrderPhiArgs();
            }
        }
    }
}

} // namespace Shader::Optimization
