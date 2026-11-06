/**
 * @file registers.h
 * @brief Contains the definition of the RegisterFile class for managing registers in the simulator.
 * @author Vishank Singh, https://github.com/VishankSingh
 */

#ifndef REGISTERS_H
#define REGISTERS_H

#include <QObject>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace Kites
{
/**
 * @brief Represents a register file containing integer, floating-point, and vector registers.
 */
class RegisterFile : public QObject
{
    Q_OBJECT
  public:
    static constexpr size_t NUM_VREG = 32;
    static constexpr size_t VLEN_BITS = 128;
    static constexpr size_t VLEN_BYTES = VLEN_BITS / 8;

    using VectorRegister = std::array<uint8_t, VLEN_BYTES>;
    
    template <typename T>
    [[nodiscard]] T ReadVregElement(size_t reg, size_t element) const
    {
        static_assert(
            std::is_same_v<T, uint8_t> ||
            std::is_same_v<T, uint16_t> ||
            std::is_same_v<T, uint32_t> ||
            std::is_same_v<T, uint64_t>,
            "Unsupported vector element type"
        );

        if (reg >= NUM_VREG)
            throw std::out_of_range("Invalid vector register index");

        if (element >= VLEN_BYTES / sizeof(T))
            throw std::out_of_range("Invalid vector element index");

        size_t offset = element * sizeof(T);
        uint64_t value = 0;

        for (size_t i = 0; i < sizeof(T); ++i)
        {
            value |= static_cast<uint64_t>(vreg_[reg][offset + i]) << (8 * i);
        }

        return static_cast<T>(value);
    }

    template <typename T>
    void WriteVregElement(size_t reg, size_t element, T value)
    {
        static_assert(
            std::is_same_v<T, uint8_t> ||
            std::is_same_v<T, uint16_t> ||
            std::is_same_v<T, uint32_t> ||
            std::is_same_v<T, uint64_t>,
            "Unsupported vector element type"
        );

        if (reg >= NUM_VREG)
            throw std::out_of_range("Invalid vector register index");

        if (element >= VLEN_BYTES / sizeof(T))
            throw std::out_of_range("Invalid vector element index");

        size_t offset = element * sizeof(T);

        for (size_t i = 0; i < sizeof(T); ++i)
        {
            vreg_[reg][offset + i] =
                static_cast<uint8_t>((static_cast<uint64_t>(value) >> (8 * i)) & 0xFF);
        }
    }

  private:
    static constexpr size_t NUM_GPR = 32; ///< Number of General-Purpose Registers (GPR).
    static constexpr size_t NUM_FPR = 32; ///< Number of Floating-Point Registers (FPR).

    std::array<uint64_t, NUM_GPR> gpr_ = {}; ///< Array for storing GPR values.
    std::array<uint64_t, NUM_FPR> fpr_ = {}; ///< Array for storing FPR values.

    static constexpr size_t NUM_CSR = 4096; ///< Number of Control and Status Registers (CSR).

    std::array<uint64_t, NUM_CSR> csr_ = {}; ///< Array for storing CSR values.

    std::array<VectorRegister, NUM_VREG> vreg_ = {};

  public:
    /**
     * @brief Enum representing the type of a register.
     */
    enum class RegisterType
    {
        INVALID,        ///< Invalid register type.
        INTEGER,        ///< General-purpose integer register.
        FLOATING_POINT, ///< Floating-point register.
        VECTOR,         ///< Vector register.
        CSR             ///< Control and Status Register (CSR).
    };

    explicit RegisterFile(QObject *parent = nullptr);
    virtual ~RegisterFile() = default;

    void Reset();

    /**
     * @brief Reads the value of a General-Purpose Register (GPR).
     * @param reg The index of the GPR to read.
     * @return The value of the GPR at the specified index.
     */
    [[nodiscard]] uint64_t ReadGpr(size_t reg) const;

    /**
     * @brief Writes a value to a General-Purpose Register (GPR).
     * @param reg The index of the GPR to write.
     * @param value The value to write.
     */
    void WriteGpr(size_t reg, uint64_t value);

    /**
     * @brief Reads the value of a Floating-Point Register (FPR).
     * @param reg The index of the FPR to read.
     * @return The value of the FPR at the specified index.
     */
    [[nodiscard]] uint64_t ReadFpr(size_t reg) const;

    /**
     * @brief Writes a value to a Floating-Point Register (FPR).
     * @param reg The index of the FPR to write.
     * @param value The value to write.
     */
    void WriteFpr(size_t reg, uint64_t value);

    [[nodiscard]] uint64_t ReadCsr(size_t reg) const;

    void WriteCsr(size_t reg, uint64_t value);
    
    /**
     * @brief Reads the contents of a Vector Register.
     * @param reg The index of the vector register.
     * @return All bytes stored in the vector register.
     */
    [[nodiscard]] VectorRegister ReadVreg(size_t reg) const;

    /**
     * @brief Writes a value to a Vector Register.
     * @param reg The index of the vector register.
     * @param value The 16-byte value to write.
     */
    void WriteVreg(size_t reg, const VectorRegister &value);

    /**
     * @brief Retrieves the values of all General-Purpose Registers (GPR).
     * @return A vector containing the values of all GPRs.
     */
    [[nodiscard]] std::vector<uint64_t> GetGprValues() const;


    /**
     * @brief Retrieves the values of all Floating-Point Registers (FPR).
     * @return A vector containing the values of all FPRs.
     */
    [[nodiscard]] std::vector<uint64_t> GetFprValues() const;

    void ModifyRegister(const std::string &reg_name, uint64_t value);

  signals:
    void updateRegister(size_t regIndex, uint64_t value);
    void updateFRegister(size_t regIndex, uint64_t value);
    void registerResetSignal();
};

extern const std::unordered_set<std::string> valid_general_purpose_registers;

extern const std::unordered_set<std::string> valid_floating_point_registers;

extern const std::unordered_set<std::string> valid_csr_registers;

extern const std::unordered_map<std::string, int> csr_to_address;

/**
 * @brief Map of register aliases to their actual names.
 */
extern const std::unordered_map<std::string, std::string> reg_alias_to_name;

bool IsValidGeneralPurposeRegister(const std::string &reg);

bool IsValidFloatingPointRegister(const std::string &reg);

bool IsValidCsr(const std::string &reg);
}//namespace Kites
#endif // REGISTERS_H