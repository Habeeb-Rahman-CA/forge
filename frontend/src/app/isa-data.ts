export interface IsaInstruction {
  opcodeHex: string;
  opcodeDec: number;
  mnemonic: string;
  category: 'Control' | 'Data' | 'Arithmetic' | 'Bitwise' | 'Shift' | 'Compare' | 'Control Flow' | 'Stack' | 'I/O';
  allowedModes: string;
  cycles: number;
  flags: string;
  syntax: string;
  description: string;
}

export const FORGE_ISA_TABLE: IsaInstruction[] = [
  {
    opcodeHex: '0x00',
    opcodeDec: 0,
    mnemonic: 'NOP',
    category: 'Control',
    allowedModes: 'REG',
    cycles: 1,
    flags: 'None',
    syntax: 'NOP',
    description: 'No operation. Consumes 1 clock cycle and advances PC by 4.'
  },
  {
    opcodeHex: '0x01',
    opcodeDec: 1,
    mnemonic: 'HALT',
    category: 'Control',
    allowedModes: 'REG',
    cycles: 1,
    flags: 'None',
    syntax: 'HALT',
    description: 'Stops CPU execution cleanly; transitions state to halted.'
  },
  {
    opcodeHex: '0x02',
    opcodeDec: 2,
    mnemonic: 'MOV',
    category: 'Data',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'None',
    syntax: 'MOV Rd, Rs | MOV Rd, imm16',
    description: 'Copies 16-bit register value or immediate constant into Rd.'
  },
  {
    opcodeHex: '0x03',
    opcodeDec: 3,
    mnemonic: 'LOAD',
    category: 'Data',
    allowedModes: 'MEM_DIR, MEM_IDX',
    cycles: 2,
    flags: 'None',
    syntax: 'LOAD Rd, [imm16] | LOAD Rd, [Rs + imm16]',
    description: 'Loads 16-bit word from memory at effective address EA into Rd.'
  },
  {
    opcodeHex: '0x04',
    opcodeDec: 4,
    mnemonic: 'STORE',
    category: 'Data',
    allowedModes: 'MEM_DIR, MEM_IDX',
    cycles: 2,
    flags: 'None',
    syntax: 'STORE Rd, [imm16] | STORE Rd, [Rs + imm16]',
    description: 'Stores 16-bit word in Rd into memory at effective address EA.'
  },
  {
    opcodeHex: '0x05',
    opcodeDec: 5,
    mnemonic: 'ADD',
    category: 'Arithmetic',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N, C, V',
    syntax: 'ADD Rd, Rs | ADD Rd, imm16',
    description: 'Adds source to Rd. Updates Zero, Negative, Carry, and Overflow flags.'
  },
  {
    opcodeHex: '0x06',
    opcodeDec: 6,
    mnemonic: 'SUB',
    category: 'Arithmetic',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N, C, V',
    syntax: 'SUB Rd, Rs | SUB Rd, imm16',
    description: 'Subtracts source from Rd. Updates Zero, Negative, Borrow (C), and Overflow flags.'
  },
  {
    opcodeHex: '0x07',
    opcodeDec: 7,
    mnemonic: 'MUL',
    category: 'Arithmetic',
    allowedModes: 'REG, IMM',
    cycles: 3,
    flags: 'Z, N, C, V',
    syntax: 'MUL Rd, Rs | MUL Rd, imm16',
    description: 'Multiplies Rd by source. Stores low 16 bits of product in Rd.'
  },
  {
    opcodeHex: '0x08',
    opcodeDec: 8,
    mnemonic: 'DIV',
    category: 'Arithmetic',
    allowedModes: 'REG, IMM',
    cycles: 4,
    flags: 'Z, N, C, V',
    syntax: 'DIV Rd, Rs | DIV Rd, imm16',
    description: 'Divides Rd by source (unsigned 16-bit). Faults with DIV_BY_ZERO if divisor is 0.'
  },
  {
    opcodeHex: '0x09',
    opcodeDec: 9,
    mnemonic: 'AND',
    category: 'Bitwise',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N (C=0, V=0)',
    syntax: 'AND Rd, Rs | AND Rd, imm16',
    description: 'Bitwise AND between Rd and source. Clears Carry and Overflow.'
  },
  {
    opcodeHex: '0x0A',
    opcodeDec: 10,
    mnemonic: 'OR',
    category: 'Bitwise',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N (C=0, V=0)',
    syntax: 'OR Rd, Rs | OR Rd, imm16',
    description: 'Bitwise inclusive OR between Rd and source. Clears Carry and Overflow.'
  },
  {
    opcodeHex: '0x0B',
    opcodeDec: 11,
    mnemonic: 'XOR',
    category: 'Bitwise',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N (C=0, V=0)',
    syntax: 'XOR Rd, Rs | XOR Rd, imm16',
    description: 'Bitwise exclusive OR between Rd and source. Clears Carry and Overflow.'
  },
  {
    opcodeHex: '0x0C',
    opcodeDec: 12,
    mnemonic: 'NOT',
    category: 'Bitwise',
    allowedModes: 'REG',
    cycles: 1,
    flags: 'Z, N (C=0, V=0)',
    syntax: 'NOT Rd',
    description: 'Bitwise NOT (one\'s complement) of Rd: Rd <- ~Rd.'
  },
  {
    opcodeHex: '0x0D',
    opcodeDec: 13,
    mnemonic: 'SHL',
    category: 'Shift',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N, C (V=0)',
    syntax: 'SHL Rd, Rs | SHL Rd, imm16',
    description: 'Logical left shift of Rd by shift count (0..15). Last bit shifted out enters Carry.'
  },
  {
    opcodeHex: '0x0E',
    opcodeDec: 14,
    mnemonic: 'SHR',
    category: 'Shift',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N, C (V=0)',
    syntax: 'SHR Rd, Rs | SHR Rd, imm16',
    description: 'Logical right shift of Rd by shift count (0..15). Zero-filled from left.'
  },
  {
    opcodeHex: '0x0F',
    opcodeDec: 15,
    mnemonic: 'CMP',
    category: 'Compare',
    allowedModes: 'REG, IMM',
    cycles: 1,
    flags: 'Z, N, C, V',
    syntax: 'CMP Rd, Rs | CMP Rd, imm16',
    description: 'Compares Rd with source by computing Rd - source; updates flags without modifying Rd.'
  },
  {
    opcodeHex: '0x10',
    opcodeDec: 16,
    mnemonic: 'JMP',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JMP label | JMP Rs',
    description: 'Unconditional jump to target address in immediate or register.'
  },
  {
    opcodeHex: '0x11',
    opcodeDec: 17,
    mnemonic: 'JZ',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JZ label | JZ Rs',
    description: 'Conditional jump if Zero flag is set (Z == 1).'
  },
  {
    opcodeHex: '0x12',
    opcodeDec: 18,
    mnemonic: 'JNZ',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JNZ label | JNZ Rs',
    description: 'Conditional jump if Zero flag is cleared (Z == 0).'
  },
  {
    opcodeHex: '0x13',
    opcodeDec: 19,
    mnemonic: 'CALL',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 3,
    flags: 'None',
    syntax: 'CALL label | CALL Rs',
    description: 'Pushes return address (PC + 4) onto stack and branches to subroutine.'
  },
  {
    opcodeHex: '0x14',
    opcodeDec: 20,
    mnemonic: 'RET',
    category: 'Control Flow',
    allowedModes: 'REG',
    cycles: 3,
    flags: 'None',
    syntax: 'RET',
    description: 'Pops return address from stack into PC, resuming caller execution.'
  },
  {
    opcodeHex: '0x15',
    opcodeDec: 21,
    mnemonic: 'PUSH',
    category: 'Stack',
    allowedModes: 'REG, IMM',
    cycles: 2,
    flags: 'None',
    syntax: 'PUSH Rs | PUSH imm16',
    description: 'Decrements SP by 2 and writes word to [SP]. Faults on stack overflow.'
  },
  {
    opcodeHex: '0x16',
    opcodeDec: 22,
    mnemonic: 'POP',
    category: 'Stack',
    allowedModes: 'REG',
    cycles: 2,
    flags: 'None',
    syntax: 'POP Rd',
    description: 'Reads word from [SP] into Rd and increments SP by 2. Faults on stack underflow.'
  },
  {
    opcodeHex: '0x17',
    opcodeDec: 23,
    mnemonic: 'IN',
    category: 'I/O',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'IN Rd, port',
    description: 'Reads 16-bit word from I/O port (0: char, 1: int, 2: status, 3: OS return) into Rd.'
  },
  {
    opcodeHex: '0x18',
    opcodeDec: 24,
    mnemonic: 'OUT',
    category: 'I/O',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'OUT Rd | OUT Rd, port',
    description: 'Emits Rd to I/O port (0: char, 1: int, 2: debug, 3: ForgeOS System Call trap).'
  },
  {
    opcodeHex: '0x19',
    opcodeDec: 25,
    mnemonic: 'JC',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JC label',
    description: 'Jump if Carry flag is set (C == 1). Unsigned comparison branch.'
  },
  {
    opcodeHex: '0x1A',
    opcodeDec: 26,
    mnemonic: 'JNC',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JNC label',
    description: 'Jump if Carry flag is cleared (C == 0).'
  },
  {
    opcodeHex: '0x1B',
    opcodeDec: 27,
    mnemonic: 'JN',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JN label',
    description: 'Jump if Negative flag is set (N == 1).'
  },
  {
    opcodeHex: '0x1C',
    opcodeDec: 28,
    mnemonic: 'JL',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JL label',
    description: 'Signed less-than jump: branches if (N != V).'
  },
  {
    opcodeHex: '0x1D',
    opcodeDec: 29,
    mnemonic: 'JLE',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JLE label',
    description: 'Signed less-than-or-equal jump: branches if (Z == 1) or (N != V).'
  },
  {
    opcodeHex: '0x1E',
    opcodeDec: 30,
    mnemonic: 'JG',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JG label',
    description: 'Signed greater-than jump: branches if (Z == 0) and (N == V).'
  },
  {
    opcodeHex: '0x1F',
    opcodeDec: 31,
    mnemonic: 'JGE',
    category: 'Control Flow',
    allowedModes: 'IMM, REG',
    cycles: 2,
    flags: 'None',
    syntax: 'JGE label',
    description: 'Signed greater-than-or-equal jump: branches if (N == V).'
  }
];

