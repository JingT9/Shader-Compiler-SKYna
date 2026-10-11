// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <shader_compiler/environment.h>
#include <shader_compiler/frontend/ir/basic_block.h>
#include <shader_compiler/frontend/maxwell/decode.h>
#include <shader_compiler/frontend/maxwell/location.h>
#include <shader_compiler/frontend/maxwell/translate/impl/impl.h>
#include <shader_compiler/frontend/maxwell/translate/translate.h>

namespace Shader::Maxwell {

void Translate(Environment& env, IR::Block* block, u32 location_begin, u32 location_end) {
    if (location_begin == location_end) {
        return;
    }
    TranslatorVisitor visitor{env, *block};
    for (Location pc = location_begin; pc != location_end; ++pc) {
        const u64 insn{env.ReadInstruction(pc.Offset())};
        try {
            const Opcode opcode{Decode(insn)};
            switch (opcode) {
#include <shader_compiler/frontend/maxwell/translate/direct_dispatch.inc>
            default:
                throw LogicError("Invalid opcode {}", opcode);
            }
        } catch (Exception& exception) {
            exception.Prepend(fmt::format("Translate {}: ", Decode(insn)));
            throw;
        }
    }
}

} // namespace Shader::Maxwell
