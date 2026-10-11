// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <shader_compiler/backend/spirv/spirv_emit_context.h>

namespace Shader::Backend::SPIRV::Direct {
enum class UniformField { U8, S8, U16, S16, U32, F32, U32x2, U32x4 };
inline Id GetUniform(const UniformDefinitions& definitions, UniformField field) {
    switch (field) {
    case UniformField::U8: return definitions.U8;
    case UniformField::S8: return definitions.S8;
    case UniformField::U16: return definitions.U16;
    case UniformField::S16: return definitions.S16;
    case UniformField::U32: return definitions.U32;
    case UniformField::F32: return definitions.F32;
    case UniformField::U32x2: return definitions.U32x2;
    case UniformField::U32x4: return definitions.U32x4;
    }
    throw LogicError("Invalid SPIR-V uniform field");
}
inline void SetUniform(UniformDefinitions& definitions, UniformField field, Id value) {
    switch (field) {
    case UniformField::U8: definitions.U8 = value; return;
    case UniformField::S8: definitions.S8 = value; return;
    case UniformField::U16: definitions.U16 = value; return;
    case UniformField::S16: definitions.S16 = value; return;
    case UniformField::U32: definitions.U32 = value; return;
    case UniformField::F32: definitions.F32 = value; return;
    case UniformField::U32x2: definitions.U32x2 = value; return;
    case UniformField::U32x4: definitions.U32x4 = value; return;
    }
    throw LogicError("Invalid SPIR-V uniform field");
}
enum class StorageField { U8, S8, U16, S16, U32, F32, U64, U32x2, U32x4 };
inline Id GetStorage(const StorageDefinitions& definitions, StorageField field) {
    switch (field) {
    case StorageField::U8: return definitions.U8;
    case StorageField::S8: return definitions.S8;
    case StorageField::U16: return definitions.U16;
    case StorageField::S16: return definitions.S16;
    case StorageField::U32: return definitions.U32;
    case StorageField::F32: return definitions.F32;
    case StorageField::U64: return definitions.U64;
    case StorageField::U32x2: return definitions.U32x2;
    case StorageField::U32x4: return definitions.U32x4;
    }
    throw LogicError("Invalid SPIR-V storage field");
}
inline void SetStorage(StorageDefinitions& definitions, StorageField field, Id value) {
    switch (field) {
    case StorageField::U8: definitions.U8 = value; return;
    case StorageField::S8: definitions.S8 = value; return;
    case StorageField::U16: definitions.U16 = value; return;
    case StorageField::S16: definitions.S16 = value; return;
    case StorageField::U32: definitions.U32 = value; return;
    case StorageField::F32: definitions.F32 = value; return;
    case StorageField::U64: definitions.U64 = value; return;
    case StorageField::U32x2: definitions.U32x2 = value; return;
    case StorageField::U32x4: definitions.U32x4 = value; return;
    }
    throw LogicError("Invalid SPIR-V storage field");
}
} // namespace Shader::Backend::SPIRV::Direct
