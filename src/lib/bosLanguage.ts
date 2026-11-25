import * as monaco from 'monaco-editor';
import { loader } from '@monaco-editor/react';

// Use locally bundled monaco-editor so there are zero external CDN failures
loader.config({ monaco });

let registered = false;

export function registerBosLanguage() {
  if (registered) return;
  registered = true;

  monaco.languages.register({
    id: 'bos',
    extensions: ['.bos'],
    aliases: ['BOS', 'bos'],
  });

  monaco.languages.setLanguageConfiguration('bos', {
    comments: {
      lineComment: '//',
      blockComment: ['/*', '*/'],
    },
    brackets: [
      ['{', '}'],
      ['[', ']'],
      ['(', ')'],
    ],
    autoClosingPairs: [
      { open: '{', close: '}' },
      { open: '[', close: ']' },
      { open: '(', close: ')' },
      { open: '"', close: '"' },
      { open: "'", close: "'" },
    ],
    indentationRules: {
      increaseIndentPattern: /^\s*(ako|inače|inace|dok|ponavljaj|za|funkcija|konstruktor|zapis|klasa|pokusaj|pokušaj|uhvati|javno:|privatno:)\b.*$/,
      decreaseIndentPattern: /^\s*(kraj|inače|inace|uhvati)\b.*$/,
    },
  });

  monaco.languages.setMonarchTokensProvider('bos', {
    defaultToken: '',
    tokenPostfix: '.bos',

    keywords: [
      'neka', 'kao', 'stalno', 'funkcija', 'vraća', 'vraca', 'vrati',
      'zapis', 'klasa', 'nasljeduje', 'nasljeđuje', 'javno', 'privatno',
      'konstruktor', 'ovaj', 'ako', 'onda', 'inače', 'inace', 'kraj',
      'ponavljaj', 'od', 'do', 'za', 'svaki', 'u', 'dok', 'radi',
      'prekini', 'nastavi', 'i', 'ili', 'ne', 'ispisi', 'ispiši',
      'ucitaj', 'učitaj', 'pokusaj', 'pokušaj', 'uhvati', 'baci', 'koristi',
    ],

    typeKeywords: [
      'cijeli', 'decimalni', 'tekst', 'znak', 'logički', 'logicki',
      'prazno', 'niz', 'lista', 'rjecnik', 'rječnik', 'Greska', 'Datoteka',
    ],

    booleans: ['tačno', 'tacno', 'netačno', 'netacno'],

    operators: [
      '=', '>', '<', '==', '<=', '>=', '!=',
      '+', '-', '*', '/', '%', '+=', '-=', '*=', '/=',
    ],

    tokenizer: {
      root: [
        [/\/\/.*$/, 'comment'],
        [/\/\*/, 'comment', '@comment'],
        [/"([^"\\]|\\.)*$/, 'string.invalid'],
        [/"/, 'string', '@string'],
        [/'([^'\\]|\\.)'/, 'string.char'],
        [/\d+\.\d+/, 'number.float'],
        [/\d+/, 'number'],
        [
          /[a-zA-Z_čćžšđČĆŽŠĐ][a-zA-Z0-9_čćžšđČĆŽŠĐ]*/,
          {
            cases: {
              '@keywords': 'keyword',
              '@typeKeywords': 'type',
              '@booleans': 'constant.language',
              '@default': 'identifier',
            },
          },
        ],
        [/[{}()\[\]]/, '@brackets'],
        [/[<>]=?|[!=]=|[+\-*/]=|[+\-*/%]=?/, 'operator'],
        [/[:,.;]/, 'delimiter'],
      ],
      comment: [
        [/[^/*]+/, 'comment'],
        [/\*\//, 'comment', '@pop'],
        [/[/*]/, 'comment'],
      ],
      string: [
        [/[^\\"]+/, 'string'],
        [/\\./, 'string.escape'],
        [/"/, 'string', '@pop'],
      ],
    },
  });

  monaco.editor.defineTheme('bos-dark', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'keyword', foreground: 'F59E0B', fontStyle: 'bold' },
      { token: 'type', foreground: '38BDF8' },
      { token: 'constant.language', foreground: 'C084FC', fontStyle: 'bold' },
      { token: 'string', foreground: '34D399' },
      { token: 'string.char', foreground: '34D399' },
      { token: 'string.escape', foreground: '6EE7B7' },
      { token: 'number', foreground: 'FB923C' },
      { token: 'number.float', foreground: 'FB923C' },
      { token: 'comment', foreground: '64748B', fontStyle: 'italic' },
      { token: 'operator', foreground: '94A3B8' },
      { token: 'identifier', foreground: 'E2E8F0' },
    ],
    colors: {
      'editor.background': '#0E131F',
      'editor.foreground': '#E2E8F0',
      'editorLineNumber.foreground': '#475569',
      'editorLineNumber.activeForeground': '#F59E0B',
      'editor.lineHighlightBackground': '#151C2C',
      'editor.selectionBackground': '#D9770640',
      'editorCursor.foreground': '#F59E0B',
      'editorIndentGuide.background1': '#1E293B',
      'editorIndentGuide.activeBackground1': '#334155',
    },
  });
}

export { monaco };
