import { Component, OnDestroy, OnInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ForgeBridgeService } from './forge-bridge.service';
import {
  ExampleProgram,
  FORGE_EXAMPLES,
  FORGE_LANG_EXAMPLES,
  ForgeDisasmLine,
  ForgeStateResponse,
} from './forge.models';
import { MonacoEditorComponent } from './monaco-editor.component';
import {
  FORGE_TUTORIALS,
  FORGE_EXERCISES,
  TutorialLesson,
  GuidedExercise,
} from './tutorials-data';
import { FORGE_ISA_TABLE, IsaInstruction } from './isa-data';

export interface MemoryRow {
  baseAddress: number;
  bytes: { addr: number; val: number; isRead: boolean; isWrite: boolean }[];
  ascii: string;
}

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [CommonModule, FormsModule, MonacoEditorComponent],
  templateUrl: './app.html',
  styleUrl: './app.css',
})
export class App implements OnInit, OnDestroy {
  public bridge = inject(ForgeBridgeService);

  public asmExamples: ExampleProgram[] = FORGE_EXAMPLES;
  public flExamples: ExampleProgram[] = FORGE_LANG_EXAMPLES;
  public allExamples: ExampleProgram[] = [...FORGE_EXAMPLES, ...FORGE_LANG_EXAMPLES];

  public editorLanguage: 'assembly' | 'forgelang' = 'assembly';
  public selectedExampleId = FORGE_EXAMPLES[0].id;
  public sourceCode = FORGE_EXAMPLES[0].source;

  public state: ForgeStateResponse | null = null;
  public statusBanner = 'Ready';
  public isRunningLoop = false;
  public isPaused = false;
  public stepsPerSecond = 8;
  private runTimer: any = null;

  /* Execution Panel View Tab: disassembly, generated-asm, ast, or decoder */
  public activeExecTab: 'disassembly' | 'generated-asm' | 'ast' | 'decoder' = 'disassembly';

  /* Memory Inspector Controls */
  public memStartAddress = 0x8000;
  public memAddressInput = '0x8000';
  public memLength = 128;

  /* Live Memory Editor Controls */
  public editMemAddrInput = '0x8000';
  public editMemValInput = '0x002A';
  public editMemWidth: 8 | 16 = 16;

  /* Watchlist Controls */
  public watchAddrInput = '0x8004';

  /* Console Input Queue Controls */
  public consoleInputText = '';

  /* Bottom Panel Active Tab */
  public activeBottomTab: 'console' | 'display' | 'variables' | 'trace' | 'microarch' | 'symbols' = 'console';

  /* Interactive Learning Modals */
  public showTutorialsModal = false;
  public showDiagramModal = false;
  public showIsaModal = false;
  public showProfilerModal = false;

  public activeTutorialTab: 'tutorials' | 'exercises' = 'tutorials';
  public selectedTutorialId = 'tut_arch_alu';
  public selectedExerciseId = 'ex_popcount';
  public showExerciseSolution = false;

  public tutorialsList = FORGE_TUTORIALS;
  public exercisesList = FORGE_EXERCISES;
  public isaTable = FORGE_ISA_TABLE;
  public isaSearchText = '';

  public readonly paletteColors: string[] = [
    '#0b0f17', /* 0: Black */
    '#1d4ed8', /* 1: Blue */
    '#15803d', /* 2: Green */
    '#0e7490', /* 3: Cyan */
    '#b91c1c', /* 4: Red */
    '#a21caf', /* 5: Magenta */
    '#b45309', /* 6: Amber */
    '#94a3b8', /* 7: Light Gray */
    '#475569', /* 8: Dark Gray */
    '#38bdf8', /* 9: Bright Blue */
    '#4ade80', /* 10: Bright Green */
    '#22d3ee', /* 11: Bright Cyan */
    '#f87171', /* 12: Bright Red */
    '#e879f9', /* 13: Bright Magenta */
    '#facc15', /* 14: Yellow */
    '#f8fafc', /* 15: White */
  ];

  constructor() {
    this.state = this.bridge.assembleSync(this.sourceCode, this.memStartAddress, this.memLength);
    this.statusBanner = `Assembled (${this.state.disassembly.length} instructions, entry PC=${this.hex16(this.state.pc)})`;
  }

  async ngOnInit(): Promise<void> {
    await this.buildProgram();
  }

  ngOnDestroy(): void {
    this.clearRunLoop();
  }

  /* ==================== Language Mode & Build Actions ==================== */

  async setLanguageMode(lang: 'assembly' | 'forgelang'): Promise<void> {
    if (this.editorLanguage === lang) return;
    this.editorLanguage = lang;
    if (lang === 'forgelang') {
      const def = this.flExamples[0];
      this.selectedExampleId = def.id;
      this.sourceCode = def.source;
      await this.compileProgram();
    } else {
      const def = this.asmExamples[0];
      this.selectedExampleId = def.id;
      this.sourceCode = def.source;
      this.activeExecTab = 'disassembly';
      await this.assembleProgram();
    }
  }

  async buildProgram(): Promise<void> {
    if (this.editorLanguage === 'forgelang') {
      await this.compileProgram();
    } else {
      await this.assembleProgram();
    }
  }

  async assembleProgram(): Promise<void> {
    this.clearRunLoop();
    this.isPaused = false;
    this.editorLanguage = 'assembly';
    this.state = await this.bridge.assemble(this.sourceCode, this.memStartAddress, this.memLength);
    if (this.state.diagnostics.length > 0) {
      this.statusBanner = `Assembly failed (${this.state.diagnostics.length} error(s))`;
      this.activeBottomTab = 'console';
    } else {
      this.statusBanner = `Assembled (${this.state.disassembly.length} instructions, entry PC=${this.hex16(this.state.pc)})`;
    }
  }

  async compileProgram(): Promise<void> {
    this.clearRunLoop();
    this.isPaused = false;
    this.editorLanguage = 'forgelang';
    this.state = await this.bridge.compile(this.sourceCode, this.memStartAddress, this.memLength);
    if (this.state.diagnostics.length > 0) {
      this.statusBanner = `Compilation failed (${this.state.diagnostics.length} error(s))`;
      this.activeBottomTab = 'console';
    } else {
      this.statusBanner = `Compiled ForgeLang (${this.state.disassembly.length} instructions, ${this.state.sourceMap?.length ?? 0} source-map entries)`;
    }
  }

  /* ==================== Execution & Source-Level Stepping ==================== */

  async stepInstruction(): Promise<void> {
    if (!this.state || this.state.halted) return;
    this.state = await this.bridge.step(1, this.memStartAddress, this.memLength);
    this.updateStatusFromState();
  }

  async stepSourceLine(): Promise<void> {
    if (!this.state || this.state.halted) return;
    this.state = await this.bridge.stepLine(this.memStartAddress, this.memLength);
    this.updateStatusFromState();
  }

  async stepOverCall(): Promise<void> {
    if (!this.state || this.state.halted) return;
    this.state = await this.bridge.stepOver(this.memStartAddress, this.memLength);
    this.updateStatusFromState();
  }

  async stepOutOfFunction(): Promise<void> {
    if (!this.state || this.state.halted) return;
    this.state = await this.bridge.stepOut(this.memStartAddress, this.memLength);
    this.updateStatusFromState();
  }

  async startRun(): Promise<void> {
    if (!this.state) {
      await this.buildProgram();
    }
    if (!this.state || this.state.halted) {
      await this.resetCpu();
    }
    this.isPaused = false;
    if (this.stepsPerSecond >= 50) {
      this.clearRunLoop();
      this.state = await this.bridge.run(100000, this.memStartAddress, this.memLength);
      this.updateStatusFromState();
      return;
    }
    this.startTimedLoop();
  }

  pauseRun(): void {
    if (!this.isRunningLoop) return;
    this.clearRunLoop();
    this.isPaused = true;
    this.statusBanner = `Paused at PC=${this.hex16(this.state?.pc ?? 0)}`;
  }

  resumeRun(): void {
    if (!this.state || this.state.halted) return;
    this.isPaused = false;
    this.startTimedLoop();
  }

  async stopExecution(): Promise<void> {
    this.clearRunLoop();
    this.isPaused = false;
    await this.resetCpu();
    this.statusBanner = 'Stopped and reset to entry point';
  }

  async resetCpu(): Promise<void> {
    this.clearRunLoop();
    this.isPaused = false;
    this.state = await this.bridge.reset(this.memStartAddress, this.memLength);
    this.statusBanner = `CPU Reset (PC=${this.hex16(this.state.pc)}, SP=${this.hex16(this.state.sp)})`;
  }

  onSpeedChange(): void {
    if (this.isRunningLoop) {
      this.startTimedLoop();
    }
  }

  private startTimedLoop(): void {
    this.clearRunLoop();
    this.isRunningLoop = true;
    this.statusBanner = `Running (${this.stepsPerSecond} Hz)...`;
    const intervalMs = Math.max(20, Math.floor(1000 / Math.max(1, this.stepsPerSecond)));

    this.runTimer = setInterval(async () => {
      if (!this.state || this.state.halted || this.state.errorCode !== 0) {
        this.clearRunLoop();
        this.updateStatusFromState();
        return;
      }
      this.state = await this.bridge.step(1, this.memStartAddress, this.memLength);
      if (this.state.breakpoints.includes(this.state.pc)) {
        this.clearRunLoop();
        this.isPaused = true;
        this.statusBanner = `Breakpoint hit at ${this.hex16(this.state.pc)} (Line ${this.activeSourceLine ?? '?'})`;
        return;
      }
      if (this.state.halted || this.state.errorCode !== 0) {
        this.clearRunLoop();
        this.updateStatusFromState();
      }
    }, intervalMs);
  }

  private clearRunLoop(): void {
    if (this.runTimer) {
      clearInterval(this.runTimer);
      this.runTimer = null;
    }
    this.isRunningLoop = false;
  }

  private updateStatusFromState(): void {
    if (!this.state) return;
    if (this.state.errorCode !== 0 && this.state.errorCode !== 10) {
      this.statusBanner = `Runtime Fault at ${this.hex16(this.state.faultPc)}: ${this.state.errorMessage}`;
    } else if (this.state.halted) {
      this.statusBanner = `Halted normally after ${this.state.instructionsExecuted} instructions (${this.state.cycles} cycles)`;
    } else if (this.state.errorCode === 10) {
      this.isPaused = true;
      this.statusBanner = `Breakpoint hit at ${this.hex16(this.state.pc)} (Line ${this.activeSourceLine ?? '?'})`;
    } else {
      const lineInfo = this.activeSourceLine ? `, Line ${this.activeSourceLine}` : '';
      this.statusBanner = `Stepped -> PC=${this.hex16(this.state.pc)}${lineInfo} (Step #${this.state.instructionsExecuted})`;
    }
  }

  /* ==================== Breakpoints, Memory & Watches ==================== */

  async toggleBreakpoint(address: number): Promise<void> {
    this.state = await this.bridge.toggleBreakpoint(address, this.memStartAddress, this.memLength);
  }

  async toggleSourceLineBreakpoint(line: number): Promise<void> {
    this.state = await this.bridge.toggleSourceLineBreakpoint(line, this.memStartAddress, this.memLength);
  }

  hasBreakpoint(address: number): boolean {
    return this.state?.breakpoints.includes(address) ?? false;
  }

  get breakpointSourceLines(): number[] {
    if (!this.state) return [];
    const lines = new Set<number>();
    for (const bpAddr of this.state.breakpoints) {
      if (this.editorLanguage === 'forgelang' && this.state.sourceMap) {
        const sm = this.state.sourceMap.find((s) => s.address === bpAddr);
        if (sm && sm.sourceLine > 0) lines.add(sm.sourceLine);
      } else {
        const d = this.state.disassembly.find((x) => x.address === bpAddr);
        if (d && d.line > 0) lines.add(d.line);
      }
    }
    return Array.from(lines);
  }

  async jumpToMemoryRegion(addr: number): Promise<void> {
    this.memStartAddress = addr & 0xfff0;
    this.memAddressInput = this.hex16(this.memStartAddress);
    this.state = await this.bridge.fetchState(this.memStartAddress, this.memLength);
  }

  async applyCustomMemoryAddress(): Promise<void> {
    const parsed = this.parseInputNumber(this.memAddressInput);
    if (parsed !== null) {
      await this.jumpToMemoryRegion(parsed);
    }
  }

  selectByteForEdit(addr: number, currentVal: number): void {
    this.editMemAddrInput = this.hex16(addr);
    this.editMemValInput = this.hex8(currentVal);
    this.editMemWidth = 8;
  }

  async writeMemoryValue(): Promise<void> {
    const addr = this.parseInputNumber(this.editMemAddrInput);
    const val = this.parseInputNumber(this.editMemValInput);
    if (addr === null || val === null) {
      this.statusBanner = 'Invalid memory address or value format';
      return;
    }
    this.state = await this.bridge.writeMemory(addr, val, this.editMemWidth, this.memStartAddress, this.memLength);
    this.statusBanner = `Wrote ${this.editMemWidth}-bit value ${val} to [${this.hex16(addr)}]`;
  }

  async addWatchAddress(): Promise<void> {
    const addr = this.parseInputNumber(this.watchAddrInput);
    if (addr === null) return;
    this.state = await this.bridge.setWatch(addr, true, this.memStartAddress, this.memLength);
  }

  async removeWatchAddress(addr: number): Promise<void> {
    this.state = await this.bridge.setWatch(addr, false, this.memStartAddress, this.memLength);
  }

  async submitConsoleInput(): Promise<void> {
    if (!this.consoleInputText) return;
    const payload = this.consoleInputText + '\n';
    this.consoleInputText = '';
    this.state = await this.bridge.sendInput(payload, undefined, this.memStartAddress, this.memLength);
  }

  async sendQuickKey(keyCharOrCode: string | number): Promise<void> {
    if (typeof keyCharOrCode === 'string') {
      this.state = await this.bridge.sendInput(keyCharOrCode, undefined, this.memStartAddress, this.memLength);
    } else {
      this.state = await this.bridge.sendInput('', keyCharOrCode, this.memStartAddress, this.memLength);
    }
  }

  /* ==================== Examples, Import & Export ==================== */

  async loadSelectedExample(): Promise<void> {
    const ex = this.allExamples.find((e) => e.id === this.selectedExampleId);
    if (!ex) return;
    this.editorLanguage = ex.language;
    this.sourceCode = ex.source;
    if (ex.language === 'forgelang') {
      await this.compileProgram();
    } else {
      this.activeExecTab = 'disassembly';
      await this.assembleProgram();
    }
  }

  exportAssemblyFile(): void {
    const ext = this.editorLanguage === 'forgelang' ? 'fl' : 'asm';
    const blob = new Blob([this.sourceCode], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${this.selectedExampleId || 'program'}.${ext}`;
    a.click();
    URL.revokeObjectURL(url);
  }

  onImportAssemblyFile(event: Event): void {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    if (!file) return;
    const isFl = file.name.endsWith('.fl');
    const reader = new FileReader();
    reader.onload = async () => {
      if (typeof reader.result === 'string') {
        this.sourceCode = reader.result;
        if (isFl) {
          await this.compileProgram();
        } else {
          await this.assembleProgram();
        }
      }
    };
    reader.readAsText(file);
  }

  /* ==================== Computed View Helpers ==================== */

  get activeSourceLine(): number | null {
    if (!this.state) return null;
    if (this.state.activeSourceLine && this.state.activeSourceLine > 0) {
      return this.state.activeSourceLine;
    }
    const found = this.state.disassembly.find((d) => d.address === this.state!.pc);
    if (!found) return null;
    return this.editorLanguage === 'forgelang' ? (found.sourceLine ?? found.line) : found.line;
  }

  get generatedAsmLines(): { lineNum: number; text: string; isActive: boolean }[] {
    const raw = this.state?.generatedAsm ?? '';
    if (!raw) return [];
    const activeAsm = this.state?.activeAsmLine ?? 0;
    return raw.split('\n').map((text, idx) => ({
      lineNum: idx + 1,
      text,
      isActive: idx + 1 === activeAsm,
    }));
  }

  get currentInstruction(): ForgeDisasmLine | null {
    if (!this.state) return null;
    return this.state.disassembly.find((d) => d.address === this.state!.pc) ?? null;
  }

  get activePixels(): { x: number; y: number; colorIdx: number; colorHex: string }[] {
    const hex = this.state?.framebuffer?.pixelsHex;
    if (!hex) return [];
    const out: { x: number; y: number; colorIdx: number; colorHex: string }[] = [];
    for (let i = 0; i < hex.length && i < 2048; i++) {
      const ch = hex[i];
      if (ch === '0') continue;
      const colorIdx = parseInt(ch, 16) || 0;
      if (colorIdx === 0) continue;
      out.push({
        x: i % 64,
        y: Math.floor(i / 64),
        colorIdx,
        colorHex: this.paletteColors[colorIdx] ?? '#f8fafc',
      });
    }
    return out;
  }

  get memoryRows(): MemoryRow[] {
    if (!this.state || !this.state.memory) return [];
    const { start, bytes, lastRead, lastWrite } = this.state.memory;
    const rows: MemoryRow[] = [];
    for (let offset = 0; offset < bytes.length; offset += 16) {
      const rowBase = (start + offset) & 0xffff;
      const rowBytes: { addr: number; val: number; isRead: boolean; isWrite: boolean }[] = [];
      let ascii = '';
      for (let i = 0; i < 16 && offset + i < bytes.length; i++) {
        const addr = (rowBase + i) & 0xffff;
        const val = bytes[offset + i] ?? 0;
        const isRead = lastRead === addr || lastRead + 1 === addr;
        const isWrite = lastWrite === addr || lastWrite + 1 === addr;
        rowBytes.push({ addr, val, isRead, isWrite });
        ascii += val >= 32 && val <= 126 ? String.fromCharCode(val) : '.';
      }
      rows.push({ baseAddress: rowBase, bytes: rowBytes, ascii });
    }
    return rows;
  }

  regChanged(idx: number): boolean {
    if (!this.state) return false;
    return this.state.registers[idx] !== this.state.prevRegisters[idx];
  }

  regDelta(idx: number): string {
    if (!this.state) return '';
    return `${this.hex16(this.state.prevRegisters[idx])} → ${this.hex16(this.state.registers[idx])}`;
  }

  toSigned16(val: number): number {
    return val >= 0x8000 ? val - 0x10000 : val;
  }

  hex16(val: number): string {
    return '0x' + (val & 0xffff).toString(16).toUpperCase().padStart(4, '0');
  }

  hex8(val: number): string {
    return (val & 0xff).toString(16).toUpperCase().padStart(2, '0');
  }

  /* ==================== Interactive Tutorials & Exercises ==================== */

  get currentTutorial(): TutorialLesson {
    return this.tutorialsList.find((t) => t.id === this.selectedTutorialId) ?? this.tutorialsList[0];
  }

  get currentExercise(): GuidedExercise {
    return this.exercisesList.find((e) => e.id === this.selectedExerciseId) ?? this.exercisesList[0];
  }

  async loadTutorial(tut: TutorialLesson): Promise<void> {
    this.selectedTutorialId = tut.id;
    this.editorLanguage = tut.language;
    this.sourceCode = tut.sampleCode;
    this.showTutorialsModal = false;
    if (this.editorLanguage === 'forgelang') {
      await this.compileProgram();
    } else {
      await this.assembleProgram();
    }
  }

  async loadExerciseStarter(ex: GuidedExercise): Promise<void> {
    this.selectedExerciseId = ex.id;
    this.editorLanguage = ex.language;
    this.sourceCode = ex.starterCode;
    this.showExerciseSolution = false;
    this.showTutorialsModal = false;
    if (this.editorLanguage === 'forgelang') {
      await this.compileProgram();
    } else {
      await this.assembleProgram();
    }
  }

  async loadExerciseSolution(ex: GuidedExercise): Promise<void> {
    this.selectedExerciseId = ex.id;
    this.editorLanguage = ex.language;
    this.sourceCode = ex.solutionCode;
    this.showExerciseSolution = true;
    this.showTutorialsModal = false;
    if (this.editorLanguage === 'forgelang') {
      await this.compileProgram();
    } else {
      await this.assembleProgram();
    }
  }

  get filteredIsa(): IsaInstruction[] {
    const q = this.isaSearchText.trim().toLowerCase();
    if (!q) return this.isaTable;
    return this.isaTable.filter(
      (i) =>
        i.mnemonic.toLowerCase().includes(q) ||
        i.category.toLowerCase().includes(q) ||
        i.description.toLowerCase().includes(q) ||
        i.syntax.toLowerCase().includes(q)
    );
  }

  /* ==================== Advanced Learning Features (Section 8) ==================== */

  get activeInstructionBreakdown() {
    const instr = this.currentInstruction;
    if (!instr) return null;
    const w0 = instr.word0 & 0xffff;
    const w1 = instr.word1 & 0xffff;
    const opcode = (w0 >> 10) & 0x3f;
    const mode = (w0 >> 8) & 0x03;
    const rd = (w0 >> 5) & 0x07;
    const rs = (w0 >> 2) & 0x07;
    const rsvd = w0 & 0x03;
    const modeNames = ['REG (Register)', 'IMM (Immediate)', 'MEM_DIR ([imm16])', 'MEM_IDX ([Rs+imm16])'];
    const fullHex = '0x' + ((w0 << 16) | w1).toString(16).toUpperCase().padStart(8, '0');
    const binW0 = w0.toString(2).padStart(16, '0');
    const binW1 = w1.toString(2).padStart(16, '0');
    return {
      text: instr.text,
      address: instr.address,
      fullHex,
      binW0,
      binW1,
      opcode,
      opcodeBin: opcode.toString(2).padStart(6, '0'),
      mode,
      modeName: modeNames[mode] ?? 'Unknown',
      modeBin: mode.toString(2).padStart(2, '0'),
      rd,
      rdName: `R${rd}`,
      rdBin: rd.toString(2).padStart(3, '0'),
      rs,
      rsName: `R${rs}`,
      rsBin: rs.toString(2).padStart(3, '0'),
      rsvd,
      imm16: w1,
      immHex: this.hex16(w1),
      immSigned: this.toSigned16(w1),
    };
  }

  get aluOperationInspection() {
    const instr = this.currentInstruction;
    if (!instr || !this.state) return null;
    const w0 = instr.word0 & 0xffff;
    const opcode = (w0 >> 10) & 0x3f;
    const mode = (w0 >> 8) & 0x03;
    const rd = (w0 >> 5) & 0x07;
    const rs = (w0 >> 2) & 0x07;
    const imm = instr.word1 & 0xffff;

    const aluOpcodes: { [key: number]: string } = {
      5: 'ADD',
      6: 'SUB',
      7: 'MUL',
      8: 'DIV',
      9: 'AND',
      10: 'OR',
      11: 'XOR',
      12: 'NOT',
      13: 'SHL',
      14: 'SHR',
      15: 'CMP',
    };
    if (!(opcode in aluOpcodes)) return null;

    const opName = aluOpcodes[opcode];
    const opA = this.state.registers[rd];
    const opB = mode === 0 ? this.state.registers[rs] : imm;
    return {
      opName,
      rdName: `R${rd}`,
      rsOrImmName: mode === 0 ? `R${rs}` : `IMM(${imm})`,
      opAVal: opA,
      opAHex: this.hex16(opA),
      opASigned: this.toSigned16(opA),
      opBVal: opB,
      opBHex: this.hex16(opB),
      opBSigned: this.toSigned16(opB),
      resultHex: this.hex16(this.state.registers[rd]),
      flagsUpdated: 'Z, N, C, V',
      currentFlags: {
        z: this.state.flagZ,
        n: this.state.flagN,
        c: this.state.flagC,
        v: this.state.flagV,
      },
    };
  }

  get profilerStats() {
    const executed = this.state?.instructionsExecuted ?? 0;
    const cycles = this.state?.cycles ?? 0;
    const cpi = executed > 0 ? (cycles / executed).toFixed(2) : '1.00';
    const trace = this.state?.trace ?? [];

    let arithCount = 0;
    let dataCount = 0;
    let branchCount = 0;
    let stackCount = 0;
    let ioCount = 0;
    let memReads = 0;
    let memWrites = 0;

    // Conceptual direct-mapped L1 16-word cache simulation
    const cacheLines = new Array(16).fill(-1);
    let cacheHits = 0;
    let cacheMisses = 0;

    for (const t of trace) {
      const m = t.mnemonic.split(' ')[0].toUpperCase();
      if (['ADD', 'SUB', 'MUL', 'DIV', 'AND', 'OR', 'XOR', 'NOT', 'SHL', 'SHR', 'CMP'].includes(m)) {
        arithCount++;
      } else if (['MOV', 'LOAD', 'STORE'].includes(m)) {
        dataCount++;
        if (m === 'LOAD') {
          memReads++;
          const idx = (t.pcAfter >> 1) & 0x0f;
          if (cacheLines[idx] === (t.pcAfter >> 4)) {
            cacheHits++;
          } else {
            cacheMisses++;
            cacheLines[idx] = t.pcAfter >> 4;
          }
        } else if (m === 'STORE') {
          memWrites++;
          const idx = (t.pcAfter >> 1) & 0x0f;
          cacheLines[idx] = t.pcAfter >> 4;
        }
      } else if (['JMP', 'JZ', 'JNZ', 'JC', 'JNC', 'JN', 'JL', 'JLE', 'JG', 'JGE'].includes(m)) {
        branchCount++;
      } else if (['PUSH', 'POP', 'CALL', 'RET'].includes(m)) {
        stackCount++;
        if (m === 'PUSH' || m === 'CALL') memWrites++;
        if (m === 'POP' || m === 'RET') memReads++;
      } else if (['IN', 'OUT'].includes(m)) {
        ioCount++;
      }
    }

    const totalCacheAccesses = cacheHits + cacheMisses;
    const hitRate = totalCacheAccesses > 0 ? ((cacheHits / totalCacheAccesses) * 100).toFixed(1) : '100.0';

    return {
      executed,
      cycles,
      cpi,
      arithCount,
      dataCount,
      branchCount,
      stackCount,
      ioCount,
      memReads,
      memWrites,
      cacheHits,
      cacheMisses,
      hitRate,
    };
  }

  private parseInputNumber(raw: string): number | null {
    const t = raw.trim();
    if (!t) return null;
    const n = t.startsWith('0x') || t.startsWith('0X') ? parseInt(t.slice(2), 16) : parseInt(t, 10);
    return Number.isNaN(n) ? null : n & 0xffff;
  }
}
