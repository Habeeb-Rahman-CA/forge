import { TestBed } from '@angular/core/testing';
import { App } from './app';

describe('Forge-16 Visual Debugger (App)', () => {
  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [App],
    }).compileComponents();
  });

  it('should create the debugger interface and assemble the default program', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    await app.ngOnInit();
    fixture.detectChanges();

    expect(app).toBeTruthy();
    expect(app.state).toBeTruthy();
    expect(app.state?.pc).toBe(0x0400);
    expect(app.state?.sp).toBe(0xf000);
    expect(app.state?.disassembly.length).toBeGreaterThan(5);
  });

  it('should assemble, step, and run a custom assembly program accurately', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    app.sourceCode = `
      MOV R0, 10
      MOV R1, 5
      ADD R0, R1
      OUT R0
      HALT
    `;
    await app.assembleProgram();
    expect(app.state?.pc).toBe(0x0400);
    expect(app.state?.registers[0]).toBe(0);

    await app.stepInstruction();
    expect(app.state?.pc).toBe(0x0404);
    expect(app.state?.registers[0]).toBe(10);
    expect(app.regChanged(0)).toBe(true);

    app.stepsPerSecond = 50; // Instant run
    await app.startRun();
    expect(app.state?.halted).toBe(true);
    expect(app.state?.registers[0]).toBe(15);
    expect(app.state?.numericOutputs).toEqual([15]);
    expect(app.state?.console).toBe('15\n');
  });

  it('should support setting breakpoints, editing memory, and watching addresses', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    app.sourceCode = `
      MOV R0, 1
      MOV R0, 2
      MOV R0, 3
      HALT
    `;
    await app.assembleProgram();

    // Set breakpoint on second instruction (0x0404)
    await app.toggleBreakpoint(0x0404);
    expect(app.hasBreakpoint(0x0404)).toBe(true);

    app.stepsPerSecond = 50;
    await app.startRun();
    expect(app.state?.pc).toBe(0x0404);
    expect(app.state?.registers[0]).toBe(1);
    expect(app.state?.halted).toBe(false);

    // Edit memory at 0x8000 (16-bit write of 0x1234)
    app.editMemAddrInput = '0x8000';
    app.editMemValInput = '0x1234';
    app.editMemWidth = 16;
    await app.writeMemoryValue();

    const watch8000 = app.state?.watches.find((w) => w.address === 0x8000);
    expect(watch8000?.value16).toBe(0x1234);
  });

  it('should surface line-numbered syntax diagnostics on invalid assembly', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    app.sourceCode = `
      MOV R0, 10
      BAD_INSTRUCTION R0, 5
      HALT
    `;
    await app.assembleProgram();
    expect(app.state?.diagnostics.length).toBe(1);
    expect(app.state?.diagnostics[0].line).toBe(3);
  });

  it('should compile and execute a high-level ForgeLang program with functions, source maps, and globals', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    app.sourceCode = [
      'let a = 10',
      'let b = 5',
      'fn add(x, y) {',
      '  return x + y',
      '}',
      'let result = add(a, b)',
      'print result',
    ].join('\n');

    await app.compileProgram();
    expect(app.editorLanguage).toBe('forgelang');
    expect(app.state?.mode).toBe('forgelang');
    expect(app.state?.sourceMap?.length).toBeGreaterThan(5);
    expect(app.state?.globals?.length).toBe(3);

    app.stepsPerSecond = 50;
    await app.startRun();
    expect(app.state?.halted).toBe(true);
    expect(app.state?.numericOutputs).toEqual([15]);
    const resGlobal = app.state?.globals?.find((g) => g.name === 'result');
    expect(resGlobal?.value).toBe(15);
  });

  it('should support source-line breakpoints, call stack frame inspection, and step-out during recursive calls', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    app.sourceCode = [
      'fn fact(n) {',
      '  if n <= 1 {',
      '    return 1',
      '  }',
      '  return n * fact(n - 1)',
      '}',
      'let ans = fact(4)',
      'print ans',
    ].join('\n');

    await app.compileProgram();
    // Toggle source-line breakpoint on line 3 (base case `return 1`)
    await app.toggleSourceLineBreakpoint(3);
    expect(app.breakpointSourceLines).toContain(3);

    app.stepsPerSecond = 50;
    await app.startRun();
    expect(app.state?.halted).toBe(false);
    expect(app.activeSourceLine).toBe(3);
    expect(app.state?.callStack?.length).toBe(4);
    expect(app.state?.callStack?.[3].function).toBe('fact');
    expect(app.state?.callStack?.[3].locals[0].name).toBe('n');
    expect(app.state?.callStack?.[3].locals[0].value).toBe(1);

    // Step out of fact(1) back into fact(2)
    await app.stepOutOfFunction();
    expect(app.state?.callStack?.length).toBe(3);
  });

  it('should compile, run ForgeOS graphics, allocate dynamic heap, and render pixels on 64x32 framebuffer', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    app.sourceCode = [
      'clear_screen(0)',
      'let buf = alloc(8)',
      'poke(buf + 0, 100)',
      'poke(buf + 2, 200)',
      'draw_pixel(10, 5, 11)',
      'draw_pixel(11, 5, 14)',
      'let v0 = peek(buf + 0)',
      'print v0',
      'exit(42)',
    ].join('\n');

    await app.compileProgram();
    expect(app.state?.diagnostics.length).toBe(0);

    app.stepsPerSecond = 50;
    await app.startRun();

    expect(app.state?.halted).toBe(true);
    expect(app.state?.os?.exitCode).toBe(42);
    expect(app.state?.os?.allocCount).toBe(1);
    expect(app.state?.os?.heapUsedBytes).toBe(8);
    expect(app.state?.numericOutputs).toEqual([100]);
    expect(app.state?.framebuffer?.width).toBe(64);
    expect(app.state?.framebuffer?.height).toBe(32);

    // Verify active pixels rendered
    const px10_5 = app.activePixels.find((p) => p.x === 10 && p.y === 5);
    const px11_5 = app.activePixels.find((p) => p.x === 11 && p.y === 5);
    expect(px10_5?.colorIdx).toBe(11);
    expect(px11_5?.colorIdx).toBe(14);

    // Switch to Display & ForgeOS tab
    app.activeBottomTab = 'display';
    expect(app.activeBottomTab).toBe('display');
    expect(app.activePixels.length).toBe(2);
  });

  it('should support interactive keyboard quick keys and keyboard queue monitoring', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;

    await app.sendQuickKey('w');
    expect(app.state?.os?.inputQueueCount).toBeGreaterThanOrEqual(1);
  });

  it('should support interactive tutorials and guided exercises loading and solution reveal', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;

    expect(app.tutorialsList.length).toBe(5);
    expect(app.exercisesList.length).toBe(4);

    // Open tutorials modal
    app.showTutorialsModal = true;
    expect(app.showTutorialsModal).toBe(true);

    // Load Tutorial 1 (ALU & Architecture)
    const tut1 = app.tutorialsList[0];
    await app.loadTutorial(tut1);
    expect(app.showTutorialsModal).toBe(false);
    expect(app.editorLanguage).toBe('assembly');
    expect(app.sourceCode).toContain('ADD R0, R1');
    expect(app.state?.hasProgram).toBe(true);

    // Load Exercise 1 Starter
    const ex1 = app.exercisesList[0];
    await app.loadExerciseStarter(ex1);
    expect(app.sourceCode).toContain('0xBEEF');
    expect(app.showExerciseSolution).toBe(false);

    // Load Exercise 1 Solution
    await app.loadExerciseSolution(ex1);
    expect(app.showExerciseSolution).toBe(true);
    expect(app.sourceCode).toContain('OUT R1');
  });

  it('should support CPU architecture diagram, ISA reference filtering, and instruction decoder', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;

    // Architecture diagram modal toggle
    app.showDiagramModal = true;
    expect(app.showDiagramModal).toBe(true);
    app.showDiagramModal = false;
    expect(app.showDiagramModal).toBe(false);

    // ISA Reference table
    expect(app.isaTable.length).toBe(32);
    app.isaSearchText = 'MUL';
    expect(app.filteredIsa.length).toBe(1);
    expect(app.filteredIsa[0].mnemonic).toBe('MUL');
    expect(app.filteredIsa[0].cycles).toBe(3);

    app.isaSearchText = 'Control Flow';
    expect(app.filteredIsa.length).toBeGreaterThanOrEqual(9);

    // Instruction Decoder tab on "MOV R0, 10"
    app.sourceCode = 'MOV R0, 10\nADD R0, 5\nHALT\n';
    await app.assembleProgram();

    app.activeExecTab = 'decoder';
    const breakdown = app.activeInstructionBreakdown;
    expect(breakdown).not.toBeNull();
    expect(breakdown?.opcode).toBe(2); // MOV = 2
    expect(breakdown?.rdName).toBe('R0');
    expect(breakdown?.fullHex).toBeDefined();

    // Step once to ADD R0, 5
    await app.stepInstruction();
    const alu = app.aluOperationInspection;
    expect(alu).not.toBeNull();
    expect(alu?.opName).toBe('ADD');
    expect(alu?.rdName).toBe('R0');
  });

  it('should compute microarchitecture profiler statistics and simulated L1 cache metrics', async () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;

    app.sourceCode = [
      'MOV R0, 10',
      'MOV R1, 20',
      'ADD R0, R1',
      'OUT R0',
      'HALT',
    ].join('\n');
    await app.assembleProgram();

    // Run to completion
    app.stepsPerSecond = 50;
    await app.startRun();

    app.activeBottomTab = 'microarch';
    const prof = app.profilerStats;
    expect(prof.executed).toBeGreaterThanOrEqual(4);
    expect(prof.cycles).toBeGreaterThanOrEqual(4);
    expect(parseFloat(prof.cpi)).toBeGreaterThanOrEqual(1.0);
    expect(prof.arithCount).toBeGreaterThanOrEqual(1);
    expect(prof.dataCount).toBeGreaterThanOrEqual(2);
  });
});

