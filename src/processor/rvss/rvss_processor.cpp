/**
 * @file rvss_vm.cpp
 * @brief RVSS VM implementation
 * @author Vishank Singh, https://github.com/VishankSingh
 */

#include "processor/rvss/rvss_processor.h"

#include "common/instructions.h"
#include "config/config.h"
#include "common/globals.h"
#include "ui/processor_tab/processor_designs/rvss_processor_circuit_scene.h"
#include "utils/utils.h"
#include <QDebug>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <queue>
#include <stack>
#include <thread>
#include <tuple>
#include "processor/vector_config.h"

namespace Kites
{
RVSSProcessor::RVSSProcessor() : ProcessorBase()
{
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
#ifndef DISABLE_GUI
    circuit_scene_ = std::make_unique<Kites::RVSSCircuitScene>();
    connect(this, &ProcessorBase::updateCircuitStateSignal, circuit_scene_.get(),
            &Kites::RVSSCircuitScene::updateCircuitState, Qt::QueuedConnection);
#endif

    active_wires_.append("IM_to_PC_pc");
    active_wires_.append("PC_to_IM_instruction");
    active_wires_.append("IM_to_RF_rs1_whole");
    active_wires_.append("RF_to_ALUMUX");
    active_wires_.append("PCMux_to_PC");
    active_wires_.append("Imm_to_ALUMUX");
    active_wires_.append("BranchALU_to_PCMUX");
    active_wires_.append("PCALU_to_PCMUX");
    active_wires_.append("Control_ALUOp");
    active_wires_.append("PC_to_InstrMem");
    active_wires_.append("ALUOp_to_ALU_alusignal");
    active_wires_.append("ALU_to_DM");

    always_active_wires_count_ = active_wires_.size();
}

RVSSProcessor::~RVSSProcessor() = default;

void RVSSProcessor::SetActiveWireNames()
{
    // first clear the current list
    // TODO * we dont have to clear wire that will always be active
    //  so make a list of those wires and only clear the rest
    //  for now this will
    // active_wires_.clear();

    // Map control signals to canonical wire names used in the scene JSON.
    // maybe sending integers would be better?
    // but for now, this is fine.
    // also the preformace boost well get of that will that really improve the user experience that
    // much?

    // first add the always active wires
    // this is not ideal but for now it will do

    // if(branch_flag_)
    // {

    // }

    if (control_unit_.GetMemWrite())
    {
        active_wires_.append("Control_to_MemWrite");
    }
    if (control_unit_.GetMemRead())
    {
        active_wires_.append("Control_to_MemRead");
    }
    if (control_unit_.GetRegWrite())
    {
        active_wires_.append("Control_to_Regwrite");
        active_wires_.append("WBMux_to_RF_lowest");
    }
    if (control_unit_.GetBranch())
    {
        active_wires_.append("Control_to_BranchAND");
    }
    if (control_unit_.GetAluSrc())
    {
        active_wires_.append("Control_to_ALUSrc_control");
    }
    if (control_unit_.GetMemToReg())
    {
        active_wires_.append("Control_to_MemtoRegMux");
    }

}

void RVSSProcessor::setProcessorState()
{
    processor_state_.programCounters = 
    {program_counter_,INVALID_PC,INVALID_PC,INVALID_PC,INVALID_PC};
    processor_state_.lastExecutedPC = last_executed_pc_;
}

void RVSSProcessor::Run()
{
    ClearStop();
    while (!stop_requested_ && program_counter_ < program_size_)
    {
        if (last_breakpoint_pc_ != UINT64_MAX && program_counter_ != last_breakpoint_pc_)
        {
            last_breakpoint_pc_ = UINT64_MAX;
        }

        if (std::find(breakpoints_.begin(), breakpoints_.end(), program_counter_) !=
                breakpoints_.end() &&
            program_counter_ != last_breakpoint_pc_)
        {
            pause_requested_ = true;
            last_breakpoint_pc_ = program_counter_;
            emit processorPausedAtBreakpointSignal();
        }
        {
            QMutexLocker locker(&pause_mutex_);
            while (pause_requested_ && !stop_requested_)
            {
                pause_wait_condition_.wait(&pause_mutex_);
            }
            if (stop_requested_)
            {
                break;
            }
        }
        StepAndNotify();
        // handling the delay
        {
            QMutexLocker locker(&pause_mutex_);
            if (stop_requested_ || pause_requested_)
                continue;
            pause_wait_condition_.wait(&pause_mutex_, step_delay_);
            // we wait for step_delay_ milliseconds or until notified to wake up
        }
    }
    if (stop_requested_)
    {
        emit processorClockedSignal(processor_state_);
    }
    if (program_counter_ >= program_size_)
    {
        output_status_ = "VM_PROGRAM_END";
    }
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}

void RVSSProcessor::Fetch()
{
    current_instruction_ = memory_controller_.readInstruction(program_counter_);
    UpdateProgramCounter(4);
}

void RVSSProcessor::Decode()
{
    control_unit_.SetControlSignals(current_instruction_);
}

void RVSSProcessor::Execute()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t funct3 = (current_instruction_ >> 12) & 0b111;
    
    // Handle vector configuration instructions.
    if (opcode == 0x57 &&
    funct3 == 0b111 &&
    ((current_instruction_ >> 31) & 1) == 0)
    {
        ExecuteVsetvli();
        return;
    }
    // Handle vector arithmetic instructions (OPIVV and OPIVX).
    if (opcode == 0x57 && (funct3 == 0b000 || funct3 == 0b100))
    {
        ExecuteVectorArithmetic();
        return;
    }
    // Handle vector memory instructions.
    if ((opcode == 0x07 || opcode == 0x27) &&
        (funct3 == 0b000 || funct3 == 0b101 ||
        funct3 == 0b110 || funct3 == 0b111))
    {
        ExecuteVectorMemory();
        return;
    }
    // Handle vector reduction instructions.
    if (opcode == 0x57 && funct3 == 0b010)
    {
        ExecuteVectorReduction();
        return;
    }

    if (opcode == 0b1110011 && funct3 == 0b000)
    {
        HandleSyscall();
        return;
    }

    if (instruction_set::isFInstruction(current_instruction_))
    { // RV64 F
        ExecuteFloat();
        return;
    }
    else if (instruction_set::isDInstruction(current_instruction_))
    {
        ExecuteDouble();
        return;
    }
    else if (opcode == 0b1110011)
    {
        ExecuteCsr();
        return;
    }

    uint8_t rs1 = (current_instruction_ >> 15) & 0b11111;
    uint8_t rs2 = (current_instruction_ >> 20) & 0b11111;

    int32_t imm = ImmGenerator(current_instruction_);

    uint64_t reg1_value = registers_.ReadGpr(rs1);
    uint64_t reg2_value = registers_.ReadGpr(rs2);

    bool overflow = false;

    if (control_unit_.GetAluSrc())
    {
        reg2_value = static_cast<uint64_t>(static_cast<int64_t>(imm));
    }

    alu::AluOp aluOperation =
        control_unit_.GetAluSignal(current_instruction_, control_unit_.GetAluOp());
    std::tie(execution_result_, overflow) = alu_.execute(aluOperation, reg1_value, reg2_value);

    if (control_unit_.GetBranch())
    {
        if (opcode == 0b1100111 || opcode == 0b1101111)
        {                                                      // JALR or JAL
            next_pc_ = static_cast<int64_t>(program_counter_); // PC was already updated in Fetch()
            UpdateProgramCounter(-4);
            return_address_ = program_counter_ + 4;
            if (opcode == 0b1100111)
            { // JALR
                UpdateProgramCounter(-program_counter_ + (execution_result_));
            }
            else
            { // JAL
                UpdateProgramCounter(imm);
            }
        }
        else if (opcode == instruction_set::instruction_encoding_map
                               .at(instruction_set::Instruction::kbeq)
                               .opcode ||
                 opcode == instruction_set::instruction_encoding_map
                               .at(instruction_set::Instruction::kbne)
                               .opcode ||
                 opcode == instruction_set::instruction_encoding_map
                               .at(instruction_set::Instruction::kblt)
                               .opcode ||
                 opcode == instruction_set::instruction_encoding_map
                               .at(instruction_set::Instruction::kbge)
                               .opcode ||
                 opcode == instruction_set::instruction_encoding_map
                               .at(instruction_set::Instruction::kbltu)
                               .opcode ||
                 opcode == instruction_set::instruction_encoding_map
                               .at(instruction_set::Instruction::kbgeu)
                               .opcode)
        {
            switch (funct3)
            {
            case 0b000:
            { // BEQ
                branch_flag_ = (execution_result_ == 0);
                break;
            }
            case 0b001:
            { // BNE
                branch_flag_ = (execution_result_ != 0);
                break;
            }
            case 0b100:
            { // BLT
                branch_flag_ = (execution_result_ == 1);
                break;
            }
            case 0b101:
            { // BGE
                branch_flag_ = (execution_result_ == 0);
                break;
            }
            case 0b110:
            { // BLTU
                branch_flag_ = (execution_result_ == 1);
                break;
            }
            case 0b111:
            { // BGEU
                branch_flag_ = (execution_result_ == 0);
                break;
            }
            }
        }
    }

    if (branch_flag_ &&
        (opcode == instruction_set::instruction_encoding_map.at(instruction_set::Instruction::kbeq)
                       .opcode ||
         opcode == instruction_set::instruction_encoding_map.at(instruction_set::Instruction::kbne)
                       .opcode ||
         opcode == instruction_set::instruction_encoding_map.at(instruction_set::Instruction::kblt)
                       .opcode ||
         opcode == instruction_set::instruction_encoding_map.at(instruction_set::Instruction::kbge)
                       .opcode ||
         opcode == instruction_set::instruction_encoding_map.at(instruction_set::Instruction::kbltu)
                       .opcode ||
         opcode == instruction_set::instruction_encoding_map.at(instruction_set::Instruction::kbgeu)
                       .opcode))
    {
        UpdateProgramCounter(-4);
        UpdateProgramCounter(imm);
        // we have jumped so the branch alu wire will send signal to the pc mux
        active_wires_.append("ALUzero_to_ANDGATElower");
        active_wires_.append("BranchAND_to_PCMUX");
    }

    if (opcode == 0b0010111)
    { // AUIPC
        execution_result_ = static_cast<int64_t>(program_counter_) - 4 + (imm << 12);
    }
}

void RVSSProcessor::ExecuteFloat()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t funct3 = (current_instruction_ >> 12) & 0b111;
    uint8_t funct7 = (current_instruction_ >> 25) & 0b1111111;
    uint8_t rm = funct3;
    uint8_t rs1 = (current_instruction_ >> 15) & 0b11111;
    uint8_t rs2 = (current_instruction_ >> 20) & 0b11111;
    uint8_t rs3 = (current_instruction_ >> 27) & 0b11111;

    uint8_t fcsr_status = 0;

    int32_t imm = ImmGenerator(current_instruction_);

    if (rm == 0b111)
    {
        rm = registers_.ReadCsr(0x002);
    }

    uint64_t reg1_value = registers_.ReadFpr(rs1);
    uint64_t reg2_value = registers_.ReadFpr(rs2);
    uint64_t reg3_value = registers_.ReadFpr(rs3);

    if (funct7 == 0b1101000 || funct7 == 0b1111000 || opcode == 0b0000111 || opcode == 0b0100111)
    {
        reg1_value = registers_.ReadGpr(rs1);
    }

    if (control_unit_.GetAluSrc())
    {
        reg2_value = static_cast<uint64_t>(static_cast<int64_t>(imm));
    }

    alu::AluOp aluOperation =
        control_unit_.GetAluSignal(current_instruction_, control_unit_.GetAluOp());
    std::tie(execution_result_, fcsr_status) =
        alu::Alu::fpexecute(aluOperation, reg1_value, reg2_value, reg3_value, rm);

    registers_.WriteCsr(0x003, fcsr_status);
}

void RVSSProcessor::ExecuteDouble()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t funct3 = (current_instruction_ >> 12) & 0b111;
    uint8_t funct7 = (current_instruction_ >> 25) & 0b1111111;
    uint8_t rm = funct3;
    uint8_t rs1 = (current_instruction_ >> 15) & 0b11111;
    uint8_t rs2 = (current_instruction_ >> 20) & 0b11111;
    uint8_t rs3 = (current_instruction_ >> 27) & 0b11111;

    uint8_t fcsr_status = 0;

    int32_t imm = ImmGenerator(current_instruction_);

    uint64_t reg1_value = registers_.ReadFpr(rs1);
    uint64_t reg2_value = registers_.ReadFpr(rs2);
    uint64_t reg3_value = registers_.ReadFpr(rs3);

    if (funct7 == 0b1101001 || funct7 == 0b1111001 || opcode == 0b0000111 || opcode == 0b0100111)
    {
        reg1_value = registers_.ReadGpr(rs1);
    }

    if (control_unit_.GetAluSrc())
    {
        reg2_value = static_cast<uint64_t>(static_cast<int64_t>(imm));
    }

    alu::AluOp aluOperation =
        control_unit_.GetAluSignal(current_instruction_, control_unit_.GetAluOp());
    std::tie(execution_result_, fcsr_status) =
        alu::Alu::dfpexecute(aluOperation, reg1_value, reg2_value, reg3_value, rm);
}

void RVSSProcessor::ExecuteCsr()
{
    uint8_t rs1 = (current_instruction_ >> 15) & 0b11111;
    uint16_t csr = (current_instruction_ >> 20) & 0xFFF;
    uint64_t csr_val = registers_.ReadCsr(csr);

    csr_target_address_ = csr;
    csr_old_value_ = csr_val;
    csr_write_val_ = registers_.ReadGpr(rs1);
    csr_uimm_ = rs1;
}

void RVSSProcessor::ExecuteVsetvli()
{
    uint8_t rd = (current_instruction_ >> 7) & 0b11111;
    uint8_t rs1 = (current_instruction_ >> 15) & 0b11111;

    // Extract the 11-bit VTYPE immediate.
    uint64_t vtype_imm = (current_instruction_ >> 20) & 0x7FF;

    vector_rd_ = rd;

    // Determine AVL (Application Vector Length).
    uint64_t avl;

    if (rs1 == 0 && rd != 0)
    {
        // rs1=x0, rd!=x0: AVL = maximum unsigned value.
        avl = UINT64_MAX;
    }
    else if (rs1 == 0 && rd == 0)
    {
        // rs1=x0, rd=x0: AVL = current vl.
        avl = registers_.ReadCsr(RegisterFile::CSR_VL);
    }
    else
    {
        avl = registers_.ReadGpr(rs1);
    }

    // Decode the requested vector configuration.
    VectorConfig config = DecodeVectorConfig(vtype_imm);

    if (config.vill)
    {
        // Invalid VTYPE: set vill and make vl zero.
        vector_new_vtype_ = uint64_t{1} << 63;
        vector_new_vl_ = 0;
    }
    else
    {
        vector_new_vtype_ = vtype_imm;
        vector_new_vl_ = CalculateVectorLength(avl, config.vlmax);
    }
}

void RVSSProcessor::ExecuteVectorArithmetic()
{
    vector_arithmetic_pending_ = false;

    const uint8_t funct6 = (current_instruction_ >> 26) & 0x3F;
    const uint8_t vm = (current_instruction_ >> 25) & 0x01;
    const uint8_t funct3 = (current_instruction_ >> 12) & 0b111;

    vector_vd_ = (current_instruction_ >> 7) & 0x1F;
    const uint8_t vs1 = (current_instruction_ >> 15) & 0x1F;
    const uint8_t vs2 = (current_instruction_ >> 20) & 0x1F;

    if (funct3 == 0b000)
    {
        switch (funct6)
        {
        case 0b000000:
        case 0b000010:
        case 0b001001:
        case 0b001010:
        case 0b001011:
            break;

        default:
            return;
        }
    }
    else if (funct3 == 0b100)
    {
        switch (funct6)
        {
        case 0b000000: // vadd.vx
        case 0b000010: // vsub.vx
            break;

        default:
            return;
        }
    }
    else
    {
        return;
    }

    const uint64_t vtype =
        registers_.ReadCsr(RegisterFile::CSR_VTYPE);

    const uint64_t vl =
        registers_.ReadCsr(RegisterFile::CSR_VL);

    const uint64_t vstart =
        registers_.ReadCsr(RegisterFile::CSR_VSTART);

    const VectorConfig config = DecodeVectorConfig(vtype);

    if (config.vill || config.sew_bits == 0 ||
        vl > config.vlmax || vstart > vl)
    {
        return;
    }

    vector_old_value_ = registers_.ReadVreg(vector_vd_);
    vector_new_value_ = vector_old_value_;

    // Snapshot v0 before modifying the destination.
    const RegisterFile::VectorRegister mask_register =
        registers_.ReadVreg(0);

    const RegisterFile::VectorRegister source1 =
        (funct3 == 0b000) ? registers_.ReadVreg(vs1) : RegisterFile::VectorRegister{};

    const RegisterFile::VectorRegister source2 =
        registers_.ReadVreg(vs2);

    const unsigned int element_bytes = config.sew_bits / 8;
    const unsigned int max_elements =
        RegisterFile::VLEN_BYTES / element_bytes;

    auto read_element = [](const RegisterFile::VectorRegister &reg,
                           unsigned int index,
                           unsigned int bytes) -> uint64_t
    {
        uint64_t value = 0;

        for (unsigned int b = 0; b < bytes; b++)
        {
            value |= static_cast<uint64_t>(
                         reg[index * bytes + b])
                     << (8 * b);
        }

        return value;
    };

    auto write_element = [](RegisterFile::VectorRegister &reg,
                            unsigned int index,
                            unsigned int bytes,
                            uint64_t value)
    {
        for (unsigned int b = 0; b < bytes; b++)
        {
            reg[index * bytes + b] =
                static_cast<uint8_t>(value >> (8 * b));
        }
    };

    const uint64_t element_mask =
        (config.sew_bits == 64)
            ? UINT64_MAX
            : ((1ULL << config.sew_bits) - 1);

    // For OP-IVX: extract the scalar operand and mask/truncate to the active SEW.
    const uint64_t scalar_val =
        (funct3 == 0b100) ? (registers_.ReadGpr(vs1) & element_mask) : 0;

    // Process active vector elements.
    for (uint64_t i = vstart; i < vl; i++)
    {
        // vm=1 means unmasked execution.
        // vm=0 means consult the corresponding bit in v0.
        const bool mask_active =
            vm == 1 ||
            ((mask_register[i / 8] >> (i % 8)) & 1) != 0;

        if (!mask_active)
        {
            // Mask-agnostic: write all ones.
            // Mask-undisturbed: retain the old destination value.
            if (config.mask_agnostic)
            {
                write_element(
                    vector_new_value_,
                    static_cast<unsigned int>(i),
                    element_bytes,
                    element_mask);
            }

            continue;
        }

        const uint64_t a =
            read_element(source2, i, element_bytes);

        const uint64_t b =
            (funct3 == 0b100) ? scalar_val : read_element(source1, i, element_bytes);

        uint64_t result = 0;

        switch (funct6)
        {
        case 0b000000:
            result = a + b;
            break;

        case 0b000010:
            result = a - b;
            break;

        case 0b001001:
            result = a & b;
            break;

        case 0b001010:
            result = a | b;
            break;

        case 0b001011:
            result = a ^ b;
            break;
        }

        write_element(
            vector_new_value_,
            static_cast<unsigned int>(i),
            element_bytes,
            result & element_mask);
    }

    // Tail elements: [vl, VLMAX).
    // Prestart elements [0, vstart) remain undisturbed.
    if (config.tail_agnostic)
    {
        for (uint64_t i = vl; i < max_elements; i++)
        {
            write_element(
                vector_new_value_,
                static_cast<unsigned int>(i),
                element_bytes,
                element_mask);
        }
    }

    vector_arithmetic_pending_ = true;
}
void RVSSProcessor::ExecuteVectorMemory()
{
    vector_mem_pending_ = false;

    const uint32_t instruction = current_instruction_;

    const uint8_t opcode = instruction & 0x7F;
    const uint8_t funct3 = (instruction >> 12) & 0x7;

    // We support unit-stride, single-field operations.
    // Both masked and unmasked operations are supported.
    const uint8_t nf = (instruction >> 29) & 0x7;
    const uint8_t mew = (instruction >> 28) & 0x1;
    const uint8_t mop = (instruction >> 26) & 0x3;
    const uint8_t vm = (instruction >> 25) & 0x1;
    const uint8_t lumop_sumop = (instruction >> 20) & 0x1F;

    if (nf != 0 || mew != 0 || mop != 0 ||
        lumop_sumop != 0)
    {
        return;
    }

    // Determine element width from the instruction.
    switch (funct3)
    {
    case 0b000:
        vector_mem_element_bytes_ = 1;
        break;

    case 0b101:
        vector_mem_element_bytes_ = 2;
        break;

    case 0b110:
        vector_mem_element_bytes_ = 4;
        break;

    case 0b111:
        vector_mem_element_bytes_ = 8;
        break;

    default:
        return;
    }

    // Decode the current vector configuration.
    const uint64_t vtype =
        registers_.ReadCsr(RegisterFile::CSR_VTYPE);

    const VectorConfig config = DecodeVectorConfig(vtype);

    // Require the memory element width to match the configured SEW.
    if (config.vill ||
        config.sew_bits != vector_mem_element_bytes_ * 8)
    {
        return;
    }

    const uint64_t vl =
        registers_.ReadCsr(RegisterFile::CSR_VL);

    const uint64_t vstart =
        registers_.ReadCsr(RegisterFile::CSR_VSTART);

    const uint64_t max_elements =
        RegisterFile::VLEN_BYTES / vector_mem_element_bytes_;

    if (vl > max_elements || vstart > vl)
    {
        return;
    }

    // Save mask and tail configuration.
    vector_mem_vm_ = vm;
    vector_mem_tail_agnostic_ = config.tail_agnostic;
    vector_mem_mask_agnostic_ = config.mask_agnostic;

    // Snapshot v0 so masking uses its original contents.
    vector_mem_mask_value_ = registers_.ReadVreg(0);

    vector_mem_load_ = (opcode == 0x07);

    // Bits 11:7 contain vd for loads and vs3 for stores.
    vector_mem_reg_ = (instruction >> 7) & 0x1F;

    // Bits 19:15 contain the base address register.
    const uint8_t rs1 = (instruction >> 15) & 0x1F;

    vector_mem_base_ = registers_.ReadGpr(rs1);

    // Preserve the original vector register.
    vector_mem_old_value_ =
        registers_.ReadVreg(vector_mem_reg_);

    // Loads modify this copy; stores use the original data.
    vector_mem_new_value_ = vector_mem_old_value_;

    vector_mem_pending_ = true;
}


void RVSSProcessor::ExecuteVectorReduction()
{
    vector_reduction_pending_ = false;

    const uint8_t funct6 = (current_instruction_ >> 26) & 0x3F;
    const uint8_t vm = (current_instruction_ >> 25) & 0x01;

    vector_vd_ = (current_instruction_ >> 7) & 0x1F;
    const uint8_t vs1 = (current_instruction_ >> 15) & 0x1F;
    const uint8_t vs2 = (current_instruction_ >> 20) & 0x1F;

    // Currently support only vredsum.vs.
    if (funct6 != 0b000000)
        return;

    const uint64_t vtype =
        registers_.ReadCsr(RegisterFile::CSR_VTYPE);

    const uint64_t vl =
        registers_.ReadCsr(RegisterFile::CSR_VL);

    const uint64_t vstart =
        registers_.ReadCsr(RegisterFile::CSR_VSTART);

    const VectorConfig config = DecodeVectorConfig(vtype);

    if (config.vill || config.sew_bits == 0 ||
        vl > config.vlmax || vstart != 0)
    {
        return;
    }

    // RVV specifies no destination update when vl == 0.
    if (vl == 0)
        return;

    // Snapshot destination and all sources before calculating.
    vector_old_value_ = registers_.ReadVreg(vector_vd_);
    vector_new_value_ = vector_old_value_;

    const RegisterFile::VectorRegister source1 =
        registers_.ReadVreg(vs1);

    const RegisterFile::VectorRegister source2 =
        registers_.ReadVreg(vs2);

    const RegisterFile::VectorRegister mask_register =
        registers_.ReadVreg(0);

    const unsigned int element_bytes = config.sew_bits / 8;

    const unsigned int max_elements =
        RegisterFile::VLEN_BYTES / element_bytes;

    auto read_element = [](const RegisterFile::VectorRegister &reg,
                           unsigned int index,
                           unsigned int bytes) -> uint64_t
    {
        uint64_t value = 0;

        for (unsigned int b = 0; b < bytes; b++)
        {
            value |= static_cast<uint64_t>(
                         reg[index * bytes + b])
                     << (8 * b);
        }

        return value;
    };

    auto write_element = [](RegisterFile::VectorRegister &reg,
                            unsigned int index,
                            unsigned int bytes,
                            uint64_t value)
    {
        for (unsigned int b = 0; b < bytes; b++)
        {
            reg[index * bytes + b] =
                static_cast<uint8_t>(value >> (8 * b));
        }
    };

    const uint64_t element_mask =
        (config.sew_bits == 64)
            ? UINT64_MAX
            : ((1ULL << config.sew_bits) - 1);

    // The scalar accumulator is always included, regardless of vm.
    uint64_t accumulator =
        read_element(source1, 0, element_bytes);

    // Reduce the active elements of vs2.
    for (uint64_t i = 0; i < vl; i++)
    {
        const bool mask_active =
            vm == 1 ||
            ((mask_register[i / 8] >> (i % 8)) & 0x01) != 0;

        if (!mask_active)
            continue;

        const uint64_t element =
            read_element(source2,
                         static_cast<unsigned int>(i),
                         element_bytes);

        // Unsigned addition followed by SEW masking gives wraparound.
        accumulator = (accumulator + element) & element_mask;
    }

    // Write the scalar reduction result into vd[0].
    write_element(vector_new_value_, 0,
                  element_bytes, accumulator);

    // Elements 1 through VLMAX-1 are destination tails.
    if (config.tail_agnostic)
    {
        for (unsigned int i = 1; i < max_elements; i++)
        {
            write_element(vector_new_value_, i,
                          element_bytes, element_mask);
        }
    }

    vector_reduction_pending_ = true;
}


// TODO: implement writeback for syscalls
void RVSSProcessor::HandleSyscall()
{
    uint64_t syscall_number = registers_.ReadGpr(17);
    switch (syscall_number)
    {
    case SYSCALL_PRINT_INT:
    {
        if (!globals::vm_as_backend)
        {
            std::cout << "[Syscall output: ";
        }
        else
        {
            std::cout << "VM_STDOUT_START";
        }
        std::cout << static_cast<int64_t>(registers_.ReadGpr(10)); // Print signed integer
        if (!globals::vm_as_backend)
        {
            std::cout << "]" << std::endl;
        }
        else
        {
            std::cout << "VM_STDOUT_END" << std::endl;
        }
        break;
    }
    case SYSCALL_PRINT_FLOAT:
    { // print float
        if (!globals::vm_as_backend)
        {
            std::cout << "[Syscall output: ";
        }
        else
        {
            std::cout << "VM_STDOUT_START";
        }
        float float_value;
        uint64_t raw = registers_.ReadGpr(10);
        std::memcpy(&float_value, &raw, sizeof(float_value));
        std::cout << std::setprecision(std::numeric_limits<float>::max_digits10) << float_value;
        if (!globals::vm_as_backend)
        {
            std::cout << "]" << std::endl;
        }
        else
        {
            std::cout << "VM_STDOUT_END" << std::endl;
        }
        break;
    }
    case SYSCALL_PRINT_DOUBLE:
    { // print double
        if (!globals::vm_as_backend)
        {
            std::cout << "[Syscall output: ";
        }
        else
        {
            std::cout << "VM_STDOUT_START";
        }
        double double_value;
        uint64_t raw = registers_.ReadGpr(10);
        std::memcpy(&double_value, &raw, sizeof(double_value));
        std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << double_value;
        if (!globals::vm_as_backend)
        {
            std::cout << "]" << std::endl;
        }
        else
        {
            std::cout << "VM_STDOUT_END" << std::endl;
        }
        break;
    }
    case SYSCALL_PRINT_STRING:
    {
        if (!globals::vm_as_backend)
        {
            std::cout << "[Syscall output: ";
        }
        PrintString(registers_.ReadGpr(10)); // Print string
        if (!globals::vm_as_backend)
        {
            std::cout << "]" << std::endl;
        }
        break;
    }
    case SYSCALL_EXIT:
    {
        stop_requested_ = true; // Stop the VM
        if (!globals::vm_as_backend)
        {
            std::cout << "VM_EXIT" << std::endl;
        }
        output_status_ = "VM_EXIT";
        std::cout << "Exited with exit code: " << registers_.ReadGpr(10) << std::endl;
        exit(0); // Exit the program
        break;
    }
    case SYSCALL_READ:
    { // Read
        uint64_t file_descriptor = registers_.ReadGpr(10);
        uint64_t buffer_address = registers_.ReadGpr(11);
        uint64_t length = registers_.ReadGpr(12);

        if (file_descriptor == 0)
        {
            // Read from stdin
            std::string input;
            {
                std::cout << "VM_STDIN_START" << std::endl;
                output_status_ = "VM_STDIN_START";
                std::unique_lock<std::mutex> lock(input_mutex_);
                input_cv_.wait(lock, [this]() { return !input_queue_.empty(); });
                output_status_ = "VM_STDIN_END";
                std::cout << "VM_STDIN_END" << std::endl;

                input = input_queue_.front();
                input_queue_.pop();
            }

            std::vector<uint8_t> old_bytes_vec(length, 0);
            std::vector<uint8_t> new_bytes_vec(length, 0);

            for (size_t i = 0; i < length; ++i)
            {
                old_bytes_vec[i] = memory_controller_.peekByte(buffer_address + i);
            }

            for (size_t i = 0; i < input.size() && i < length; ++i)
            {
                memory_controller_.writeByte(buffer_address + i, static_cast<uint8_t>(input[i]));
            }
            if (input.size() < length)
            {
                memory_controller_.writeByte(buffer_address + input.size(), '\0');
            }

            for (size_t i = 0; i < length; ++i)
            {
                new_bytes_vec[i] = memory_controller_.peekByte(buffer_address + i);
            }

            current_delta_.memory_changes.push_back({buffer_address, old_bytes_vec, new_bytes_vec});

            uint64_t old_reg = registers_.ReadGpr(10);
            unsigned int reg_index = 10;
            unsigned int reg_type = 0; // 0 for GPR, 1 for CSR, 2 for FPR
            uint64_t new_reg =
                std::min(static_cast<uint64_t>(length), static_cast<uint64_t>(input.size()));
            registers_.WriteGpr(10, new_reg);
            if (old_reg != new_reg)
            {
                current_delta_.register_changes.push_back({reg_index, reg_type, old_reg, new_reg});
            }
        }
        else
        {
            std::cerr << "Unsupported file descriptor: " << file_descriptor << std::endl;
        }
        break;
    }
    case SYSCALL_WRITE:
    { // Write
        uint64_t file_descriptor = registers_.ReadGpr(10);
        uint64_t buffer_address = registers_.ReadGpr(11);
        uint64_t length = registers_.ReadGpr(12);

        if (file_descriptor == 1)
        { // stdout
            std::cout << "VM_STDOUT_START";
            output_status_ = "VM_STDOUT_START";
            uint64_t bytes_printed = 0;
            for (uint64_t i = 0; i < length; ++i)
            {
                char c = memory_controller_.readByte(buffer_address + i);
                // if (c == '\0') {
                //     break;
                // }
                std::cout << c;
                bytes_printed++;
            }
            std::cout << std::flush;
            output_status_ = "VM_STDOUT_END";
            std::cout << "VM_STDOUT_END" << std::endl;

            uint64_t old_reg = registers_.ReadGpr(10);
            unsigned int reg_index = 10;
            unsigned int reg_type = 0; // 0 for GPR, 1 for CSR, 2 for FPR
            uint64_t new_reg = std::min(static_cast<uint64_t>(length), bytes_printed);
            registers_.WriteGpr(10, new_reg);
            if (old_reg != new_reg)
            {
                current_delta_.register_changes.push_back({reg_index, reg_type, old_reg, new_reg});
            }
        }
        else
        {
            std::cerr << "Unsupported file descriptor: " << file_descriptor << std::endl;
        }
        break;
    }
    default:
    {
        std::cerr << "Unknown syscall number: " << syscall_number << std::endl;
        break;
    }
    }
}

void RVSSProcessor::WriteMemory()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t rs2 = (current_instruction_ >> 20) & 0b11111;
    uint8_t funct3 = (current_instruction_ >> 12) & 0b111;

    if (opcode == 0b1110011 && funct3 == 0b000)
    {
        return;
    }
    // Handle vector memory instructions.
    if ((opcode == 0x07 || opcode == 0x27) &&
    (funct3 == 0b000 || funct3 == 0b101 ||
     funct3 == 0b110 || funct3 == 0b111))
    {
        WriteMemoryVector();
        return;
    }
    if (instruction_set::isFInstruction(current_instruction_))
    { // RV64 F
        WriteMemoryFloat();
        return;
    }
    else if (instruction_set::isDInstruction(current_instruction_))
    {
        WriteMemoryDouble();
        return;
    }

    if (control_unit_.GetMemRead())
    {
        switch (funct3)
        {
        case 0b000:
        { // LB
            memory_result_ = static_cast<int8_t>(memory_controller_.readByte(execution_result_));
            break;
        }
        case 0b001:
        { // LH
            memory_result_ =
                static_cast<int16_t>(memory_controller_.readHalfWord(execution_result_));
            break;
        }
        case 0b010:
        { // LW
            memory_result_ = static_cast<int32_t>(memory_controller_.readWord(execution_result_));
            break;
        }
        case 0b011:
        { // LD
            memory_result_ = memory_controller_.readDoubleWord(execution_result_);
            break;
        }
        case 0b100:
        { // LBU
            memory_result_ = static_cast<uint8_t>(memory_controller_.readByte(execution_result_));
            break;
        }
        case 0b101:
        { // LHU
            memory_result_ =
                static_cast<uint16_t>(memory_controller_.readHalfWord(execution_result_));
            break;
        }
        case 0b110:
        { // LWU
            memory_result_ = static_cast<uint32_t>(memory_controller_.readWord(execution_result_));
            break;
        }
        }
    }

    uint64_t addr = 0;
    std::vector<uint8_t> old_bytes_vec;
    std::vector<uint8_t> new_bytes_vec;

    // Undo bookkeeping reads use peekByte so they do not show up as cache accesses.

    if (control_unit_.GetMemWrite())
    {
        switch (funct3)
        {
        case 0b000:
        { // SB
            addr = execution_result_;
            old_bytes_vec.push_back(memory_controller_.peekByte(addr));
            memory_controller_.writeByte(execution_result_, registers_.ReadGpr(rs2) & 0xFF);
            new_bytes_vec.push_back(memory_controller_.peekByte(addr));
            break;
        }
        case 0b001:
        { // SH
            addr = execution_result_;
            for (size_t i = 0; i < 2; ++i)
            {
                old_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
            }
            memory_controller_.writeHalfWord(execution_result_, registers_.ReadGpr(rs2) & 0xFFFF);
            for (size_t i = 0; i < 2; ++i)
            {
                new_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
            }
            break;
        }
        case 0b010:
        { // SW
            addr = execution_result_;
            for (size_t i = 0; i < 4; ++i)
            {
                old_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
            }
            memory_controller_.writeWord(execution_result_, registers_.ReadGpr(rs2) & 0xFFFFFFFF);
            for (size_t i = 0; i < 4; ++i)
            {
                new_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
            }
            break;
        }
        case 0b011:
        { // SD
            addr = execution_result_;
            for (size_t i = 0; i < 8; ++i)
            {
                old_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
            }
            memory_controller_.writeDoubleWord(execution_result_,
                                               registers_.ReadGpr(rs2) & 0xFFFFFFFFFFFFFFFF);
            for (size_t i = 0; i < 8; ++i)
            {
                new_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
            }
            break;
        }
        }
    }

    if (old_bytes_vec != new_bytes_vec)
    {
        current_delta_.memory_changes.push_back({addr, old_bytes_vec, new_bytes_vec});
    }
}

void RVSSProcessor::WriteMemoryFloat()
{
    uint8_t rs2 = (current_instruction_ >> 20) & 0b11111;

    if (control_unit_.GetMemRead())
    { // FLW
        memory_result_ = memory_controller_.readWord(execution_result_);
    }

    uint64_t addr = 0;
    std::vector<uint8_t> old_bytes_vec;
    std::vector<uint8_t> new_bytes_vec;

    if (control_unit_.GetMemWrite())
    { // FSW
        addr = execution_result_;
        for (size_t i = 0; i < 4; ++i)
        {
            old_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
        }
        uint32_t val = registers_.ReadFpr(rs2) & 0xFFFFFFFF;
        memory_controller_.writeWord(execution_result_, val);
        // new_bytes_vec.push_back(memory_controller_.ReadByte(addr));
        for (size_t i = 0; i < 4; ++i)
        {
            new_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
        }
    }

    if (old_bytes_vec != new_bytes_vec)
    {
        current_delta_.memory_changes.push_back({addr, old_bytes_vec, new_bytes_vec});
    }
}

void RVSSProcessor::WriteMemoryDouble()
{
    uint8_t rs2 = (current_instruction_ >> 20) & 0b11111;

    if (control_unit_.GetMemRead())
    { // FLD
        memory_result_ = memory_controller_.readDoubleWord(execution_result_);
    }

    uint64_t addr = 0;
    std::vector<uint8_t> old_bytes_vec;
    std::vector<uint8_t> new_bytes_vec;

    if (control_unit_.GetMemWrite())
    { // FSD
        addr = execution_result_;
        for (size_t i = 0; i < 8; ++i)
        {
            old_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
        }
        memory_controller_.writeDoubleWord(execution_result_, registers_.ReadFpr(rs2));
        for (size_t i = 0; i < 8; ++i)
        {
            new_bytes_vec.push_back(memory_controller_.peekByte(addr + i));
        }
    }

    if (old_bytes_vec != new_bytes_vec)
    {
        current_delta_.memory_changes.push_back({addr, old_bytes_vec, new_bytes_vec});
    }
}

void RVSSProcessor::WriteMemoryVector()
{
    if (!vector_mem_pending_)
        return;

    const uint64_t vl =
        registers_.ReadCsr(RegisterFile::CSR_VL);

    const uint64_t vstart =
        registers_.ReadCsr(RegisterFile::CSR_VSTART);

    const size_t element_bytes = vector_mem_element_bytes_;

    const size_t max_elements =
        RegisterFile::VLEN_BYTES / element_bytes;

    for (uint64_t i = vstart; i < vl && i < max_elements; ++i)
    {
        const size_t offset = i * element_bytes;

        // vm=1 means unmasked; otherwise check bit i in v0.
        const bool mask_active =
            vector_mem_vm_ == 1 ||
            ((vector_mem_mask_value_[i / 8] >> (i % 8)) & 0x01) != 0;

        if (!mask_active)
        {
            if (vector_mem_load_ && vector_mem_mask_agnostic_)
            {
                // Mask-agnostic load: inactive destination elements become all ones.
                for (size_t b = 0; b < element_bytes; ++b)
                {
                    vector_mem_new_value_[offset + b] = 0xFF;
                }
            }

            // Masked-off stores never access memory.
            continue;
        }

        const uint64_t address =
            vector_mem_base_ + i * element_bytes;

        if (vector_mem_load_)
        {
            // Load the active element into the pending destination.
            for (size_t b = 0; b < element_bytes; ++b)
            {
                vector_mem_new_value_[offset + b] =
                    memory_controller_.readByte(address + b);
            }
        }
        else
        {
            // Record old memory contents for Undo.
            std::vector<uint8_t> old_bytes;
            std::vector<uint8_t> new_bytes;

            for (size_t b = 0; b < element_bytes; ++b)
            {
                old_bytes.push_back(
                    memory_controller_.peekByte(address + b));

                new_bytes.push_back(
                    vector_mem_old_value_[offset + b]);
            }

            // Write the active vector element to memory.
            for (size_t b = 0; b < element_bytes; ++b)
            {
                memory_controller_.writeByte(
                    address + b,
                    new_bytes[b]);
            }

            if (old_bytes != new_bytes)
            {
                current_delta_.memory_changes.push_back(
                    {address, old_bytes, new_bytes});
            }
        }
    }

    // Tail-agnostic behavior applies to loads, not stores.
    if (vector_mem_load_ && vector_mem_tail_agnostic_)
    {
        for (uint64_t i = vl; i < max_elements; ++i)
        {
            const size_t offset = i * element_bytes;

            for (size_t b = 0; b < element_bytes; ++b)
            {
                vector_mem_new_value_[offset + b] = 0xFF;
            }
        }
    }
}

void RVSSProcessor::WriteBack()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t funct3 = (current_instruction_ >> 12) & 0b111;
    uint8_t rd = (current_instruction_ >> 7) & 0b11111;
    int32_t imm = ImmGenerator(current_instruction_);

    if (opcode == 0x57 &&
    funct3 == 0b111 &&
    ((current_instruction_ >> 31) & 1) == 0)
    {
        WriteBackVsetvli();
        return;
    }
    // Handle vector arithmetic writeback (OPIVV and OPIVX).
    if (opcode == 0x57 && (funct3 == 0b000 || funct3 == 0b100))
    {
        WriteBackVectorArithmetic();
        return;
    }
    // Handle vector reduction writeback.
    if (opcode == 0x57 && funct3 == 0b010)
    {
         WriteBackVectorReduction();
        return;
    }
    // Handle vector memory instructions.
    if ((opcode == 0x07 || opcode == 0x27) &&
    (funct3 == 0b000 || funct3 == 0b101 ||
     funct3 == 0b110 || funct3 == 0b111))
    {
        WriteBackVectorMemory();
        return;
    }
    if (opcode == 0b1110011 && funct3 == 0b000)
    {
        return;
    }

    if (instruction_set::isFInstruction(current_instruction_))
    { // RV64 F
        WriteBackFloat();
        return;
    }
    else if (instruction_set::isDInstruction(current_instruction_))
    {
        WriteBackDouble();
        return;
    }
    else if (opcode == 0b1110011)
    { // CSR opcode
        WriteBackCsr();
        return;
    }

    uint64_t old_reg = registers_.ReadGpr(rd);
    unsigned int reg_index = rd;
    unsigned int reg_type = 0; // 0 for GPR, 1 for CSR, 2 for FPR

    if (control_unit_.GetRegWrite())
    {
        switch (opcode)
        {
        case 0b0110011: // R-Type
        case 0b0010011: // I-Type
        case 0b0010111: // AUIPC
        case 0b0111011: // R4-Type / OP-32 (ADDW, SUBW, MULW, DIVW, DIVUW, REMW, REMUW, SLLW, SRLW, SRAW)
        case 0b0011011: // I-type OP-32 (ADDIW, SLLIW, SRLIW, SRAIW)
        {
            registers_.WriteGpr(rd, execution_result_);
            break;
        }
        case 0b0000011:
        { // Load
            registers_.WriteGpr(rd, memory_result_);
            break;
        }
        case 0b1100111: // JALR
        case 0b1101111:
        { // JAL
            registers_.WriteGpr(rd, next_pc_);
            break;
        }
        case 0b0110111:
        { // LUI
            registers_.WriteGpr(rd, (imm << 12));
            break;
        }
        default:
            break;
        }
    }

    if (opcode == 0b1101111)
    { // JAL
      // Updated in Execute()
    }
    if (opcode == 0b1100111)
    { // JALR
      // registers_.WriteGpr(rd, return_address_); // Write back to rs1
      // Updated in Execute()
    }

    uint64_t new_reg = registers_.ReadGpr(rd);
    if (old_reg != new_reg)
    {
        current_delta_.register_changes.push_back({reg_index, reg_type, old_reg, new_reg});
    }
}

void RVSSProcessor::WriteBackFloat()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t funct7 = (current_instruction_ >> 25) & 0b1111111;
    uint8_t rd = (current_instruction_ >> 7) & 0b11111;

    uint64_t old_reg = 0;
    unsigned int reg_index = rd;
    unsigned int reg_type = 2; // 0 for GPR, 1 for CSR, 2 for FPR
    uint64_t new_reg = 0;

    if (control_unit_.GetRegWrite())
    {
        // write to GPR
        if (funct7 == 0b1010000 || funct7 == 0b1100000 || funct7 == 0b1110000)
        { // f(eq|lt|le).s, fcvt.(w|wu|l|lu).s
            old_reg = registers_.ReadGpr(rd);
            registers_.WriteGpr(rd, execution_result_);
            new_reg = execution_result_;
            reg_type = 0; // GPR
        }
        // write to FPR
        else if (opcode == 0b0000111)
        {
            old_reg = registers_.ReadFpr(rd);
            registers_.WriteFpr(rd, memory_result_);
            new_reg = memory_result_;
            reg_type = 2; // FPR
        }
        else
        {
            old_reg = registers_.ReadFpr(rd);
            registers_.WriteFpr(rd, execution_result_);
            new_reg = execution_result_;
            reg_type = 2; // FPR
        }
    }

    if (old_reg != new_reg)
    {
        current_delta_.register_changes.push_back({reg_index, reg_type, old_reg, new_reg});
    }
}

void RVSSProcessor::WriteBackDouble()
{
    uint8_t opcode = current_instruction_ & 0b1111111;
    uint8_t funct7 = (current_instruction_ >> 25) & 0b1111111;
    uint8_t rd = (current_instruction_ >> 7) & 0b11111;

    uint64_t old_reg = 0;
    unsigned int reg_index = rd;
    unsigned int reg_type = 2; // 0 for GPR, 1 for CSR, 2 for FPR
    uint64_t new_reg = 0;

    if (control_unit_.GetRegWrite())
    {
        // write to GPR
        if (funct7 == 0b1010001 || funct7 == 0b1100001 || funct7 == 0b1110001)
        { // f(eq|lt|le).d, fcvt.(w|wu|l|lu).d
            old_reg = registers_.ReadGpr(rd);
            registers_.WriteGpr(rd, execution_result_);
            new_reg = execution_result_;
            reg_type = 0; // GPR
        }
        // write to FPR
        else if (opcode == 0b0000111)
        {
            old_reg = registers_.ReadFpr(rd);
            registers_.WriteFpr(rd, memory_result_);
            new_reg = memory_result_;
            reg_type = 2; // FPR
        }
        else
        {
            old_reg = registers_.ReadFpr(rd);
            registers_.WriteFpr(rd, execution_result_);
            new_reg = execution_result_;
            reg_type = 2; // FPR
        }
    }

    if (old_reg != new_reg)
    {
        current_delta_.register_changes.push_back({reg_index, reg_type, old_reg, new_reg});
    }

    return;
}

void RVSSProcessor::WriteBackCsr()
{
    uint8_t rd = (current_instruction_ >> 7) & 0b11111;
    uint8_t funct3 = (current_instruction_ >> 12) & 0b111;

    uint64_t old_gpr = registers_.ReadGpr(rd);
    uint64_t old_csr = registers_.ReadCsr(csr_target_address_);

    switch (funct3)
    {
    case 0b001:
    { // CSRRW
        registers_.WriteGpr(rd, csr_old_value_);
        registers_.WriteCsr(csr_target_address_, csr_write_val_);
        break;
    }
    case 0b010:
    { // CSRRS
        registers_.WriteGpr(rd, csr_old_value_);
        if (csr_write_val_ != 0)
        {
            registers_.WriteCsr(csr_target_address_, csr_old_value_ | csr_write_val_);
        }
        break;
    }
    case 0b011:
    { // CSRRC
        registers_.WriteGpr(rd, csr_old_value_);
        if (csr_write_val_ != 0)
        {
            registers_.WriteCsr(csr_target_address_, csr_old_value_ & ~csr_write_val_);
        }
        break;
    }
    case 0b101:
    { // CSRRWI
        registers_.WriteGpr(rd, csr_old_value_);
        registers_.WriteCsr(csr_target_address_, csr_uimm_);
        break;
    }
    case 0b110:
    { // CSRRSI
        registers_.WriteGpr(rd, csr_old_value_);
        if (csr_uimm_ != 0)
        {
            registers_.WriteCsr(csr_target_address_, csr_old_value_ | csr_uimm_);
        }
        break;
    }
    case 0b111:
    { // CSRRCI
        registers_.WriteGpr(rd, csr_old_value_);
        if (csr_uimm_ != 0)
        {
            registers_.WriteCsr(csr_target_address_, csr_old_value_ & ~csr_uimm_);
        }
        break;
    }
    }

    uint64_t new_gpr = registers_.ReadGpr(rd);
    uint64_t new_csr = registers_.ReadCsr(csr_target_address_);

    if (old_gpr != new_gpr)
    {
        current_delta_.register_changes.push_back({rd, 0, old_gpr, new_gpr});
    }
    if (old_csr != new_csr)
    {
        current_delta_.register_changes.push_back({csr_target_address_, 1, old_csr, new_csr});
    }
}

void RVSSProcessor::WriteBackVsetvli()
{
    // Update vl.
    uint64_t old_vl = registers_.ReadCsr(RegisterFile::CSR_VL);
    registers_.WriteCsr(RegisterFile::CSR_VL, vector_new_vl_);

    if (old_vl != vector_new_vl_)
    {
        current_delta_.register_changes.push_back(
            {RegisterFile::CSR_VL, 1, old_vl, vector_new_vl_}
        );
    }

    // Update vtype.
    uint64_t old_vtype = registers_.ReadCsr(RegisterFile::CSR_VTYPE);
    registers_.WriteCsr(RegisterFile::CSR_VTYPE, vector_new_vtype_);

    if (old_vtype != vector_new_vtype_)
    {
        current_delta_.register_changes.push_back(
            {RegisterFile::CSR_VTYPE, 1, old_vtype, vector_new_vtype_}
        );
    }

    // Reset vstart to zero.
    uint64_t old_vstart = registers_.ReadCsr(RegisterFile::CSR_VSTART);
    registers_.WriteCsr(RegisterFile::CSR_VSTART, 0);

    if (old_vstart != 0)
    {
        current_delta_.register_changes.push_back(
            {RegisterFile::CSR_VSTART, 1, old_vstart, 0}
        );
    }

    // Write the resulting vl into rd, unless rd is x0.
    if (vector_rd_ != 0)
    {
        uint64_t old_rd = registers_.ReadGpr(vector_rd_);
        registers_.WriteGpr(vector_rd_, vector_new_vl_);

        if (old_rd != vector_new_vl_)
        {
            current_delta_.register_changes.push_back(
                {vector_rd_, 0, old_rd, vector_new_vl_}
            );
        }
    }
}
void RVSSProcessor::WriteBackVectorArithmetic()
{
    if (!vector_arithmetic_pending_)
        return;

    // Write the calculated result to the destination register.
    registers_.WriteVreg(vector_vd_, vector_new_value_);

    // Record the change for Undo/Redo.
    if (vector_old_value_ != vector_new_value_)
    {
        current_delta_.vector_register_changes.push_back(
            {vector_vd_, vector_old_value_, vector_new_value_});
    }

    // A successfully completed vector instruction resets vstart.
    uint64_t old_vstart =
        registers_.ReadCsr(RegisterFile::CSR_VSTART);

    if (old_vstart != 0)
    {
        registers_.WriteCsr(RegisterFile::CSR_VSTART, 0);

        current_delta_.register_changes.push_back(
            {RegisterFile::CSR_VSTART, 1, old_vstart, 0});
    }

    vector_arithmetic_pending_ = false;
}
void RVSSProcessor::WriteBackVectorMemory()
{
    if (!vector_mem_pending_)
        return;

    // Loads update the destination vector register.
    if (vector_mem_load_)
    {
        registers_.WriteVreg(
            vector_mem_reg_,
            vector_mem_new_value_);

        if (vector_mem_old_value_ != vector_mem_new_value_)
        {
            current_delta_.vector_register_changes.push_back(
                {
                    vector_mem_reg_,
                    vector_mem_old_value_,
                    vector_mem_new_value_
                });
        }
    }

    // A successfully completed vector instruction resets vstart.
    const uint64_t old_vstart =
        registers_.ReadCsr(RegisterFile::CSR_VSTART);

    if (old_vstart != 0)
    {
        registers_.WriteCsr(RegisterFile::CSR_VSTART, 0);

        current_delta_.register_changes.push_back(
            {
                RegisterFile::CSR_VSTART,
                1,
                old_vstart,
                0
            });
    }

    vector_mem_pending_ = false;
}


void RVSSProcessor::WriteBackVectorReduction()
{
    if (!vector_reduction_pending_)
        return;

    registers_.WriteVreg(vector_vd_, vector_new_value_);

    if (vector_old_value_ != vector_new_value_)
    {
        current_delta_.vector_register_changes.push_back(
            {vector_vd_, vector_old_value_, vector_new_value_});
    }

    vector_reduction_pending_ = false;
}


void RVSSProcessor::DebugRun()
{
    ClearStop();
    while (!stop_requested_ && program_counter_ < program_size_)
    {
        current_delta_.old_pc = program_counter_;
        if (std::find(breakpoints_.begin(), breakpoints_.end(), program_counter_) ==
            breakpoints_.end())
        {
            Fetch();
            Decode();
            Execute();
            WriteMemory();
            WriteBack();
            instructions_retired_++;
            cycle_s_++;
            std::cout << "Program Counter: " << program_counter_ << std::endl;

            current_delta_.new_pc = program_counter_;
            // history_.push(current_delta_);
            undo_stack_.push(current_delta_);
            while (!redo_stack_.empty())
            {
                redo_stack_.pop();
            }
            current_delta_ = RVSingleStageStepDelta();
            if (program_counter_ < program_size_)
            {
                std::cout << "VM_STEP_COMPLETED" << std::endl;
                output_status_ = "VM_STEP_COMPLETED";
            }
            else if (program_counter_ >= program_size_)
            {
                std::cout << "VM_LAST_INSTRUCTION_STEPPED" << std::endl;
                output_status_ = "VM_LAST_INSTRUCTION_STEPPED";
            }
            DumpRegisters(globals::registers_dump_file_path, registers_);
            DumpState(globals::vm_state_dump_file_path);
            // update circuit UI after the debug step
            SetActiveWireNames();
            emit updateCircuitStateSignal(active_wires_);

            //unsigned int delay_ms = vm_config::config.getRunStepDelay();
            // std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        else
        {
            std::cout << "VM_BREAKPOINT_HIT " << program_counter_ << std::endl;
            output_status_ = "VM_BREAKPOINT_HIT";
            break;
        }
    }
    if (program_counter_ >= program_size_)
    {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}

void RVSSProcessor::Step()
{
    current_delta_.old_pc = program_counter_;
    if (program_counter_ < program_size_)
    {
        Fetch();
        Decode();
        Execute();
        WriteMemory();
        WriteBack();
        instructions_retired_++;
        cycle_s_++;
        std::cout << "Program Counter: " << std::hex << program_counter_ << std::dec << std::endl;

        current_delta_.new_pc = program_counter_;

        // history_.push(current_delta_);

        undo_stack_.push(current_delta_);
        while (!redo_stack_.empty())
        {
            redo_stack_.pop();
        }

        current_delta_ = RVSingleStageStepDelta();

        if (program_counter_ < program_size_)
        {
            std::cout << "VM_STEP_COMPLETED" << std::endl;
            output_status_ = "VM_STEP_COMPLETED";
        }
        else if (program_counter_ >= program_size_)
        {
            std::cout << "VM_LAST_INSTRUCTION_STEPPED" << std::endl;
            output_status_ = "VM_LAST_INSTRUCTION_STEPPED";
        }
    }
    else if (program_counter_ >= program_size_)
    {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);

}

bool RVSSProcessor::StepAndNotify()
{
    QMutexLocker locker(&step_mutex_);
    if (program_counter_ >= program_size_)
    {
        return false;
    }
    last_executed_pc_ = program_counter_;
    Step();
    SetActiveWireNames();
    setProcessorState();
    emit processorClockedSignal(processor_state_);
    emit updateCircuitStateSignal(active_wires_);
    active_wires_.erase(active_wires_.begin() + always_active_wires_count_,
                        active_wires_.end());
    // clear active wires after emitting signal except always active wires
    return program_counter_ < program_size_;
}

void RVSSProcessor::Undo()
{
    qInfo() << "Attempting to undo last step in rvss";
    if (undo_stack_.empty())
    {
        std::cout << "VM_NO_MORE_UNDO" << std::endl;
        output_status_ = "VM_NO_MORE_UNDO";
        return;
    }

    StepDelta last = undo_stack_.top();
    undo_stack_.pop();

    for (const auto &change : last.register_changes)
    {
        switch (change.reg_type)
        {
        case 0:
        { // GPR
            registers_.WriteGpr(change.reg_index, change.old_value);
            break;
        }
        case 1:
        { // CSR
            registers_.WriteCsr(change.reg_index, change.old_value);
            break;
        }
        case 2:
        { // FPR
            registers_.WriteFpr(change.reg_index, change.old_value);
            break;
        }
        default:
            std::cerr << "Invalid register type: " << change.reg_type << std::endl;
            break;
        }
    }
    for (const auto &change : last.vector_register_changes)
    {
        registers_.WriteVreg(change.reg_index, change.old_value);
    }
    for (const auto &change : last.memory_changes)
    {
        for (size_t i = 0; i < change.old_bytes_vec.size(); ++i)
        {
            memory_controller_.writeByte(change.address + i, change.old_bytes_vec[i]);
        }
    }

    program_counter_ = last.old_pc;
    instructions_retired_--;
    cycle_s_--;
    std::cout << "Program Counter: " << program_counter_ << std::endl;

    redo_stack_.push(last);

    output_status_ = "VM_UNDO_COMPLETED";
    std::cout << "VM_UNDO_COMPLETED" << std::endl;

    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);

    SetActiveWireNames();
    setProcessorState();
    emit updateCircuitStateSignal(active_wires_);
    emit processorClockedSignal(processor_state_);
    qInfo() << "Undo completed in rvss";
}

void RVSSProcessor::Redo()
{
    qInfo() << "Attempting to redo last undone step in rvss";
    if (redo_stack_.empty())
    {
        std::cout << "VM_NO_MORE_REDO" << std::endl;
        return;
    }

    StepDelta next = redo_stack_.top();
    redo_stack_.pop();

    // if (!history_.can_redo()) {
    //       std::cout << "Nothing to redo.\n";
    //       return;
    //   }

    //   StepDelta next = history_.redo();

    for (const auto &change : next.register_changes)
    {
        switch (change.reg_type)
        {
        case 0:
        { // GPR
            registers_.WriteGpr(change.reg_index, change.new_value);
            break;
        }
        case 1:
        { // CSR
            registers_.WriteCsr(change.reg_index, change.new_value);
            break;
        }
        case 2:
        { // FPR
            registers_.WriteFpr(change.reg_index, change.new_value);
            break;
        }
        default:
            std::cerr << "Invalid register type: " << change.reg_type << std::endl;
            break;
        }
    }
    for (const auto &change : next.vector_register_changes)
    {
        registers_.WriteVreg(change.reg_index, change.new_value);
    }
    for (const auto &change : next.memory_changes)
    {
        for (size_t i = 0; i < change.new_bytes_vec.size(); ++i)
        {
            memory_controller_.writeByte(change.address + i, change.new_bytes_vec[i]);
        }
    }

    program_counter_ = next.new_pc;
    instructions_retired_++;
    cycle_s_++;
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
    std::cout << "Program Counter: " << program_counter_ << std::endl;
    undo_stack_.push(next);

    SetActiveWireNames();
    setProcessorState();
    emit updateCircuitStateSignal(active_wires_);
    emit processorClockedSignal(processor_state_);
}

void RVSSProcessor::Reset()
{
    program_counter_ = 0;
    instructions_retired_ = 0;
    cycle_s_ = 0;
    last_breakpoint_pc_ = UINT64_MAX; // Clear breakpoint tracking on reset
    registers_.Reset();
    memory_controller_.reset();
    control_unit_.Reset();
    branch_flag_ = false;
    next_pc_ = 0;
    execution_result_ = 0;
    memory_result_ = 0;

    return_address_ = 0;
    csr_target_address_ = 0;
    csr_old_value_ = 0;
    csr_write_val_ = 0;
    csr_uimm_ = 0;
    current_delta_.register_changes.clear();
    current_delta_.memory_changes.clear();
    current_delta_.old_pc = 0;
    current_delta_.new_pc = 0;
    undo_stack_ = std::stack<StepDelta>();
    redo_stack_ = std::stack<StepDelta>();

    vector_arithmetic_pending_ = false;
    vector_vd_ = 0;
    vector_old_value_.fill(0);
    vector_new_value_.fill(0);

    vector_mem_reg_ = 0;
    vector_mem_base_ = 0;
    vector_mem_element_bytes_ = 0;
    vector_mem_load_ = false;
    vector_mem_pending_ = false;
    vector_mem_old_value_.fill(0);
    vector_mem_new_value_.fill(0);
    vector_mem_vm_ = 1;
    vector_mem_tail_agnostic_ = false;
    vector_mem_mask_agnostic_ = false;
    vector_mem_mask_value_.fill(0);

    current_delta_.vector_register_changes.clear();
}
}//namespaace Kites
