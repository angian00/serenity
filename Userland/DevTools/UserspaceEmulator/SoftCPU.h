/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "AK/Debug.h"
#include "Emulator.h"
#include "Region.h"
#include "SoftFPU.h"
#include "SoftVPU.h"
#include "ValueWithShadow.h"
#include <AK/ByteReader.h>
#include <LibX86/Instruction.h>
#include <LibX86/Interpreter.h>

namespace UserspaceEmulator {

class Emulator;
class Region;

union PartAddressableRegister {
    struct {
        u32 full_u32;
    };
    struct {
        u16 low_u16;
        u16 high_u16;
    };
    struct {
        u8 low_u8;
        u8 high_u8;
        u16 also_high_u16;
    };
};

class SoftCPU final
    : public X86::Interpreter
    , public X86::InstructionStream {
    friend SoftFPU;

public:
    using ValueWithShadowType8 = ValueWithShadow<u8>;
    using ValueWithShadowType16 = ValueWithShadow<u16>;
    using ValueWithShadowType32 = ValueWithShadow<u32>;
    using ValueWithShadowType64 = ValueWithShadow<u64>;
    using ValueWithShadowType128 = ValueWithShadow<u128>;
    using ValueWithShadowType256 = ValueWithShadow<u256>;

    explicit SoftCPU(Emulator&);
    void dump() const;

    u32 base_eip() const { return m_base_eip; }
    void save_base_eip() { m_base_eip = m_eip; }

    u32 eip() const { return m_eip; }
    void set_eip(u32 eip)
    {
        m_eip = eip;
    }

    struct Flags {
        enum Flag {
            CF = 0x0001, // 0b0000'0000'0000'0001
            PF = 0x0004, // 0b0000'0000'0000'0100
            AF = 0x0010, // 0b0000'0000'0001'0000
            ZF = 0x0040, // 0b0000'0000'0100'0000
            SF = 0x0080, // 0b0000'0000'1000'0000
            TF = 0x0100, // 0b0000'0001'0000'0000
            IF = 0x0200, // 0b0000'0010'0000'0000
            DF = 0x0400, // 0b0000'0100'0000'0000
            OF = 0x0800, // 0b0000'1000'0000'0000
        };
    };

    void push32(ValueWithShadow<u32>);
    ValueWithShadow<u32> pop32();

    void push16(ValueWithShadow<u16>);
    ValueWithShadow<u16> pop16();

    void push_string(StringView);
    void push_buffer(u8 const* data, size_t);

    u16 segment(X86::SegmentRegister seg) const { return m_segment[(int)seg]; }
    u16& segment(X86::SegmentRegister seg) { return m_segment[(int)seg]; }

    ValueAndShadowReference<u8> gpr8(X86::RegisterIndex8 reg)
    {
        switch (reg) {
        case X86::RegisterAL:
            return m_gpr[X86::RegisterEAX].reference_to<&PartAddressableRegister::low_u8>();
        case X86::RegisterAH:
            return m_gpr[X86::RegisterEAX].reference_to<&PartAddressableRegister::high_u8>();
        case X86::RegisterBL:
            return m_gpr[X86::RegisterEBX].reference_to<&PartAddressableRegister::low_u8>();
        case X86::RegisterBH:
            return m_gpr[X86::RegisterEBX].reference_to<&PartAddressableRegister::high_u8>();
        case X86::RegisterCL:
            return m_gpr[X86::RegisterECX].reference_to<&PartAddressableRegister::low_u8>();
        case X86::RegisterCH:
            return m_gpr[X86::RegisterECX].reference_to<&PartAddressableRegister::high_u8>();
        case X86::RegisterDL:
            return m_gpr[X86::RegisterEDX].reference_to<&PartAddressableRegister::low_u8>();
        case X86::RegisterDH:
            return m_gpr[X86::RegisterEDX].reference_to<&PartAddressableRegister::high_u8>();
        default:
            VERIFY_NOT_REACHED();
        }
    }

    ValueWithShadow<u8> const_gpr8(X86::RegisterIndex8 reg) const
    {
        switch (reg) {
        case X86::RegisterAL:
            return m_gpr[X86::RegisterEAX].slice<&PartAddressableRegister::low_u8>();
        case X86::RegisterAH:
            return m_gpr[X86::RegisterEAX].slice<&PartAddressableRegister::high_u8>();
        case X86::RegisterBL:
            return m_gpr[X86::RegisterEBX].slice<&PartAddressableRegister::low_u8>();
        case X86::RegisterBH:
            return m_gpr[X86::RegisterEBX].slice<&PartAddressableRegister::high_u8>();
        case X86::RegisterCL:
            return m_gpr[X86::RegisterECX].slice<&PartAddressableRegister::low_u8>();
        case X86::RegisterCH:
            return m_gpr[X86::RegisterECX].slice<&PartAddressableRegister::high_u8>();
        case X86::RegisterDL:
            return m_gpr[X86::RegisterEDX].slice<&PartAddressableRegister::low_u8>();
        case X86::RegisterDH:
            return m_gpr[X86::RegisterEDX].slice<&PartAddressableRegister::high_u8>();
        default:
            VERIFY_NOT_REACHED();
        }
    }

    ValueWithShadow<u16> const_gpr16(X86::RegisterIndex16 reg) const
    {
        return m_gpr[reg].slice<&PartAddressableRegister::low_u16>();
    }

    ValueAndShadowReference<u16> gpr16(X86::RegisterIndex16 reg)
    {
        return m_gpr[reg].reference_to<&PartAddressableRegister::low_u16>();
    }

    ValueWithShadow<u32> const_gpr32(X86::RegisterIndex32 reg) const
    {
        return m_gpr[reg].slice<&PartAddressableRegister::full_u32>();
    }

    ValueAndShadowReference<u32> gpr32(X86::RegisterIndex32 reg)
    {
        return m_gpr[reg].reference_to<&PartAddressableRegister::full_u32>();
    }

    template<typename T>
    ValueWithShadow<T> const_gpr(unsigned register_index) const
    {
        if constexpr (sizeof(T) == 1)
            return const_gpr8((X86::RegisterIndex8)register_index);
        if constexpr (sizeof(T) == 2)
            return const_gpr16((X86::RegisterIndex16)register_index);
        if constexpr (sizeof(T) == 4)
            return const_gpr32((X86::RegisterIndex32)register_index);
    }

    template<typename T>
    ValueAndShadowReference<T> gpr(unsigned register_index)
    {
        if constexpr (sizeof(T) == 1)
            return gpr8((X86::RegisterIndex8)register_index);
        if constexpr (sizeof(T) == 2)
            return gpr16((X86::RegisterIndex16)register_index);
        if constexpr (sizeof(T) == 4)
            return gpr32((X86::RegisterIndex32)register_index);
    }

    ValueWithShadow<u32> source_index(X86::AddressSize address_size) const
    {
        if (address_size == X86::AddressSize::Size32)
            return esi();
        if (address_size == X86::AddressSize::Size16)
            return { si().value(), (u32)si().shadow_as_value() & 0xffff };
        VERIFY_NOT_REACHED();
    }

    ValueWithShadow<u32> destination_index(X86::AddressSize address_size) const
    {
        if (address_size == X86::AddressSize::Size32)
            return edi();
        if (address_size == X86::AddressSize::Size16)
            return { di().value(), (u32)di().shadow_as_value() & 0xffff };
        VERIFY_NOT_REACHED();
    }

    ValueWithShadow<u32> loop_index(X86::AddressSize address_size) const
    {
        if (address_size == X86::AddressSize::Size32)
            return ecx();
        if (address_size == X86::AddressSize::Size16)
            return { cx().value(), (u32)cx().shadow_as_value() & 0xffff };
        VERIFY_NOT_REACHED();
    }

    bool decrement_loop_index(X86::AddressSize address_size)
    {
        switch (address_size) {
        case X86::AddressSize::Size32:
            set_ecx({ ecx().value() - 1, ecx().shadow() });
            return ecx().value() == 0;
        case X86::AddressSize::Size16:
            set_cx(ValueWithShadow<u16>(cx().value() - 1, cx().shadow()));
            return cx().value() == 0;
        default:
            VERIFY_NOT_REACHED();
        }
    }

    ALWAYS_INLINE void step_source_index(X86::AddressSize address_size, u32 step)
    {
        switch (address_size) {
        case X86::AddressSize::Size32:
            if (df())
                set_esi({ esi().value() - step, esi().shadow() });
            else
                set_esi({ esi().value() + step, esi().shadow() });
            break;
        case X86::AddressSize::Size16:
            if (df())
                set_si(ValueWithShadow<u16>(si().value() - step, si().shadow()));
            else
                set_si(ValueWithShadow<u16>(si().value() + step, si().shadow()));
            break;
        default:
            VERIFY_NOT_REACHED();
        }
    }

    ALWAYS_INLINE void step_destination_index(X86::AddressSize address_size, u32 step)
    {
        switch (address_size) {
        case X86::AddressSize::Size32:
            if (df())
                set_edi({ edi().value() - step, edi().shadow() });
            else
                set_edi({ edi().value() + step, edi().shadow() });
            break;
        case X86::AddressSize::Size16:
            if (df())
                set_di(ValueWithShadow<u16>(di().value() - step, di().shadow()));
            else
                set_di(ValueWithShadow<u16>(di().value() + step, di().shadow()));
            break;
        default:
            VERIFY_NOT_REACHED();
        }
    }

    u32 eflags() const { return m_eflags; }
    void set_eflags(ValueWithShadow<u32> eflags)
    {
        m_eflags = eflags.value();
        m_flags_tainted = eflags.is_uninitialized();
    }

    ValueWithShadow<u32> eax() const { return const_gpr32(X86::RegisterEAX); }
    ValueWithShadow<u32> ebx() const { return const_gpr32(X86::RegisterEBX); }
    ValueWithShadow<u32> ecx() const { return const_gpr32(X86::RegisterECX); }
    ValueWithShadow<u32> edx() const { return const_gpr32(X86::RegisterEDX); }
    ValueWithShadow<u32> esp() const { return const_gpr32(X86::RegisterESP); }
    ValueWithShadow<u32> ebp() const { return const_gpr32(X86::RegisterEBP); }
    ValueWithShadow<u32> esi() const { return const_gpr32(X86::RegisterESI); }
    ValueWithShadow<u32> edi() const { return const_gpr32(X86::RegisterEDI); }

    ValueWithShadow<u16> ax() const { return const_gpr16(X86::RegisterAX); }
    ValueWithShadow<u16> bx() const { return const_gpr16(X86::RegisterBX); }
    ValueWithShadow<u16> cx() const { return const_gpr16(X86::RegisterCX); }
    ValueWithShadow<u16> dx() const { return const_gpr16(X86::RegisterDX); }
    ValueWithShadow<u16> sp() const { return const_gpr16(X86::RegisterSP); }
    ValueWithShadow<u16> bp() const { return const_gpr16(X86::RegisterBP); }
    ValueWithShadow<u16> si() const { return const_gpr16(X86::RegisterSI); }
    ValueWithShadow<u16> di() const { return const_gpr16(X86::RegisterDI); }

    ValueWithShadow<u8> al() const { return const_gpr8(X86::RegisterAL); }
    ValueWithShadow<u8> ah() const { return const_gpr8(X86::RegisterAH); }
    ValueWithShadow<u8> bl() const { return const_gpr8(X86::RegisterBL); }
    ValueWithShadow<u8> bh() const { return const_gpr8(X86::RegisterBH); }
    ValueWithShadow<u8> cl() const { return const_gpr8(X86::RegisterCL); }
    ValueWithShadow<u8> ch() const { return const_gpr8(X86::RegisterCH); }
    ValueWithShadow<u8> dl() const { return const_gpr8(X86::RegisterDL); }
    ValueWithShadow<u8> dh() const { return const_gpr8(X86::RegisterDH); }

    long double fpu_get(u8 index) { return m_fpu.fpu_get(index); }
    long double fpu_pop() { return m_fpu.fpu_pop(); }
    MMX mmx_get(u8 index) const { return m_fpu.mmx_get(index); };

    void set_eax(ValueWithShadow<u32> value) { gpr32(X86::RegisterEAX) = value; }
    void set_ebx(ValueWithShadow<u32> value) { gpr32(X86::RegisterEBX) = value; }
    void set_ecx(ValueWithShadow<u32> value) { gpr32(X86::RegisterECX) = value; }
    void set_edx(ValueWithShadow<u32> value) { gpr32(X86::RegisterEDX) = value; }
    void set_esp(ValueWithShadow<u32> value) { gpr32(X86::RegisterESP) = value; }
    void set_ebp(ValueWithShadow<u32> value) { gpr32(X86::RegisterEBP) = value; }
    void set_esi(ValueWithShadow<u32> value) { gpr32(X86::RegisterESI) = value; }
    void set_edi(ValueWithShadow<u32> value) { gpr32(X86::RegisterEDI) = value; }

    void set_ax(ValueWithShadow<u16> value) { gpr16(X86::RegisterAX) = value; }
    void set_bx(ValueWithShadow<u16> value) { gpr16(X86::RegisterBX) = value; }
    void set_cx(ValueWithShadow<u16> value) { gpr16(X86::RegisterCX) = value; }
    void set_dx(ValueWithShadow<u16> value) { gpr16(X86::RegisterDX) = value; }
    void set_sp(ValueWithShadow<u16> value) { gpr16(X86::RegisterSP) = value; }
    void set_bp(ValueWithShadow<u16> value) { gpr16(X86::RegisterBP) = value; }
    void set_si(ValueWithShadow<u16> value) { gpr16(X86::RegisterSI) = value; }
    void set_di(ValueWithShadow<u16> value) { gpr16(X86::RegisterDI) = value; }

    void set_al(ValueWithShadow<u8> value) { gpr8(X86::RegisterAL) = value; }
    void set_ah(ValueWithShadow<u8> value) { gpr8(X86::RegisterAH) = value; }
    void set_bl(ValueWithShadow<u8> value) { gpr8(X86::RegisterBL) = value; }
    void set_bh(ValueWithShadow<u8> value) { gpr8(X86::RegisterBH) = value; }
    void set_cl(ValueWithShadow<u8> value) { gpr8(X86::RegisterCL) = value; }
    void set_ch(ValueWithShadow<u8> value) { gpr8(X86::RegisterCH) = value; }
    void set_dl(ValueWithShadow<u8> value) { gpr8(X86::RegisterDL) = value; }
    void set_dh(ValueWithShadow<u8> value) { gpr8(X86::RegisterDH) = value; }

    void fpu_push(long double value) { m_fpu.fpu_push(value); }
    void fpu_set(u8 index, long double value) { m_fpu.fpu_set(index, value); }
    void mmx_set(u8 index, MMX value) { m_fpu.mmx_set(index, value); }

    bool of() const { return m_eflags & Flags::OF; }
    bool sf() const { return m_eflags & Flags::SF; }
    bool zf() const { return m_eflags & Flags::ZF; }
    bool af() const { return m_eflags & Flags::AF; }
    bool pf() const { return m_eflags & Flags::PF; }
    bool cf() const { return m_eflags & Flags::CF; }
    bool df() const { return m_eflags & Flags::DF; }

    void set_flag(Flags::Flag flag, bool value)
    {
        if (value)
            m_eflags |= flag;
        else
            m_eflags &= ~flag;
    }

    void set_of(bool value) { set_flag(Flags::OF, value); }
    void set_sf(bool value) { set_flag(Flags::SF, value); }
    void set_zf(bool value) { set_flag(Flags::ZF, value); }
    void set_af(bool value) { set_flag(Flags::AF, value); }
    void set_pf(bool value) { set_flag(Flags::PF, value); }
    void set_cf(bool value) { set_flag(Flags::CF, value); }
    void set_df(bool value) { set_flag(Flags::DF, value); }

    void set_flags_with_mask(u32 new_flags, u32 mask)
    {
        m_eflags &= ~mask;
        m_eflags |= new_flags & mask;
    }

    void set_flags_oszapc(u32 new_flags)
    {
        set_flags_with_mask(new_flags, Flags::OF | Flags::SF | Flags::ZF | Flags::AF | Flags::PF | Flags::CF);
    }

    void set_flags_oszap(u32 new_flags)
    {
        set_flags_with_mask(new_flags, Flags::OF | Flags::SF | Flags::ZF | Flags::AF | Flags::PF);
    }

    void set_flags_oszpc(u32 new_flags)
    {
        set_flags_with_mask(new_flags, Flags::OF | Flags::SF | Flags::ZF | Flags::PF | Flags::CF);
    }

    void set_flags_oc(u32 new_flags)
    {
        set_flags_with_mask(new_flags, Flags::OF | Flags::CF);
    }

    u16 cs() const { return m_segment[(int)X86::SegmentRegister::CS]; }
    u16 ds() const { return m_segment[(int)X86::SegmentRegister::DS]; }
    u16 es() const { return m_segment[(int)X86::SegmentRegister::ES]; }
    u16 ss() const { return m_segment[(int)X86::SegmentRegister::SS]; }

    ValueWithShadow<u8> read_memory8(X86::LogicalAddress);
    ValueWithShadow<u16> read_memory16(X86::LogicalAddress);
    ValueWithShadow<u32> read_memory32(X86::LogicalAddress);
    ValueWithShadow<u64> read_memory64(X86::LogicalAddress);
    ValueWithShadow<u128> read_memory128(X86::LogicalAddress);
    ValueWithShadow<u256> read_memory256(X86::LogicalAddress);

    template<typename T>
    ValueWithShadow<T> read_memory(X86::LogicalAddress address)
    {
        auto value = m_emulator.mmu().read<T>(address);
        if constexpr (AK::HasFormatter<T>)
            outln_if(MEMORY_DEBUG, "\033[36;1mread_memory: @{:#04x}:{:p} -> {:#064x} ({:hex-dump})\033[0m", address.selector(), address.offset(), value.value(), value.shadow().span());
        else
            outln_if(MEMORY_DEBUG, "\033[36;1mread_memory: @{:#04x}:{:p} -> ??? ({:hex-dump})\033[0m", address.selector(), address.offset(), value.shadow().span());
        return value;
    }

    void write_memory8(X86::LogicalAddress, ValueWithShadow<u8>);
    void write_memory16(X86::LogicalAddress, ValueWithShadow<u16>);
    void write_memory32(X86::LogicalAddress, ValueWithShadow<u32>);
    void write_memory64(X86::LogicalAddress, ValueWithShadow<u64>);
    void write_memory128(X86::LogicalAddress, ValueWithShadow<u128>);
    void write_memory256(X86::LogicalAddress, ValueWithShadow<u256>);

    template<typename T>
    void write_memory(X86::LogicalAddress address, ValueWithShadow<T> data)
    {
        if constexpr (sizeof(T) == 1)
            return write_memory8(address, data);
        if constexpr (sizeof(T) == 2)
            return write_memory16(address, data);
        if constexpr (sizeof(T) == 4)
            return write_memory32(address, data);
        if constexpr (sizeof(T) == 8)
            return write_memory64(address, data);
        if constexpr (sizeof(T) == 16)
            return write_memory128(address, data);
        if constexpr (sizeof(T) == 32)
            return write_memory256(address, data);
    }

    bool evaluate_condition(u8 condition) const
    {
        switch (condition) {
        case 0:
            return of(); // O
        case 1:
            return !of(); // NO
        case 2:
            return cf(); // B, C, NAE
        case 3:
            return !cf(); // NB, NC, AE
        case 4:
            return zf(); // E, Z
        case 5:
            return !zf(); // NE, NZ
        case 6:
            return cf() || zf(); // BE, NA
        case 7:
            return !(cf() || zf()); // NBE, A
        case 8:
            return sf(); // S
        case 9:
            return !sf(); // NS
        case 10:
            return pf(); // P, PE
        case 11:
            return !pf(); // NP, PO
        case 12:
            return sf() != of(); // L, NGE
        case 13:
            return sf() == of(); // NL, GE
        case 14:
            return (sf() != of()) || zf(); // LE, NG
        case 15:
            return !((sf() != of()) || zf()); // NLE, G
        default:
            VERIFY_NOT_REACHED();
        }
        return 0;
    }

    template<bool check_zf, typename Callback>
    void do_once_or_repeat(const X86::Instruction& insn, Callback);

    template<typename A>
    void taint_flags_from(const A& a)
    {
        m_flags_tainted = a.is_uninitialized();
    }

    template<typename A, typename B>
    void taint_flags_from(const A& a, const B& b)
    {
        m_flags_tainted = a.is_uninitialized() || b.is_uninitialized();
    }

    template<typename A, typename B, typename C>
    void taint_flags_from(const A& a, const B& b, const C& c)
    {
        m_flags_tainted = a.is_uninitialized() || b.is_uninitialized() || c.is_uninitialized();
    }

    void warn_if_flags_tainted(char const* message) const;

    // ^X86::InstructionStream
    virtual bool can_read() override { return false; }
    virtual u8 read8() override;
    virtual u16 read16() override;
    virtual u32 read32() override;
    virtual u64 read64() override;

private:
    // ^X86::Interpreter
    virtual void AAA(const X86::Instruction&) override;
    virtual void AAD(const X86::Instruction&) override;
    virtual void AAM(const X86::Instruction&) override;
    virtual void AAS(const X86::Instruction&) override;
    virtual void ADC_AL_imm8(const X86::Instruction&) override;
    virtual void ADC_AX_imm16(const X86::Instruction&) override;
    virtual void ADC_EAX_imm32(const X86::Instruction&) override;
    virtual void ADC_RM16_imm16(const X86::Instruction&) override;
    virtual void ADC_RM16_imm8(const X86::Instruction&) override;
    virtual void ADC_RM16_reg16(const X86::Instruction&) override;
    virtual void ADC_RM32_imm32(const X86::Instruction&) override;
    virtual void ADC_RM32_imm8(const X86::Instruction&) override;
    virtual void ADC_RM32_reg32(const X86::Instruction&) override;
    virtual void ADC_RM8_imm8(const X86::Instruction&) override;
    virtual void ADC_RM8_reg8(const X86::Instruction&) override;
    virtual void ADC_reg16_RM16(const X86::Instruction&) override;
    virtual void ADC_reg32_RM32(const X86::Instruction&) override;
    virtual void ADC_reg8_RM8(const X86::Instruction&) override;
    virtual void ADD_AL_imm8(const X86::Instruction&) override;
    virtual void ADD_AX_imm16(const X86::Instruction&) override;
    virtual void ADD_EAX_imm32(const X86::Instruction&) override;
    virtual void ADD_RM16_imm16(const X86::Instruction&) override;
    virtual void ADD_RM16_imm8(const X86::Instruction&) override;
    virtual void ADD_RM16_reg16(const X86::Instruction&) override;
    virtual void ADD_RM32_imm32(const X86::Instruction&) override;
    virtual void ADD_RM32_imm8(const X86::Instruction&) override;
    virtual void ADD_RM32_reg32(const X86::Instruction&) override;
    virtual void ADD_RM8_imm8(const X86::Instruction&) override;
    virtual void ADD_RM8_reg8(const X86::Instruction&) override;
    virtual void ADD_reg16_RM16(const X86::Instruction&) override;
    virtual void ADD_reg32_RM32(const X86::Instruction&) override;
    virtual void ADD_reg8_RM8(const X86::Instruction&) override;
    virtual void AND_AL_imm8(const X86::Instruction&) override;
    virtual void AND_AX_imm16(const X86::Instruction&) override;
    virtual void AND_EAX_imm32(const X86::Instruction&) override;
    virtual void AND_RM16_imm16(const X86::Instruction&) override;
    virtual void AND_RM16_imm8(const X86::Instruction&) override;
    virtual void AND_RM16_reg16(const X86::Instruction&) override;
    virtual void AND_RM32_imm32(const X86::Instruction&) override;
    virtual void AND_RM32_imm8(const X86::Instruction&) override;
    virtual void AND_RM32_reg32(const X86::Instruction&) override;
    virtual void AND_RM8_imm8(const X86::Instruction&) override;
    virtual void AND_RM8_reg8(const X86::Instruction&) override;
    virtual void AND_reg16_RM16(const X86::Instruction&) override;
    virtual void AND_reg32_RM32(const X86::Instruction&) override;
    virtual void AND_reg8_RM8(const X86::Instruction&) override;
    virtual void ARPL(const X86::Instruction&) override;
    virtual void BOUND(const X86::Instruction&) override;
    virtual void BSF_reg16_RM16(const X86::Instruction&) override;
    virtual void BSF_reg32_RM32(const X86::Instruction&) override;
    virtual void BSR_reg16_RM16(const X86::Instruction&) override;
    virtual void BSR_reg32_RM32(const X86::Instruction&) override;
    virtual void BSWAP_reg32(const X86::Instruction&) override;
    virtual void BTC_RM16_imm8(const X86::Instruction&) override;
    virtual void BTC_RM16_reg16(const X86::Instruction&) override;
    virtual void BTC_RM32_imm8(const X86::Instruction&) override;
    virtual void BTC_RM32_reg32(const X86::Instruction&) override;
    virtual void BTR_RM16_imm8(const X86::Instruction&) override;
    virtual void BTR_RM16_reg16(const X86::Instruction&) override;
    virtual void BTR_RM32_imm8(const X86::Instruction&) override;
    virtual void BTR_RM32_reg32(const X86::Instruction&) override;
    virtual void BTS_RM16_imm8(const X86::Instruction&) override;
    virtual void BTS_RM16_reg16(const X86::Instruction&) override;
    virtual void BTS_RM32_imm8(const X86::Instruction&) override;
    virtual void BTS_RM32_reg32(const X86::Instruction&) override;
    virtual void BT_RM16_imm8(const X86::Instruction&) override;
    virtual void BT_RM16_reg16(const X86::Instruction&) override;
    virtual void BT_RM32_imm8(const X86::Instruction&) override;
    virtual void BT_RM32_reg32(const X86::Instruction&) override;
    virtual void CALL_FAR_mem16(const X86::Instruction&) override;
    virtual void CALL_FAR_mem32(const X86::Instruction&) override;
    virtual void CALL_RM16(const X86::Instruction&) override;
    virtual void CALL_RM32(const X86::Instruction&) override;
    virtual void CALL_imm16(const X86::Instruction&) override;
    virtual void CALL_imm16_imm16(const X86::Instruction&) override;
    virtual void CALL_imm16_imm32(const X86::Instruction&) override;
    virtual void CALL_imm32(const X86::Instruction&) override;
    virtual void CBW(const X86::Instruction&) override;
    virtual void CDQ(const X86::Instruction&) override;
    virtual void CLC(const X86::Instruction&) override;
    virtual void CLD(const X86::Instruction&) override;
    virtual void CLI(const X86::Instruction&) override;
    virtual void CLTS(const X86::Instruction&) override;
    virtual void CMC(const X86::Instruction&) override;
    virtual void CMOVcc_reg16_RM16(const X86::Instruction&) override;
    virtual void CMOVcc_reg32_RM32(const X86::Instruction&) override;
    virtual void CMPSB(const X86::Instruction&) override;
    virtual void CMPSD(const X86::Instruction&) override;
    virtual void CMPSW(const X86::Instruction&) override;
    virtual void CMPXCHG_RM16_reg16(const X86::Instruction&) override;
    virtual void CMPXCHG_RM32_reg32(const X86::Instruction&) override;
    virtual void CMPXCHG_RM8_reg8(const X86::Instruction&) override;
    virtual void CMP_AL_imm8(const X86::Instruction&) override;
    virtual void CMP_AX_imm16(const X86::Instruction&) override;
    virtual void CMP_EAX_imm32(const X86::Instruction&) override;
    virtual void CMP_RM16_imm16(const X86::Instruction&) override;
    virtual void CMP_RM16_imm8(const X86::Instruction&) override;
    virtual void CMP_RM16_reg16(const X86::Instruction&) override;
    virtual void CMP_RM32_imm32(const X86::Instruction&) override;
    virtual void CMP_RM32_imm8(const X86::Instruction&) override;
    virtual void CMP_RM32_reg32(const X86::Instruction&) override;
    virtual void CMP_RM8_imm8(const X86::Instruction&) override;
    virtual void CMP_RM8_reg8(const X86::Instruction&) override;
    virtual void CMP_reg16_RM16(const X86::Instruction&) override;
    virtual void CMP_reg32_RM32(const X86::Instruction&) override;
    virtual void CMP_reg8_RM8(const X86::Instruction&) override;
    virtual void CPUID(const X86::Instruction&) override;
    virtual void CWD(const X86::Instruction&) override;
    virtual void CWDE(const X86::Instruction&) override;
    virtual void DAA(const X86::Instruction&) override;
    virtual void DAS(const X86::Instruction&) override;
    virtual void DEC_RM16(const X86::Instruction&) override;
    virtual void DEC_RM32(const X86::Instruction&) override;
    virtual void DEC_RM8(const X86::Instruction&) override;
    virtual void DEC_reg16(const X86::Instruction&) override;
    virtual void DEC_reg32(const X86::Instruction&) override;
    virtual void DIV_RM16(const X86::Instruction&) override;
    virtual void DIV_RM32(const X86::Instruction&) override;
    virtual void DIV_RM8(const X86::Instruction&) override;
    virtual void ENTER16(const X86::Instruction&) override;
    virtual void ENTER32(const X86::Instruction&) override;
    virtual void ESCAPE(const X86::Instruction&) override;
    virtual void FADD_RM32(const X86::Instruction&) override;
    virtual void FMUL_RM32(const X86::Instruction&) override;
    virtual void FCOM_RM32(const X86::Instruction&) override;
    virtual void FCOMP_RM32(const X86::Instruction&) override;
    virtual void FSUB_RM32(const X86::Instruction&) override;
    virtual void FSUBR_RM32(const X86::Instruction&) override;
    virtual void FDIV_RM32(const X86::Instruction&) override;
    virtual void FDIVR_RM32(const X86::Instruction&) override;
    virtual void FLD_RM32(const X86::Instruction&) override;
    virtual void FXCH(const X86::Instruction&) override;
    virtual void FST_RM32(const X86::Instruction&) override;
    virtual void FNOP(const X86::Instruction&) override;
    virtual void FSTP_RM32(const X86::Instruction&) override;
    virtual void FLDENV(const X86::Instruction&) override;
    virtual void FCHS(const X86::Instruction&) override;
    virtual void FABS(const X86::Instruction&) override;
    virtual void FTST(const X86::Instruction&) override;
    virtual void FXAM(const X86::Instruction&) override;
    virtual void FLDCW(const X86::Instruction&) override;
    virtual void FLD1(const X86::Instruction&) override;
    virtual void FLDL2T(const X86::Instruction&) override;
    virtual void FLDL2E(const X86::Instruction&) override;
    virtual void FLDPI(const X86::Instruction&) override;
    virtual void FLDLG2(const X86::Instruction&) override;
    virtual void FLDLN2(const X86::Instruction&) override;
    virtual void FLDZ(const X86::Instruction&) override;
    virtual void FNSTENV(const X86::Instruction&) override;
    virtual void F2XM1(const X86::Instruction&) override;
    virtual void FYL2X(const X86::Instruction&) override;
    virtual void FPTAN(const X86::Instruction&) override;
    virtual void FPATAN(const X86::Instruction&) override;
    virtual void FXTRACT(const X86::Instruction&) override;
    virtual void FPREM1(const X86::Instruction&) override;
    virtual void FDECSTP(const X86::Instruction&) override;
    virtual void FINCSTP(const X86::Instruction&) override;
    virtual void FNSTCW(const X86::Instruction&) override;
    virtual void FPREM(const X86::Instruction&) override;
    virtual void FYL2XP1(const X86::Instruction&) override;
    virtual void FSQRT(const X86::Instruction&) override;
    virtual void FSINCOS(const X86::Instruction&) override;
    virtual void FRNDINT(const X86::Instruction&) override;
    virtual void FSCALE(const X86::Instruction&) override;
    virtual void FSIN(const X86::Instruction&) override;
    virtual void FCOS(const X86::Instruction&) override;
    virtual void FIADD_RM32(const X86::Instruction&) override;
    virtual void FCMOVB(const X86::Instruction&) override;
    virtual void FIMUL_RM32(const X86::Instruction&) override;
    virtual void FCMOVE(const X86::Instruction&) override;
    virtual void FICOM_RM32(const X86::Instruction&) override;
    virtual void FCMOVBE(const X86::Instruction&) override;
    virtual void FICOMP_RM32(const X86::Instruction&) override;
    virtual void FCMOVU(const X86::Instruction&) override;
    virtual void FISUB_RM32(const X86::Instruction&) override;
    virtual void FISUBR_RM32(const X86::Instruction&) override;
    virtual void FUCOMPP(const X86::Instruction&) override;
    virtual void FIDIV_RM32(const X86::Instruction&) override;
    virtual void FIDIVR_RM32(const X86::Instruction&) override;
    virtual void FILD_RM32(const X86::Instruction&) override;
    virtual void FCMOVNB(const X86::Instruction&) override;
    virtual void FISTTP_RM32(const X86::Instruction&) override;
    virtual void FCMOVNE(const X86::Instruction&) override;
    virtual void FIST_RM32(const X86::Instruction&) override;
    virtual void FCMOVNBE(const X86::Instruction&) override;
    virtual void FISTP_RM32(const X86::Instruction&) override;
    virtual void FCMOVNU(const X86::Instruction&) override;
    virtual void FNENI(const X86::Instruction&) override;
    virtual void FNDISI(const X86::Instruction&) override;
    virtual void FNCLEX(const X86::Instruction&) override;
    virtual void FNINIT(const X86::Instruction&) override;
    virtual void FNSETPM(const X86::Instruction&) override;
    virtual void FLD_RM80(const X86::Instruction&) override;
    virtual void FUCOMI(const X86::Instruction&) override;
    virtual void FCOMI(const X86::Instruction&) override;
    virtual void FSTP_RM80(const X86::Instruction&) override;
    virtual void FADD_RM64(const X86::Instruction&) override;
    virtual void FMUL_RM64(const X86::Instruction&) override;
    virtual void FCOM_RM64(const X86::Instruction&) override;
    virtual void FCOMP_RM64(const X86::Instruction&) override;
    virtual void FSUB_RM64(const X86::Instruction&) override;
    virtual void FSUBR_RM64(const X86::Instruction&) override;
    virtual void FDIV_RM64(const X86::Instruction&) override;
    virtual void FDIVR_RM64(const X86::Instruction&) override;
    virtual void FLD_RM64(const X86::Instruction&) override;
    virtual void FFREE(const X86::Instruction&) override;
    virtual void FISTTP_RM64(const X86::Instruction&) override;
    virtual void FST_RM64(const X86::Instruction&) override;
    virtual void FSTP_RM64(const X86::Instruction&) override;
    virtual void FRSTOR(const X86::Instruction&) override;
    virtual void FUCOM(const X86::Instruction&) override;
    virtual void FUCOMP(const X86::Instruction&) override;
    virtual void FNSAVE(const X86::Instruction&) override;
    virtual void FNSTSW(const X86::Instruction&) override;
    virtual void FIADD_RM16(const X86::Instruction&) override;
    virtual void FADDP(const X86::Instruction&) override;
    virtual void FIMUL_RM16(const X86::Instruction&) override;
    virtual void FMULP(const X86::Instruction&) override;
    virtual void FICOM_RM16(const X86::Instruction&) override;
    virtual void FICOMP_RM16(const X86::Instruction&) override;
    virtual void FCOMPP(const X86::Instruction&) override;
    virtual void FISUB_RM16(const X86::Instruction&) override;
    virtual void FSUBRP(const X86::Instruction&) override;
    virtual void FISUBR_RM16(const X86::Instruction&) override;
    virtual void FSUBP(const X86::Instruction&) override;
    virtual void FIDIV_RM16(const X86::Instruction&) override;
    virtual void FDIVRP(const X86::Instruction&) override;
    virtual void FIDIVR_RM16(const X86::Instruction&) override;
    virtual void FDIVP(const X86::Instruction&) override;
    virtual void FILD_RM16(const X86::Instruction&) override;
    virtual void FFREEP(const X86::Instruction&) override;
    virtual void FISTTP_RM16(const X86::Instruction&) override;
    virtual void FIST_RM16(const X86::Instruction&) override;
    virtual void FISTP_RM16(const X86::Instruction&) override;
    virtual void FBLD_M80(const X86::Instruction&) override;
    virtual void FNSTSW_AX(const X86::Instruction&) override;
    virtual void FILD_RM64(const X86::Instruction&) override;
    virtual void FUCOMIP(const X86::Instruction&) override;
    virtual void FBSTP_M80(const X86::Instruction&) override;
    virtual void FCOMIP(const X86::Instruction&) override;
    virtual void FISTP_RM64(const X86::Instruction&) override;
    virtual void HLT(const X86::Instruction&) override;
    virtual void IDIV_RM16(const X86::Instruction&) override;
    virtual void IDIV_RM32(const X86::Instruction&) override;
    virtual void IDIV_RM8(const X86::Instruction&) override;
    virtual void IMUL_RM16(const X86::Instruction&) override;
    virtual void IMUL_RM32(const X86::Instruction&) override;
    virtual void IMUL_RM8(const X86::Instruction&) override;
    virtual void IMUL_reg16_RM16(const X86::Instruction&) override;
    virtual void IMUL_reg16_RM16_imm16(const X86::Instruction&) override;
    virtual void IMUL_reg16_RM16_imm8(const X86::Instruction&) override;
    virtual void IMUL_reg32_RM32(const X86::Instruction&) override;
    virtual void IMUL_reg32_RM32_imm32(const X86::Instruction&) override;
    virtual void IMUL_reg32_RM32_imm8(const X86::Instruction&) override;
    virtual void INC_RM16(const X86::Instruction&) override;
    virtual void INC_RM32(const X86::Instruction&) override;
    virtual void INC_RM8(const X86::Instruction&) override;
    virtual void INC_reg16(const X86::Instruction&) override;
    virtual void INC_reg32(const X86::Instruction&) override;
    virtual void INSB(const X86::Instruction&) override;
    virtual void INSD(const X86::Instruction&) override;
    virtual void INSW(const X86::Instruction&) override;
    virtual void INT1(const X86::Instruction&) override;
    virtual void INT3(const X86::Instruction&) override;
    virtual void INTO(const X86::Instruction&) override;
    virtual void INT_imm8(const X86::Instruction&) override;
    virtual void INVLPG(const X86::Instruction&) override;
    virtual void IN_AL_DX(const X86::Instruction&) override;
    virtual void IN_AL_imm8(const X86::Instruction&) override;
    virtual void IN_AX_DX(const X86::Instruction&) override;
    virtual void IN_AX_imm8(const X86::Instruction&) override;
    virtual void IN_EAX_DX(const X86::Instruction&) override;
    virtual void IN_EAX_imm8(const X86::Instruction&) override;
    virtual void IRET(const X86::Instruction&) override;
    virtual void JCXZ_imm8(const X86::Instruction&) override;
    virtual void JMP_FAR_mem16(const X86::Instruction&) override;
    virtual void JMP_FAR_mem32(const X86::Instruction&) override;
    virtual void JMP_RM16(const X86::Instruction&) override;
    virtual void JMP_RM32(const X86::Instruction&) override;
    virtual void JMP_imm16(const X86::Instruction&) override;
    virtual void JMP_imm16_imm16(const X86::Instruction&) override;
    virtual void JMP_imm16_imm32(const X86::Instruction&) override;
    virtual void JMP_imm32(const X86::Instruction&) override;
    virtual void JMP_short_imm8(const X86::Instruction&) override;
    virtual void Jcc_NEAR_imm(const X86::Instruction&) override;
    virtual void Jcc_imm8(const X86::Instruction&) override;
    virtual void LAHF(const X86::Instruction&) override;
    virtual void LAR_reg16_RM16(const X86::Instruction&) override;
    virtual void LAR_reg32_RM32(const X86::Instruction&) override;
    virtual void LDS_reg16_mem16(const X86::Instruction&) override;
    virtual void LDS_reg32_mem32(const X86::Instruction&) override;
    virtual void LEAVE16(const X86::Instruction&) override;
    virtual void LEAVE32(const X86::Instruction&) override;
    virtual void LEA_reg16_mem16(const X86::Instruction&) override;
    virtual void LEA_reg32_mem32(const X86::Instruction&) override;
    virtual void LES_reg16_mem16(const X86::Instruction&) override;
    virtual void LES_reg32_mem32(const X86::Instruction&) override;
    virtual void LFS_reg16_mem16(const X86::Instruction&) override;
    virtual void LFS_reg32_mem32(const X86::Instruction&) override;
    virtual void LGDT(const X86::Instruction&) override;
    virtual void LGS_reg16_mem16(const X86::Instruction&) override;
    virtual void LGS_reg32_mem32(const X86::Instruction&) override;
    virtual void LIDT(const X86::Instruction&) override;
    virtual void LLDT_RM16(const X86::Instruction&) override;
    virtual void LMSW_RM16(const X86::Instruction&) override;
    virtual void LODSB(const X86::Instruction&) override;
    virtual void LODSD(const X86::Instruction&) override;
    virtual void LODSW(const X86::Instruction&) override;
    virtual void LOOPNZ_imm8(const X86::Instruction&) override;
    virtual void LOOPZ_imm8(const X86::Instruction&) override;
    virtual void LOOP_imm8(const X86::Instruction&) override;
    virtual void LSL_reg16_RM16(const X86::Instruction&) override;
    virtual void LSL_reg32_RM32(const X86::Instruction&) override;
    virtual void LSS_reg16_mem16(const X86::Instruction&) override;
    virtual void LSS_reg32_mem32(const X86::Instruction&) override;
    virtual void LTR_RM16(const X86::Instruction&) override;
    virtual void MOVSB(const X86::Instruction&) override;
    virtual void MOVSD(const X86::Instruction&) override;
    virtual void MOVSW(const X86::Instruction&) override;
    virtual void MOVSX_reg16_RM8(const X86::Instruction&) override;
    virtual void MOVSX_reg32_RM16(const X86::Instruction&) override;
    virtual void MOVSX_reg32_RM8(const X86::Instruction&) override;
    virtual void MOVZX_reg16_RM8(const X86::Instruction&) override;
    virtual void MOVZX_reg32_RM16(const X86::Instruction&) override;
    virtual void MOVZX_reg32_RM8(const X86::Instruction&) override;
    virtual void MOV_AL_moff8(const X86::Instruction&) override;
    virtual void MOV_AX_moff16(const X86::Instruction&) override;
    virtual void MOV_CR_reg32(const X86::Instruction&) override;
    virtual void MOV_DR_reg32(const X86::Instruction&) override;
    virtual void MOV_EAX_moff32(const X86::Instruction&) override;
    virtual void MOV_RM16_imm16(const X86::Instruction&) override;
    virtual void MOV_RM16_reg16(const X86::Instruction&) override;
    virtual void MOV_RM16_seg(const X86::Instruction&) override;
    virtual void MOV_RM32_imm32(const X86::Instruction&) override;
    virtual void MOV_RM32_reg32(const X86::Instruction&) override;
    virtual void MOV_RM8_imm8(const X86::Instruction&) override;
    virtual void MOV_RM8_reg8(const X86::Instruction&) override;
    virtual void MOV_moff16_AX(const X86::Instruction&) override;
    virtual void MOV_moff32_EAX(const X86::Instruction&) override;
    virtual void MOV_moff8_AL(const X86::Instruction&) override;
    virtual void MOV_reg16_RM16(const X86::Instruction&) override;
    virtual void MOV_reg16_imm16(const X86::Instruction&) override;
    virtual void MOV_reg32_CR(const X86::Instruction&) override;
    virtual void MOV_reg32_DR(const X86::Instruction&) override;
    virtual void MOV_reg32_RM32(const X86::Instruction&) override;
    virtual void MOV_reg32_imm32(const X86::Instruction&) override;
    virtual void MOV_reg8_RM8(const X86::Instruction&) override;
    virtual void MOV_reg8_imm8(const X86::Instruction&) override;
    virtual void MOV_seg_RM16(const X86::Instruction&) override;
    virtual void MOV_seg_RM32(const X86::Instruction&) override;
    virtual void MUL_RM16(const X86::Instruction&) override;
    virtual void MUL_RM32(const X86::Instruction&) override;
    virtual void MUL_RM8(const X86::Instruction&) override;
    virtual void NEG_RM16(const X86::Instruction&) override;
    virtual void NEG_RM32(const X86::Instruction&) override;
    virtual void NEG_RM8(const X86::Instruction&) override;
    virtual void NOP(const X86::Instruction&) override;
    virtual void NOT_RM16(const X86::Instruction&) override;
    virtual void NOT_RM32(const X86::Instruction&) override;
    virtual void NOT_RM8(const X86::Instruction&) override;
    virtual void OR_AL_imm8(const X86::Instruction&) override;
    virtual void OR_AX_imm16(const X86::Instruction&) override;
    virtual void OR_EAX_imm32(const X86::Instruction&) override;
    virtual void OR_RM16_imm16(const X86::Instruction&) override;
    virtual void OR_RM16_imm8(const X86::Instruction&) override;
    virtual void OR_RM16_reg16(const X86::Instruction&) override;
    virtual void OR_RM32_imm32(const X86::Instruction&) override;
    virtual void OR_RM32_imm8(const X86::Instruction&) override;
    virtual void OR_RM32_reg32(const X86::Instruction&) override;
    virtual void OR_RM8_imm8(const X86::Instruction&) override;
    virtual void OR_RM8_reg8(const X86::Instruction&) override;
    virtual void OR_reg16_RM16(const X86::Instruction&) override;
    virtual void OR_reg32_RM32(const X86::Instruction&) override;
    virtual void OR_reg8_RM8(const X86::Instruction&) override;
    virtual void OUTSB(const X86::Instruction&) override;
    virtual void OUTSD(const X86::Instruction&) override;
    virtual void OUTSW(const X86::Instruction&) override;
    virtual void OUT_DX_AL(const X86::Instruction&) override;
    virtual void OUT_DX_AX(const X86::Instruction&) override;
    virtual void OUT_DX_EAX(const X86::Instruction&) override;
    virtual void OUT_imm8_AL(const X86::Instruction&) override;
    virtual void OUT_imm8_AX(const X86::Instruction&) override;
    virtual void OUT_imm8_EAX(const X86::Instruction&) override;
    virtual void PACKSSDW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PACKSSWB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PACKUSWB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDSB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDSW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDUSB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PADDUSW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PAND_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PANDN_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PCMPEQB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PCMPEQW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PCMPEQD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PCMPGTB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PCMPGTW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PCMPGTD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PMADDWD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PMULHW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PMULLW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void POPA(const X86::Instruction&) override;
    virtual void POPAD(const X86::Instruction&) override;
    virtual void POPF(const X86::Instruction&) override;
    virtual void POPFD(const X86::Instruction&) override;
    virtual void POP_DS(const X86::Instruction&) override;
    virtual void POP_ES(const X86::Instruction&) override;
    virtual void POP_FS(const X86::Instruction&) override;
    virtual void POP_GS(const X86::Instruction&) override;
    virtual void POP_RM16(const X86::Instruction&) override;
    virtual void POP_RM32(const X86::Instruction&) override;
    virtual void POP_SS(const X86::Instruction&) override;
    virtual void POP_reg16(const X86::Instruction&) override;
    virtual void POP_reg32(const X86::Instruction&) override;
    virtual void POR_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSLLW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSLLW_mm1_imm8(const X86::Instruction&) override;
    virtual void PSLLD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSLLD_mm1_imm8(const X86::Instruction&) override;
    virtual void PSLLQ_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSLLQ_mm1_imm8(const X86::Instruction&) override;
    virtual void PSRAW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSRAW_mm1_imm8(const X86::Instruction&) override;
    virtual void PSRAD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSRAD_mm1_imm8(const X86::Instruction&) override;
    virtual void PSRLW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSRLW_mm1_imm8(const X86::Instruction&) override;
    virtual void PSRLD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSRLD_mm1_imm8(const X86::Instruction&) override;
    virtual void PSRLQ_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSRLQ_mm1_imm8(const X86::Instruction&) override;
    virtual void PSUBB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSUBW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSUBD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSUBSB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSUBSW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSUBUSB_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PSUBUSW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PUNPCKHBW_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PUNPCKHWD_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PUNPCKHDQ_mm1_mm2m64(const X86::Instruction&) override;
    virtual void PUNPCKLBW_mm1_mm2m32(const X86::Instruction&) override;
    virtual void PUNPCKLWD_mm1_mm2m32(const X86::Instruction&) override;
    virtual void PUNPCKLDQ_mm1_mm2m32(const X86::Instruction&) override;
    virtual void PUSHA(const X86::Instruction&) override;
    virtual void PUSHAD(const X86::Instruction&) override;
    virtual void PUSHF(const X86::Instruction&) override;
    virtual void PUSHFD(const X86::Instruction&) override;
    virtual void PUSH_CS(const X86::Instruction&) override;
    virtual void PUSH_DS(const X86::Instruction&) override;
    virtual void PUSH_ES(const X86::Instruction&) override;
    virtual void PUSH_FS(const X86::Instruction&) override;
    virtual void PUSH_GS(const X86::Instruction&) override;
    virtual void PUSH_RM16(const X86::Instruction&) override;
    virtual void PUSH_RM32(const X86::Instruction&) override;
    virtual void PUSH_SP_8086_80186(const X86::Instruction&) override;
    virtual void PUSH_SS(const X86::Instruction&) override;
    virtual void PUSH_imm16(const X86::Instruction&) override;
    virtual void PUSH_imm32(const X86::Instruction&) override;
    virtual void PUSH_imm8(const X86::Instruction&) override;
    virtual void PUSH_reg16(const X86::Instruction&) override;
    virtual void PUSH_reg32(const X86::Instruction&) override;
    virtual void PXOR_mm1_mm2m64(const X86::Instruction&) override;
    virtual void RCL_RM16_1(const X86::Instruction&) override;
    virtual void RCL_RM16_CL(const X86::Instruction&) override;
    virtual void RCL_RM16_imm8(const X86::Instruction&) override;
    virtual void RCL_RM32_1(const X86::Instruction&) override;
    virtual void RCL_RM32_CL(const X86::Instruction&) override;
    virtual void RCL_RM32_imm8(const X86::Instruction&) override;
    virtual void RCL_RM8_1(const X86::Instruction&) override;
    virtual void RCL_RM8_CL(const X86::Instruction&) override;
    virtual void RCL_RM8_imm8(const X86::Instruction&) override;
    virtual void RCR_RM16_1(const X86::Instruction&) override;
    virtual void RCR_RM16_CL(const X86::Instruction&) override;
    virtual void RCR_RM16_imm8(const X86::Instruction&) override;
    virtual void RCR_RM32_1(const X86::Instruction&) override;
    virtual void RCR_RM32_CL(const X86::Instruction&) override;
    virtual void RCR_RM32_imm8(const X86::Instruction&) override;
    virtual void RCR_RM8_1(const X86::Instruction&) override;
    virtual void RCR_RM8_CL(const X86::Instruction&) override;
    virtual void RCR_RM8_imm8(const X86::Instruction&) override;
    virtual void RDTSC(const X86::Instruction&) override;
    virtual void RET(const X86::Instruction&) override;
    virtual void RETF(const X86::Instruction&) override;
    virtual void RETF_imm16(const X86::Instruction&) override;
    virtual void RET_imm16(const X86::Instruction&) override;
    virtual void ROL_RM16_1(const X86::Instruction&) override;
    virtual void ROL_RM16_CL(const X86::Instruction&) override;
    virtual void ROL_RM16_imm8(const X86::Instruction&) override;
    virtual void ROL_RM32_1(const X86::Instruction&) override;
    virtual void ROL_RM32_CL(const X86::Instruction&) override;
    virtual void ROL_RM32_imm8(const X86::Instruction&) override;
    virtual void ROL_RM8_1(const X86::Instruction&) override;
    virtual void ROL_RM8_CL(const X86::Instruction&) override;
    virtual void ROL_RM8_imm8(const X86::Instruction&) override;
    virtual void ROR_RM16_1(const X86::Instruction&) override;
    virtual void ROR_RM16_CL(const X86::Instruction&) override;
    virtual void ROR_RM16_imm8(const X86::Instruction&) override;
    virtual void ROR_RM32_1(const X86::Instruction&) override;
    virtual void ROR_RM32_CL(const X86::Instruction&) override;
    virtual void ROR_RM32_imm8(const X86::Instruction&) override;
    virtual void ROR_RM8_1(const X86::Instruction&) override;
    virtual void ROR_RM8_CL(const X86::Instruction&) override;
    virtual void ROR_RM8_imm8(const X86::Instruction&) override;
    virtual void SAHF(const X86::Instruction&) override;
    virtual void SALC(const X86::Instruction&) override;
    virtual void SAR_RM16_1(const X86::Instruction&) override;
    virtual void SAR_RM16_CL(const X86::Instruction&) override;
    virtual void SAR_RM16_imm8(const X86::Instruction&) override;
    virtual void SAR_RM32_1(const X86::Instruction&) override;
    virtual void SAR_RM32_CL(const X86::Instruction&) override;
    virtual void SAR_RM32_imm8(const X86::Instruction&) override;
    virtual void SAR_RM8_1(const X86::Instruction&) override;
    virtual void SAR_RM8_CL(const X86::Instruction&) override;
    virtual void SAR_RM8_imm8(const X86::Instruction&) override;
    virtual void SBB_AL_imm8(const X86::Instruction&) override;
    virtual void SBB_AX_imm16(const X86::Instruction&) override;
    virtual void SBB_EAX_imm32(const X86::Instruction&) override;
    virtual void SBB_RM16_imm16(const X86::Instruction&) override;
    virtual void SBB_RM16_imm8(const X86::Instruction&) override;
    virtual void SBB_RM16_reg16(const X86::Instruction&) override;
    virtual void SBB_RM32_imm32(const X86::Instruction&) override;
    virtual void SBB_RM32_imm8(const X86::Instruction&) override;
    virtual void SBB_RM32_reg32(const X86::Instruction&) override;
    virtual void SBB_RM8_imm8(const X86::Instruction&) override;
    virtual void SBB_RM8_reg8(const X86::Instruction&) override;
    virtual void SBB_reg16_RM16(const X86::Instruction&) override;
    virtual void SBB_reg32_RM32(const X86::Instruction&) override;
    virtual void SBB_reg8_RM8(const X86::Instruction&) override;
    virtual void SCASB(const X86::Instruction&) override;
    virtual void SCASD(const X86::Instruction&) override;
    virtual void SCASW(const X86::Instruction&) override;
    virtual void SETcc_RM8(const X86::Instruction&) override;
    virtual void SGDT(const X86::Instruction&) override;
    virtual void SHLD_RM16_reg16_CL(const X86::Instruction&) override;
    virtual void SHLD_RM16_reg16_imm8(const X86::Instruction&) override;
    virtual void SHLD_RM32_reg32_CL(const X86::Instruction&) override;
    virtual void SHLD_RM32_reg32_imm8(const X86::Instruction&) override;
    virtual void SHL_RM16_1(const X86::Instruction&) override;
    virtual void SHL_RM16_CL(const X86::Instruction&) override;
    virtual void SHL_RM16_imm8(const X86::Instruction&) override;
    virtual void SHL_RM32_1(const X86::Instruction&) override;
    virtual void SHL_RM32_CL(const X86::Instruction&) override;
    virtual void SHL_RM32_imm8(const X86::Instruction&) override;
    virtual void SHL_RM8_1(const X86::Instruction&) override;
    virtual void SHL_RM8_CL(const X86::Instruction&) override;
    virtual void SHL_RM8_imm8(const X86::Instruction&) override;
    virtual void SHRD_RM16_reg16_CL(const X86::Instruction&) override;
    virtual void SHRD_RM16_reg16_imm8(const X86::Instruction&) override;
    virtual void SHRD_RM32_reg32_CL(const X86::Instruction&) override;
    virtual void SHRD_RM32_reg32_imm8(const X86::Instruction&) override;
    virtual void SHR_RM16_1(const X86::Instruction&) override;
    virtual void SHR_RM16_CL(const X86::Instruction&) override;
    virtual void SHR_RM16_imm8(const X86::Instruction&) override;
    virtual void SHR_RM32_1(const X86::Instruction&) override;
    virtual void SHR_RM32_CL(const X86::Instruction&) override;
    virtual void SHR_RM32_imm8(const X86::Instruction&) override;
    virtual void SHR_RM8_1(const X86::Instruction&) override;
    virtual void SHR_RM8_CL(const X86::Instruction&) override;
    virtual void SHR_RM8_imm8(const X86::Instruction&) override;
    virtual void SIDT(const X86::Instruction&) override;
    virtual void SLDT_RM16(const X86::Instruction&) override;
    virtual void SMSW_RM16(const X86::Instruction&) override;
    virtual void STC(const X86::Instruction&) override;
    virtual void STD(const X86::Instruction&) override;
    virtual void STI(const X86::Instruction&) override;
    virtual void STOSB(const X86::Instruction&) override;
    virtual void STOSD(const X86::Instruction&) override;
    virtual void STOSW(const X86::Instruction&) override;
    virtual void STR_RM16(const X86::Instruction&) override;
    virtual void SUB_AL_imm8(const X86::Instruction&) override;
    virtual void SUB_AX_imm16(const X86::Instruction&) override;
    virtual void SUB_EAX_imm32(const X86::Instruction&) override;
    virtual void SUB_RM16_imm16(const X86::Instruction&) override;
    virtual void SUB_RM16_imm8(const X86::Instruction&) override;
    virtual void SUB_RM16_reg16(const X86::Instruction&) override;
    virtual void SUB_RM32_imm32(const X86::Instruction&) override;
    virtual void SUB_RM32_imm8(const X86::Instruction&) override;
    virtual void SUB_RM32_reg32(const X86::Instruction&) override;
    virtual void SUB_RM8_imm8(const X86::Instruction&) override;
    virtual void SUB_RM8_reg8(const X86::Instruction&) override;
    virtual void SUB_reg16_RM16(const X86::Instruction&) override;
    virtual void SUB_reg32_RM32(const X86::Instruction&) override;
    virtual void SUB_reg8_RM8(const X86::Instruction&) override;
    virtual void TEST_AL_imm8(const X86::Instruction&) override;
    virtual void TEST_AX_imm16(const X86::Instruction&) override;
    virtual void TEST_EAX_imm32(const X86::Instruction&) override;
    virtual void TEST_RM16_imm16(const X86::Instruction&) override;
    virtual void TEST_RM16_reg16(const X86::Instruction&) override;
    virtual void TEST_RM32_imm32(const X86::Instruction&) override;
    virtual void TEST_RM32_reg32(const X86::Instruction&) override;
    virtual void TEST_RM8_imm8(const X86::Instruction&) override;
    virtual void TEST_RM8_reg8(const X86::Instruction&) override;
    virtual void UD0(const X86::Instruction&) override;
    virtual void UD1(const X86::Instruction&) override;
    virtual void UD2(const X86::Instruction&) override;
    virtual void VERR_RM16(const X86::Instruction&) override;
    virtual void VERW_RM16(const X86::Instruction&) override;
    virtual void WAIT(const X86::Instruction&) override;
    virtual void WBINVD(const X86::Instruction&) override;
    virtual void XADD_RM16_reg16(const X86::Instruction&) override;
    virtual void XADD_RM32_reg32(const X86::Instruction&) override;
    virtual void XADD_RM8_reg8(const X86::Instruction&) override;
    virtual void XCHG_AX_reg16(const X86::Instruction&) override;
    virtual void XCHG_EAX_reg32(const X86::Instruction&) override;
    virtual void XCHG_reg16_RM16(const X86::Instruction&) override;
    virtual void XCHG_reg32_RM32(const X86::Instruction&) override;
    virtual void XCHG_reg8_RM8(const X86::Instruction&) override;
    virtual void XLAT(const X86::Instruction&) override;
    virtual void XOR_AL_imm8(const X86::Instruction&) override;
    virtual void XOR_AX_imm16(const X86::Instruction&) override;
    virtual void XOR_EAX_imm32(const X86::Instruction&) override;
    virtual void XOR_RM16_imm16(const X86::Instruction&) override;
    virtual void XOR_RM16_imm8(const X86::Instruction&) override;
    virtual void XOR_RM16_reg16(const X86::Instruction&) override;
    virtual void XOR_RM32_imm32(const X86::Instruction&) override;
    virtual void XOR_RM32_imm8(const X86::Instruction&) override;
    virtual void XOR_RM32_reg32(const X86::Instruction&) override;
    virtual void XOR_RM8_imm8(const X86::Instruction&) override;
    virtual void XOR_RM8_reg8(const X86::Instruction&) override;
    virtual void XOR_reg16_RM16(const X86::Instruction&) override;
    virtual void XOR_reg32_RM32(const X86::Instruction&) override;
    virtual void XOR_reg8_RM8(const X86::Instruction&) override;
    virtual void MOVQ_mm1_mm2m64(const X86::Instruction&) override;
    virtual void MOVQ_mm1m64_mm2(const X86::Instruction&) override;
    virtual void MOVD_mm1_rm32(const X86::Instruction&) override;
    virtual void MOVQ_mm1_rm64(const X86::Instruction&) override; // long mode
    virtual void MOVD_rm32_mm2(const X86::Instruction&) override;
    virtual void MOVQ_rm64_mm2(const X86::Instruction&) override; // long mode
    virtual void EMMS(const X86::Instruction&) override;

    virtual void CMPXCHG8B_m64(X86::Instruction const&) override;
    virtual void RDRAND_reg(X86::Instruction const&) override;
    virtual void RDSEED_reg(X86::Instruction const&) override;

    virtual void PREFETCHTNTA(X86::Instruction const&) override;
    virtual void PREFETCHT0(X86::Instruction const&) override;
    virtual void PREFETCHT1(X86::Instruction const&) override;
    virtual void PREFETCHT2(X86::Instruction const&) override;
    virtual void LDMXCSR(X86::Instruction const&) override;
    virtual void STMXCSR(X86::Instruction const&) override;
    virtual void MOVUPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MOVUPS_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void MOVSS_xmm1m32_xmm2(X86::Instruction const&) override;
    virtual void MOVLPS_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void MOVLPS_m64_xmm2(X86::Instruction const&) override;
    virtual void UNPCKLPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void UNPCKHPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVHPS_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void MOVHPS_m64_xmm2(X86::Instruction const&) override;
    virtual void MOVAPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVAPS_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void CVTPI2PS_xmm1_mm2m64(X86::Instruction const&) override;
    virtual void CVTSI2SS_xmm1_rm32(X86::Instruction const&) override;
    virtual void MOVNTPS_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void CVTTPS2PI_mm1_xmm2m64(X86::Instruction const&) override;
    virtual void CVTTSS2SI_r32_xmm2m32(X86::Instruction const&) override;
    virtual void CVTPS2PI_xmm1_mm2m64(X86::Instruction const&) override;
    virtual void CVTSS2SI_r32_xmm2m32(X86::Instruction const&) override;
    virtual void UCOMISS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void COMISS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MOVMSKPS_reg_xmm(X86::Instruction const&) override;
    virtual void SQRTPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void SQRTSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void RSQRTPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void RSQRTSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void RCPPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void RCPSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void ANDPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ANDNPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ORPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void XORPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ADDPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ADDSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MULPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MULSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void SUBPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void SUBSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MINPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MINSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void DIVPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void DIVSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MAXPS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MAXSS_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void PSHUFW_mm1_mm2m64_imm8(X86::Instruction const&) override;
    virtual void CMPPS_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void CMPSS_xmm1_xmm2m32_imm8(X86::Instruction const&) override;
    virtual void PINSRW_mm1_r32m16_imm8(X86::Instruction const&) override;
    virtual void PINSRW_xmm1_r32m16_imm8(X86::Instruction const&) override;
    virtual void PEXTRW_reg_mm1_imm8(X86::Instruction const&) override;
    virtual void PEXTRW_reg_xmm1_imm8(X86::Instruction const&) override;
    virtual void SHUFPS_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void PMOVMSKB_reg_mm1(X86::Instruction const&) override;
    virtual void PMOVMSKB_reg_xmm1(X86::Instruction const&) override;
    virtual void PMINUB_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PMINUB_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PMAXUB_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PMAXUB_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PAVGB_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PAVGB_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PAVGW_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PAVGW_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PMULHUW_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PMULHUW_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void MOVNTQ_m64_mm1(X86::Instruction const&) override;
    virtual void PMINSB_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PMINSB_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PMAXSB_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PMAXSB_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PSADBB_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PSADBB_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MASKMOVQ_mm1_mm2m64(X86::Instruction const&) override;

    virtual void MOVUPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MOVUPD_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void MOVSD_xmm1m32_xmm2(X86::Instruction const&) override;
    virtual void MOVLPD_xmm1_m64(X86::Instruction const&) override;
    virtual void MOVLPD_m64_xmm2(X86::Instruction const&) override;
    virtual void UNPCKLPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void UNPCKHPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVHPD_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void MOVAPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVAPD_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void CVTPI2PD_xmm1_mm2m64(X86::Instruction const&) override;
    virtual void CVTSI2SD_xmm1_rm32(X86::Instruction const&) override;
    virtual void CVTTPD2PI_mm1_xmm2m128(X86::Instruction const&) override;
    virtual void CVTTSS2SI_r32_xmm2m64(X86::Instruction const&) override;
    virtual void CVTPD2PI_xmm1_mm2m128(X86::Instruction const&) override;
    virtual void CVTSD2SI_xmm1_rm64(X86::Instruction const&) override;
    virtual void UCOMISD_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void COMISD_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void MOVMSKPD_reg_xmm(X86::Instruction const&) override;
    virtual void SQRTPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void SQRTSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void ANDPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ANDNPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ORPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void XORPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ADDPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void ADDSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MULPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MULSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void CVTPS2PD_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void CVTPD2PS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void CVTSS2SD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void CVTSD2SS_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void CVTDQ2PS_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void CVTPS2DQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void CVTTPS2DQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void SUBPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void SUBSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MINPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MINSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void DIVPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void DIVSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void MAXPD_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MAXSD_xmm1_xmm2m32(X86::Instruction const&) override;
    virtual void PUNPCKLQDQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PUNPCKHQDQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVDQA_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVDQU_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void PSHUFD_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void PSHUFHW_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void PSHUFLW_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void PSRLQ_xmm1_imm8(X86::Instruction const&) override;
    virtual void PSRLDQ_xmm1_imm8(X86::Instruction const&) override;
    virtual void PSLLQ_xmm1_imm8(X86::Instruction const&) override;
    virtual void PSLLDQ_xmm1_imm8(X86::Instruction const&) override;
    virtual void MOVD_rm32_xmm2(X86::Instruction const&) override;
    virtual void MOVQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void MOVDQA_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void MOVDQU_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void CMPPD_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void CMPSD_xmm1_xmm2m32_imm8(X86::Instruction const&) override;
    virtual void SHUFPD_xmm1_xmm2m128_imm8(X86::Instruction const&) override;
    virtual void PADDQ_mm1_mm2m64(X86::Instruction const&) override;
    virtual void MOVQ_xmm1m128_xmm2(X86::Instruction const&) override;
    virtual void MOVQ2DQ_xmm_mm(X86::Instruction const&) override;
    virtual void MOVDQ2Q_mm_xmm(X86::Instruction const&) override;
    virtual void CVTTPD2DQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void CVTPD2DQ_xmm1_xmm2m128(X86::Instruction const&) override;
    virtual void CVTDQ2PD_xmm1_xmm2m64(X86::Instruction const&) override;
    virtual void PMULUDQ_mm1_mm2m64(X86::Instruction const&) override;
    virtual void PMULUDQ_mm1_mm2m128(X86::Instruction const&) override;
    virtual void PSUBQ_mm1_mm2m64(X86::Instruction const&) override;

    virtual void wrap_0xC0(const X86::Instruction&) override;
    virtual void wrap_0xC1_16(const X86::Instruction&) override;
    virtual void wrap_0xC1_32(const X86::Instruction&) override;
    virtual void wrap_0xD0(const X86::Instruction&) override;
    virtual void wrap_0xD1_16(const X86::Instruction&) override;
    virtual void wrap_0xD1_32(const X86::Instruction&) override;
    virtual void wrap_0xD2(const X86::Instruction&) override;
    virtual void wrap_0xD3_16(const X86::Instruction&) override;
    virtual void wrap_0xD3_32(const X86::Instruction&) override;

    template<bool update_dest, bool is_or, typename Op>
    void generic_AL_imm8(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_AX_imm16(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_EAX_imm32(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_RM16_imm16(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_RM16_imm8(Op, const X86::Instruction&);
    template<bool update_dest, typename Op>
    void generic_RM16_unsigned_imm8(Op, const X86::Instruction&);
    template<bool update_dest, bool is_zero_idiom_if_both_operands_same, typename Op>
    void generic_RM16_reg16(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_RM32_imm32(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_RM32_imm8(Op, const X86::Instruction&);
    template<bool update_dest, typename Op>
    void generic_RM32_unsigned_imm8(Op, const X86::Instruction&);
    template<bool update_dest, bool is_zero_idiom_if_both_operands_same, typename Op>
    void generic_RM32_reg32(Op, const X86::Instruction&);
    template<bool update_dest, bool is_or, typename Op>
    void generic_RM8_imm8(Op, const X86::Instruction&);
    template<bool update_dest, bool is_zero_idiom_if_both_operands_same, typename Op>
    void generic_RM8_reg8(Op, const X86::Instruction&);
    template<bool update_dest, bool is_zero_idiom_if_both_operands_same, typename Op>
    void generic_reg16_RM16(Op, const X86::Instruction&);
    template<bool update_dest, bool is_zero_idiom_if_both_operands_same, typename Op>
    void generic_reg32_RM32(Op, const X86::Instruction&);
    template<bool update_dest, bool is_zero_idiom_if_both_operands_same, typename Op>
    void generic_reg8_RM8(Op, const X86::Instruction&);

    template<typename Op>
    void generic_RM8_1(Op, const X86::Instruction&);
    template<typename Op>
    void generic_RM8_CL(Op, const X86::Instruction&);
    template<typename Op>
    void generic_RM16_1(Op, const X86::Instruction&);
    template<typename Op>
    void generic_RM16_CL(Op, const X86::Instruction&);
    template<typename Op>
    void generic_RM32_1(Op, const X86::Instruction&);
    template<typename Op>
    void generic_RM32_CL(Op, const X86::Instruction&);

    void update_code_cache();

    Emulator& m_emulator;
    SoftFPU m_fpu;
    SoftVPU m_vpu;

    ValueWithShadow<PartAddressableRegister> m_gpr[8];

    u16 m_segment[8] { 0 };
    u32 m_eflags { 0 };

    bool m_flags_tainted { false };

    u32 m_eip { 0 };
    u32 m_base_eip { 0 };

    Region* m_cached_code_region { nullptr };
    u8* m_cached_code_base_ptr { nullptr };
};

ALWAYS_INLINE u8 SoftCPU::read8()
{
    if (!m_cached_code_region || !m_cached_code_region->contains(m_eip))
        update_code_cache();

    u8 value = m_cached_code_base_ptr[m_eip - m_cached_code_region->base()];
    m_eip += 1;
    return value;
}

ALWAYS_INLINE u16 SoftCPU::read16()
{
    if (!m_cached_code_region || !m_cached_code_region->contains(m_eip))
        update_code_cache();

    u16 value;
    ByteReader::load<u16>(&m_cached_code_base_ptr[m_eip - m_cached_code_region->base()], value);
    m_eip += 2;
    return value;
}

ALWAYS_INLINE u32 SoftCPU::read32()
{
    if (!m_cached_code_region || !m_cached_code_region->contains(m_eip))
        update_code_cache();

    u32 value;
    ByteReader::load<u32>(&m_cached_code_base_ptr[m_eip - m_cached_code_region->base()], value);

    m_eip += 4;
    return value;
}

ALWAYS_INLINE u64 SoftCPU::read64()
{
    if (!m_cached_code_region || !m_cached_code_region->contains(m_eip))
        update_code_cache();

    u64 value;
    ByteReader::load<u64>(&m_cached_code_base_ptr[m_eip - m_cached_code_region->base()], value);

    m_eip += 8;
    return value;
}

}
