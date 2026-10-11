// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <shader_compiler/common/bit_field.h>
#include <shader_compiler/common/common_types.h>
#include <shader_compiler/frontend/ir/modifiers.h>
#include <shader_compiler/frontend/maxwell/translate/impl/impl.h>

namespace Shader::Maxwell {
namespace {
enum class Precision : u64 {
    F16,
    F32,
};

constexpr unsigned R = 1;
constexpr unsigned G = 2;
constexpr unsigned B = 4;
constexpr unsigned A = 8;

constexpr std::array RG_LUT{
    R,     //
    G,     //
    B,     //
    A,     //
    R | G, //
    R | A, //
    G | A, //
    B | A, //
};

constexpr std::array RGBA_LUT{
    R | G | B,     //
    R | G | A,     //
    R | B | A,     //
    G | B | A,     //
    R | G | B | A, //
};

union TextureLoadSwizzledEncoding {
    u64 raw;
    BitField<59, 1, Precision> precision;
    BitField<54, 1, u64> aoffi;
    BitField<53, 1, u64> lod;
    BitField<55, 1, u64> ms;
    BitField<49, 1, u64> nodep;
    BitField<28, 8, IR::Reg> dest_reg_b;
    BitField<0, 8, IR::Reg> dest_reg_a;
    BitField<8, 8, IR::Reg> src_reg_a;
    BitField<20, 8, IR::Reg> src_reg_b;
    BitField<36, 13, u64> cbuf_offset;
    BitField<50, 3, u64> swizzle;
    BitField<53, 4, u64> encoding;
};

void CheckAlignment(IR::Reg reg, size_t alignment) {
    if (!IR::IsAligned(reg, alignment)) {
        throw NotImplementedException("Unaligned source register {}", reg);
    }
}

IR::Value MakeOffset(TranslatorVisitor& v, IR::Reg reg) {
    const IR::U32 value{v.X(reg)};
    return v.ir.CompositeConstruct(v.ir.BitFieldExtract(value, v.ir.Imm32(0), v.ir.Imm32(4), true),
                                   v.ir.BitFieldExtract(value, v.ir.Imm32(4), v.ir.Imm32(4), true));
}

IR::Value Sample(TranslatorVisitor& v, u64 insn) {
    const TextureLoadSwizzledEncoding tlds{insn};
    const IR::U32 handle{v.ir.Imm32(static_cast<u32>(tlds.cbuf_offset * 4))};
    const IR::Reg reg_a{tlds.src_reg_a};
    const IR::Reg reg_b{tlds.src_reg_b};
    IR::Value coords;
    IR::U32 lod{v.ir.Imm32(0U)};
    IR::Value offsets;
    IR::U32 multisample;
    Shader::TextureType texture_type{};
    {
        const auto switch_value_0161_4{tlds.encoding.Value()};
        do {
            if (switch_value_0161_4 == 0) {
                texture_type = Shader::TextureType::Color1D;
                coords = v.X(reg_a);
                break;
            } else if (switch_value_0161_4 == 1) {
                texture_type = Shader::TextureType::Color1D;
                coords = v.X(reg_a);
                lod = v.X(reg_b);
                break;
            } else if (switch_value_0161_4 == 2) {
                texture_type = Shader::TextureType::Color2D;
                coords = v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_b));
                break;
            } else if (switch_value_0161_4 == 4) {
                CheckAlignment(reg_a, 2);
                texture_type = Shader::TextureType::Color2D;
                coords = v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_a + 1));
                offsets = MakeOffset(v, reg_b);
                break;
            } else if (switch_value_0161_4 == 5) {
                CheckAlignment(reg_a, 2);
                texture_type = Shader::TextureType::Color2D;
                coords = v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_a + 1));
                lod = v.X(reg_b);
                break;
            } else if (switch_value_0161_4 == 6) {
                CheckAlignment(reg_a, 2);
                texture_type = Shader::TextureType::Color2D;
                coords = v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_a + 1));
                multisample = v.X(reg_b);
                break;
            } else if (switch_value_0161_4 == 7) {
                CheckAlignment(reg_a, 2);
                texture_type = Shader::TextureType::Color3D;
                coords = v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_a + 1), v.X(reg_b));
                break;
            } else if (switch_value_0161_4 == 8) {
                CheckAlignment(reg_b, 2);
                const IR::U32 array{v.ir.BitFieldExtract(v.X(reg_a), v.ir.Imm32(0), v.ir.Imm32(16))};
                texture_type = Shader::TextureType::ColorArray2D;
                coords = v.ir.CompositeConstruct(v.X(reg_b), v.X(reg_b + 1), array);
                break;
            } else if (switch_value_0161_4 == 12) {
                CheckAlignment(reg_a, 2);
                CheckAlignment(reg_b, 2);
                texture_type = Shader::TextureType::Color2D;
                coords = v.ir.CompositeConstruct(v.X(reg_a), v.X(reg_a + 1));
                lod = v.X(reg_b);
                offsets = MakeOffset(v, reg_b + 1);
                break;
            } else {
                throw NotImplementedException("Illegal encoding {}", tlds.encoding.Value());
            }
        } while (false);
    }
    IR::TextureInstInfo info{};
    if (tlds.precision == Precision::F16) {
        info.relaxed_precision.Assign(1);
    }
    info.type.Assign(texture_type);
    return v.ir.ImageFetch(handle, coords, offsets, lod, multisample, info);
}

unsigned Swizzle(u64 insn) {
    const TextureLoadSwizzledEncoding tlds{insn};
    const size_t encoding{tlds.swizzle};
    if (tlds.dest_reg_b == IR::Reg::RZ) {
        if (encoding >= RG_LUT.size()) {
            throw NotImplementedException("Illegal RG encoding {}", encoding);
        }
        return RG_LUT[encoding];
    } else {
        if (encoding >= RGBA_LUT.size()) {
            throw NotImplementedException("Illegal RGBA encoding {}", encoding);
        }
        return RGBA_LUT[encoding];
    }
}

IR::F32 Extract(TranslatorVisitor& v, const IR::Value& sample, unsigned component) {
    return IR::F32{v.ir.CompositeExtract(sample, component)};
}

IR::Reg RegStoreComponent32(u64 insn, unsigned index) {
    const TextureLoadSwizzledEncoding tlds{insn};
    {
        const auto switch_value_0161_3{index};
        do {
            if (switch_value_0161_3 == 0) {
                return tlds.dest_reg_a;
            } else if (switch_value_0161_3 == 1) {
                CheckAlignment(tlds.dest_reg_a, 2);
                return tlds.dest_reg_a + 1;
            } else if (switch_value_0161_3 == 2) {
                return tlds.dest_reg_b;
            } else if (switch_value_0161_3 == 3) {
                CheckAlignment(tlds.dest_reg_b, 2);
                return tlds.dest_reg_b + 1;
            }
        } while (false);
    }
    throw LogicError("Invalid store index {}", index);
}

void Store32(TranslatorVisitor& v, u64 insn, const IR::Value& sample) {
    const unsigned swizzle{Swizzle(insn)};
    unsigned store_index{0};
    for (unsigned component = 0; component < 4; ++component) {
        if (((swizzle >> component) & 1) == 0) {
            continue;
        }
        const IR::Reg dest{RegStoreComponent32(insn, store_index)};
        v.F(dest, Extract(v, sample, component));
        ++store_index;
    }
}

IR::U32 Pack(TranslatorVisitor& v, const IR::F32& lhs, const IR::F32& rhs) {
    return v.ir.PackHalf2x16(v.ir.CompositeConstruct(lhs, rhs));
}

void Store16(TranslatorVisitor& v, u64 insn, const IR::Value& sample) {
    const unsigned swizzle{Swizzle(insn)};
    unsigned store_index{0};
    std::array<IR::F32, 4> swizzled;
    for (unsigned component = 0; component < 4; ++component) {
        if (((swizzle >> component) & 1) == 0) {
            continue;
        }
        swizzled[store_index] = Extract(v, sample, component);
        ++store_index;
    }
    const IR::F32 zero{v.ir.Imm32(0.0f)};
    const TextureLoadSwizzledEncoding tlds{insn};
    {
        const auto switch_value_0161_2{store_index};
        do {
            if (switch_value_0161_2 == 1) {
                v.X(tlds.dest_reg_a, Pack(v, swizzled[0], zero));
                break;
            } else if (switch_value_0161_2 == 2 || switch_value_0161_2 == 3 || switch_value_0161_2 == 4) {
                v.X(tlds.dest_reg_a, Pack(v, swizzled[0], swizzled[1]));
                {
                    const auto switch_value_0161_1{store_index};
                    do {
                        if (switch_value_0161_1 == 2) {
                            break;
                        } else if (switch_value_0161_1 == 3) {
                            v.X(tlds.dest_reg_b, Pack(v, swizzled[2], zero));
                            break;
                        } else if (switch_value_0161_1 == 4) {
                            v.X(tlds.dest_reg_b, Pack(v, swizzled[2], swizzled[3]));
                            break;
                        }
                    } while (false);
                }
                break;
            }
        } while (false);
    }
}
} // Anonymous namespace

void TranslatorVisitor::TLDS(u64 insn) {
    const IR::Value sample{Sample(*this, insn)};
    if (TextureLoadSwizzledEncoding{insn}.precision == Precision::F32) {
        Store32(*this, insn, sample);
    } else {
        Store16(*this, insn, sample);
    }
}
} // namespace Shader::Maxwell
