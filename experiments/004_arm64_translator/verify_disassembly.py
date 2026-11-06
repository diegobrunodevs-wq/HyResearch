from capstone import Cs, CS_ARCH_X86, CS_MODE_64

# Bytes gerados para MOV X0, X1
code = bytes.fromhex("48 8B 41 08 48 89 01")

disassembler = Cs(CS_ARCH_X86, CS_MODE_64)

print("Desassemblagem independente:\n")

for instruction in disassembler.disasm(code, 0x1000):
    print(
        f"{instruction.address:08X}: "
        f"{instruction.bytes.hex(' '):<20} "
        f"{instruction.mnemonic} {instruction.op_str}"
    )