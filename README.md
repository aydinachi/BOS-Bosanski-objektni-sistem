# BOS — Bosanski objektni sistem

**BOS** je statički tipizirani programski jezik opšte namjene sa originalnom sintaksom na bosanskom jeziku, napisan u modernom **C++17** sa vlastitim kompajlerom, statičkim semantičkim analizatorom, bajtkod IR generatorom, virtuelnom mašinom (VM), standardnom bibliotekom, CLI alatom i web IDE okruženjem (**BOS Studio**).

---

## Primjer BOS koda

```bos
neka ime kao tekst = "Ajdin"
neka godine kao cijeli = 23

ispisi "Dobro dosao u BOS!"
ispisi "Ime: " + ime
ispisi "Godine: " + godine

ako godine >= 18 onda
    ispisi "Punoljetan"
inače
    ispisi "Maloljetan"
kraj

neka rezultat = 10 + 20 * 3
ispisi "Rezultat: " + rezultat
```

Izlaz:

```text
Dobro dosao u BOS!
Ime: Ajdin
Godine: 23
Punoljetan
Rezultat: 70
```

---

## Mogućnosti (v0.1)

- **Puna UTF-8 podrška** za bosanska slova (`č, ć, ž, š, đ, Č, Ć, Ž, Š, Đ`) u ključnim riječima, identifikatorima, tekstovima i standardnoj biblioteci.
- **Statički sistem tipova sa inferencijom**: `cijeli`, `decimalni`, `tekst`, `znak`, `logički`, `prazno`, te generičke kolekcije `niz<T>`, `lista<T>`, `rjecnik<K, V>`.
- **Kontrola toka**: `ako / inače ako / inače / kraj`, `ponavljaj ... od ... do`, `za svaki ... u`, `dok ... radi`, `prekini`, `nastavi`, logički operatori `i`, `ili`, `ne` (sa kratkim spojem).
- **Funkcije i rekurzija**: parametri, povratni tipovi (`vraća`), lokalni opseg, rekurzivni pozivi.
- **Strukture i objektno-orijentisano programiranje**: `zapis`, `klasa`, konstruktori (`konstruktor`), `ovaj`, kontrola pristupa (`javno:`, `privatno:`) provjerena u fazi kompajliranja i nasljeđivanje (`nasljeduje`).
- **Obrada grešaka**: `pokusaj / uhvati greska / kraj`, `baci`, hvatanje dijeljenja sa nulom, indeksa izvan opsega i I/O grešaka preko `greska.poruka` i `greska.kod`.
- **Standardna biblioteka**:
  - `matematika`: `apsolutno`, `korijen`, `stepen`, `minimum`, `maksimum`, `PI`, `E`
  - `tekst`: `duzina`, `veliko`, `malo`, `sadrzi` (UTF-8 svjestan za bosanska slova)
  - `kolekcije`: `lista`, `niz`, `dodaj`, `ukloni`, `velicina`
  - `datoteke`: `otvori`, `procitaj`, `zapisi`, `zatvori`
- **Binarni bajtkod (`.bosb`) i VM**: `bos prevedi` generiše kompajliranu `.bosb` bajtkod datoteku koju `bos pokreni` može direktno izvršiti bez ponovnog parsiranja izvora.
- **BOS Studio (Web IDE)**: Monaco editor sa prilagođenim BOS označavanjem sintakse, više tabova, radnim prostorom, klikabilnom dijagnostikom na bosanskom jeziku, pregledom tokena, AST-a i IR bajtkoda, te ugrađenim pokretačem C++17 testova.

---

## Arhitektura kompajlera

```text
.bos izvor (UTF-8)
  → Lekser (compiler/lexer)
  → Parser (compiler/parser) → AST (compiler/ast)
  → Semantički analizator i provjera tipova (compiler/semantic, compiler/types)
  → IR bajtkod generator (compiler/ir)
  → [prevedi] Binarni .bosb format (compiler/backend/bytecode_format)
  → [pokreni] BOS Bajtkod VM (compiler/backend/vm) + Standardna biblioteka (compiler/runtime)
```

---

## Gradnja i pokretanje

### Preduslovi
- **Za BOS kompajler i CLI**: `g++` (GCC 10+ ili Clang 11+ sa C++17 podrškom) i `make`
- **Za BOS Studio (Web IDE)**: Node.js 20+ i npm

### 1. Gradnja C++17 kompajlera i pokretanje testova

```bash
make          # Gradi ./bin/bos
make test     # Gradi i pokreće svih 21 automatskih testova (./bin/bos_tests)
```

### 2. Korištenje BOS CLI-a

```bash
./bin/bos pokreni examples/dobrodosli.bos       # Kompajlira i izvršava na BOS VM-u
./bin/bos prevedi examples/dobrodosli.bos       # Prevodi u binarni bajtkod (.bosb)
./bin/bos pokreni examples/dobrodosli.bosb      # Pokreće prevedeni .bosb fajl
./bin/bos provjeri examples/dobrodosli.bos      # Samo leksička, sintaksna i semantička provjera
./bin/bos verzija                               # Verzija kompajlera i informacije o backendu
./bin/bos pomoc                                 # Pomoć na bosanskom jeziku
```

Dodatne opcije za inspekciju kompajlera:
```bash
./bin/bos provjeri examples/dobrodosli.bos --tokens --ast --ir
```

### 3. Pokretanje BOS Studio Web IDE-a

```bash
npm install
make
npm run dev
```

Zatim otvorite `http://localhost:3000`.

---

## Struktura projekta

```text
├── compiler/
│   ├── ast/            # AST čvorovi i AST printer
│   ├── backend/        # BOS Bajtkod VM i .bosb binarni format
│   ├── cli/            # CLI ulazna tačka (bos pokreni|prevedi|provjeri|verzija|pomoc)
│   ├── diagnostics/    # Dijagnostika grešaka na bosanskom jeziku (fajl:linija:kolona)
│   ├── ir/             # BOS bajtkod opkodovi, disasembler i AST→IR generator
│   ├── lexer/          # UTF-8 lekser i tabela bosanskih ključnih riječi
│   ├── parser/         # Rekurzivni parser sa prioritetom operatora
│   ├── runtime/        # Runtime vrijednosti i standardna biblioteka
│   ├── semantic/       # Tabela simbola, provjera tipova, opsega i pristupa
│   └── types/          # Statički sistem tipova
├── docs/
│   ├── SPECIFICATION.md  # Zvanična specifikacija BOS v0.1 jezika
│   └── ARCHITECTURE.md   # Dokumentacija arhitekture kompajlera
├── examples/           # Primjeri .bos programa
├── tests/              # C++17 jedinični i integracijski testovi
├── src/                # BOS Studio frontend (React, TypeScript, Monaco Editor)
├── server.ts           # Express + Vite server za BOS Studio
└── Makefile            # C++17 sistem gradnje
```

---

## Ograničenja v0.1 i plan razvoja

- Trenutni backend kompajlira u **BOS binarni bajtkod (`.bosb`)** koji izvršava BOS VM. Nativni **LLVM** mašinski kod je planiran za buduće izdanje.
- Planirane funkcionalnosti nakon v0.1: preopterećenje funkcija (*function overloading*), korisnički višejedinični moduli i korisnički generički tipovi (`klasa Kutija<T>`).

---

## Doprinos i licenca

Pogledajte [CONTRIBUTING.md](CONTRIBUTING.md) za upute o doprinosu. Projekat je licenciran pod [MIT licencom](LICENSE).
