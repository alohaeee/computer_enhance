static char const *GetMnemonic(operation_type Op)
{
    char const *Result = "";
    if(Op < Op_Count)
    {
        Result = Sim86_MnemonicFromOperationType(Op);
    }
    
    return Result;
}

static char const *GetRegName(register_access Reg)
{
    char const *Result = Sim86_RegisterNameFromOperand(&Reg);
    return Result;
}

static void PrintEffectiveAddressExpression(effective_address_expression Address, FILE *Dest)
{
    char const *Separator = "";
    for(u32 Index = 0; Index < ArrayCount(Address.Terms); ++Index)
    {
        effective_address_term Term = Address.Terms[Index];
        register_access Reg = Term.Register;
        
        if(Reg.Index)
        {
            fprintf(Dest, "%s", Separator);
            if(Term.Scale != 1)
            {
                fprintf(Dest, "%d*", Term.Scale);
            }
            fprintf(Dest, "%s", GetRegName(Reg));
            Separator = "+";
        }
    }
    
    if(Address.Displacement != 0)
    {
        fprintf(Dest, "%+d", Address.Displacement);
    }
}

static void PrintInstruction(instruction Instruction, FILE *Dest)
{
    u32 Flags = Instruction.Flags;
    u32 W = Flags & Inst_Wide;
    
    if(Flags & Inst_Lock)
    {
        if(Instruction.Op == Op_xchg)
        {
            // NOTE(casey): This is just a stupidity for matching assembler expectations.
            instruction_operand Temp = Instruction.Operands[0];
            Instruction.Operands[0] = Instruction.Operands[1];
            Instruction.Operands[1] = Temp;
        }
        fprintf(Dest, "lock ");
    }
    
    char const *MnemonicSuffix = "";
    if(Flags & Inst_Rep)
    {
        fprintf(Dest, "rep ");
        MnemonicSuffix = W ? "w" : "b";
    }
    
    fprintf(Dest, "%s%s ", GetMnemonic(Instruction.Op), MnemonicSuffix);
    
    char const *Separator = "";
    for(u32 OperandIndex = 0; OperandIndex < ArrayCount(Instruction.Operands); ++OperandIndex)
    {
        instruction_operand Operand = Instruction.Operands[OperandIndex];
        if(Operand.Type != Operand_None)
        {
            fprintf(Dest, "%s", Separator);
            Separator = ", ";
            
            switch(Operand.Type)
            {
                case Operand_None: {} break;
                
                case Operand_Register:
                {
                    fprintf(Dest, "%s", GetRegName(Operand.Register));
                } break;
                
                case Operand_Memory:
                {
                    effective_address_expression Address = Operand.Address;

                    if(Flags & Inst_Far)
                    {
                        fprintf(Dest, "far ");
                    }
                    
                    if(Address.Flags & Address_ExplicitSegment)
                    {
                        fprintf(Dest, "%u:%u", Address.ExplicitSegment, Address.Displacement);
                    }
                    else
                    {
                        if(Instruction.Operands[0].Type != Operand_Register)
                        {
                            fprintf(Dest, "%s ", W ? "word" : "byte");
                        }
                        
                        if(Flags & Inst_Segment)
                        {
                            fprintf(Dest, "%s:", GetRegName({Instruction.SegmentOverride, 0, 2}));
                        }
                        
                        fprintf(Dest, "[");
                        PrintEffectiveAddressExpression(Address, Dest);
                        fprintf(Dest, "]");
                    }
                } break;
                
                case Operand_Immediate:
                {
                    immediate Immediate = Operand.Immediate;
                    if(Immediate.Flags & Immediate_RelativeJumpDisplacement)
                    {
                        fprintf(Dest, "$%+d", Immediate.Value + Instruction.Size);
                    }
                    else
                    {
                        fprintf(Dest, "%d", Immediate.Value);
                    }
                } break;
            }
        }
    }
}
