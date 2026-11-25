# BOS Language Specification — Version 0.1

**Name:** BOS (*Bosanski objektni sistem*)  
**File Extension:** `.bos` (source), `.bosb` (compiled BOS bytecode binary)  
**Character Encoding:** UTF-8 (full support for Bosnian diacritics: `č, ć, ž, š, đ, Č, Ć, Ž, Š, Đ`)  
**Typing Discipline:** Statically typed with local type inference  
**Paradigms:** Procedural, Structured, and Object-Oriented  
**Implementation:** C++17 Compiler, Static Semantic Analyzer, IR Bytecode Generator, and Stack-Based Bytecode Virtual Machine

---

## 1. Lexical Structure

### 1.1 Encoding & Identifiers
Source files are decoded as UTF-8. Identifiers may begin with an ASCII letter (`a-z`, `A-Z`), underscore (`_`), or any Bosnian letter (`č, ć, ž, š, đ, Č, Ć, Ž, Š, Đ`), followed by letters, digits (`0-9`), underscores, or Bosnian letters.

### 1.2 Comments
- Single-line comment: `// komentar do kraja reda`
- Multi-line comment: `/* blok komentar */`

### 1.3 Keywords
Canonical BOS v0.1 keywords (optional ASCII/diacritic equivalents in parentheses):

| Category | Keywords |
|---|---|
| Declarations | `neka`, `kao`, `stalno`, `funkcija`, `vraća` (`vraca`), `zapis`, `klasa`, `nasljeduje` (`nasljeđuje`), `konstruktor` |
| Visibility | `javno`, `privatno` |
| Types | `cijeli`, `decimalni`, `tekst`, `znak`, `logički` (`logicki`), `prazno`, `niz`, `lista`, `rjecnik` (`rječnik`) |
| Control Flow | `ako`, `onda`, `inače` (`inace`), `kraj`, `ponavljaj`, `od`, `do`, `za`, `svaki`, `u`, `dok`, `radi`, `prekini`, `nastavi`, `vrati` |
| Boolean & Logic | `tačno` (`tacno`), `netačno` (`netacno`), `i`, `ili`, `ne` |
| Error Handling | `pokusaj` (`pokušaj`), `uhvati`, `baci` |
| I/O & Modules | `ispisi` (`ispiši`), `ucitaj` (`učitaj`), `koristi` |
| Self Reference | `ovaj` |

### 1.4 Literals
- **Integer (`cijeli`)**: 64-bit signed decimal integers (`0`, `42`, `1000000`).
- **Floating-point (`decimalni`)**: 64-bit IEEE-754 numbers (`3.14159`, `0.5`).
- **String (`tekst`)**: Double-quoted UTF-8 strings (`"Zdravo svijete!"`, `"Čestitam!"`) with escape sequences (`\n`, `\t`, `\r`, `\"`, `\\`).
- **Character (`znak`)**: Single-quoted single UTF-8 codepoint (`'A'`, `'č'`, `'\n'`).
- **Boolean (`logički`)**: `tačno`, `netačno`.

---

## 2. Type System

### 2.1 Primitive Types
| BOS Type | Description | Default Value |
|---|---|---|
| `cijeli` | 64-bit signed integer | `0` |
| `decimalni` | 64-bit floating-point | `0.0` |
| `tekst` | UTF-8 string | `""` |
| `znak` | Unicode scalar value (UTF-8 char) | `'\0'` |
| `logički` | Boolean truth value | `netačno` |
| `prazno` | Absence of value (return type only) | — |

### 2.2 Generic Collection Types
- `niz<T>` — Typed array of element type `T`.
- `lista<T>` — Resizable typed list of element type `T`.
- `rjecnik<K, V>` — Typed associative map from key `K` to value `V`.

### 2.3 User-Defined Types
- `zapis Naziv ... kraj` — Value-like structured record with public typed fields.
- `klasa Naziv [nasljeduje Bazna] ... kraj` — Object-oriented class with `javno:` and `privatno:` members, methods, constructors, and single inheritance.

### 2.4 Implicit Conversions & Operators
- Arithmetic on `cijeli` and `decimalni` promotes `cijeli` to `decimalni` when mixed.
- Assigning `cijeli` to a `decimalni` target promotes cleanly to `decimalni`.
- String concatenation (`tekst + X` or `X + tekst`) converts primitive `cijeli`, `decimalni`, `logički`, or `znak` operands into their `tekst` representation.
- Boolean operators (`i`, `ili`, `ne`) require `logički` operands and short-circuit.

---

## 3. Grammar & Constructs

### 3.1 Variables & Constants
```bos
neka ime kao tekst = "Ajdin"
neka godine kao cijeli = 23
neka rezultat = 100
neka ulaz kao tekst
stalno PI kao decimalni = 3.14159
```

### 3.2 Output & Input
```bos
ispisi "Zdravo svijete!"
neka ime kao tekst
ucitaj ime
```

### 3.3 Conditionals
```bos
ako godine >= 18 onda
    ispisi "Punoljetan"
inače ako godine >= 13 onda
    ispisi "Tinejdzer"
inače
    ispisi "Maloljetan"
kraj
```

### 3.4 Loops
```bos
ponavljaj i od 1 do 10
    ispisi i
kraj

za svaki broj u brojevi
    ispisi broj
kraj

dok broj < 100 radi
    broj = broj + 1
kraj
```
Loop control statements: `prekini` (exit innermost loop), `nastavi` (skip to next iteration).

### 3.5 Functions
```bos
funkcija saberi(a kao cijeli, b kao cijeli) vraća cijeli
    vrati a + b
kraj

funkcija pozdrav(ime kao tekst) vraća prazno
    ispisi "Zdravo " + ime
kraj
```

### 3.6 Structures (`zapis`)
```bos
zapis Student
    ime kao tekst
    godine kao cijeli
    prosjek kao decimalni
kraj
```
Construct via `Student("Ajdin", 23, 9.5)` or `Student()` / `neka s kao Student`.

### 3.7 Classes (`klasa`) & Inheritance (`nasljeduje`)
```bos
klasa Vozilo
    javno:
        marka kao tekst
kraj

klasa Automobil nasljeduje Vozilo
    javno:
        vrata kao cijeli
        funkcija predstavi() vraća prazno
            ispisi marka
        kraj
    privatno:
        brzina kao cijeli
kraj
```
Members declared under `privatno:` are accessible only inside methods of the declaring class; external access is rejected at compile time by the semantic analyzer.

### 3.8 Collections
```bos
neka brojevi kao niz<cijeli> = [10, 20, 30]
neka imena kao lista<tekst> = ["Ajdin", "Amar"]
imena.dodaj("Haris")
imena.ukloni(0)
ispisi imena.velicina()
```

### 3.9 Error Handling
```bos
pokusaj
    neka rezultat = 10 / 0
uhvati greska
    ispisi greska.poruka
kraj
```
Caught error objects expose `greska.poruka` (`tekst`) and `greska.kod` (`cijeli`).

### 3.10 Modules & Standard Library
```bos
koristi matematika
neka r = matematika.korijen(144)
```

Standard library modules in v0.1:
- **`matematika`**: `apsolutno(x)`, `korijen(x)`, `stepen(a, b)`, `minimum(a, b)`, `maksimum(a, b)`, `PI`, `E`
- **`tekst`**: `duzina(s)`, `veliko(s)`, `malo(s)`, `sadrzi(s, pod)`
- **`kolekcije`**: `lista()`, `niz()`, `dodaj(k, v)`, `ukloni(k, i)`, `velicina(k)`
- **`datoteke`**: `otvori(putanja, nacin)`, `procitaj(d)`, `zapisi(d, sadrzaj)`, `zatvori(d)`

---

## 4. Compilation & Execution Model (v0.1)

1. **Source Loader** reads UTF-8 `.bos` files.
2. **UTF-8 Lexer** tokenizes source into `Token` streams with line and column tracking.
3. **Recursive-Descent Parser** constructs a typed **AST** with operator precedence.
4. **Semantic Analyzer & Static Type Checker** validates scopes, symbols, types, arity, return contracts, loop controls, constants, and `privatno`/`javno` access rules.
5. **IR / Bytecode Generator** lowers the checked AST into BOS stack-based bytecode (`BytecodeModule`).
6. **Backend**:
   - `bos pokreni file.bos` compiles to BOS bytecode and executes on the **BOS Bytecode Virtual Machine**.
   - `bos prevedi file.bos` compiles and serializes a standalone **BOS Bytecode Binary (`.bosb`)** executable by `bos pokreni file.bosb`. Native LLVM machine-code emission is planned for a future release when LLVM is available in the build environment.
   - `bos provjeri file.bos` performs full lexical, syntax, and static semantic verification without execution.

---

## 5. Planned Future Features (Post-v0.1)
- Function overloading based on parameter types.
- User-defined multi-file `.bos` module imports.
- Native machine code backend via LLVM IR when LLVM toolchains are installed.
- Generic user-defined functions and classes (`klasa Kutija<T>`).
