/**
 * @file code_generator.cpp
 * @brief Contains the implementation of functions for generating machine code from intermediate
 * code.
 * @author Vishank Singh, https://github.com/VishankSingh
 */

#include "code_generator.h"
#include "common/instructions.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace Kites
{

static bool isVectorVVInstruction(const std::string &opcode)
{
    return opcode == "vadd.vv" ||
           opcode == "vsub.vv" ||
           opcode == "vand.vv" ||
           opcode == "vor.vv"  ||
           opcode == "vxor.vv";
}
bool isVectorMemoryInstruction(const std::string &opcode)
{
    return opcode == "vle8.v"  ||
           opcode == "vle16.v" ||
           opcode == "vle32.v" ||
           opcode == "vle64.v" ||
           opcode == "vse8.v"  ||
           opcode == "vse16.v" ||
           opcode == "vse32.v" ||
           opcode == "vse64.v";
}
static bool isVectorReductionInstruction(const std::string &opcode)
{
    return opcode == "vredsum.vs";
}
bool isVectorVXInstruction(const std::string &opcode)
{
    return opcode == "vadd.vx" ||
           opcode == "vsub.vx";
}
std::vector<std::string>
printIntermediateCode(const std::vector<std::pair<ICUnit, bool>> &IntermediateCode)
{
    std::vector<std::string> ICList;
    for (const auto &pair : IntermediateCode)
    {
        const ICUnit &block = pair.first;
        std::string code;
        if (isVectorMemoryInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() +
                ", (" + block.getRs1() + ")";
        }
        else if (isVectorVVInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + ", " +
                block.getRs2() + ", " + block.getRs1();
        }
        else if (isVectorReductionInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + ", " +
                block.getRs2() + ", " + block.getRs1();
        }
        else if (isVectorVXInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + ", " +
                block.getRs2() + ", " + block.getRs1();
        }
        else if (instruction_set::isValidRTypeInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + " " + block.getRs1() + " " +
                   block.getRs2();
        }
        else if (instruction_set::isValidITypeInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + " " + block.getRs1() + " " +
                   block.getImm();
        }
        else if (instruction_set::isValidSTypeInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRs2() + " " + block.getImm() + "(" +
                   block.getRs1() + ")";
        }
        else if (instruction_set::isValidBTypeInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRs1() + " " + block.getRs2() + " " +
                   block.getImm() + " <" + block.getLabel() + ">";
        }
        else if (instruction_set::isValidUTypeInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + " " + block.getImm();
        }
        else if (instruction_set::isValidJTypeInstruction(block.getOpcode()))
        {
            code = block.getOpcode() + " " + block.getRd() + " " + block.getImm() + " <" +
                   block.getLabel() + ">";
        }
        else
        {
            code = block.getOpcode() + " " + block.getImm();
        }

        ICList.push_back(code);
    }
    return ICList;
}

static inline uint32_t extractRegisterIndex(const std::string &reg)
{
    return static_cast<uint32_t>(std::stoi(reg.substr(1)));
}

uint32_t generateVectorVVTypeMachineCode(const ICUnit &block)
{
    uint32_t funct6 = 0;

    const std::string opcode = block.getOpcode();

    if (opcode == "vadd.vv")
        funct6 = 0b000000;
    else if (opcode == "vsub.vv")
        funct6 = 0b000010;
    else if (opcode == "vand.vv")
        funct6 = 0b001001;
    else if (opcode == "vor.vv")
        funct6 = 0b001010;
    else if (opcode == "vxor.vv")
        funct6 = 0b001011;
    else
        throw std::runtime_error("Unsupported vector instruction: " + opcode);

    const uint32_t vd  = extractRegisterIndex(block.getRd());
    const uint32_t vs1 = extractRegisterIndex(block.getRs1());
    const uint32_t vs2 = extractRegisterIndex(block.getRs2());

    constexpr uint32_t vm = 1;
    constexpr uint32_t funct3 = 0b000;
    constexpr uint32_t opcodeBits = 0b1010111;

    uint32_t machineCode = 0;

    machineCode |= (funct6 << 26);
    machineCode |= (vm << 25);
    machineCode |= (vs2 << 20);
    machineCode |= (vs1 << 15);
    machineCode |= (funct3 << 12);
    machineCode |= (vd << 7);
    machineCode |= opcodeBits;

    return machineCode;
}
uint32_t generateVectorMemoryTypeMachineCode(const ICUnit &block)
{
    const std::string opcode = block.getOpcode();

    uint32_t funct3;
    uint32_t opcodeBits;

    if (opcode == "vle8.v" || opcode == "vse8.v")
        funct3 = 0b000;
    else if (opcode == "vle16.v" || opcode == "vse16.v")
        funct3 = 0b101;
    else if (opcode == "vle32.v" || opcode == "vse32.v")
        funct3 = 0b110;
    else if (opcode == "vle64.v" || opcode == "vse64.v")
        funct3 = 0b111;
    else
        throw std::runtime_error("Unsupported vector memory instruction: " + opcode);

    if (opcode.substr(0, 3) == "vle")
        opcodeBits = 0b0000111; // Vector load
    else
        opcodeBits = 0b0100111; // Vector store

    const uint32_t vectorReg = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());

    constexpr uint32_t vm = 1; // Unmasked

    uint32_t machineCode = 0;

    machineCode |= (vm << 25);
    machineCode |= (rs1 << 15);
    machineCode |= (funct3 << 12);
    machineCode |= (vectorReg << 7);
    machineCode |= opcodeBits;

    return machineCode;
}
uint32_t generateVectorReductionMachineCode(const ICUnit &block)
{
    const uint32_t vd  = extractRegisterIndex(block.getRd());
    const uint32_t vs1 = extractRegisterIndex(block.getRs1());
    const uint32_t vs2 = extractRegisterIndex(block.getRs2());

    constexpr uint32_t funct6 = 0b000000;
    constexpr uint32_t vm = 1;
    constexpr uint32_t funct3 = 0b010;
    constexpr uint32_t opcodeBits = 0b1010111;

    uint32_t machineCode = 0;

    machineCode |= (funct6 << 26);
    machineCode |= (vm << 25);
    machineCode |= (vs2 << 20);
    machineCode |= (vs1 << 15);
    machineCode |= (funct3 << 12);
    machineCode |= (vd << 7);
    machineCode |= opcodeBits;

    return machineCode;
}
uint32_t generateVectorVXTypeMachineCode(const ICUnit &block)
{
    uint32_t funct6 = 0;
    const std::string opcode = block.getOpcode();

    if (opcode == "vadd.vx")
        funct6 = 0b000000;
    else if (opcode == "vsub.vx")
        funct6 = 0b000010;
    else
        throw std::runtime_error("Unsupported vector instruction: " + opcode);

    const uint32_t vd  = extractRegisterIndex(block.getRd());
    const uint32_t vs2 = extractRegisterIndex(block.getRs2());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());

    constexpr uint32_t vm = 1;
    constexpr uint32_t funct3 = 0b100;
    constexpr uint32_t opcodeBits = 0b1010111;

    uint32_t machineCode = 0;

    machineCode |= (funct6 << 26);
    machineCode |= (vm << 25);
    machineCode |= (vs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (funct3 << 12);
    machineCode |= (vd << 7);
    machineCode |= opcodeBits;

    return machineCode;
}

uint32_t generateRTypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::R_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rs2 = extractRegisterIndex(block.getRs2());
    const uint32_t funct3 = encoding.funct3.to_ulong();
    const uint32_t funct7 = encoding.funct7.to_ulong();
    const uint32_t opcode = encoding.opcode.to_ulong();
    uint32_t machineCode = 0;
    machineCode |= (funct7 << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (funct3 << 12);
    machineCode |= (rd << 7);
    machineCode |= opcode;
    return machineCode;
}

uint32_t generateI1TypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::I1_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t imm = static_cast<uint32_t>(std::stoi(block.getImm()));
    const uint32_t funct3 = encoding.funct3.to_ulong();
    const uint32_t opcode = encoding.opcode.to_ulong();
    uint32_t machineCode = 0;
    machineCode |= (imm << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (funct3 << 12);
    machineCode |= (rd << 7);
    machineCode |= opcode;
    return machineCode;
}

uint32_t generateI2TypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::I2_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t imm = static_cast<uint32_t>(std::stoi(block.getImm()));
    uint32_t machineCode = 0;
    machineCode |= (encoding.funct6.to_ulong() << 26);
    machineCode |= (imm << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateI3TypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::I3_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = 0;
    const uint32_t rs1 = 0;
    const uint32_t imm = 0;
    uint32_t machineCode = 0;
    machineCode |= (imm << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateSTypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::S_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rs2 = extractRegisterIndex(block.getRs2());
    const uint32_t imm = static_cast<uint32_t>(std::stoi(block.getImm()));
    const uint32_t imm_lo = imm & 0b11111;          // bits [4:0]
    const uint32_t imm_hi = (imm >> 5) & 0b1111111; // bits [11:5]
    uint32_t machineCode = 0;
    machineCode |= (imm_hi << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (imm_lo << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateBTypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::B_type_instruction_encoding_map.at(block.getOpcode());
    uint32_t rs1 = extractRegisterIndex(block.getRs1());
    uint32_t rs2 = extractRegisterIndex(block.getRs2());
    int32_t imm = std::stoi(block.getImm());
    uint32_t imm12 = (imm >> 12) & 0b1;
    uint32_t imm10_5 = (imm >> 5) & 0b111111;
    uint32_t imm4_1 = (imm >> 1) & 0b1111;
    uint32_t imm11 = (imm >> 11) & 0b1;
    uint32_t machineCode = 0;
    machineCode |= (imm12 << 31);
    machineCode |= (imm10_5 << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (imm4_1 << 8);
    machineCode |= (imm11 << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateUTypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::U_type_instruction_encoding_map.at(block.getOpcode());
    uint32_t rd = extractRegisterIndex(block.getRd());
    uint32_t imm =
        static_cast<uint32_t>(std::stoi(block.getImm())) & 0xFFFFF; // U-type: top 20 bits
    uint32_t machineCode = 0;
    machineCode |= (imm << 12);                // bits [31:12]
    machineCode |= (rd << 7);                  // bits [11:7]
    machineCode |= encoding.opcode.to_ulong(); // bits [6:0]
    return machineCode;
}

uint32_t generateJTypeMachineCode(const ICUnit &block)
{
    const auto &encoding = instruction_set::J_type_instruction_encoding_map.at(block.getOpcode());
    uint32_t rd = extractRegisterIndex(block.getRd());
    int32_t imm = static_cast<int32_t>(std::stoi(block.getImm()));
    uint32_t machineCode = 0;
    machineCode |= ((imm & 0x100000) << 11);   // imm[20] to bit 31
    machineCode |= ((imm & 0x7FE) << 20);      // imm[10:1] to bits 30:21
    machineCode |= ((imm & 0x800) << 9);       // imm[11] to bit 20
    machineCode |= ((imm & 0xFF000) << 0);     // imm[19:12] to bits 19:12
    machineCode |= (rd << 7);                  // bits 11:7
    machineCode |= encoding.opcode.to_ulong(); // bits 6:0
    return machineCode;
}

uint32_t generateCSRRTypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::CSR_R_type_instruction_encoding_map.at(block.getOpcode());
    uint32_t rd = extractRegisterIndex(block.getRd());
    uint32_t rs1 = extractRegisterIndex(block.getRs1());
    uint32_t csr = static_cast<uint32_t>(block.getCsr()) & 0xFFF; // CSR is 12-bit
    uint32_t machineCode = 0;
    machineCode |= (csr << 20);                        // csr[31:20]
    machineCode |= (rs1 << 15);                        // rs1[19:15]
    machineCode |= (encoding.funct3.to_ulong() << 12); // funct3[14:12]
    machineCode |= (rd << 7);                          // rd[11:7]
    machineCode |= encoding.opcode.to_ulong();         // opcode[6:0]
    return machineCode;
}

uint32_t generateCSRITypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::CSR_I_type_instruction_encoding_map.at(block.getOpcode());
    uint32_t rd = extractRegisterIndex(block.getRd());
    uint32_t zimm =
        static_cast<uint32_t>(std::stoi(block.getImm())) & 0b11111; // zimm is 5-bit (not 3-bit!)
    uint32_t csr = static_cast<uint32_t>(block.getCsr()) & 0xFFF;   // csr is 12-bit
    uint32_t machineCode = 0;
    machineCode |= (csr << 20);                        // csr[31:20]
    machineCode |= (zimm << 15);                       // zimm[19:15] (not rs1)
    machineCode |= (encoding.funct3.to_ulong() << 12); // funct3[14:12]
    machineCode |= (rd << 7);                          // rd[11:7]
    machineCode |= encoding.opcode.to_ulong();         // opcode[6:0]
    return machineCode;
}

uint32_t generateFDRTypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_R_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rs2 = extractRegisterIndex(block.getRs2());
    uint32_t machineCode = 0;
    machineCode |= (encoding.funct7.to_ulong() << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateFDR1TypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_R1_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rs2 = extractRegisterIndex(block.getRs2());
    const uint32_t rm = static_cast<uint32_t>(block.getRm() & 0b111);
    uint32_t machineCode = 0;
    machineCode |= (encoding.funct7.to_ulong() << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (rm << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateFDR2TypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_R2_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rm = static_cast<uint32_t>(block.getRm() & 0b111);
    uint32_t machineCode = 0;
    machineCode |= (encoding.funct7.to_ulong() << 25);
    machineCode |= (encoding.funct5.to_ulong() << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (rm << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateFDR3TypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_R3_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    uint32_t machineCode = 0;
    machineCode |= (encoding.funct7.to_ulong() << 25);
    machineCode |= (encoding.funct5.to_ulong() << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateFDR4TypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_R4_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rs2 = extractRegisterIndex(block.getRs2());
    const uint32_t rs3 = extractRegisterIndex(block.getRs3());
    const uint32_t rm = static_cast<uint32_t>(block.getRm() & 0b111);
    uint32_t machineCode = 0;
    machineCode |= (rs3 << 27);
    machineCode |= (encoding.funct2.to_ulong() << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (rm << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateFDITypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_I_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rd = extractRegisterIndex(block.getRd());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t imm = static_cast<uint32_t>(std::stoi(block.getImm()));
    uint32_t machineCode = 0;
    machineCode |= (imm << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (rd << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

uint32_t generateFDSTypeMachineCode(const ICUnit &block)
{
    const auto &encoding =
        instruction_set::F_D_S_type_instruction_encoding_map.at(block.getOpcode());
    const uint32_t rs1 = extractRegisterIndex(block.getRs1());
    const uint32_t rs2 = extractRegisterIndex(block.getRs2());
    const uint32_t imm = static_cast<uint32_t>(std::stoi(block.getImm()));
    const uint32_t imm_lo = imm & 0b11111;          // bits [4:0]
    const uint32_t imm_hi = (imm >> 5) & 0b1111111; // bits [11:5]
    uint32_t machineCode = 0;
    machineCode |= (imm_hi << 25);
    machineCode |= (rs2 << 20);
    machineCode |= (rs1 << 15);
    machineCode |= (encoding.funct3.to_ulong() << 12);
    machineCode |= (imm_lo << 7);
    machineCode |= encoding.opcode.to_ulong();
    return machineCode;
}

std::vector<uint32_t>
generateMachineCode(const std::vector<std::pair<ICUnit, bool>> &IntermediateCode)
{
    std::vector<uint32_t> machine_code;
    for (const auto &pair : IntermediateCode)
    {
        const ICUnit &block = pair.first;
        uint32_t code;
        if (isVectorMemoryInstruction(block.getOpcode()))
        {
            code = generateVectorMemoryTypeMachineCode(block);
        }
        else if (isVectorVVInstruction(block.getOpcode()))
        {
            code = generateVectorVVTypeMachineCode(block);
        }
        else if (isVectorReductionInstruction(block.getOpcode()))
        {
             code = generateVectorReductionMachineCode(block);
        }
        else if (isVectorVXInstruction(block.getOpcode()))
        {
            code = generateVectorVXTypeMachineCode(block);
        }
        else if (instruction_set::isValidRTypeInstruction(block.getOpcode()))
        {
            code = generateRTypeMachineCode(block);
        }
        else if (instruction_set::isValidI1TypeInstruction(block.getOpcode()))
        {
            code = generateI1TypeMachineCode(block);
        }
        else if (instruction_set::isValidI2TypeInstruction(block.getOpcode()))
        {
            code = generateI2TypeMachineCode(block);
        }
        else if (instruction_set::isValidI3TypeInstruction(block.getOpcode()))
        {
            code = generateI3TypeMachineCode(block);
        }
        else if (instruction_set::isValidSTypeInstruction(block.getOpcode()))
        {
            code = generateSTypeMachineCode(block);
        }
        else if (instruction_set::isValidBTypeInstruction(block.getOpcode()))
        {
            code = generateBTypeMachineCode(block);
        }
        else if (instruction_set::isValidUTypeInstruction(block.getOpcode()))
        {
            code = generateUTypeMachineCode(block);
        }
        else if (instruction_set::isValidJTypeInstruction(block.getOpcode()))
        {
            code = generateJTypeMachineCode(block);
        }
        else if (instruction_set::isValidCSRRTypeInstruction(block.getOpcode()))
        {
            code = generateCSRRTypeMachineCode(block);
        }
        else if (instruction_set::isValidCSRITypeInstruction(block.getOpcode()))
        {
            code = generateCSRITypeMachineCode(block);
        }
        else if (instruction_set::isValidFDRTypeInstruction(block.getOpcode()))
        {
            code = generateFDRTypeMachineCode(block);
        }
        else if (instruction_set::isValidFDR1TypeInstruction(block.getOpcode()))
        {
            code = generateFDR1TypeMachineCode(block);
        }
        else if (instruction_set::isValidFDR2TypeInstruction(block.getOpcode()))
        {
            code = generateFDR2TypeMachineCode(block);
        }
        else if (instruction_set::isValidFDR3TypeInstruction(block.getOpcode()))
        {
            code = generateFDR3TypeMachineCode(block);
        }
        else if (instruction_set::isValidFDR4TypeInstruction(block.getOpcode()))
        {
            code = generateFDR4TypeMachineCode(block);
        }
        else if (instruction_set::isValidFDITypeInstruction(block.getOpcode()))
        {
            code = generateFDITypeMachineCode(block);
        }
        else if (instruction_set::isValidFDSTypeInstruction(block.getOpcode()))
        {
            code = generateFDSTypeMachineCode(block);
        }
        else
        {
            throw std::runtime_error("Invalid instruction type: " + block.getOpcode());
        }
        machine_code.push_back(code);
    }
    return machine_code;
}
}// namespace Kites