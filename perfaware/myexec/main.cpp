#include <cassert>
#include <cstdio>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../sim86/sim86_lib.h"
#include "text.cpp"

struct memory_t {
    u8* Memory;
    u32 Size;
};

static u8 ReadMemory(memory_t *Memory, u32 AbsoluteAddress)
{
    assert(AbsoluteAddress < Memory->Size);
    u8 Result = Memory->Memory[AbsoluteAddress];
    return Result;
}

static u32 ReadMemory4(memory_t *Memory, u32 AbsoluteAddress, u32 Count)
{
    assert(Count <= sizeof(u32));
    u32 slot = 0x0;
    for (u32 i = 0; i < Count; i++) {
        slot |= (u32)ReadMemory(Memory, AbsoluteAddress + i) << (8 * i);
    }
    return slot;
}

static void SetMemory(memory_t *Memory, u32 AbsoluteAddress, u8* Bytes, u32 Count)
{
    assert(AbsoluteAddress + Count < Memory->Size);
    for (u32 i = AbsoluteAddress, j = 0; i < AbsoluteAddress + Count; i++, j++) {
        Memory->Memory[i] = Bytes[j];
    }
}

static void MoveMemory(memory_t *Memory, u32 AbsoluteAddress, u32 AbsoluteFromAddress, u32 Count)
{
    assert(AbsoluteFromAddress + Count < Memory->Size);
    SetMemory(Memory, AbsoluteAddress, Memory->Memory + AbsoluteFromAddress, Count);
}

enum register_index
{
    Register_none,
    
    Register_a,
    Register_b,
    Register_c,
    Register_d,
    Register_sp,
    Register_bp,
    Register_si,
    Register_di,
    Register_es,
    Register_cs,
    Register_ss,
    Register_ds,
    Register_ip,
    Register_flags,
    
    Register_count,
};

static char const *GetRegNameWide(register_access Reg)
{
    Reg.Count = 2;
    Reg.Offset = 0;
    return GetRegName(Reg);
}

static u32 RegisterMemoryOffset(register_access RegAccess) {
    return (RegAccess.Index - 1) * 2 + RegAccess.Offset;
}

static u32 GetRegisterValue(memory_t *Memory, register_access RegAccess) {
    return ReadMemory4(Memory, RegisterMemoryOffset(RegAccess), RegAccess.Count);
}

static void SetRegisterValue(memory_t *Memory, register_access RegAccess, u32 Value) {
    u32 RegisterOffset = RegisterMemoryOffset(RegAccess);
    SetMemory(Memory, RegisterOffset, (u8*)&Value, RegAccess.Count);
}

static u32 GetRegisterValueWide(memory_t *Memory, register_access RegAccess) {
    RegAccess.Count = 2;
    RegAccess.Offset = 0;
    return GetRegisterValue(Memory, RegAccess);
}

static void PrintFinalRegisters(memory_t *Memory, FILE *Dest) {
    fprintf(Dest, "Final registers:\n");
    for (u32 i = 0; i < Register_count; i++) {
        register_access RegAccess = {.Index = (register_index)(i + 1), .Count = 2};
        u32 RegValue = GetRegisterValue(Memory, RegAccess);
        fprintf(Dest, "\t%s: 0x%04X (%u)\n", GetRegName(RegAccess), RegValue, RegValue);
    }
}
bool MathOp(u32 Lhs, u32 Rhs, u32* Result, operation_type Op) {
    switch (Op) {
        case Op_add:
            *Result = Lhs + Rhs;
            return true;
        case Op_sub:
            *Result = Lhs - Rhs;
            return true;
        case Op_cmp:
            *Result = Lhs - Rhs;
            return false;
    }
    assert(false);
    return false;
}

u32 GetOperandValue(memory_t* Memory, instruction_operand Operand) {

    if (Operand.Type == Operand_Immediate) {
        return Operand.Immediate.Value;
    }
    if (Operand.Type == Operand_Register) {
        return GetRegisterValue(Memory, Operand.Register);
    }
    assert(false );
    return 0;

}

enum Register_Flag {
    RGF_None = 0, 
    RGF_Zero = 0x1,
    RGF_Sign = 0x2,
};

static bool SetRegisterFlag(memory_t *Memory, Register_Flag Flag, bool Value) {

    register_access Reg {
        .Index = Register_flags, .Count = 2 };
    u32 Flags = GetRegisterValue(Memory, Reg);
    u32 Cpy = Flags;
    Flags = (Flags & ~Flag) | (Value ? Flag : 0);
    SetRegisterValue(Memory, Reg, Flags);
    return Cpy != Flags;
}

static Register_Flag GetRegisterFlag(memory_t *Memory) {

    register_access Reg {
        .Index = Register_flags, .Count = 2 };
    return (Register_Flag)GetRegisterValue(Memory, Reg);
}

static void PrintRegisterFlag_Mnemonic(Register_Flag Flag, FILE* Dest) {
    if (Flag & RGF_Zero) {
        fprintf(Dest, "Z");
    }
    if (Flag & RGF_Sign) {
        fprintf(Dest, "S");
    }
}
    

static void PrintRegister(instruction Instruction, FILE *Dest, memory_t *Memory) {
    instruction_operand op1 = Instruction.Operands[0];
    instruction_operand op2 = Instruction.Operands[1];
    switch (Instruction.Op) {
        case Op_mov:
        {
            if (op1.Type == Operand_Register) {
                u32 ValueBefore = GetRegisterValueWide(Memory, op1.Register);
                u32 Rhs = GetOperandValue(Memory, op2);
                SetRegisterValue(Memory, op1.Register, Rhs);
                u32 ValueAfter = GetRegisterValueWide(Memory, op1.Register);
                fprintf(Dest, "; %s:0x%X->0x%X", GetRegNameWide(op1.Register), ValueBefore, ValueAfter);
            }
            break;
        }
        case Op_add:
        case Op_sub:
        case Op_cmp:
        {
            if (op1.Type == Operand_Register) {
                u32 ValueBefore = GetRegisterValue(Memory, op1.Register);
                u32 Rhs = GetOperandValue(Memory, op2);
                u32 Result = 0x0;
                bool change = MathOp(ValueBefore, Rhs, &Result, Instruction.Op);
                if (change) {
                    // just doit |= RGF_Zero and etc
                    SetRegisterFlag(Memory, RGF_Zero, Result == 0x0);
                    SetRegisterFlag(Memory, RGF_Sign, Result > 0);
                    SetRegisterValue(Memory, op1.Register, Result);
                }
                u32 ValueAfter = GetRegisterValueWide(Memory, op1.Register);
                fprintf(Dest, "; %s:0x%X->0x%X", GetRegNameWide(op1.Register), ValueBefore, ValueAfter);
            }
            break;
        }
        default:
            fprintf(Dest, "unsupported!!!");

    }
    fprintf(Dest, " ");
    PrintRegisterFlag_Mnemonic(GetRegisterFlag(Memory), Dest);
}

static u32 LoadMemoryFromFile(char *FileName, memory_t* Memory)
{
    u32 Result = 0;
    
    FILE *File = fopen(FileName, "rb");
    if(File)
    {
        Result = fread(Memory->Memory, 1, Memory->Size, File);
        fclose(File);
    }
    else
    {
        fprintf(stderr, "ERROR: Unable to open %s.\n", FileName);
    }
    
    return Result;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }
    
    memory_t Memory = { .Memory = (u8*)malloc(1024 * 1024), .Size = 1024 * 1024 };
    memset(Memory.Memory, 0, Memory.Size);

    memory_t Registers = { .Memory = (u8*)malloc(Register_count * 2), .Size = Register_count * 2 };
    memset(Registers.Memory, 0, Registers.Size);
    instruction_table Table;
    Sim86_Get8086InstructionTable(&Table);

    for (int i = 1; i < argc; i++) {
        u32 BytesRead = LoadMemoryFromFile(argv[i], &Memory);

        u32 Offset = 0;
        while(Offset < BytesRead)
        {
            instruction Decoded;
            Sim86_Decode8086Instruction(BytesRead - Offset, Memory.Memory + Offset, &Decoded);
            if(Decoded.Op)
            {
                Offset += Decoded.Size;
                PrintInstruction(Decoded, stdout);
                PrintRegister(Decoded, stdout, &Registers);
                printf("\n");
            }
            else
            {
                printf("Unrecognized instruction\n");
                break;
            }
        }

        PrintFinalRegisters(&Registers, stdout);
    }

    return 0;
}