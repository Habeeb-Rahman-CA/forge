import { Injectable } from '@angular/core';
import {
  ForgeCallFrame,
  ForgeDiagnostic,
  ForgeDisasmLine,
  ForgeGlobalVar,
  ForgeLocalVar,
  ForgeSourceMapEntry,
  ForgeStackEntry,
  ForgeStateResponse,
  ForgeSymbol,
  ForgeTraceEntry,
  ForgeWatchEntry,
} from './forge.models';

const OPCODE_MAP: Record<string, { code: number; cycles: number }> = {
  NOP:   { code: 0x00, cycles: 1 },
  HALT:  { code: 0x01, cycles: 1 },
  MOV:   { code: 0x02, cycles: 1 },
  LOAD:  { code: 0x03, cycles: 2 },
  STORE: { code: 0x04, cycles: 2 },
  ADD:   { code: 0x05, cycles: 1 },
  SUB:   { code: 0x06, cycles: 1 },
  MUL:   { code: 0x07, cycles: 3 },
  DIV:   { code: 0x08, cycles: 4 },
  AND:   { code: 0x09, cycles: 1 },
  OR:    { code: 0x0a, cycles: 1 },
  XOR:   { code: 0x0b, cycles: 1 },
  NOT:   { code: 0x0c, cycles: 1 },
  SHL:   { code: 0x0d, cycles: 1 },
  SHR:   { code: 0x0e, cycles: 1 },
  CMP:   { code: 0x0f, cycles: 1 },
  JMP:   { code: 0x10, cycles: 2 },
  JZ:    { code: 0x11, cycles: 2 },
  JNZ:   { code: 0x12, cycles: 2 },
  CALL:  { code: 0x13, cycles: 3 },
  RET:   { code: 0x14, cycles: 3 },
  PUSH:  { code: 0x15, cycles: 2 },
  POP:   { code: 0x16, cycles: 2 },
  IN:    { code: 0x17, cycles: 2 },
  OUT:   { code: 0x18, cycles: 2 },
  JC:    { code: 0x19, cycles: 2 },
  JNC:   { code: 0x1a, cycles: 2 },
  JN:    { code: 0x1b, cycles: 2 },
  JL:    { code: 0x1c, cycles: 2 },
  JLE:   { code: 0x1d, cycles: 2 },
  JG:    { code: 0x1e, cycles: 2 },
  JGE:   { code: 0x1f, cycles: 2 },
};

const CODE_TO_MNEMONIC: Record<number, string> = Object.entries(OPCODE_MAP).reduce(
  (acc, [k, v]) => {
    acc[v.code] = k;
    return acc;
  },
  {} as Record<number, string>
);

const MODE_REG = 0;
const MODE_IMM = 1;
const MODE_MEM_DIR = 2;
const MODE_MEM_IDX = 3;

const FLAG_Z = 1;
const FLAG_N = 2;
const FLAG_C = 4;
const FLAG_V = 8;

interface CompSymbol {
  name: string;
  scopeFn: string;
  kind: 'global' | 'param' | 'local' | 'fn';
  globalAddr: number;
  fpOffset: number;
  paramCount: number;
  localWords: number;
  declLine: number;
}

interface RawCallFrame {
  calleeAddr: number;
  callerPc: number;
  returnAddr: number;
  frameFp: number;
  frameSp: number;
}

@Injectable({
  providedIn: 'root',
})
export class ForgeBridgeService {
  private apiUrl = 'http://127.0.0.1:8080/api';
  public bridgeConnected = false;

  /* Local real Forge-16 CPU, Assembler & Compiler state */
  private mem = new Uint8Array(65536);
  private programImage = new Uint8Array(0);
  private loadAddr = 0x0400;
  private entryPc = 0x0400;
  private regs = new Uint16Array(8);
  private prevRegs = new Uint16Array(8);
  private pc = 0x0400;
  private prevPc = 0x0400;
  private sp = 0xf000;
  private prevSp = 0xf000;
  private flags = 0;
  private prevFlags = 0;
  private halted = false;
  private errorCode = 0;
  private errorMessage = 'OK';
  private faultPc = 0;
  private cycles = 0;
  private instructionsExecuted = 0;
  private lastRead = -1;
  private lastWrite = -1;
  private breakpoints = new Set<number>();
  private watches: number[] = [0x8000, 0x8002];
  private disassembly: ForgeDisasmLine[] = [];
  private symbols: ForgeSymbol[] = [];
  private diagnostics: ForgeDiagnostic[] = [];
  private trace: ForgeTraceEntry[] = [];
  private consoleBuf = '';
  private numericOutputs: number[] = [];
  private inputQueue: number[] = [];
  private hasProgram = false;

  /* High-Level Compiler & Call Stack state */
  private isCompiled = false;
  private generatedAsm = '';
  private astText = '';
  private sourceMap: ForgeSourceMapEntry[] = [];
  private compSymbols: CompSymbol[] = [];
  private rawCallStack: RawCallFrame[] = [];

  /* ForgeOS Supervisor & Framebuffer State */
  private heapStart = 0x9000;
  private heapBrk = 0x9000;
  private heapLimit = 0xe000;
  private allocCount = 0;
  private syscallCount = 0;
  private lastSyscall = 0;
  private exitCode = 0;
  private rngState = 0xace1;
  private fbWrites = 0;

  async fetchState(memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/state?memStart=${memStart}&memLen=${memLen}`);
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    return this.buildLocalState(memStart, memLen);
  }

  assembleSync(source: string, memStart = 0x0400, memLen = 256): ForgeStateResponse {
    this.isCompiled = false;
    this.generatedAsm = '';
    this.astText = '';
    this.sourceMap = [];
    this.compSymbols = [];
    this.localAssemble(source);
    return this.buildLocalState(memStart, memLen);
  }

  compileSync(source: string, memStart = 0x0400, memLen = 256): ForgeStateResponse {
    this.localCompile(source);
    return this.buildLocalState(memStart, memLen);
  }

  async assemble(source: string, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/assemble`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    return this.assembleSync(source, memStart, memLen);
  }

  async compile(source: string, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/compile`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    return this.compileSync(source, memStart, memLen);
  }

  async step(count = 1, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/step`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ count, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    this.localStep(count);
    return this.buildLocalState(memStart, memLen);
  }

  async stepLine(memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/step`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: 'line', memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    this.localStepLine();
    return this.buildLocalState(memStart, memLen);
  }

  async stepOver(memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/step`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: 'over', memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    this.localStepOver();
    return this.buildLocalState(memStart, memLen);
  }

  async stepOut(memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/step`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: 'out', memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    this.localStepOut();
    return this.buildLocalState(memStart, memLen);
  }

  async run(maxSteps = 100000, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/run`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ maxSteps, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    this.localRun(maxSteps);
    return this.buildLocalState(memStart, memLen);
  }

  async reset(memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/reset`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    this.localReset();
    return this.buildLocalState(memStart, memLen);
  }

  async toggleBreakpoint(address: number, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    const enabled = !this.breakpoints.has(address);
    try {
      const res = await fetch(`${this.apiUrl}/breakpoint`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ address, enabled, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        const data = (await res.json()) as ForgeStateResponse;
        this.breakpoints = new Set(data.breakpoints);
        return data;
      }
    } catch {
      this.bridgeConnected = false;
    }
    if (this.breakpoints.has(address)) {
      this.breakpoints.delete(address);
    } else {
      this.breakpoints.add(address);
    }
    return this.buildLocalState(memStart, memLen);
  }

  async toggleSourceLineBreakpoint(sourceLine: number, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/breakpoint`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ sourceLine, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        const data = (await res.json()) as ForgeStateResponse;
        this.breakpoints = new Set(data.breakpoints);
        return data;
      }
    } catch {
      this.bridgeConnected = false;
    }
    const addr = this.resolveSourceLineToAddr(sourceLine);
    if (addr !== null) {
      if (this.breakpoints.has(addr)) {
        this.breakpoints.delete(addr);
      } else {
        this.breakpoints.add(addr);
      }
    }
    return this.buildLocalState(memStart, memLen);
  }

  async writeMemory(address: number, value: number, width: 8 | 16 = 8, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/memory/write`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ address, value, width, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    if (address >= 0x0400 && address <= 0xffff) {
      if (width === 16 && address < 0xffff) {
        this.mem[address] = value & 0xff;
        this.mem[address + 1] = (value >> 8) & 0xff;
        this.lastWrite = address;
      } else if (width === 8) {
        this.mem[address] = value & 0xff;
        this.lastWrite = address;
      }
    } else {
      this.errorCode = 2;
      this.errorMessage = 'Memory access out of bounds or protected';
    }
    return this.buildLocalState(memStart, memLen);
  }

  async setWatch(address: number, enabled: boolean, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/watch`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ address, enabled, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    if (enabled && !this.watches.includes(address)) {
      this.watches.push(address & 0xffff);
    } else if (!enabled) {
      this.watches = this.watches.filter((a) => a !== address);
    }
    return this.buildLocalState(memStart, memLen);
  }

  async sendInput(text: string, value?: number, memStart = 0x0400, memLen = 256): Promise<ForgeStateResponse> {
    try {
      const res = await fetch(`${this.apiUrl}/io/input`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text, value, memStart, memLen }),
      });
      if (res.ok) {
        this.bridgeConnected = true;
        return (await res.json()) as ForgeStateResponse;
      }
    } catch {
      this.bridgeConnected = false;
    }
    if (text) {
      for (let i = 0; i < text.length; i++) {
        this.inputQueue.push(text.charCodeAt(i) & 0xff);
      }
    } else if (value !== undefined) {
      this.inputQueue.push(value & 0xffff);
    }
    return this.buildLocalState(memStart, memLen);
  }

  /* ==================== Local ForgeLang Compiler (Fallback & Unit Test Engine) ==================== */

  private localCompile(source: string): boolean {
    this.isCompiled = true;
    this.diagnostics = [];
    this.sourceMap = [];
    this.compSymbols = [];
    this.generatedAsm = '';
    this.astText = '';

    /* 1. Tokenize */
    interface Tok {
      type: string;
      text: string;
      intVal: number;
      line: number;
      col: number;
    }
    const tokens: Tok[] = [];
    let line = 1;
    let col = 1;
    let i = 0;

    const keywords = new Set([
      'let', 'fn', 'return', 'if', 'else', 'while', 'for', 'break', 'continue', 'print', 'putc', 'in'
    ]);

    while (i < source.length) {
      const c = source[i];
      if (c === '\n') {
        line++;
        col = 1;
        i++;
        continue;
      }
      if (/\s/.test(c)) {
        col++;
        i++;
        continue;
      }
      if ((c === '/' && source[i + 1] === '/') || c === '#') {
        while (i < source.length && source[i] !== '\n') i++;
        continue;
      }
      const startCol = col;
      if (c === "'") {
        i++; col++;
        let chVal = 0;
        if (source[i] === '\\') {
          i++; col++;
          const esc: Record<string, number> = { n: 10, t: 9, r: 13, '0': 0, '\\': 92, "'": 39 };
          chVal = esc[source[i]] ?? source.charCodeAt(i);
          i++; col++;
        } else {
          chVal = source.charCodeAt(i);
          i++; col++;
        }
        if (source[i] === "'") {
          i++; col++;
        }
        tokens.push({ type: 'NUMBER', text: `'${chVal}'`, intVal: chVal, line, col: startCol });
        continue;
      }
      if (/\d/.test(c)) {
        const start = i;
        if (c === '0' && (source[i + 1] === 'x' || source[i + 1] === 'X')) {
          i += 2; col += 2;
          while (/[0-9a-fA-F]/.test(source[i] ?? '')) { i++; col++; }
        } else {
          while (/\d/.test(source[i] ?? '')) { i++; col++; }
        }
        const raw = source.slice(start, i);
        const val = raw.startsWith('0x') || raw.startsWith('0X') ? parseInt(raw.slice(2), 16) : parseInt(raw, 10);
        tokens.push({ type: 'NUMBER', text: raw, intVal: val & 0xffff, line, col: startCol });
        continue;
      }
      if (/[A-Za-z_]/.test(c)) {
        const start = i;
        while (/\w/.test(source[i] ?? '')) { i++; col++; }
        const id = source.slice(start, i);
        tokens.push({ type: keywords.has(id) ? id.toUpperCase() : 'IDENT', text: id, intVal: 0, line, col: startCol });
        continue;
      }
      const two = source.slice(i, i + 2);
      if (['==', '!=', '<=', '>=', '<<', '>>', '&&', '||'].includes(two)) {
        tokens.push({ type: two, text: two, intVal: 0, line, col: startCol });
        i += 2; col += 2;
        continue;
      }
      if ('+-*/%&|^~!=<>(){},;'.includes(c)) {
        tokens.push({ type: c, text: c, intVal: 0, line, col: startCol });
        i++; col++;
        continue;
      }
      this.diagnostics.push({ line, column: startCol, stage: 'lexer', message: `Unexpected character '${c}'` });
      return false;
    }
    tokens.push({ type: 'EOF', text: '<eof>', intVal: 0, line, col });

    /* 2. Recursive-Descent Parser */
    let pos = 0;
    const peek = () => tokens[pos] ?? tokens[tokens.length - 1];
    const advance = () => {
      const t = peek();
      if (t.type !== 'EOF') pos++;
      return t;
    };
    const match = (tp: string) => {
      if (peek().type === tp) {
        advance();
        return true;
      }
      return false;
    };
    const expect = (tp: string, desc: string): Tok | null => {
      const t = peek();
      if (t.type !== tp) {
        this.diagnostics.push({ line: t.line, column: t.col, stage: 'parser', message: `Expected ${desc}, found '${t.text}'` });
        return null;
      }
      return advance();
    };

    interface AstNode {
      kind: string;
      line: number;
      col: number;
      name?: string;
      op?: string;
      intVal?: number;
      params?: string[];
      left?: AstNode | null;
      right?: AstNode | null;
      cond?: AstNode | null;
      thenBr?: AstNode | null;
      elseBr?: AstNode | null;
      body?: AstNode | null;
      children?: AstNode[];
    }

    const parsePrimary = (): AstNode | null => {
      const t = peek();
      if (t.type === 'NUMBER') {
        advance();
        return { kind: 'NUMBER', line: t.line, col: t.col, intVal: t.intVal };
      }
      if (t.type === 'IN') {
        advance();
        if (!expect('(', "'('") || !expect(')', "')'")) return null;
        return { kind: 'IN', line: t.line, col: t.col };
      }
      if (t.type === 'IDENT') {
        advance();
        if (match('(')) {
          const args: AstNode[] = [];
          if (peek().type !== ')') {
            while (true) {
              const a = parseExpr();
              if (!a) return null;
              args.push(a);
              if (!match(',')) break;
            }
          }
          if (!expect(')', "')'")) return null;
          return { kind: 'CALL', line: t.line, col: t.col, name: t.text, children: args };
        }
        return { kind: 'VAR', line: t.line, col: t.col, name: t.text };
      }
      if (match('(')) {
        const e = parseExpr();
        if (!e || !expect(')', "')'")) return null;
        return e;
      }
      this.diagnostics.push({ line: t.line, column: t.col, stage: 'parser', message: `Unexpected token '${t.text}'` });
      return null;
    };

    const parseUnary = (): AstNode | null => {
      const t = peek();
      if (t.type === '-' || t.type === '~' || t.type === '!') {
        advance();
        const opnd = parseUnary();
        if (!opnd) return null;
        return { kind: 'UNARY', line: t.line, col: t.col, op: t.type, left: opnd };
      }
      return parsePrimary();
    };

    const makeBin = (ops: string[], nextFn: () => AstNode | null) => (): AstNode | null => {
      let left = nextFn();
      while (left && ops.includes(peek().type)) {
        const t = advance();
        const right = nextFn();
        if (!right) return null;
        left = { kind: 'BIN', line: t.line, col: t.col, op: t.type, left, right };
      }
      return left;
    };

    const parseMul = makeBin(['*', '/', '%'], parseUnary);
    const parseAdd = makeBin(['+', '-'], parseMul);
    const parseShift = makeBin(['<<', '>>'], parseAdd);
    const parseRel = makeBin(['<', '<=', '>', '>='], parseShift);
    const parseEq = makeBin(['==', '!='], parseRel);
    const parseBitAnd = makeBin(['&'], parseEq);
    const parseBitXor = makeBin(['^'], parseBitAnd);
    const parseBitOr = makeBin(['|'], parseBitXor);
    const parseLogAnd = makeBin(['&&'], parseBitOr);
    const parseExpr = makeBin(['||'], parseLogAnd);

    const parseBlock = (): AstNode | null => {
      const lb = expect('{', "'{'");
      if (!lb) return null;
      const stmts: AstNode[] = [];
      while (peek().type !== '}' && peek().type !== 'EOF') {
        while (match(';')) {}
        if (peek().type === '}') break;
        const s = parseStmt();
        if (!s) return null;
        stmts.push(s);
        while (match(';')) {}
      }
      if (!expect('}', "'}'")) return null;
      return { kind: 'BLOCK', line: lb.line, col: lb.col, children: stmts };
    };

    const parseIf = (): AstNode | null => {
      const it = advance();
      const cond = parseExpr();
      if (!cond) return null;
      const thenBr = parseBlock();
      if (!thenBr) return null;
      let elseBr: AstNode | null = null;
      if (match('ELSE')) {
        elseBr = peek().type === 'IF' ? parseIf() : parseBlock();
        if (!elseBr) return null;
      }
      return { kind: 'IF', line: it.line, col: it.col, cond, thenBr, elseBr };
    };

    const parseStmt = (): AstNode | null => {
      const t = peek();
      if (t.type === 'LET') {
        advance();
        const id = expect('IDENT', 'identifier');
        if (!id || !expect('=', "'='")) return null;
        const v = parseExpr();
        if (!v) return null;
        return { kind: 'LET', line: t.line, col: t.col, name: id.text, left: v };
      }
      if (t.type === 'PRINT' || t.type === 'PUTC') {
        advance();
        const v = parseExpr();
        if (!v) return null;
        return { kind: t.type, line: t.line, col: t.col, left: v };
      }
      if (t.type === 'IF') return parseIf();
      if (t.type === 'WHILE') {
        advance();
        const cond = parseExpr();
        if (!cond) return null;
        const body = parseBlock();
        if (!body) return null;
        return { kind: 'WHILE', line: t.line, col: t.col, cond, body };
      }
      if (t.type === 'FOR') {
        advance();
        const hp = match('(');
        const init = parseStmt();
        if (!init || !expect(';', "';'")) return null;
        const cond = parseExpr();
        if (!cond || !expect(';', "';'")) return null;
        const step = parseStmt();
        if (!step) return null;
        if (hp && !expect(')', "')'")) return null;
        const body = parseBlock();
        if (!body) return null;
        return { kind: 'FOR', line: t.line, col: t.col, left: init, cond, right: step, body };
      }
      if (t.type === 'BREAK' || t.type === 'CONTINUE') {
        advance();
        return { kind: t.type, line: t.line, col: t.col };
      }
      if (t.type === 'FN') {
        advance();
        const id = expect('IDENT', 'function name');
        if (!id || !expect('(', "'('")) return null;
        const params: string[] = [];
        if (peek().type !== ')') {
          while (true) {
            const p = expect('IDENT', 'parameter name');
            if (!p) return null;
            params.push(p.text);
            if (!match(',')) break;
          }
        }
        if (!expect(')', "')'")) return null;
        const body = parseBlock();
        if (!body) return null;
        return { kind: 'FN', line: t.line, col: t.col, name: id.text, params, body };
      }
      if (t.type === 'RETURN') {
        advance();
        const v = parseExpr();
        if (!v) return null;
        return { kind: 'RETURN', line: t.line, col: t.col, left: v };
      }
      if (t.type === 'IDENT' && tokens[pos + 1]?.type === '=') {
        advance();
        advance();
        const v = parseExpr();
        if (!v) return null;
        return { kind: 'ASSIGN', line: t.line, col: t.col, name: t.text, left: v };
      }
      return parseExpr();
    };

    const rootChildren: AstNode[] = [];
    while (peek().type !== 'EOF') {
      while (match(';')) {}
      if (peek().type === 'EOF') break;
      const s = parseStmt();
      if (!s) return false;
      rootChildren.push(s);
      while (match(';')) {}
    }
    const root: AstNode = { kind: 'PROGRAM', line: 1, col: 1, children: rootChildren };

    /* 3. Semantic Analysis & Symbol Table */
    const findFn = (name: string) => this.compSymbols.find((s) => s.kind === 'fn' && s.name === name);
    const findVar = (scopeFn: string, name: string) => {
      if (scopeFn) {
        const loc = this.compSymbols.find(
          (s) => (s.kind === 'local' || s.kind === 'param') && s.scopeFn === scopeFn && s.name === name
        );
        if (loc) return loc;
      }
      return this.compSymbols.find((s) => s.kind === 'global' && s.name === name);
    };

    for (const st of rootChildren) {
      if (st.kind === 'FN') {
        const fnSym: CompSymbol = {
          name: st.name!,
          scopeFn: '',
          kind: 'fn',
          globalAddr: 0,
          fpOffset: 0,
          paramCount: st.params!.length,
          localWords: 0,
          declLine: st.line,
        };
        this.compSymbols.push(fnSym);
        st.params!.forEach((p, idx) => {
          this.compSymbols.push({
            name: p,
            scopeFn: st.name!,
            kind: 'param',
            globalAddr: 0,
            fpOffset: idx * 2,
            paramCount: 0,
            localWords: 0,
            declLine: st.line,
          });
        });
      }
    }

    let nextGlobalAddr = 0x8000;
    const checkSem = (nd: AstNode | null | undefined, curFn: string, loopDepth: number) => {
      if (!nd) return;
      switch (nd.kind) {
        case 'PROGRAM':
        case 'BLOCK':
          nd.children?.forEach((c) => checkSem(c, curFn, loopDepth));
          break;
        case 'LET':
          checkSem(nd.left, curFn, loopDepth);
          if (!curFn) {
            this.compSymbols.push({
              name: nd.name!,
              scopeFn: '',
              kind: 'global',
              globalAddr: nextGlobalAddr,
              fpOffset: 0,
              paramCount: 0,
              localWords: 0,
              declLine: nd.line,
            });
            nextGlobalAddr += 2;
          } else {
            const fnSym = findFn(curFn)!;
            this.compSymbols.push({
              name: nd.name!,
              scopeFn: curFn,
              kind: 'local',
              globalAddr: 0,
              fpOffset: (fnSym.paramCount + fnSym.localWords) * 2,
              paramCount: 0,
              localWords: 0,
              declLine: nd.line,
            });
            fnSym.localWords++;
          }
          break;
        case 'ASSIGN':
          checkSem(nd.left, curFn, loopDepth);
          if (!findVar(curFn, nd.name!)) {
            this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `Assignment to undeclared variable '${nd.name}'` });
          }
          break;
        case 'VAR':
          if (!findVar(curFn, nd.name!)) {
            this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `Use of undeclared variable '${nd.name}'` });
          }
          break;
        case 'CALL': {
          const fn = findFn(nd.name!);
          const builtinArity: Record<string, number> = {
            free_all: 0,
            key_ready: 0,
            read_key: 0,
            cycles: 0,
            rand: 0,
            alloc: 1,
            peek: 1,
            clear_screen: 1,
            exit: 1,
            poke: 2,
            get_pixel: 2,
            draw_pixel: 3,
          };
          if (fn) {
            if ((nd.children?.length ?? 0) !== fn.paramCount) {
              this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `Function '${nd.name}' arity mismatch` });
            }
          } else if (builtinArity[nd.name!] !== undefined) {
            if ((nd.children?.length ?? 0) !== builtinArity[nd.name!]) {
              this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `Built-in '${nd.name}' arity mismatch` });
            }
          } else {
            this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `Call to undefined function '${nd.name}'` });
          }
          nd.children?.forEach((c) => checkSem(c, curFn, loopDepth));
          break;
        }
        case 'FN':
          checkSem(nd.body, nd.name!, 0);
          break;
        case 'RETURN':
          if (!curFn) {
            this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `'return' outside function` });
          }
          checkSem(nd.left, curFn, loopDepth);
          break;
        case 'IF':
          checkSem(nd.cond, curFn, loopDepth);
          checkSem(nd.thenBr, curFn, loopDepth);
          checkSem(nd.elseBr, curFn, loopDepth);
          break;
        case 'WHILE':
          checkSem(nd.cond, curFn, loopDepth);
          checkSem(nd.body, curFn, loopDepth + 1);
          break;
        case 'FOR':
          checkSem(nd.left, curFn, loopDepth);
          checkSem(nd.cond, curFn, loopDepth);
          checkSem(nd.right, curFn, loopDepth);
          checkSem(nd.body, curFn, loopDepth + 1);
          break;
        case 'BREAK':
        case 'CONTINUE':
          if (loopDepth <= 0) {
            this.diagnostics.push({ line: nd.line, column: nd.col, stage: 'semantic', message: `'${nd.kind.toLowerCase()}' outside loop` });
          }
          break;
        default:
          checkSem(nd.left, curFn, loopDepth);
          checkSem(nd.right, curFn, loopDepth);
          break;
      }
    };
    checkSem(root, '', 0);
    if (this.diagnostics.length > 0) return false;

    /* Format AST text */
    const astLines: string[] = [];
    const fmtAst = (n: AstNode | null | undefined, ind: number) => {
      if (!n) return;
      const pad = '  '.repeat(ind);
      astLines.push(`${pad}${n.kind}${n.name ? '(' + n.name + ')' : ''} [line ${n.line}]`);
      fmtAst(n.left, ind + 1);
      fmtAst(n.cond, ind + 1);
      fmtAst(n.right, ind + 1);
      fmtAst(n.thenBr, ind + 1);
      fmtAst(n.elseBr, ind + 1);
      fmtAst(n.body, ind + 1);
      n.children?.forEach((c) => fmtAst(c, ind + 1));
    };
    fmtAst(root, 0);
    this.astText = astLines.join('\n');

    /* 4. Code Generation */
    const asmLines: string[] = [];
    const asmToSrc: number[] = [1];
    let curSrcLine = 1;
    let curFn = '';
    let lblCount = 0;
    const breakStack: string[] = [];
    const contStack: string[] = [];

    const emit = (txt: string) => {
      asmLines.push(txt);
      asmToSrc.push(curSrcLine);
    };

    const emitLoad = (sym: CompSymbol) => {
      if (sym.kind === 'global') emit(`    LOAD R0, [0x${sym.globalAddr.toString(16).toUpperCase().padStart(4, '0')}] ; global ${sym.name}`);
      else emit(`    LOAD R0, [FP + ${sym.fpOffset}] ; ${sym.name}`);
    };
    const emitStore = (sym: CompSymbol) => {
      if (sym.kind === 'global') emit(`    STORE R0, [0x${sym.globalAddr.toString(16).toUpperCase().padStart(4, '0')}] ; global ${sym.name}`);
      else emit(`    STORE R0, [FP + ${sym.fpOffset}] ; ${sym.name}`);
    };

    const genExpr = (n: AstNode | null | undefined) => {
      if (!n) return;
      curSrcLine = n.line;
      if (n.kind === 'NUMBER') {
        emit(`    MOV R0, ${n.intVal}`);
      } else if (n.kind === 'IN') {
        emit(`    IN R0, 1`);
      } else if (n.kind === 'VAR') {
        const s = findVar(curFn, n.name!);
        if (s) emitLoad(s);
      } else if (n.kind === 'UNARY') {
        genExpr(n.left);
        if (n.op === '-') {
          emit(`    MOV R1, R0`);
          emit(`    MOV R0, 0`);
          emit(`    SUB R0, R1`);
        } else if (n.op === '~') {
          emit(`    NOT R0`);
        } else if (n.op === '!') {
          const l = ++lblCount;
          emit(`    CMP R0, 0`);
          emit(`    MOV R0, 1`);
          emit(`    JZ _not_${l}`);
          emit(`    MOV R0, 0`);
          emit(`_not_${l}:`);
        }
      } else if (n.kind === 'BIN') {
        if (n.op === '&&') {
          const l = ++lblCount;
          genExpr(n.left);
          emit(`    CMP R0, 0`);
          emit(`    JZ _land_f_${l}`);
          genExpr(n.right);
          emit(`    CMP R0, 0`);
          emit(`    JZ _land_f_${l}`);
          emit(`    MOV R0, 1`);
          emit(`    JMP _land_e_${l}`);
          emit(`_land_f_${l}:`);
          emit(`    MOV R0, 0`);
          emit(`_land_e_${l}:`);
          return;
        }
        if (n.op === '||') {
          const l = ++lblCount;
          genExpr(n.left);
          emit(`    CMP R0, 0`);
          emit(`    JNZ _lor_t_${l}`);
          genExpr(n.right);
          emit(`    CMP R0, 0`);
          emit(`    JNZ _lor_t_${l}`);
          emit(`    MOV R0, 0`);
          emit(`    JMP _lor_e_${l}`);
          emit(`_lor_t_${l}:`);
          emit(`    MOV R0, 1`);
          emit(`_lor_e_${l}:`);
          return;
        }
        genExpr(n.left);
        emit(`    PUSH R0`);
        genExpr(n.right);
        emit(`    MOV R1, R0`);
        emit(`    POP R0`);
        const mapOp: Record<string, string> = {
          '+': 'ADD', '-': 'SUB', '*': 'MUL', '/': 'DIV', '&': 'AND', '|': 'OR', '^': 'XOR', '<<': 'SHL', '>>': 'SHR'
        };
        if (mapOp[n.op!]) {
          emit(`    ${mapOp[n.op!]} R0, R1`);
        } else if (n.op === '%') {
          emit(`    MOV R2, R0`);
          emit(`    DIV R0, R1`);
          emit(`    MUL R0, R1`);
          emit(`    SUB R2, R0`);
          emit(`    MOV R0, R2`);
        } else {
          const l = ++lblCount;
          const jmap: Record<string, string> = { '==': 'JZ', '!=': 'JNZ', '<': 'JL', '<=': 'JLE', '>': 'JG', '>=': 'JGE' };
          emit(`    CMP R0, R1`);
          emit(`    MOV R0, 1`);
          emit(`    ${jmap[n.op!] ?? 'JZ'} _cmp_${l}`);
          emit(`    MOV R0, 0`);
          emit(`_cmp_${l}:`);
        }
      } else if (n.kind === 'CALL') {
        const fn = findFn(n.name!);
        if (!fn) {
          const args = n.children ?? [];
          if (n.name === 'peek' && args.length === 1) {
            genExpr(args[0]);
            emit(`    LOAD R0, [R0]`);
          } else if (n.name === 'poke' && args.length === 2) {
            genExpr(args[0]);
            emit(`    PUSH R0`);
            genExpr(args[1]);
            emit(`    POP R1`);
            emit(`    STORE R0, [R1]`);
          } else if (n.name === 'free_all') {
            emit(`    MOV R0, 7`);
            emit(`    OUT R0, 3`);
          } else if (n.name === 'key_ready') {
            emit(`    MOV R0, 5`);
            emit(`    OUT R0, 3`);
          } else if (n.name === 'read_key') {
            emit(`    MOV R0, 4`);
            emit(`    OUT R0, 3`);
          } else if (n.name === 'cycles') {
            emit(`    MOV R0, 11`);
            emit(`    OUT R0, 3`);
          } else if (n.name === 'rand') {
            emit(`    MOV R0, 12`);
            emit(`    OUT R0, 3`);
          } else if ((n.name === 'alloc' || n.name === 'clear_screen' || n.name === 'exit') && args.length === 1) {
            const sysId = n.name === 'alloc' ? 6 : n.name === 'clear_screen' ? 9 : 0;
            genExpr(args[0]);
            emit(`    MOV R1, R0`);
            emit(`    MOV R0, ${sysId}`);
            emit(`    OUT R0, 3`);
          } else if (n.name === 'get_pixel' && args.length === 2) {
            genExpr(args[0]);
            emit(`    PUSH R0`);
            genExpr(args[1]);
            emit(`    MOV R2, R0`);
            emit(`    POP R1`);
            emit(`    MOV R0, 10`);
            emit(`    OUT R0, 3`);
          } else if (n.name === 'draw_pixel' && args.length === 3) {
            genExpr(args[0]);
            emit(`    PUSH R0`);
            genExpr(args[1]);
            emit(`    PUSH R0`);
            genExpr(args[2]);
            emit(`    MOV R3, R0`);
            emit(`    POP R2`);
            emit(`    POP R1`);
            emit(`    MOV R0, 8`);
            emit(`    OUT R0, 3`);
          }
          return;
        }
        for (const arg of n.children ?? []) {
          genExpr(arg);
          emit(`    PUSH R0`);
        }
        const fbytes = Math.max(1, fn.paramCount + fn.localWords) * 2;
        emit(`    SUB FP, ${fbytes}`);
        for (let idx = (n.children?.length ?? 0) - 1; idx >= 0; idx--) {
          emit(`    POP R0`);
          emit(`    STORE R0, [FP + ${idx * 2}]`);
        }
        emit(`    CALL fn_${n.name}`);
        emit(`    ADD FP, ${fbytes}`);
      }
    };

    const genStmt = (n: AstNode | null | undefined) => {
      if (!n) return;
      curSrcLine = n.line;
      if (n.kind === 'BLOCK') {
        n.children?.forEach((c) => genStmt(c));
      } else if (n.kind === 'LET' || n.kind === 'ASSIGN') {
        genExpr(n.left);
        curSrcLine = n.line;
        const s = findVar(curFn, n.name!);
        if (s) emitStore(s);
      } else if (n.kind === 'PRINT') {
        genExpr(n.left);
        curSrcLine = n.line;
        emit(`    OUT R0, 1`);
      } else if (n.kind === 'PUTC') {
        genExpr(n.left);
        curSrcLine = n.line;
        emit(`    OUT R0, 0`);
      } else if (n.kind === 'IF') {
        const l = ++lblCount;
        genExpr(n.cond);
        curSrcLine = n.line;
        emit(`    CMP R0, 0`);
        if (n.elseBr) {
          emit(`    JZ _else_${l}`);
          genStmt(n.thenBr);
          emit(`    JMP _endif_${l}`);
          emit(`_else_${l}:`);
          genStmt(n.elseBr);
          emit(`_endif_${l}:`);
        } else {
          emit(`    JZ _endif_${l}`);
          genStmt(n.thenBr);
          emit(`_endif_${l}:`);
        }
      } else if (n.kind === 'WHILE') {
        const l = ++lblCount;
        contStack.push(`_wstart_${l}`);
        breakStack.push(`_wend_${l}`);
        emit(`_wstart_${l}:`);
        genExpr(n.cond);
        curSrcLine = n.line;
        emit(`    CMP R0, 0`);
        emit(`    JZ _wend_${l}`);
        genStmt(n.body);
        emit(`    JMP _wstart_${l}`);
        emit(`_wend_${l}:`);
        contStack.pop();
        breakStack.pop();
      } else if (n.kind === 'FOR') {
        const l = ++lblCount;
        genStmt(n.left);
        contStack.push(`_fstep_${l}`);
        breakStack.push(`_fend_${l}`);
        emit(`_fcond_${l}:`);
        genExpr(n.cond);
        curSrcLine = n.line;
        emit(`    CMP R0, 0`);
        emit(`    JZ _fend_${l}`);
        genStmt(n.body);
        emit(`_fstep_${l}:`);
        genStmt(n.right);
        emit(`    JMP _fcond_${l}`);
        emit(`_fend_${l}:`);
        contStack.pop();
        breakStack.pop();
      } else if (n.kind === 'BREAK') {
        if (breakStack.length > 0) emit(`    JMP ${breakStack[breakStack.length - 1]}`);
      } else if (n.kind === 'CONTINUE') {
        if (contStack.length > 0) emit(`    JMP ${contStack[contStack.length - 1]}`);
      } else if (n.kind === 'RETURN') {
        genExpr(n.left);
        curSrcLine = n.line;
        emit(`    RET`);
      } else if (n.kind !== 'FN') {
        genExpr(n);
      }
    };

    emit(`; ForgeLang Compiled Assembly Output`);
    emit(`.entry _start`);
    emit(`_start:`);
    emit(`    MOV FP, 0xDF00`);
    for (const st of rootChildren) {
      if (st.kind !== 'FN') genStmt(st);
    }
    emit(`    HALT`);

    for (const st of rootChildren) {
      if (st.kind === 'FN') {
        curFn = st.name!;
        curSrcLine = st.line;
        emit(`fn_${st.name}:`);
        genStmt(st.body);
        curSrcLine = st.line;
        emit(`    RET`);
        curFn = '';
      }
    }

    this.generatedAsm = asmLines.join('\n');
    const ok = this.localAssemble(this.generatedAsm);
    if (!ok) return false;

    this.sourceMap = this.disassembly.map((d) => {
      const srcLine = asmToSrc[d.line] ?? 1;
      d.sourceLine = srcLine;
      return {
        sourceLine: srcLine,
        asmLine: d.line,
        address: d.address,
      };
    });
    return true;
  }

  private getSourceLineForPc(pc: number): number {
    if (this.isCompiled) {
      const sm = this.sourceMap.find((s) => s.address === pc);
      return sm ? sm.sourceLine : 0;
    }
    const d = this.disassembly.find((x) => x.address === pc);
    return d ? d.line : 0;
  }

  private resolveSourceLineToAddr(sourceLine: number): number | null {
    if (this.isCompiled) {
      const sm = this.sourceMap.find((s) => s.sourceLine === sourceLine);
      return sm ? sm.address : null;
    }
    const d = this.disassembly.find((x) => x.line === sourceLine);
    return d ? d.address : null;
  }

  private localStepLine(): void {
    if (this.errorCode === 10) this.errorCode = 0;
    const startLine = this.getSourceLineForPc(this.pc);
    for (let i = 0; i < 4096; i++) {
      if (this.halted || this.errorCode !== 0) break;
      if (i > 0 && this.breakpoints.has(this.pc)) {
        this.errorCode = 10;
        break;
      }
      this.localStep(1);
      const cur = this.getSourceLineForPc(this.pc);
      if (cur !== 0 && cur !== startLine) break;
    }
  }

  private localStepOver(): void {
    if (this.errorCode === 10) this.errorCode = 0;
    const startDepth = this.rawCallStack.length;
    const startLine = this.getSourceLineForPc(this.pc);
    for (let i = 0; i < 65536; i++) {
      if (this.halted || this.errorCode !== 0) break;
      if (i > 0 && this.breakpoints.has(this.pc)) {
        this.errorCode = 10;
        break;
      }
      this.localStep(1);
      if (this.rawCallStack.length < startDepth) break;
      if (this.rawCallStack.length === startDepth) {
        if (!this.isCompiled) break;
        const cur = this.getSourceLineForPc(this.pc);
        if (cur !== 0 && cur !== startLine) break;
      }
    }
  }

  private localStepOut(): void {
    if (this.errorCode === 10) this.errorCode = 0;
    const startDepth = this.rawCallStack.length;
    if (startDepth === 0) {
      this.localStepLine();
      return;
    }
    const target = startDepth - 1;
    for (let i = 0; i < 65536; i++) {
      if (this.halted || this.errorCode !== 0) break;
      if (i > 0 && this.breakpoints.has(this.pc)) {
        this.errorCode = 10;
        break;
      }
      this.localStep(1);
      if (this.rawCallStack.length <= target) break;
    }
  }

  /* ==================== Local Forge-16 Two-Pass Assembler & CPU Engine ==================== */

  private parseReg(tok: string): number | null {
    const up = tok.trim().toUpperCase();
    if (up === 'FP') return 7;
    if (/^R[0-7]$/.test(up)) return parseInt(up[1], 10);
    return null;
  }

  private parseNumber(tok: string): number | null {
    const t = tok.trim();
    if (!t) return null;
    if (t.length >= 3 && t.startsWith("'") && t.endsWith("'")) {
      if (t.length === 3 && t[1] !== '\\') return t.charCodeAt(1);
      if (t.length === 4 && t[1] === '\\') {
        const map: Record<string, number> = { n: 10, t: 9, r: 13, '0': 0, '\\': 92, "'": 39 };
        return map[t[2]] ?? null;
      }
      return null;
    }
    let sign = 1;
    let body = t;
    if (body.startsWith('-')) {
      sign = -1;
      body = body.slice(1);
    } else if (body.startsWith('+')) {
      body = body.slice(1);
    }
    let val = NaN;
    if (body.startsWith('0x') || body.startsWith('0X')) {
      val = parseInt(body.slice(2), 16);
    } else if (body.startsWith('0b') || body.startsWith('0B')) {
      val = parseInt(body.slice(2), 2);
    } else if (/^\d+$/.test(body)) {
      val = parseInt(body, 10);
    }
    if (Number.isNaN(val)) return null;
    const signed = sign * val;
    if (signed < -32768 || signed > 65535) return null;
    return signed & 0xffff;
  }

  private resolveSymbolOrNum(tok: string, symMap: Map<string, number>): number | null {
    const direct = this.parseNumber(tok);
    if (direct !== null) return direct;
    const trimmed = tok.trim();
    if (symMap.has(trimmed)) return symMap.get(trimmed)!;
    const match = trimmed.match(/^([A-Za-z_]\w*)\s*([+-])\s*(.+)$/);
    if (match && symMap.has(match[1])) {
      const base = symMap.get(match[1])!;
      const off = this.parseNumber(match[3]);
      if (off !== null) {
        return match[2] === '+' ? (base + off) & 0xffff : (base - off) & 0xffff;
      }
    }
    return null;
  }

  private disasmWords(word0: number, word1: number): string {
    const op = (word0 >> 10) & 0x3f;
    const mode = (word0 >> 8) & 0x03;
    const rd = (word0 >> 5) & 0x07;
    const rs = (word0 >> 2) & 0x07;
    const mnem = CODE_TO_MNEMONIC[op] ?? 'INVALID';
    const hex16 = `0x${word1.toString(16).toUpperCase().padStart(4, '0')}`;

    if (['NOP', 'HALT', 'RET'].includes(mnem)) return mnem;
    if (['NOT', 'POP'].includes(mnem)) return `${mnem} R${rd}`;
    if (mnem === 'PUSH') return mode === MODE_IMM ? `PUSH ${hex16}` : `PUSH R${rd}`;
    if (['JMP', 'JZ', 'JNZ', 'CALL', 'JC', 'JNC', 'JN', 'JL', 'JLE', 'JG', 'JGE'].includes(mnem)) {
      return mode === MODE_IMM ? `${mnem} ${hex16}` : `${mnem} R${rs}`;
    }
    if (mnem === 'LOAD' || mnem === 'STORE') {
      if (mode === MODE_MEM_DIR) return `${mnem} R${rd}, [${hex16}]`;
      const signedOff = word1 >= 0x8000 ? word1 - 0x10000 : word1;
      if (signedOff === 0) return `${mnem} R${rd}, [R${rs}]`;
      return signedOff > 0
        ? `${mnem} R${rd}, [R${rs} + ${signedOff}]`
        : `${mnem} R${rd}, [R${rs} - ${-signedOff}]`;
    }
    return mode === MODE_IMM ? `${mnem} R${rd}, ${hex16}` : `${mnem} R${rd}, R${rs}`;
  }

  private localAssemble(source: string): boolean {
    this.diagnostics = [];
    this.symbols = [];
    this.disassembly = [];
    const symMap = new Map<string, number>();

    let origin = 0x0400;
    let entry = 0x0400;
    let explicitEntry = false;
    let currAddr = origin;
    let hasEmitted = false;

    const rawLines = source.split(/\r?\n/);

    const stripComments = (line: string): string => {
      let inSq = false;
      let inDq = false;
      for (let i = 0; i < line.length; i++) {
        if (line[i] === '\\') {
          i++;
          continue;
        }
        if (line[i] === "'" && !inDq) inSq = !inSq;
        else if (line[i] === '"' && !inSq) inDq = !inDq;
        else if (!inSq && !inDq) {
          if (line[i] === ';' || (line[i] === '/' && line[i + 1] === '/')) {
            return line.slice(0, i);
          }
        }
      }
      return line;
    };

    /* Pass 1 */
    for (let idx = 0; idx < rawLines.length; idx++) {
      const lineNum = idx + 1;
      let cur = stripComments(rawLines[idx]).trim();
      if (!cur) continue;

      while (cur.includes(':')) {
        const colonIdx = cur.indexOf(':');
        const quoteIdx = cur.indexOf('"');
        if (quoteIdx !== -1 && quoteIdx < colonIdx) break;
        const label = cur.slice(0, colonIdx).trim();
        if (!/^[A-Za-z_]\w*$/.test(label) || this.parseReg(label) !== null || OPCODE_MAP[label.toUpperCase()]) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid label name '${label}'` });
          break;
        }
        if (symMap.has(label)) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Duplicate symbol '${label}'` });
          break;
        }
        symMap.set(label, currAddr);
        this.symbols.push({ name: label, value: currAddr, line: lineNum, isConstant: false });
        cur = cur.slice(colonIdx + 1).trim();
      }
      if (!cur) continue;

      const m = cur.match(/^(\S+)\s*(.*)$/);
      if (!m) continue;
      const head = m[1];
      const rest = m[2].trim();

      if (head.startsWith('.')) {
        const dir = head.toLowerCase();
        if (dir === '.org') {
          const v = this.resolveSymbolOrNum(rest, symMap);
          if (v === null) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid .org address '${rest}'` });
          } else if (!hasEmitted) {
            origin = v;
            if (!explicitEntry) entry = v;
            currAddr = v;
          } else {
            currAddr = v;
          }
        } else if (dir === '.equ' || dir === '.const') {
          const parts = rest.includes(',') ? rest.split(',') : rest.split(/\s+/);
          if (parts.length < 2) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `${head} requires name and value` });
          } else {
            const name = parts[0].trim();
            const val = this.resolveSymbolOrNum(parts.slice(1).join(' ').trim(), symMap);
            if (val === null) {
              this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid constant value in ${head}` });
            } else if (symMap.has(name)) {
              this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Duplicate symbol '${name}'` });
            } else {
              symMap.set(name, val);
              this.symbols.push({ name, value: val, line: lineNum, isConstant: true });
            }
          }
        } else if (dir === '.entry') {
          /* Resolved in pass 2 */
        } else if (dir === '.word' || dir === '.dw') {
          hasEmitted = true;
          currAddr += rest.split(',').length * 2;
        } else if (dir === '.byte' || dir === '.db') {
          hasEmitted = true;
          currAddr += rest.split(',').length;
        } else if (dir === '.asciz' || dir === '.string') {
          hasEmitted = true;
          const strContent = rest.replace(/^"|"$/g, '').replace(/\\./g, 'x');
          currAddr += strContent.length + 1;
        } else if (dir === '.space' || dir === '.resb') {
          hasEmitted = true;
          const cnt = this.resolveSymbolOrNum(rest, symMap) ?? 0;
          currAddr += cnt;
        } else {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Unknown directive '${head}'` });
        }
      } else {
        if (!OPCODE_MAP[head.toUpperCase()]) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Unknown instruction mnemonic '${head}'` });
        } else {
          hasEmitted = true;
          currAddr += 4;
        }
      }
    }

    if (this.diagnostics.length > 0) {
      return false;
    }

    /* Pass 2 */
    const bytes: number[] = [];
    currAddr = origin;
    hasEmitted = false;

    const emit8 = (b: number) => {
      bytes.push(b & 0xff);
      currAddr = (currAddr + 1) & 0xffff;
    };
    const emit16 = (w: number) => {
      emit8(w & 0xff);
      emit8((w >> 8) & 0xff);
    };

    for (let idx = 0; idx < rawLines.length; idx++) {
      const lineNum = idx + 1;
      let cur = stripComments(rawLines[idx]).trim();
      if (!cur) continue;

      while (cur.includes(':')) {
        const colonIdx = cur.indexOf(':');
        const quoteIdx = cur.indexOf('"');
        if (quoteIdx !== -1 && quoteIdx < colonIdx) break;
        cur = cur.slice(colonIdx + 1).trim();
      }
      if (!cur) continue;

      const m = cur.match(/^(\S+)\s*(.*)$/);
      if (!m) continue;
      const head = m[1];
      const rest = m[2].trim();

      if (head.startsWith('.')) {
        const dir = head.toLowerCase();
        if (dir === '.org') {
          const v = this.resolveSymbolOrNum(rest, symMap)!;
          if (!hasEmitted) {
            currAddr = v;
          } else {
            while (currAddr < v) emit8(0);
          }
        } else if (dir === '.entry') {
          const v = this.resolveSymbolOrNum(rest, symMap);
          if (v === null) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Undefined entry symbol '${rest}'` });
          } else {
            entry = v;
            explicitEntry = true;
          }
        } else if (dir === '.word' || dir === '.dw') {
          hasEmitted = true;
          for (const part of rest.split(',')) {
            const v = this.resolveSymbolOrNum(part, symMap);
            if (v === null) {
              this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Undefined word value '${part.trim()}'` });
            } else {
              emit16(v);
            }
          }
        } else if (dir === '.byte' || dir === '.db') {
          hasEmitted = true;
          for (const part of rest.split(',')) {
            const v = this.resolveSymbolOrNum(part, symMap);
            if (v === null) {
              this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid byte value '${part.trim()}'` });
            } else {
              emit8(v & 0xff);
            }
          }
        } else if (dir === '.asciz' || dir === '.string') {
          hasEmitted = true;
          const inner = rest.slice(1, -1);
          for (let i = 0; i < inner.length; i++) {
            if (inner[i] === '\\' && i + 1 < inner.length) {
              i++;
              const esc: Record<string, number> = { n: 10, t: 9, r: 13, '0': 0, '\\': 92, '"': 34 };
              emit8(esc[inner[i]] ?? inner.charCodeAt(i));
            } else {
              emit8(inner.charCodeAt(i));
            }
          }
          emit8(0);
        } else if (dir === '.space' || dir === '.resb') {
          hasEmitted = true;
          const cnt = this.resolveSymbolOrNum(rest, symMap) ?? 0;
          for (let i = 0; i < cnt; i++) emit8(0);
        }
        continue;
      }

      hasEmitted = true;
      const mnem = head.toUpperCase();
      const opInfo = OPCODE_MAP[mnem];
      const ops = rest ? rest.split(',').map((s) => s.trim()) : [];
      const instrAddr = currAddr;

      let mode = MODE_REG;
      let rd = 0;
      let rs = 0;
      let imm16 = 0;

      if (['NOP', 'HALT', 'RET'].includes(mnem)) {
        if (ops.length !== 0) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `'${mnem}' takes no operands` });
          continue;
        }
      } else if (['NOT', 'POP'].includes(mnem)) {
        const r = ops.length === 1 ? this.parseReg(ops[0]) : null;
        if (r === null) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid register for '${mnem}'` });
          continue;
        }
        rd = r;
      } else if (mnem === 'PUSH') {
        if (ops.length !== 1) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `'PUSH' expects 1 operand` });
          continue;
        }
        const r = this.parseReg(ops[0]);
        if (r !== null) {
          mode = MODE_REG;
          rd = r;
        } else {
          const v = this.resolveSymbolOrNum(ops[0], symMap);
          if (v === null) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid operand '${ops[0]}' for PUSH` });
            continue;
          }
          mode = MODE_IMM;
          imm16 = v;
        }
      } else if (['JMP', 'JZ', 'JNZ', 'CALL', 'JC', 'JNC', 'JN', 'JL', 'JLE', 'JG', 'JGE'].includes(mnem)) {
        if (ops.length !== 1) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `'${mnem}' expects 1 target operand` });
          continue;
        }
        const r = this.parseReg(ops[0]);
        if (r !== null) {
          mode = MODE_REG;
          rs = r;
        } else {
          const v = this.resolveSymbolOrNum(ops[0], symMap);
          if (v === null) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Undefined label '${ops[0]}'` });
            continue;
          }
          mode = MODE_IMM;
          imm16 = v;
        }
      } else if (mnem === 'IN' || mnem === 'OUT') {
        const r = ops.length >= 1 ? this.parseReg(ops[0]) : null;
        if (r === null || ops.length > 2) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid operands for '${mnem}'` });
          continue;
        }
        rd = r;
        if (ops.length === 1) {
          mode = MODE_IMM;
          imm16 = 1;
        } else {
          const pr = this.parseReg(ops[1]);
          if (pr !== null) {
            mode = MODE_REG;
            rs = pr;
          } else {
            const pv = this.resolveSymbolOrNum(ops[1], symMap);
            if (pv === null || pv > 255) {
              this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid I/O port '${ops[1]}'` });
              continue;
            }
            mode = MODE_IMM;
            imm16 = pv;
          }
        }
      } else if (mnem === 'LOAD' || mnem === 'STORE') {
        const r = ops.length === 2 ? this.parseReg(ops[0]) : null;
        if (r === null || !ops[1].startsWith('[') || !ops[1].endsWith(']')) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `'${mnem}' expects Rd, [address]` });
          continue;
        }
        rd = r;
        const inner = ops[1].slice(1, -1).trim();
        const idxMatch = inner.match(/^([A-Za-z0-9_]+)(?:\s*([+-])\s*(.+))?$/);
        const baseReg = idxMatch ? this.parseReg(idxMatch[1]) : null;
        if (baseReg !== null && idxMatch) {
          mode = MODE_MEM_IDX;
          rs = baseReg;
          if (idxMatch[2] && idxMatch[3]) {
            const off = this.resolveSymbolOrNum(idxMatch[3], symMap);
            if (off === null) {
              this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid offset '${idxMatch[3]}'` });
              continue;
            }
            imm16 = idxMatch[2] === '-' ? (-off) & 0xffff : off & 0xffff;
          } else {
            imm16 = 0;
          }
        } else {
          const addrVal = this.resolveSymbolOrNum(inner, symMap);
          if (addrVal === null) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Undefined address '${inner}'` });
            continue;
          }
          mode = MODE_MEM_DIR;
          imm16 = addrVal;
        }
      } else {
        if (ops.length !== 2) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `'${mnem}' expects 2 operands` });
          continue;
        }
        const rDest = this.parseReg(ops[0]);
        if (rDest === null) {
          this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Invalid destination register '${ops[0]}'` });
          continue;
        }
        rd = rDest;
        const rSrc = this.parseReg(ops[1]);
        if (rSrc !== null) {
          mode = MODE_REG;
          rs = rSrc;
        } else {
          const v = this.resolveSymbolOrNum(ops[1], symMap);
          if (v === null) {
            this.diagnostics.push({ line: lineNum, column: 1, stage: 'assembler', message: `Undefined operand '${ops[1]}'` });
            continue;
          }
          mode = MODE_IMM;
          imm16 = v;
        }
      }

      const word0 = ((opInfo.code & 0x3f) << 10) | ((mode & 0x03) << 8) | ((rd & 0x07) << 5) | ((rs & 0x07) << 2);
      const word1 = imm16 & 0xffff;
      emit16(word0);
      emit16(word1);

      this.disassembly.push({
        address: instrAddr,
        line: lineNum,
        sourceLine: lineNum,
        word0,
        word1,
        text: this.disasmWords(word0, word1),
      });
    }

    if (this.diagnostics.length > 0) {
      return false;
    }

    if (!explicitEntry) {
      if (symMap.has('_start')) entry = symMap.get('_start')!;
      else if (symMap.has('main')) entry = symMap.get('main')!;
    }

    this.loadAddr = origin;
    this.entryPc = entry;
    this.programImage = new Uint8Array(bytes);
    this.hasProgram = true;
    this.localReset();
    return true;
  }

  private localReset(): void {
    this.mem.fill(0);
    if (this.hasProgram) {
      this.mem.set(this.programImage, this.loadAddr);
    }
    this.regs.fill(0);
    this.prevRegs.fill(0);
    this.pc = this.entryPc;
    this.prevPc = this.entryPc;
    this.sp = 0xf000;
    this.prevSp = 0xf000;
    this.flags = 0;
    this.prevFlags = 0;
    this.halted = false;
    this.errorCode = 0;
    this.errorMessage = 'OK';
    this.faultPc = 0;
    this.cycles = 0;
    this.instructionsExecuted = 0;
    this.lastRead = -1;
    this.lastWrite = -1;
    this.trace = [];
    this.rawCallStack = [];
    this.consoleBuf = '';
    this.numericOutputs = [];
    this.inputQueue = [];
    this.heapStart = 0x9000;
    this.heapBrk = 0x9000;
    this.heapLimit = 0xe000;
    this.allocCount = 0;
    this.syscallCount = 0;
    this.lastSyscall = 0;
    this.exitCode = 0;
    this.rngState = 0xace1;
    this.fbWrites = 0;
  }

  private nextRandom(): number {
    let x = this.rngState || 0xace1;
    x ^= (x << 7) & 0xffff;
    x ^= x >>> 9;
    x ^= (x << 8) & 0xffff;
    this.rngState = x & 0xffff;
    return this.rngState;
  }

  private handleSyscall(sysId: number): void {
    this.syscallCount++;
    this.lastSyscall = sysId & 0xffff;
    const arg0 = this.regs[1];
    const arg1 = this.regs[2];
    const arg2 = this.regs[3];

    switch (sysId) {
      case 0: /* FORGE_SYS_EXIT */
        this.exitCode = arg0;
        this.regs[0] = arg0;
        this.halted = true;
        break;
      case 1: /* FORGE_SYS_PRINT_INT */
        this.numericOutputs.push(arg0);
        this.consoleBuf += `${arg0}\n`;
        break;
      case 2: /* FORGE_SYS_PRINT_CHAR */
        this.consoleBuf += String.fromCharCode(arg0 & 0xff);
        break;
      case 3: { /* FORGE_SYS_PRINT_STR */
        let addr = arg0;
        for (let i = 0; i < 1024; i++) {
          const ch = this.mem[addr];
          if (!ch) break;
          this.consoleBuf += String.fromCharCode(ch);
          if (addr === 0xffff) break;
          addr++;
        }
        break;
      }
      case 4: /* FORGE_SYS_READ_CHAR */
        this.regs[0] = this.inputQueue.shift() ?? 0;
        break;
      case 5: /* FORGE_SYS_KEY_AVAILABLE */
        this.regs[0] = this.inputQueue.length > 0 ? 1 : 0;
        break;
      case 6: { /* FORGE_SYS_ALLOC */
        if (arg0 === 0) {
          this.regs[0] = 0;
          break;
        }
        const aligned = (arg0 + 1) & ~1;
        if (this.heapBrk + aligned > this.heapLimit) {
          this.regs[0] = 0;
          break;
        }
        const ptr = this.heapBrk;
        this.mem.fill(0, ptr, ptr + aligned);
        this.heapBrk += aligned;
        this.allocCount++;
        this.regs[0] = ptr;
        break;
      }
      case 7: /* FORGE_SYS_FREE_ALL */
        this.heapBrk = this.heapStart;
        this.allocCount = 0;
        this.regs[0] = 0;
        break;
      case 8: /* FORGE_SYS_DRAW_PIXEL */
        if (arg0 < 64 && arg1 < 32) {
          const off = 0xf000 + arg1 * 64 + arg0;
          this.mem[off] = arg2 & 0x0f;
          this.lastWrite = off;
          this.fbWrites++;
        }
        break;
      case 9: /* FORGE_SYS_CLEAR_SCREEN */
        this.mem.fill(arg0 & 0x0f, 0xf000, 0xf000 + 2048);
        this.fbWrites++;
        break;
      case 10: /* FORGE_SYS_GET_PIXEL */
        if (arg0 < 64 && arg1 < 32) {
          this.regs[0] = this.mem[0xf000 + arg1 * 64 + arg0] & 0x0f;
        } else {
          this.regs[0] = 0;
        }
        break;
      case 11: /* FORGE_SYS_CYCLES */
        this.regs[0] = this.cycles & 0xffff;
        break;
      case 12: /* FORGE_SYS_RANDOM */
        this.regs[0] = this.nextRandom();
        break;
    }
  }

  private read16(addr: number): number {
    this.lastRead = addr;
    return this.mem[addr] | (this.mem[(addr + 1) & 0xffff] << 8);
  }

  private write16(addr: number, val: number): void {
    this.lastWrite = addr;
    this.mem[addr] = val & 0xff;
    this.mem[(addr + 1) & 0xffff] = (val >> 8) & 0xff;
  }

  private znFlags(res16: number): number {
    let f = 0;
    if ((res16 & 0xffff) === 0) f |= FLAG_Z;
    if ((res16 & 0x8000) !== 0) f |= FLAG_N;
    return f;
  }

  private localStep(count: number): void {
    if (this.errorCode === 10) this.errorCode = 0;
    this.prevRegs.set(this.regs);
    this.prevPc = this.pc;
    this.prevSp = this.sp;
    this.prevFlags = this.flags;

    for (let s = 0; s < count; s++) {
      if (this.halted || this.errorCode !== 0) break;
      const pcBefore = this.pc;
      if (pcBefore > 0xfffc) {
        this.errorCode = 2;
        this.errorMessage = 'Memory access out of bounds';
        this.faultPc = pcBefore;
        this.halted = true;
        break;
      }

      const w0 = this.read16(pcBefore);
      const w1 = this.read16(pcBefore + 2);
      const op = (w0 >> 10) & 0x3f;
      const mode = (w0 >> 8) & 0x03;
      const rd = (w0 >> 5) & 0x07;
      const rs = (w0 >> 2) & 0x07;
      const mnem = CODE_TO_MNEMONIC[op];

      if (!mnem || (w0 & 0x03) !== 0) {
        this.errorCode = 1;
        this.errorMessage = 'Invalid instruction';
        this.faultPc = pcBefore;
        this.halted = true;
        break;
      }

      let nextPc = (pcBefore + 4) & 0xffff;
      const srcVal = mode === MODE_IMM ? w1 : this.regs[rs];
      const effAddr = mode === MODE_MEM_DIR ? w1 : (this.regs[rs] + w1) & 0xffff;
      const cyc = OPCODE_MAP[mnem].cycles;

      switch (mnem) {
        case 'NOP':
          break;
        case 'HALT':
          this.halted = true;
          break;
        case 'MOV':
          this.regs[rd] = srcVal & 0xffff;
          break;
        case 'LOAD':
          if (effAddr === 0xffff) {
            this.errorCode = 2;
            this.errorMessage = 'Memory access out of bounds';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          if (effAddr === 0xff00) {
            this.lastRead = effAddr;
            this.regs[rd] = this.cycles & 0xffff;
          } else if (effAddr === 0xff02) {
            this.lastRead = effAddr;
            this.regs[rd] = this.inputQueue.length > 0 ? 1 : 0;
          } else if (effAddr === 0xff04) {
            this.lastRead = effAddr;
            this.regs[rd] = this.inputQueue.shift() ?? 0;
          } else if (effAddr === 0xff06) {
            this.lastRead = effAddr;
            this.regs[rd] = this.heapBrk;
          } else if (effAddr === 0xff08) {
            this.lastRead = effAddr;
            this.regs[rd] = this.nextRandom();
          } else {
            this.regs[rd] = this.read16(effAddr);
          }
          break;
        case 'STORE':
          if (effAddr === 0xffff || effAddr <= 0x03ff) {
            this.errorCode = 2;
            this.errorMessage = 'Memory access out of bounds or protected';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          this.write16(effAddr, this.regs[rd]);
          if (effAddr >= 0xf000 && effAddr <= 0xfeff) {
            this.fbWrites++;
          }
          break;
        case 'ADD': {
          const a = this.regs[rd];
          const b = srcVal;
          const sum = a + b;
          const res16 = sum & 0xffff;
          this.regs[rd] = res16;
          this.flags = this.znFlags(res16);
          if (sum > 0xffff) this.flags |= FLAG_C;
          if ((~(a ^ b) & (a ^ res16) & 0x8000) !== 0) this.flags |= FLAG_V;
          break;
        }
        case 'SUB':
        case 'CMP': {
          const a = this.regs[rd];
          const b = srcVal;
          const res16 = (a - b) & 0xffff;
          if (mnem === 'SUB') this.regs[rd] = res16;
          this.flags = this.znFlags(res16);
          if (a < b) this.flags |= FLAG_C;
          if (((a ^ b) & (a ^ res16) & 0x8000) !== 0) this.flags |= FLAG_V;
          break;
        }
        case 'MUL': {
          const a = this.regs[rd];
          const b = srcVal;
          const prodU = a * b;
          const sa = a >= 0x8000 ? a - 0x10000 : a;
          const sb = b >= 0x8000 ? b - 0x10000 : b;
          const prodS = sa * sb;
          const res16 = prodU & 0xffff;
          this.regs[rd] = res16;
          this.flags = this.znFlags(res16);
          if (prodU > 0xffff) this.flags |= FLAG_C;
          if (prodS < -32768 || prodS > 32767) this.flags |= FLAG_V;
          break;
        }
        case 'DIV': {
          if (srcVal === 0) {
            this.errorCode = 3;
            this.errorMessage = 'Division by zero';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          const res16 = Math.floor(this.regs[rd] / srcVal) & 0xffff;
          this.regs[rd] = res16;
          this.flags = this.znFlags(res16);
          break;
        }
        case 'AND':
          this.regs[rd] = this.regs[rd] & srcVal;
          this.flags = this.znFlags(this.regs[rd]);
          break;
        case 'OR':
          this.regs[rd] = this.regs[rd] | srcVal;
          this.flags = this.znFlags(this.regs[rd]);
          break;
        case 'XOR':
          this.regs[rd] = this.regs[rd] ^ srcVal;
          this.flags = this.znFlags(this.regs[rd]);
          break;
        case 'NOT':
          this.regs[rd] = (~this.regs[rd]) & 0xffff;
          this.flags = this.znFlags(this.regs[rd]);
          break;
        case 'SHL': {
          const cnt = srcVal & 0x1f;
          const a = this.regs[rd];
          const carry = cnt > 0 && cnt <= 16 ? (a >> (16 - cnt)) & 1 : 0;
          const res16 = cnt < 16 ? (a << cnt) & 0xffff : 0;
          this.regs[rd] = res16;
          this.flags = this.znFlags(res16) | (carry ? FLAG_C : 0);
          break;
        }
        case 'SHR': {
          const cnt = srcVal & 0x1f;
          const a = this.regs[rd];
          const carry = cnt > 0 && cnt <= 16 ? (a >> (cnt - 1)) & 1 : 0;
          const res16 = cnt < 16 ? (a >>> cnt) & 0xffff : 0;
          this.regs[rd] = res16;
          this.flags = this.znFlags(res16) | (carry ? FLAG_C : 0);
          break;
        }
        case 'JMP':
        case 'JZ':
        case 'JNZ':
        case 'JC':
        case 'JNC':
        case 'JN':
        case 'JL':
        case 'JLE':
        case 'JG':
        case 'JGE': {
          const z = (this.flags & FLAG_Z) !== 0;
          const n = (this.flags & FLAG_N) !== 0;
          const c = (this.flags & FLAG_C) !== 0;
          const v = (this.flags & FLAG_V) !== 0;
          const take =
            mnem === 'JMP' ||
            (mnem === 'JZ' && z) ||
            (mnem === 'JNZ' && !z) ||
            (mnem === 'JC' && c) ||
            (mnem === 'JNC' && !c) ||
            (mnem === 'JN' && n) ||
            (mnem === 'JL' && n !== v) ||
            (mnem === 'JLE' && (z || n !== v)) ||
            (mnem === 'JG' && !z && n === v) ||
            (mnem === 'JGE' && n === v);
          if (take) nextPc = srcVal;
          break;
        }
        case 'CALL':
          if (this.sp < 0xe002) {
            this.errorCode = 4;
            this.errorMessage = 'Stack overflow';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          this.sp -= 2;
          this.write16(this.sp, nextPc);
          this.rawCallStack.push({
            calleeAddr: srcVal,
            callerPc: pcBefore,
            returnAddr: nextPc,
            frameFp: this.regs[7],
            frameSp: this.sp,
          });
          nextPc = srcVal;
          break;
        case 'RET':
          if (this.sp >= 0xf000) {
            this.errorCode = 5;
            this.errorMessage = 'Stack underflow';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          nextPc = this.read16(this.sp);
          this.sp += 2;
          if (this.rawCallStack.length > 0) {
            this.rawCallStack.pop();
          }
          break;
        case 'PUSH': {
          if (this.sp < 0xe002) {
            this.errorCode = 4;
            this.errorMessage = 'Stack overflow';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          const pval = mode === MODE_IMM ? w1 : this.regs[rd];
          this.sp -= 2;
          this.write16(this.sp, pval);
          break;
        }
        case 'POP':
          if (this.sp >= 0xf000) {
            this.errorCode = 5;
            this.errorMessage = 'Stack underflow';
            this.faultPc = pcBefore;
            this.halted = true;
            return;
          }
          this.regs[rd] = this.read16(this.sp);
          this.sp += 2;
          break;
        case 'IN':
          if (srcVal === 2) {
            this.regs[rd] = this.inputQueue.length > 0 ? 1 : 0;
          } else {
            this.regs[rd] = this.inputQueue.shift() ?? 0;
          }
          break;
        case 'OUT':
          if (srcVal === 0) {
            this.consoleBuf += String.fromCharCode(this.regs[rd] & 0xff);
          } else if (srcVal === 1) {
            this.numericOutputs.push(this.regs[rd]);
            this.consoleBuf += `${this.regs[rd]}\n`;
          } else if (srcVal === 3) {
            this.handleSyscall(this.regs[rd]);
          }
          break;
      }

      this.pc = nextPc;
      this.instructionsExecuted++;
      this.cycles += cyc;

      this.trace.push({
        step: this.instructionsExecuted,
        pcBefore,
        pcAfter: this.pc,
        spAfter: this.sp,
        flagsAfter: this.flags,
        cycles: cyc,
        mnemonic: this.disasmWords(w0, w1),
      });
      if (this.trace.length > 64) {
        this.trace.shift();
      }
    }
  }

  private localRun(maxSteps: number): void {
    if (this.errorCode === 10) this.errorCode = 0;
    this.prevRegs.set(this.regs);
    this.prevPc = this.pc;
    this.prevSp = this.sp;
    this.prevFlags = this.flags;

    let first = true;
    for (let i = 0; i < maxSteps; i++) {
      if (this.halted || this.errorCode !== 0) break;
      if (!first && this.breakpoints.has(this.pc)) {
        this.errorCode = 10;
        break;
      }
      first = false;
      this.localStep(1);
    }
  }

  private peek16(addr: number): number {
    if (addr >= 0xffff) return 0;
    return this.mem[addr] | (this.mem[addr + 1] << 8);
  }

  private toSigned16(v: number): number {
    return v >= 0x8000 ? v - 0x10000 : v;
  }

  private buildLocalState(memStart: number, memLen: number): ForgeStateResponse {
    const slice: number[] = [];
    for (let i = 0; i < memLen; i++) {
      const addr = memStart + i;
      slice.push(addr < 65536 ? this.mem[addr] : 0);
    }

    const watchList: ForgeWatchEntry[] = this.watches.map((addr) => ({
      address: addr,
      value8: this.mem[addr] ?? 0,
      value16: this.peek16(addr),
    }));

    const stackList: ForgeStackEntry[] = [];
    for (let a = this.sp; a + 2 <= 0xf000 && stackList.length < 32; a += 2) {
      stackList.push({
        address: a,
        value: this.peek16(a),
      });
    }

    const globals: ForgeGlobalVar[] = this.compSymbols
      .filter((s) => s.kind === 'global')
      .map((s) => {
        const val = this.peek16(s.globalAddr);
        return {
          name: s.name,
          address: s.globalAddr,
          value: val,
          signedValue: this.toSigned16(val),
          declLine: s.declLine,
        };
      });

    const callStack: ForgeCallFrame[] = this.rawCallStack.map((cf, idx) => {
      const sym = this.symbols.find((s) => !s.isConstant && s.value === cf.calleeAddr);
      const fnName = sym
        ? sym.name.startsWith('fn_')
          ? sym.name.slice(3)
          : sym.name
        : 'subroutine';
      const locals: ForgeLocalVar[] = this.compSymbols
        .filter((s) => (s.kind === 'param' || s.kind === 'local') && s.scopeFn === fnName)
        .map((s) => {
          const addr = (cf.frameFp + s.fpOffset) & 0xffff;
          const val = this.peek16(addr);
          return {
            name: s.name,
            kind: s.kind as 'param' | 'local',
            offset: s.fpOffset,
            address: addr,
            value: val,
            signedValue: this.toSigned16(val),
          };
        });
      return {
        depth: idx,
        function: fnName,
        calleeAddr: cf.calleeAddr,
        callerPc: cf.callerPc,
        callerSourceLine: this.getSourceLineForPc(cf.callerPc),
        returnAddr: cf.returnAddr,
        frameFp: cf.frameFp,
        frameSp: cf.frameSp,
        locals,
      };
    });

    const activeSourceLine = this.getSourceLineForPc(this.pc);
    const activeDisasm = this.disassembly.find((d) => d.address === this.pc);

    const hexChars = '0123456789ABCDEF';
    const fbChars: string[] = [];
    for (let i = 0; i < 2048; i++) {
      const b = this.mem[0xf000 + i];
      let c = b & 0x0f;
      if (c === 0 && b !== 0) c = 15;
      fbChars.push(hexChars[c]);
    }

    return {
      success: this.diagnostics.length === 0,
      mode: this.isCompiled ? 'forgelang' : 'assembly',
      hasProgram: this.hasProgram,
      activeSourceLine,
      activeAsmLine: activeDisasm?.line ?? 0,
      pc: this.pc,
      prevPc: this.prevPc,
      sp: this.sp,
      prevSp: this.prevSp,
      flags: this.flags,
      prevFlags: this.prevFlags,
      flagZ: (this.flags & FLAG_Z) !== 0,
      flagN: (this.flags & FLAG_N) !== 0,
      flagC: (this.flags & FLAG_C) !== 0,
      flagV: (this.flags & FLAG_V) !== 0,
      halted: this.halted,
      errorCode: this.errorCode,
      errorMessage: this.errorMessage,
      faultPc: this.faultPc,
      cycles: this.cycles,
      instructionsExecuted: this.instructionsExecuted,
      registers: Array.from(this.regs),
      prevRegisters: Array.from(this.prevRegs),
      breakpoints: Array.from(this.breakpoints),
      disassembly: [...this.disassembly],
      sourceMap: [...this.sourceMap],
      generatedAsm: this.generatedAsm,
      astText: this.astText,
      globals,
      callStack,
      symbols: [...this.symbols],
      memory: {
        start: memStart,
        length: memLen,
        lastRead: this.lastRead,
        lastWrite: this.lastWrite,
        bytes: slice,
      },
      watches: watchList,
      stack: stackList,
      trace: [...this.trace],
      console: this.consoleBuf,
      numericOutputs: [...this.numericOutputs],
      framebuffer: {
        width: 64,
        height: 32,
        vramStart: 0xf000,
        fbWrites: this.fbWrites,
        pixelsHex: fbChars.join(''),
      },
      os: {
        heapStart: this.heapStart,
        heapBrk: this.heapBrk,
        heapLimit: this.heapLimit,
        heapUsedBytes: this.heapBrk - this.heapStart,
        allocCount: this.allocCount,
        syscallCount: this.syscallCount,
        lastSyscall: this.lastSyscall,
        exitCode: this.exitCode,
        inputQueueCount: this.inputQueue.length,
      },
      diagnostics: [...this.diagnostics],
    };
  }
}
