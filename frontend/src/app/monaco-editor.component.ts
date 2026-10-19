import {
  AfterViewInit,
  Component,
  ElementRef,
  EventEmitter,
  Input,
  OnChanges,
  OnDestroy,
  Output,
  SimpleChanges,
  ViewChild,
} from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ForgeDiagnostic } from './forge.models';

@Component({
  selector: 'app-monaco-editor',
  standalone: true,
  imports: [CommonModule, FormsModule],
  template: `
    <div class="relative w-full h-full flex flex-col bg-[#0d1117]">
      <!-- Monaco container when running in browser -->
      <div
        #editorContainer
        class="w-full flex-1"
        [class.hidden]="useFallbackTextarea"
      ></div>

      <!-- Synchronized fallback code editor with interactive breakpoint & active-line gutter -->
      @if (useFallbackTextarea) {
        <div class="flex flex-1 overflow-auto font-mono text-xs leading-5 bg-[#0d1117]">
          <div class="select-none py-3 px-1.5 text-right bg-[#090d14] text-slate-500 border-r border-slate-800/80 min-w-[3.75rem]">
            @for (line of lineNumbers; track line) {
              <div
                (click)="onGutterClick(line)"
                class="px-1 rounded-sm cursor-pointer flex items-center justify-end gap-1 hover:bg-slate-800/60"
                [class.bg-emerald-500/20]="line === activeLine"
                [class.text-emerald-300]="line === activeLine"
                [class.font-bold]="line === activeLine"
                [title]="'Click to toggle breakpoint on line ' + line"
              >
                @if (hasBreakpointOnLine(line)) {
                  <span class="text-rose-500 font-bold">●</span>
                }
                @if (line === activeLine) {
                  <span class="text-emerald-400">▶</span>
                }
                <span>{{ line }}</span>
              </div>
            }
          </div>
          <textarea
            aria-label="Source Code Editor"
            class="flex-1 p-3 bg-transparent text-slate-100 font-mono text-xs leading-5 focus:outline-none resize-none"
            [ngModel]="code"
            (ngModelChange)="onTextareaInput($event)"
            spellcheck="false"
          ></textarea>
        </div>
      }
    </div>
  `,
})
export class MonacoEditorComponent implements AfterViewInit, OnChanges, OnDestroy {
  @ViewChild('editorContainer', { static: true }) editorContainer!: ElementRef<HTMLDivElement>;

  @Input() code = '';
  @Input() language: 'forgelang' | 'assembly' = 'assembly';
  @Input() activeLine: number | null = null;
  @Input() breakpointLines: number[] = [];
  @Input() diagnostics: ForgeDiagnostic[] = [];
  @Output() codeChange = new EventEmitter<string>();
  @Output() toggleLineBreakpoint = new EventEmitter<number>();

  public useFallbackTextarea = true;
  private editorInstance: any = null;
  private monacoRef: any = null;
  private decorationIds: string[] = [];

  get lineNumbers(): number[] {
    const count = Math.max(1, this.code.split('\n').length);
    return Array.from({ length: count }, (_, i) => i + 1);
  }

  hasBreakpointOnLine(line: number): boolean {
    return this.breakpointLines.includes(line);
  }

  onGutterClick(line: number): void {
    this.toggleLineBreakpoint.emit(line);
  }

  async ngAfterViewInit(): Promise<void> {
    if (typeof window === 'undefined' || typeof navigator === 'undefined' || /jsdom/i.test(navigator.userAgent)) {
      this.useFallbackTextarea = true;
      return;
    }

    try {
      (window as any).MonacoEnvironment = {
        getWorker: () => {
          const blob = new Blob(['self.onmessage = function() {};'], { type: 'text/javascript' });
          return new Worker(URL.createObjectURL(blob));
        },
      };

      const monaco = await import('monaco-editor');
      this.monacoRef = monaco;
      this.registerForgeLanguages(monaco);

      this.useFallbackTextarea = false;
      this.editorInstance = monaco.editor.create(this.editorContainer.nativeElement, {
        value: this.code,
        language: this.language === 'forgelang' ? 'forgelang' : 'forge-asm',
        theme: 'forge-dark',
        automaticLayout: true,
        minimap: { enabled: false },
        fontSize: 13,
        lineHeight: 20,
        fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
        scrollBeyondLastLine: false,
        glyphMargin: true,
        lineNumbersMinChars: 3,
        padding: { top: 10, bottom: 10 },
      });

      this.editorInstance.onDidChangeModelContent(() => {
        const val = this.editorInstance.getValue();
        if (val !== this.code) {
          this.code = val;
          this.codeChange.emit(val);
        }
      });

      this.editorInstance.onMouseDown((e: any) => {
        if (
          e?.target?.position?.lineNumber &&
          (e.target.type === monaco.editor.MouseTargetType.GUTTER_GLYPH_MARGIN ||
            e.target.type === monaco.editor.MouseTargetType.GUTTER_LINE_NUMBERS)
        ) {
          this.toggleLineBreakpoint.emit(e.target.position.lineNumber);
        }
      });

      this.applyDecorations();
    } catch {
      this.useFallbackTextarea = true;
    }
  }

  ngOnChanges(changes: SimpleChanges): void {
    if (this.editorInstance && this.monacoRef) {
      if (changes['code'] && this.editorInstance.getValue() !== this.code) {
        this.editorInstance.setValue(this.code);
      }
      if (changes['language']) {
        const model = this.editorInstance.getModel();
        if (model) {
          this.monacoRef.editor.setModelLanguage(
            model,
            this.language === 'forgelang' ? 'forgelang' : 'forge-asm'
          );
        }
      }
      if (changes['activeLine'] || changes['diagnostics'] || changes['breakpointLines']) {
        this.applyDecorations();
      }
    }
  }

  ngOnDestroy(): void {
    if (this.editorInstance) {
      this.editorInstance.dispose();
      this.editorInstance = null;
    }
  }

  onTextareaInput(next: string): void {
    this.code = next;
    this.codeChange.emit(next);
  }

  private registerForgeLanguages(monaco: any): void {
    const hasAsm = monaco.languages.getLanguages().some((l: any) => l.id === 'forge-asm');
    if (!hasAsm) {
      monaco.languages.register({ id: 'forge-asm' });
      monaco.languages.setMonarchTokensProvider('forge-asm', {
        ignoreCase: true,
        tokenizer: {
          root: [
            [/;.*$/, 'comment'],
            [/\/\/.*$/, 'comment'],
            [/^[ \t]*[A-Za-z_]\w*:/, 'type.identifier'],
            [/\.(org|entry|equ|const|word|dw|byte|db|asciz|string|space|resb)\b/, 'keyword.directive'],
            [
              /\b(NOP|HALT|MOV|LOAD|STORE|ADD|SUB|MUL|DIV|AND|OR|XOR|NOT|SHL|SHR|CMP|JMP|JZ|JNZ|CALL|RET|PUSH|POP|IN|OUT|JC|JNC|JN|JL|JLE|JG|JGE)\b/i,
              'keyword',
            ],
            [/\b(R[0-7]|FP)\b/i, 'variable.predefined'],
            [/0x[0-9a-fA-F]+\b/, 'number.hex'],
            [/0b[01]+\b/, 'number.binary'],
            [/-?\d+\b/, 'number'],
            [/"([^"\\]|\\.)*"/, 'string'],
            [/'([^'\\]|\\.)'/, 'string'],
          ],
        },
      });
    }

    const hasFl = monaco.languages.getLanguages().some((l: any) => l.id === 'forgelang');
    if (!hasFl) {
      monaco.languages.register({ id: 'forgelang' });
      monaco.languages.setMonarchTokensProvider('forgelang', {
        tokenizer: {
          root: [
            [/\/\/.*$/, 'comment'],
            [/#.*$/, 'comment'],
            [/\b(let|fn|return|if|else|while|for|break|continue|print|putc|in)\b/, 'keyword'],
            [/\b[A-Za-z_]\w*(?=\s*\()/, 'type.identifier'],
            [/0x[0-9a-fA-F]+\b/, 'number.hex'],
            [/-?\d+\b/, 'number'],
            [/'([^'\\]|\\.)'/, 'string'],
          ],
        },
      });

      monaco.editor.defineTheme('forge-dark', {
        base: 'vs-dark',
        inherit: true,
        rules: [
          { token: 'comment', foreground: '64748b', fontStyle: 'italic' },
          { token: 'keyword', foreground: '38bdf8', fontStyle: 'bold' },
          { token: 'keyword.directive', foreground: 'c084fc' },
          { token: 'variable.predefined', foreground: '34d399', fontStyle: 'bold' },
          { token: 'type.identifier', foreground: 'fbbf24', fontStyle: 'bold' },
          { token: 'number', foreground: 'f472b6' },
          { token: 'number.hex', foreground: 'f472b6' },
          { token: 'string', foreground: 'a3e635' },
        ],
        colors: {
          'editor.background': '#0d1117',
          'editor.lineHighlightBackground': '#1e293b50',
          'editorLineNumber.foreground': '#475569',
          'editorLineNumber.activeForeground': '#e2e8f0',
        },
      });
    }
  }

  private applyDecorations(): void {
    if (!this.editorInstance || !this.monacoRef) return;
    const monaco = this.monacoRef;
    const newDecorations: any[] = [];

    if (this.activeLine && this.activeLine > 0) {
      newDecorations.push({
        range: new monaco.Range(this.activeLine, 1, this.activeLine, 1),
        options: {
          isWholeLine: true,
          className: 'forge-active-pc-line',
        },
      });
    }

    for (const bpLine of this.breakpointLines) {
      if (bpLine > 0) {
        newDecorations.push({
          range: new monaco.Range(bpLine, 1, bpLine, 1),
          options: {
            isWholeLine: false,
            glyphMarginClassName: 'forge-bp-glyph',
          },
        });
      }
    }

    for (const diag of this.diagnostics) {
      if (diag.line > 0) {
        newDecorations.push({
          range: new monaco.Range(diag.line, 1, diag.line, 1),
          options: {
            isWholeLine: true,
            className: 'forge-error-line',
          },
        });
      }
    }

    this.decorationIds = this.editorInstance.deltaDecorations(this.decorationIds, newDecorations);
  }
}
