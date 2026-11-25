# Doprinos projektu BOS

Hvala na interesu za razvoj **BOS (Bosanski objektni sistem)** programskog jezika i kompajlera.

## Postavljanje razvojnog okruženja

1. Klonirajte repozitorij i izgradite C++17 kompajler:
   ```bash
   make
   make test
   ```
2. Za razvoj BOS Studio web okruženja:
   ```bash
   npm install
   npm run lint
   npm run dev
   ```

## Dodavanje novih jezičkih mogućnosti

Svaka promjena sintakse ili semantike treba pratiti kompajlerski tok i ostati usklađena sa specifikacijom:

1. **Specifikacija**: Ažurirajte `docs/SPECIFICATION.md`.
2. **Lekser**: Dodajte nove tokene/ključne riječi u `compiler/lexer/`.
3. **AST i Parser**: Dodajte čvorove u `compiler/ast/` i pravila parsiranja u `compiler/parser/`.
4. **Semantička analiza**: Dodajte provjeru tipova, opsega i grešaka u `compiler/semantic/`.
5. **IR i VM**: Dodajte generisanje instrukcija u `compiler/ir/` i izvršavanje u `compiler/backend/`.
6. **Testovi**: Dodajte automatske testove (pozitivne i negativne slučajeve) u `tests/test_runner.cpp`.

## Provjere prije Pull Request-a

- `make clean && make test` prolazi bez upozorenja (`-Wall -Wextra`) i sa 100% prolaznosti testova.
- `npm run lint` i `npm run build` prolaze bez grešaka.
- Sve dijagnostičke poruke kompajlera su jasne, napisane na bosanskom jeziku (UTF-8) i sadrže liniju i kolonu.
