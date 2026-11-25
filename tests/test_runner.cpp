#include "backend/bytecode_format.hpp"
#include "backend/vm.hpp"
#include "diagnostics/diagnostic.hpp"
#include "ir/ir_generator.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/analyzer.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct TestReport {
    std::string name;
    bool passed = false;
    std::string details;
};

struct RunOutcome {
    bool compiled = false;
    bool executed = false;
    std::string output;
    std::vector<bos::Diagnostic> diagnostics;
};

RunOutcome compileAndRun(const std::string& source,
                         const std::string& stdinData = "",
                         const std::string& sandboxDir = "") {
    RunOutcome res;
    bos::DiagnosticEngine diag;

    bos::Lexer lexer(source, "test.bos", diag);
    auto tokens = lexer.tokenize();

    bos::Parser parser(tokens, "test.bos", diag);
    auto program = parser.parseProgram();

    bos::SemanticAnalyzer analyzer(diag);
    if (!diag.hasErrors()) {
        analyzer.analyze(program);
    }

    if (diag.hasErrors()) {
        res.compiled = false;
        res.diagnostics = diag.diagnostics();
        return res;
    }

    bos::IRGenerator irGen(analyzer.symbols(), diag);
    auto mod = irGen.generate(program);
    if (diag.hasErrors()) {
        res.compiled = false;
        res.diagnostics = diag.diagnostics();
        return res;
    }
    res.compiled = true;

    std::istringstream in(stdinData);
    std::ostringstream out;
    bos::VMOptions opts;
    opts.input = &in;
    opts.output = &out;
    opts.sandboxDir = sandboxDir;

    bos::VirtualMachine vm(opts);
    auto vmRes = vm.execute(mod, diag);
    res.executed = vmRes.ok;
    res.output = out.str();
    res.diagnostics = diag.diagnostics();
    return res;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(static_cast<char>(c));
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    bool jsonMode = (argc > 1 && std::string(argv[1]) == "--json");
    std::vector<TestReport> reports;

    auto runTest = [&](const std::string& name, auto fn) {
        TestReport r;
        r.name = name;
        try {
            std::string err = fn();
            r.passed = err.empty();
            r.details = err;
        } catch (const std::exception& ex) {
            r.passed = false;
            r.details = std::string("Neočekivana iznimka: ") + ex.what();
        }
        reports.push_back(std::move(r));
    };

    // 0. Mandatory specification program
    runTest("00. Obavezni testni program (Dobro dosao u BOS)", []() -> std::string {
        const std::string src = R"BOS(
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
)BOS";
        auto res = compileAndRun(src);
        std::string expected =
            "Dobro dosao u BOS!\n"
            "Ime: Ajdin\n"
            "Godine: 23\n"
            "Punoljetan\n"
            "Rezultat: 70\n";
        if (!res.compiled || !res.executed) return "Program nije uspio: " + (res.diagnostics.empty() ? "" : res.diagnostics[0].message);
        if (res.output != expected) return "Pogrešan izlaz:\nDobijeno:\n" + res.output + "\nOčekivano:\n" + expected;
        return "";
    });

    // 1. Lexer unit test (UTF-8 Bosnian characters & comments)
    runTest("01. Lekser: UTF-8 ključne riječi, čćžšđ identifikatori i komentari", []() -> std::string {
        bos::DiagnosticEngine diag;
        const std::string src = R"BOS(
// Jednolinijski komentar sa čćžšđ
/* Višelinijski
   komentar */
neka čovjek_štit kao tekst = "Čestitamo, đak živi!"
neka logika kao logički = tačno
)BOS";
        bos::Lexer lex(src, "lex.bos", diag);
        auto toks = lex.tokenize();
        if (diag.hasErrors()) return diag.formatAll();
        bool foundIdent = false, foundBool = false, foundStr = false;
        for (const auto& t : toks) {
            if (t.type == bos::TokenType::Identifier && t.lexeme == "čovjek_štit") foundIdent = true;
            if (t.type == bos::TokenType::KwTacno) foundBool = true;
            if (t.type == bos::TokenType::StringLiteral && t.lexeme == "Čestitamo, đak živi!") foundStr = true;
        }
        if (!foundIdent || !foundBool || !foundStr) return "Nedostaju očekivani UTF-8 tokeni.";
        return "";
    });

    // 2. Variables and constants
    runTest("02. Promjenljive, inferencija tipova i konstante", []() -> std::string {
        const std::string src = R"BOS(
stalno PI kao decimalni = 3.14159
neka rezultat = 100
neka naziv = "BOS"
rezultat += 25
ispisi naziv + " " + rezultat + " " + PI
)BOS";
        auto res = compileAndRun(src);
        if (!res.executed) return res.diagnostics.empty() ? "Greška" : res.diagnostics[0].message;
        if (res.output != "BOS 125 3.14159\n") return "Neočekivan izlaz: " + res.output;

        // Mutating constant must fail semantic analysis
        const std::string bad = "stalno X kao cijeli = 5\nX = 10\n";
        auto badRes = compileAndRun(bad);
        if (badRes.compiled) return "Izmjena konstante je morala biti odbijena!";
        return "";
    });

    // 3. All primitive types
    runTest("03. Svi primitivni tipovi (cijeli, decimalni, tekst, znak, logički)", []() -> std::string {
        const std::string src = R"BOS(
neka a kao cijeli = -42
neka b kao decimalni = 1.75
neka c kao tekst = "Sarajevo"
neka d kao znak = 'Č'
neka e kao logički = netačno
ispisi a
ispisi b
ispisi c
ispisi d
ispisi e
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "-42\n1.75\nSarajevo\nČ\nnetačno\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 4. Arithmetic operators & precedence
    runTest("04. Aritmetički operatori i prioritet operacija", []() -> std::string {
        const std::string src = R"BOS(
ispisi 2 + 3 * 4
ispisi (2 + 3) * 4
ispisi 17 % 5
ispisi -10 + 25 / 5
ispisi 5.0 / 2
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "14\n20\n2\n-5\n2.5\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 5. Boolean expressions & short-circuit
    runTest("05. Logički izrazi (i, ili, ne) i kratki spoj", []() -> std::string {
        const std::string src = R"BOS(
neka a = tačno
neka b = netačno
ispisi a i b
ispisi a ili b
ispisi ne b
ispisi (10 > 5) i (3 <= 3) i (4 != 5)
// Kratki spoj ne smije izazvati dijeljenje sa nulom:
neka x = 0
ako (x != 0) i (10 / x > 1) onda
    ispisi "LOSE"
inače
    ispisi "KRATKI_SPOJ_OK"
kraj
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "netačno\ntačno\ntačno\ntačno\nKRATKI_SPOJ_OK\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 6. Nested conditions
    runTest("06. Ugniježđeni uslovi (ako / inače ako / inače)", []() -> std::string {
        const std::string src = R"BOS(
funkcija ocijeni(bodovi kao cijeli) vraća tekst
    ako bodovi >= 90 onda
        vrati "Odličan"
    inače ako bodovi >= 75 onda
        ako bodovi >= 85 onda
            vrati "Vrlo dobar+"
        inače
            vrati "Vrlo dobar"
        kraj
    inače
        vrati "Dovoljan"
    kraj
kraj
ispisi ocijeni(95)
ispisi ocijeni(87)
ispisi ocijeni(78)
ispisi ocijeni(50)
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "Odličan\nVrlo dobar+\nVrlo dobar\nDovoljan\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 7. All loop types + prekini / nastavi
    runTest("07. Sve vrste petlji (ponavljaj, za svaki, dok) + prekini/nastavi", []() -> std::string {
        const std::string src = R"BOS(
neka zbir = 0
ponavljaj i od 1 do 10
    ako i == 5 onda
        nastavi
    kraj
    ako i == 9 onda
        prekini
    kraj
    zbir += i
kraj
ispisi zbir

neka brojevi kao niz<cijeli> = [2, 4, 6]
neka proizvod = 1
za svaki broj u brojevi
    proizvod *= broj
kraj
ispisi proizvod

neka k = 0
dok k < 5 radi
    k = k + 1
kraj
ispisi k
)BOS";
        auto res = compileAndRun(src);
        // 1+2+3+4+6+7+8 = 31; 2*4*6 = 48; k = 5
        std::string exp = "31\n48\n5\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 8. Functions & local scope
    runTest("08. Funkcije, parametri, povratne vrijednosti i opseg", []() -> std::string {
        const std::string src = R"BOS(
funkcija saberi(a kao cijeli, b kao cijeli) vraća cijeli
    vrati a + b
kraj

funkcija pozdrav(ime kao tekst) vraća prazno
    ispisi "Zdravo " + ime
kraj

ispisi saberi(15, 27)
pozdrav("Bosna")
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "42\nZdravo Bosna\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 9. Recursion
    runTest("09. Rekurzija (faktorijel i Fibonacci)", []() -> std::string {
        const std::string src = R"BOS(
funkcija faktorijel(n kao cijeli) vraća cijeli
    ako n <= 1 onda
        vrati 1
    kraj
    vrati n * faktorijel(n - 1)
kraj

funkcija fib(n kao cijeli) vraća cijeli
    ako n <= 1 onda
        vrati n
    kraj
    vrati fib(n - 1) + fib(n - 2)
kraj

ispisi faktorijel(6)
ispisi fib(10)
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "720\n55\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 10. Collections: niz, lista, rjecnik
    runTest("10. Kolekcije (niz, lista, rjecnik, dodaj, ukloni, velicina)", []() -> std::string {
        const std::string src = R"BOS(
neka brojevi kao niz<cijeli> = [10, 20, 30]
neka imena kao lista<tekst> = ["Ajdin", "Amar"]
imena.dodaj("Haris")
imena.ukloni(0)
ispisi brojevi[1]
ispisi imena.velicina()
ispisi imena[0] + "," + imena[1]

neka ocjene kao rjecnik<tekst, cijeli> = {"Ajdin": 10, "Amar": 9}
ocjene["Haris"] = 8
ispisi ocjene["Ajdin"] + ocjene["Haris"]
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "20\n2\nAmar,Haris\n18\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 11. Structures (zapis)
    runTest("11. Strukture (zapis Student)", []() -> std::string {
        const std::string src = R"BOS(
zapis Student
    ime kao tekst
    godine kao cijeli
    prosjek kao decimalni
kraj

neka s kao Student = Student("Ajdin", 23, 9.4)
ispisi s.ime + " (" + s.godine + ") - " + s.prosjek
s.prosjek = 9.8
ispisi s.prosjek
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "Ajdin (23) - 9.4\n9.8\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 12. Classes + privatno encapsulation check
    runTest("12. Klase, metode, konstruktor i kontrola pristupa (javno/privatno)", []() -> std::string {
        const std::string src = R"BOS(
klasa Automobil
    javno:
        marka kao tekst
        godina kao cijeli

        konstruktor(m kao tekst, g kao cijeli)
            marka = m
            godina = g
            brzina = 0
        kraj

        funkcija ubrzaj(iznos kao cijeli) vraća prazno
            brzina += iznos
        kraj

        funkcija predstavi() vraća prazno
            ispisi marka + " (" + godina + ") brzina=" + brzina
        kraj

    privatno:
        brzina kao cijeli
kraj

neka a = Automobil("Golf", 2022)
a.ubrzaj(60)
a.predstavi()
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "Golf (2022) brzina=60\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;

        // Accessing private field from outside must fail semantic check!
        const std::string badPriv = src + "\nispisi a.brzina\n";
        auto badRes = compileAndRun(badPriv);
        if (badRes.compiled) return "Pristup privatnom polju 'brzina' je morao biti odbijen!";
        return "";
    });

    // 13. Inheritance
    runTest("13. Nasljeđivanje klasa (nasljeduje) i preglasavanje metoda", []() -> std::string {
        const std::string src = R"BOS(
klasa Vozilo
    javno:
        marka kao tekst
        funkcija opis() vraća tekst
            vrati "Vozilo: " + marka
        kraj
kraj

klasa Automobil nasljeduje Vozilo
    javno:
        vrata kao cijeli
        funkcija opis() vraća tekst
            vrati "Automobil: " + marka + ", vrata=" + vrata
        kraj
kraj

neka auto kao Automobil = Automobil()
auto.marka = "Škoda"
auto.vrata = 5
ispisi auto.opis()
)BOS";
        auto res = compileAndRun(src);
        std::string exp = "Automobil: Škoda, vrata=5\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 14. Input & Output (ucitaj + ispisi)
    runTest("14. Ulaz i izlaz (ucitaj + ispisi)", []() -> std::string {
        const std::string src = R"BOS(
neka ime kao tekst
neka godine kao cijeli
ucitaj ime
ucitaj godine
ispisi "Korisnik: " + ime + ", dob: " + (godine + 1)
)BOS";
        auto res = compileAndRun(src, "Amina\n24\n");
        std::string exp = "Korisnik: Amina, dob: 25\n";
        if (!res.executed || res.output != exp) return "Pogrešan izlaz: " + res.output;
        return "";
    });

    // 15. Invalid syntax detection
    runTest("15. Detekcija neispravne sintakse", []() -> std::string {
        const std::string src = R"BOS(
ako 5 > 3
    ispisi "Nedostaje onda"
kraj
)BOS";
        auto res = compileAndRun(src);
        if (res.compiled || res.diagnostics.empty()) return "Sintaksna greška nije otkrivena!";
        return "";
    });

    // 16. Static type errors
    runTest("16. Detekcija grešaka tipova", []() -> std::string {
        const std::string src = R"BOS(
neka broj kao cijeli = "nije broj"
)BOS";
        auto res = compileAndRun(src);
        if (res.compiled || res.diagnostics.empty()) return "Greška tipa nije otkrivena!";
        return "";
    });

    // 17. Undefined variables
    runTest("17. Detekcija nedefinisanih promjenljivih", []() -> std::string {
        const std::string src = R"BOS(
ispisi nepostojecaPromjenljiva + 5
)BOS";
        auto res = compileAndRun(src);
        if (res.compiled || res.diagnostics.empty()) return "Nedefinisana promjenljiva nije otkrivena!";
        return "";
    });

    // 18. Division by zero + pokusaj/uhvati
    runTest("18. Dijeljenje sa nulom i hvatanje greške (pokusaj/uhvati)", []() -> std::string {
        const std::string src = R"BOS(
pokusaj
    neka rezultat = 10 / 0
    ispisi rezultat
uhvati greska
    ispisi "Uhvaćeno: " + greska.poruka
kraj
)BOS";
        auto res = compileAndRun(src);
        if (!res.executed) return "Greška nije uhvaćena!";
        if (res.output.find("Dijeljenje sa nulom") == std::string::npos) {
            return "Neočekivana poruka greške: " + res.output;
        }
        // Uncaught division by zero must report runtime error cleanly
        auto uncaught = compileAndRun("neka x = 10 / 0\n");
        if (uncaught.executed) return "Neuhvaćeno dijeljenje sa nulom moralo je zaustaviti VM!";
        return "";
    });

    // 19. UTF-8 identifiers & Standard library (matematika, tekst, kolekcije, datoteke)
    runTest("19. Standardna biblioteka (matematika, tekst, kolekcije, datoteke) i UTF-8", []() -> std::string {
        fs::path tmpDir = fs::temp_directory_path() / "bos_test_sandbox";
        fs::create_directories(tmpDir);

        const std::string src = R"BOS(
koristi matematika
koristi tekst
koristi kolekcije
koristi datoteke

neka ključnaRiječ = "čćžšđ"
ispisi matematika.korijen(144)
ispisi matematika.apsolutno(-19)
ispisi matematika.stepen(2, 8)
ispisi matematika.minimum(7, 3)
ispisi matematika.maksimum(7, 3)

ispisi tekst.duzina(ključnaRiječ)
ispisi tekst.veliko(ključnaRiječ)
ispisi tekst.malo("ČĆŽŠĐ")
ispisi tekst.sadrzi("Bosanski jezik", "jezik")

neka k = kolekcije.lista()
kolekcije.dodaj(k, "prvi")
kolekcije.dodaj(k, "drugi")
ispisi kolekcije.velicina(k)

neka f = datoteke.otvori("poruka.txt", "w")
datoteke.zapisi(f, "Pozdrav iz BOS-a! čćžšđ")
datoteke.zatvori(f)

neka fr = datoteke.otvori("poruka.txt", "r")
ispisi datoteke.procitaj(fr)
datoteke.zatvori(fr)
)BOS";
        auto res = compileAndRun(src, "", tmpDir.string());
        fs::remove_all(tmpDir);

        std::string exp =
            "12\n19\n256\n3\n7\n"
            "5\nČĆŽŠĐ\nčćžšđ\ntačno\n"
            "2\n"
            "Pozdrav iz BOS-a! čćžšđ\n";
        if (!res.executed || res.output != exp) {
            return "Pogrešan izlaz:\nDobijeno:\n" + res.output + "\nOčekivano:\n" + exp;
        }
        return "";
    });

    // 20. Binary .bosb compilation & reload execution roundtrip
    runTest("20. Binarni bajtkod (.bosb) zapis i ponovno izvršavanje na VM-u", []() -> std::string {
        const std::string src = R"BOS(
funkcija kvadrat(x kao cijeli) vraća cijeli
    vrati x * x
kraj
ispisi kvadrat(12)
)BOS";
        bos::DiagnosticEngine diag;
        bos::Lexer lex(src, "roundtrip.bos", diag);
        auto toks = lex.tokenize();
        bos::Parser parser(toks, "roundtrip.bos", diag);
        auto prog = parser.parseProgram();
        bos::SemanticAnalyzer sem(diag);
        sem.analyze(prog);
        if (diag.hasErrors()) return diag.formatAll();

        bos::IRGenerator gen(sem.symbols(), diag);
        auto mod = gen.generate(prog);

        std::string tmpPath = (fs::temp_directory_path() / "bos_roundtrip_test.bosb").string();
        std::string ioErr;
        if (!bos::BytecodeFormat::writeToFile(mod, tmpPath, ioErr)) return ioErr;

        bos::BytecodeModule loaded;
        if (!bos::BytecodeFormat::readFromFile(tmpPath, loaded, ioErr)) {
            std::remove(tmpPath.c_str());
            return ioErr;
        }
        std::remove(tmpPath.c_str());

        std::ostringstream out;
        bos::VMOptions opts;
        opts.output = &out;
        bos::VirtualMachine vm(opts);
        auto vmRes = vm.execute(loaded, diag);
        if (!vmRes.ok || out.str() != "144\n") {
            return "Pogrešan izlaz nakon učitavanja .bosb: " + out.str();
        }
        return "";
    });

    int passedCount = 0;
    for (const auto& r : reports) {
        if (r.passed) passedCount++;
    }

    if (jsonMode) {
        std::cout << "{\n  \"passed\": " << passedCount
                  << ",\n  \"total\": " << reports.size()
                  << ",\n  \"tests\": [\n";
        for (size_t i = 0; i < reports.size(); ++i) {
            if (i > 0) std::cout << ",\n";
            std::cout << "    {\"name\": \"" << jsonEscape(reports[i].name)
                      << "\", \"passed\": " << (reports[i].passed ? "true" : "false")
                      << ", \"details\": \"" << jsonEscape(reports[i].details) << "\"}";
        }
        std::cout << "\n  ]\n}\n";
    } else {
        std::cout << "=== BOS v0.1 Automatski Testovi ===\n\n";
        for (const auto& r : reports) {
            std::cout << (r.passed ? "[PROŠAO] " : "[PAO]    ") << r.name << "\n";
            if (!r.passed && !r.details.empty()) {
                std::cout << "         -> " << r.details << "\n";
            }
        }
        std::cout << "\nUkupno: " << passedCount << " / " << reports.size() << " testova prošlo.\n";
    }

    return (passedCount == static_cast<int>(reports.size())) ? 0 : 1;
}
