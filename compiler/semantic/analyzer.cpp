#include "semantic/analyzer.hpp"

#include <unordered_set>

namespace bos {

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {}

void SemanticAnalyzer::analyze(Program& program) {
    registerTypeNames(program);
    registerTypeMembersAndFunctions(program);
    validateInheritance(program);

    for (auto& stmt : program.statements) {
        if (stmt) analyzeStmt(*stmt);
    }
}

TypePtr SemanticAnalyzer::resolveTypeAnnotation(const TypeAnnotation& ann) {
    if (ann.name == "<greska>") return Type::error();
    if (ann.name == "cijeli") return Type::cijeli();
    if (ann.name == "decimalni") return Type::decimalni();
    if (ann.name == "tekst") return Type::tekst();
    if (ann.name == "znak") return Type::znak();
    if (ann.name == "logički" || ann.name == "logicki") return Type::logicki();
    if (ann.name == "prazno") return Type::prazno();
    if (ann.name == "Greska") return Type::greska();
    if (ann.name == "Datoteka") return Type::datoteka();

    if (ann.name == "niz") {
        if (ann.typeArgs.size() != 1) {
            diagnostics_.error(DiagnosticStage::Semantic, ann.location,
                               "Tip 'niz' zahtijeva tačno jedan generički tip, npr. niz<cijeli>.", "S001");
            return Type::niz(Type::error());
        }
        return Type::niz(resolveTypeAnnotation(ann.typeArgs[0]));
    }
    if (ann.name == "lista") {
        if (ann.typeArgs.size() != 1) {
            diagnostics_.error(DiagnosticStage::Semantic, ann.location,
                               "Tip 'lista' zahtijeva tačno jedan generički tip, npr. lista<tekst>.", "S002");
            return Type::lista(Type::error());
        }
        return Type::lista(resolveTypeAnnotation(ann.typeArgs[0]));
    }
    if (ann.name == "rjecnik" || ann.name == "rječnik") {
        if (ann.typeArgs.size() != 2) {
            diagnostics_.error(DiagnosticStage::Semantic, ann.location,
                               "Tip 'rjecnik' zahtijeva dva generička tipa, npr. rjecnik<tekst, cijeli>.", "S003");
            return Type::rjecnik(Type::error(), Type::error());
        }
        return Type::rjecnik(resolveTypeAnnotation(ann.typeArgs[0]),
                             resolveTypeAnnotation(ann.typeArgs[1]));
    }

    if (symbols_.lookupStruct(ann.name)) {
        return Type::zapis(ann.name);
    }
    if (symbols_.lookupClass(ann.name)) {
        return Type::klasa(ann.name);
    }

    diagnostics_.error(DiagnosticStage::Semantic, ann.location,
                       "Nepoznat tip '" + ann.name + "'.", "S004");
    return Type::error();
}

bool SemanticAnalyzer::isAssignable(const TypePtr& target, const TypePtr& source) const {
    if (!target || !source) return false;
    if (target->isError() || source->isError()) return true;
    if (target->isAny() || source->isAny()) return true;
    if (target->equals(*source)) return true;

    // Allow cijeli -> decimalni widening
    if (target->kind == TypeKind::Decimalni && source->kind == TypeKind::Cijeli) {
        return true;
    }
    // Allow derived class -> base class assignment
    if (target->kind == TypeKind::Klasa && source->kind == TypeKind::Klasa) {
        return symbols_.isSubclassOf(source->name, target->name);
    }
    return false;
}

void SemanticAnalyzer::registerTypeNames(Program& program) {
    for (auto& stmt : program.statements) {
        if (auto* st = dynamic_cast<StructDeclStmt*>(stmt.get())) {
            StructInfo info;
            info.name = st->name;
            info.location = st->location;
            if (!symbols_.declareStruct(info)) {
                diagnostics_.error(DiagnosticStage::Semantic, st->location,
                                   "Tip '" + st->name + "' je već definisan.", "S005");
            }
        } else if (auto* cl = dynamic_cast<ClassDeclStmt*>(stmt.get())) {
            ClassInfo info;
            info.name = cl->name;
            info.baseClass = cl->baseClass;
            info.location = cl->location;
            if (!symbols_.declareClass(info)) {
                diagnostics_.error(DiagnosticStage::Semantic, cl->location,
                                   "Tip '" + cl->name + "' je već definisan.", "S006");
            }
        }
    }
}

void SemanticAnalyzer::registerTypeMembersAndFunctions(Program& program) {
    for (auto& stmt : program.statements) {
        if (auto* st = dynamic_cast<StructDeclStmt*>(stmt.get())) {
            auto* info = symbols_.lookupStructMut(st->name);
            if (!info) continue;
            std::unordered_set<std::string> seen;
            for (auto& f : st->fields) {
                f.resolvedType = resolveTypeAnnotation(f.typeAnn);
                if (!seen.insert(f.name).second) {
                    diagnostics_.error(DiagnosticStage::Semantic, f.location,
                                       "Duplikat polja '" + f.name + "' u zapisu '" + st->name + "'.", "S007");
                    continue;
                }
                info->fields.push_back(FieldInfo{f.name, f.resolvedType, Visibility::Javno, st->name, f.location});
            }
        } else if (auto* cl = dynamic_cast<ClassDeclStmt*>(stmt.get())) {
            auto* info = symbols_.lookupClassMut(cl->name);
            if (!info) continue;
            std::unordered_set<std::string> seenFields;
            for (auto& f : cl->fields) {
                f.resolvedType = resolveTypeAnnotation(f.typeAnn);
                if (!seenFields.insert(f.name).second) {
                    diagnostics_.error(DiagnosticStage::Semantic, f.location,
                                       "Duplikat polja '" + f.name + "' u klasi '" + cl->name + "'.", "S008");
                    continue;
                }
                info->fields.push_back(FieldInfo{f.name, f.resolvedType, f.visibility, cl->name, f.location});
            }
            for (auto& m : cl->methods) {
                FunctionSignature sig;
                sig.name = m->name;
                sig.visibility = m->visibility;
                sig.ownerClass = cl->name;
                sig.location = m->location;
                for (auto& p : m->params) {
                    p.resolvedType = resolveTypeAnnotation(p.typeAnn);
                    sig.paramTypes.push_back(p.resolvedType);
                }
                m->resolvedReturnType = resolveTypeAnnotation(m->returnTypeAnn);
                sig.returnType = m->resolvedReturnType;

                if (m->isConstructor) {
                    if (info->constructor.has_value()) {
                        diagnostics_.error(DiagnosticStage::Semantic, m->location,
                                           "Klasa '" + cl->name + "' već ima definisan konstruktor.", "S009");
                    } else {
                        info->constructor = sig;
                    }
                } else {
                    if (info->methods.find(m->name) != info->methods.end()) {
                        diagnostics_.error(DiagnosticStage::Semantic, m->location,
                                           "Duplikat metode '" + m->name + "' u klasi '" + cl->name + "'.", "S010");
                    } else {
                        info->methods.emplace(m->name, sig);
                    }
                }
            }
        } else if (auto* fn = dynamic_cast<FuncDeclStmt*>(stmt.get())) {
            FunctionSignature sig;
            sig.name = fn->name;
            sig.location = fn->location;
            for (auto& p : fn->params) {
                p.resolvedType = resolveTypeAnnotation(p.typeAnn);
                sig.paramTypes.push_back(p.resolvedType);
            }
            fn->resolvedReturnType = resolveTypeAnnotation(fn->returnTypeAnn);
            sig.returnType = fn->resolvedReturnType;
            if (!symbols_.declareFunction(sig)) {
                diagnostics_.error(DiagnosticStage::Semantic, fn->location,
                                   "Funkcija '" + fn->name + "' je već definisana.", "S011");
            }
        }
    }
}

void SemanticAnalyzer::validateInheritance(Program& program) {
    for (auto& stmt : program.statements) {
        auto* cl = dynamic_cast<ClassDeclStmt*>(stmt.get());
        if (!cl || cl->baseClass.empty()) continue;

        if (!symbols_.lookupClass(cl->baseClass)) {
            diagnostics_.error(DiagnosticStage::Semantic, cl->location,
                               "Bazna klasa '" + cl->baseClass + "' za klasu '" + cl->name + "' ne postoji.", "S012");
            continue;
        }
        // Check for cycle
        std::unordered_set<std::string> visited;
        visited.insert(cl->name);
        std::string cur = cl->baseClass;
        while (!cur.empty()) {
            if (!visited.insert(cur).second) {
                diagnostics_.error(DiagnosticStage::Semantic, cl->location,
                                   "Otkriveno ciklično nasljeđivanje u hijerarhiji klase '" + cl->name + "'.", "S013");
                break;
            }
            const ClassInfo* ci = symbols_.lookupClass(cur);
            cur = ci ? ci->baseClass : "";
        }
    }
}

void SemanticAnalyzer::analyzeStmt(Stmt& stmt) {
    if (auto* v = dynamic_cast<VarDeclStmt*>(&stmt)) {
        analyzeVarDecl(*v);
    } else if (auto* a = dynamic_cast<AssignStmt*>(&stmt)) {
        analyzeAssign(*a);
    } else if (auto* e = dynamic_cast<ExprStmt*>(&stmt)) {
        analyzeExpr(*e->expr);
    } else if (auto* p = dynamic_cast<PrintStmt*>(&stmt)) {
        TypePtr t = analyzeExpr(*p->expr);
        if (t->kind == TypeKind::Prazno) {
            diagnostics_.error(DiagnosticStage::Semantic, p->location,
                               "Nije moguće ispisati izraz tipa 'prazno'.", "S014");
        }
    } else if (auto* in = dynamic_cast<InputStmt*>(&stmt)) {
        const Symbol* sym = symbols_.lookup(in->targetName);
        if (sym) {
            if (sym->isConst) {
                diagnostics_.error(DiagnosticStage::Semantic, in->location,
                                   "Nije moguće učitati vrijednost u konstantu '" + in->targetName + "'.", "S015");
            }
            in->targetType = sym->type;
        } else if (!currentClass_.empty()) {
            const FieldInfo* f = symbols_.resolveClassField(currentClass_, in->targetName);
            if (f && (f->visibility == Visibility::Javno || f->ownerClass == currentClass_)) {
                in->targetType = f->type;
                in->resolvedAsSelfField = true;
            } else {
                diagnostics_.error(DiagnosticStage::Semantic, in->location,
                                   "Nedefinisana promjenljiva '" + in->targetName + "' u naredbi 'ucitaj'.", "S016");
                in->targetType = Type::error();
            }
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, in->location,
                               "Nedefinisana promjenljiva '" + in->targetName + "' u naredbi 'ucitaj'.", "S016");
            in->targetType = Type::error();
        }
    } else if (auto* iff = dynamic_cast<IfStmt*>(&stmt)) {
        analyzeIf(*iff);
    } else if (auto* w = dynamic_cast<WhileStmt*>(&stmt)) {
        analyzeWhile(*w);
    } else if (auto* fr = dynamic_cast<ForRangeStmt*>(&stmt)) {
        analyzeForRange(*fr);
    } else if (auto* fe = dynamic_cast<ForEachStmt*>(&stmt)) {
        analyzeForEach(*fe);
    } else if (dynamic_cast<BreakStmt*>(&stmt)) {
        if (loopDepth_ <= 0) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Naredba 'prekini' može se koristiti samo unutar petlje.", "S017");
        }
    } else if (dynamic_cast<ContinueStmt*>(&stmt)) {
        if (loopDepth_ <= 0) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Naredba 'nastavi' može se koristiti samo unutar petlje.", "S018");
        }
    } else if (auto* r = dynamic_cast<ReturnStmt*>(&stmt)) {
        analyzeReturn(*r);
    } else if (auto* tc = dynamic_cast<TryCatchStmt*>(&stmt)) {
        analyzeTryCatch(*tc);
    } else if (auto* th = dynamic_cast<ThrowStmt*>(&stmt)) {
        TypePtr t = analyzeExpr(*th->expr);
        if (!t->isError() && t->kind != TypeKind::Tekst && t->kind != TypeKind::Greska) {
            diagnostics_.error(DiagnosticStage::Semantic, th->location,
                               "Naredba 'baci' očekuje izraz tipa 'tekst' ili 'Greska'.", "S019");
        }
    } else if (auto* imp = dynamic_cast<ImportStmt*>(&stmt)) {
        analyzeImport(*imp);
    } else if (auto* fn = dynamic_cast<FuncDeclStmt*>(&stmt)) {
        analyzeFuncBody(*fn, "");
    } else if (auto* st = dynamic_cast<StructDeclStmt*>(&stmt)) {
        for (auto& f : st->fields) {
            if (f.defaultValue) {
                TypePtr vt = analyzeExpr(*f.defaultValue, f.resolvedType);
                if (!isAssignable(f.resolvedType, vt)) {
                    diagnostics_.error(DiagnosticStage::Semantic, f.location,
                                       "Podrazumijevana vrijednost polja '" + f.name + "' ima nekompatibilan tip '"
                                       + vt->toString() + "', očekivan '" + f.resolvedType->toString() + "'.", "S020");
                }
            }
        }
    } else if (auto* cl = dynamic_cast<ClassDeclStmt*>(&stmt)) {
        analyzeClassBody(*cl);
    }
}

void SemanticAnalyzer::analyzeVarDecl(VarDeclStmt& stmt) {
    TypePtr annotatedType;
    if (stmt.typeAnn) {
        annotatedType = resolveTypeAnnotation(*stmt.typeAnn);
        if (annotatedType->kind == TypeKind::Prazno) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Promjenljiva '" + stmt.name + "' ne može imati tip 'prazno'.", "S021");
            annotatedType = Type::error();
        }
    }

    TypePtr initType;
    if (stmt.initializer) {
        initType = analyzeExpr(*stmt.initializer, annotatedType);
        if (initType->kind == TypeKind::Prazno) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Nije moguće inicijalizovati promjenljivu '" + stmt.name + "' izrazom tipa 'prazno'.", "S022");
            initType = Type::error();
        }
    }

    if (annotatedType && initType) {
        if (!isAssignable(annotatedType, initType)) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Nepodudaranje tipova za '" + stmt.name + "': očekivan tip '" + annotatedType->toString()
                               + "', ali je dobijen '" + initType->toString() + "'.", "S023");
        }
        stmt.resolvedType = annotatedType;
    } else if (annotatedType) {
        stmt.resolvedType = annotatedType;
    } else if (initType) {
        stmt.resolvedType = initType;
    } else {
        stmt.resolvedType = Type::error();
    }

    if (!symbols_.declare(Symbol{stmt.name, stmt.resolvedType, stmt.isConst, stmt.location})) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                           "Promjenljiva '" + stmt.name + "' je već deklarisana u ovom opsegu.", "S024");
    }
}

void SemanticAnalyzer::analyzeAssign(AssignStmt& stmt) {
    TypePtr targetType = Type::error();

    if (auto* id = dynamic_cast<IdentifierExpr*>(stmt.target.get())) {
        const Symbol* sym = symbols_.lookup(id->name);
        if (sym) {
            if (sym->isConst) {
                diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                                   "Nije moguće mijenjati vrijednost konstante '" + id->name + "'.", "S025");
            }
            targetType = sym->type;
            id->inferredType = targetType;
        } else if (!currentClass_.empty()) {
            const FieldInfo* f = symbols_.resolveClassField(currentClass_, id->name);
            if (f) {
                if (f->visibility == Visibility::Privatno && f->ownerClass != currentClass_) {
                    diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                                       "Greška pristupa: polje '" + id->name + "' klase '" + f->ownerClass + "' je privatno.", "S026");
                }
                id->resolvedAsSelfField = true;
                targetType = f->type;
                id->inferredType = targetType;
            } else {
                diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                                   "Nedefinisana promjenljiva '" + id->name + "'.", "S027");
            }
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Nedefinisana promjenljiva '" + id->name + "'.", "S027");
        }
    } else {
        targetType = analyzeExpr(*stmt.target);
    }

    TypePtr valueType = analyzeExpr(*stmt.value, targetType);

    if (stmt.op != AssignOp::Assign) {
        if (stmt.op == AssignOp::AddAssign && targetType->kind == TypeKind::Tekst) {
            // tekst += anything printable is valid
        } else if (!targetType->isError() && !valueType->isError()) {
            if (!targetType->isNumeric() || !valueType->isNumeric()) {
                diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                                   "Složena dodjela zahtijeva numeričke tipove, dobijeno '" + targetType->toString()
                                   + "' i '" + valueType->toString() + "'.", "S028");
            }
        }
    } else if (!isAssignable(targetType, valueType)) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                           "Nepodudaranje tipova pri dodjeli: ne može se dodijeliti '" + valueType->toString()
                           + "' u '" + targetType->toString() + "'.", "S029");
    }
}

void SemanticAnalyzer::analyzeIf(IfStmt& stmt) {
    TypePtr condType = analyzeExpr(*stmt.condition);
    if (!condType->isError() && condType->kind != TypeKind::Logicki) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.condition->location,
                           "Uslov u naredbi 'ako' mora biti tipa 'logički', ali je dobijen '" + condType->toString() + "'.", "S030");
    }

    symbols_.pushScope();
    for (auto& s : stmt.thenBranch) analyzeStmt(*s);
    symbols_.popScope();

    for (auto& ei : stmt.elseIfBranches) {
        TypePtr eiType = analyzeExpr(*ei.condition);
        if (!eiType->isError() && eiType->kind != TypeKind::Logicki) {
            diagnostics_.error(DiagnosticStage::Semantic, ei.condition->location,
                               "Uslov u naredbi 'inače ako' mora biti tipa 'logički', ali je dobijen '" + eiType->toString() + "'.", "S031");
        }
        symbols_.pushScope();
        for (auto& s : ei.body) analyzeStmt(*s);
        symbols_.popScope();
    }

    if (!stmt.elseBranch.empty()) {
        symbols_.pushScope();
        for (auto& s : stmt.elseBranch) analyzeStmt(*s);
        symbols_.popScope();
    }
}

void SemanticAnalyzer::analyzeWhile(WhileStmt& stmt) {
    TypePtr condType = analyzeExpr(*stmt.condition);
    if (!condType->isError() && condType->kind != TypeKind::Logicki) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.condition->location,
                           "Uslov u 'dok' petlji mora biti tipa 'logički', ali je dobijen '" + condType->toString() + "'.", "S032");
    }
    loopDepth_++;
    symbols_.pushScope();
    for (auto& s : stmt.body) analyzeStmt(*s);
    symbols_.popScope();
    loopDepth_--;
}

void SemanticAnalyzer::analyzeForRange(ForRangeStmt& stmt) {
    TypePtr startType = analyzeExpr(*stmt.start);
    TypePtr endType = analyzeExpr(*stmt.end);
    if (!startType->isError() && startType->kind != TypeKind::Cijeli) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.start->location,
                           "Početna vrijednost u 'ponavljaj' petlji mora biti tipa 'cijeli'.", "S033");
    }
    if (!endType->isError() && endType->kind != TypeKind::Cijeli) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.end->location,
                           "Završna vrijednost u 'ponavljaj' petlji mora biti tipa 'cijeli'.", "S034");
    }

    loopDepth_++;
    symbols_.pushScope();
    symbols_.declare(Symbol{stmt.varName, Type::cijeli(), false, stmt.location});
    for (auto& s : stmt.body) analyzeStmt(*s);
    symbols_.popScope();
    loopDepth_--;
}

void SemanticAnalyzer::analyzeForEach(ForEachStmt& stmt) {
    TypePtr iterType = analyzeExpr(*stmt.iterable);
    TypePtr elemType = Type::error();

    if (iterType->kind == TypeKind::Niz || iterType->kind == TypeKind::Lista) {
        elemType = iterType->typeArgs.empty() ? Type::any() : iterType->typeArgs[0];
    } else if (iterType->kind == TypeKind::Tekst) {
        elemType = Type::znak();
    } else if (!iterType->isError()) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.iterable->location,
                           "Izraz tipa '" + iterType->toString() + "' se ne može iterirati u 'za svaki' petlji (očekivan niz, lista ili tekst).", "S035");
    }

    stmt.elementType = elemType;
    loopDepth_++;
    symbols_.pushScope();
    symbols_.declare(Symbol{stmt.varName, elemType, false, stmt.location});
    for (auto& s : stmt.body) analyzeStmt(*s);
    symbols_.popScope();
    loopDepth_--;
}

void SemanticAnalyzer::analyzeReturn(ReturnStmt& stmt) {
    if (!inFunction_) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                           "Naredba 'vrati' može se koristiti samo unutar funkcije ili metode.", "S036");
        return;
    }

    if (inConstructor_) {
        if (stmt.value) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Konstruktor ne može vratiti vrijednost.", "S037");
        }
        return;
    }

    if (currentReturnType_->kind == TypeKind::Prazno) {
        if (stmt.value) {
            analyzeExpr(*stmt.value);
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Funkcija sa povratnim tipom 'prazno' ne može vratiti vrijednost.", "S038");
        }
    } else {
        if (!stmt.value) {
            diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                               "Funkcija mora vratiti vrijednost tipa '" + currentReturnType_->toString() + "'.", "S039");
        } else {
            TypePtr valType = analyzeExpr(*stmt.value, currentReturnType_);
            if (!isAssignable(currentReturnType_, valType)) {
                diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                                   "Pogrešan tip povratne vrijednosti: očekivan '" + currentReturnType_->toString()
                                   + "', ali je vraćen '" + valType->toString() + "'.", "S040");
            }
        }
    }
}

void SemanticAnalyzer::analyzeTryCatch(TryCatchStmt& stmt) {
    symbols_.pushScope();
    for (auto& s : stmt.tryBody) analyzeStmt(*s);
    symbols_.popScope();

    symbols_.pushScope();
    symbols_.declare(Symbol{stmt.catchVar, Type::greska(), false, stmt.location});
    for (auto& s : stmt.catchBody) analyzeStmt(*s);
    symbols_.popScope();
}

void SemanticAnalyzer::analyzeImport(ImportStmt& stmt) {
    static const std::unordered_set<std::string> knownModules = {
        "matematika", "tekst", "kolekcije", "datoteke"
    };
    if (knownModules.find(stmt.moduleName) == knownModules.end()) {
        diagnostics_.error(DiagnosticStage::Semantic, stmt.location,
                           "Nepoznat standardni modul '" + stmt.moduleName
                           + "'. Dostupni moduli su: matematika, tekst, kolekcije, datoteke.", "S041");
        return;
    }
    importedModules_.insert(stmt.moduleName);
    if (!symbols_.lookupCurrentScope(stmt.moduleName)) {
        symbols_.declare(Symbol{stmt.moduleName, Type::modul(stmt.moduleName), true, stmt.location});
    }
}

void SemanticAnalyzer::analyzeFuncBody(FuncDeclStmt& fn, const std::string& ownerClass) {
    std::string prevClass = currentClass_;
    TypePtr prevReturn = currentReturnType_;
    bool prevInFunc = inFunction_;
    bool prevInCtor = inConstructor_;
    int prevLoop = loopDepth_;

    currentClass_ = ownerClass;
    currentReturnType_ = fn.resolvedReturnType ? fn.resolvedReturnType : Type::prazno();
    inFunction_ = true;
    inConstructor_ = fn.isConstructor;
    loopDepth_ = 0;

    symbols_.pushScope();
    for (const auto& p : fn.params) {
        if (!symbols_.declare(Symbol{p.name, p.resolvedType ? p.resolvedType : Type::error(), false, p.location})) {
            diagnostics_.error(DiagnosticStage::Semantic, p.location,
                               "Duplikat parametra '" + p.name + "' u funkciji '" + fn.name + "'.", "S042");
        }
    }
    for (auto& s : fn.body) analyzeStmt(*s);
    symbols_.popScope();

    currentClass_ = prevClass;
    currentReturnType_ = prevReturn;
    inFunction_ = prevInFunc;
    inConstructor_ = prevInCtor;
    loopDepth_ = prevLoop;
}

void SemanticAnalyzer::analyzeClassBody(ClassDeclStmt& cl) {
    for (auto& f : cl.fields) {
        if (f.defaultValue) {
            TypePtr vt = analyzeExpr(*f.defaultValue, f.resolvedType);
            if (!isAssignable(f.resolvedType, vt)) {
                diagnostics_.error(DiagnosticStage::Semantic, f.location,
                                   "Podrazumijevana vrijednost polja '" + f.name + "' ima nekompatibilan tip '"
                                   + vt->toString() + "', očekivan '" + f.resolvedType->toString() + "'.", "S043");
            }
        }
    }
    for (auto& m : cl.methods) {
        analyzeFuncBody(*m, cl.name);
    }
}

// ===================== Expression Analysis =====================

TypePtr SemanticAnalyzer::analyzeExpr(Expr& expr, TypePtr expectedType) {
    if (dynamic_cast<IntLiteralExpr*>(&expr)) {
        expr.inferredType = Type::cijeli();
        return expr.inferredType;
    }
    if (dynamic_cast<FloatLiteralExpr*>(&expr)) {
        expr.inferredType = Type::decimalni();
        return expr.inferredType;
    }
    if (dynamic_cast<StringLiteralExpr*>(&expr)) {
        expr.inferredType = Type::tekst();
        return expr.inferredType;
    }
    if (dynamic_cast<CharLiteralExpr*>(&expr)) {
        expr.inferredType = Type::znak();
        return expr.inferredType;
    }
    if (dynamic_cast<BoolLiteralExpr*>(&expr)) {
        expr.inferredType = Type::logicki();
        return expr.inferredType;
    }
    if (dynamic_cast<SelfExpr*>(&expr)) {
        if (currentClass_.empty()) {
            diagnostics_.error(DiagnosticStage::Semantic, expr.location,
                               "Ključna riječ 'ovaj' može se koristiti samo unutar metode klase.", "S044");
            expr.inferredType = Type::error();
        } else {
            expr.inferredType = Type::klasa(currentClass_);
        }
        return expr.inferredType;
    }
    if (auto* id = dynamic_cast<IdentifierExpr*>(&expr)) {
        if (const Symbol* sym = symbols_.lookup(id->name)) {
            id->inferredType = sym->type;
            return id->inferredType;
        }
        if (!currentClass_.empty()) {
            if (const FieldInfo* f = symbols_.resolveClassField(currentClass_, id->name)) {
                if (f->visibility == Visibility::Privatno && f->ownerClass != currentClass_) {
                    diagnostics_.error(DiagnosticStage::Semantic, id->location,
                                       "Greška pristupa: polje '" + id->name + "' klase '" + f->ownerClass + "' je privatno.", "S045");
                }
                id->resolvedAsSelfField = true;
                id->inferredType = f->type;
                return id->inferredType;
            }
            if (const FunctionSignature* m = symbols_.resolveClassMethod(currentClass_, id->name)) {
                if (m->visibility == Visibility::Privatno && m->ownerClass != currentClass_) {
                    diagnostics_.error(DiagnosticStage::Semantic, id->location,
                                       "Greška pristupa: metoda '" + id->name + "' klase '" + m->ownerClass + "' je privatna.", "S046");
                }
                id->resolvedAsSelfMethod = true;
                id->inferredType = m->returnType;
                return id->inferredType;
            }
        }
        // Built-in module name used without `koristi`
        if (id->name == "matematika" || id->name == "tekst" || id->name == "kolekcije" || id->name == "datoteke") {
            diagnostics_.error(DiagnosticStage::Semantic, id->location,
                               "Modul '" + id->name + "' nije uvezen. Dodajte 'koristi " + id->name + "' na početak programa.", "S047");
            id->inferredType = Type::modul(id->name);
            return id->inferredType;
        }
        diagnostics_.error(DiagnosticStage::Semantic, id->location,
                           "Nedefinisana promjenljiva '" + id->name + "'.", "S048");
        id->inferredType = Type::error();
        return id->inferredType;
    }
    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        TypePtr opType = analyzeExpr(*un->operand);
        if (opType->isError()) {
            un->inferredType = Type::error();
            return un->inferredType;
        }
        if (un->op == UnaryOp::Not) {
            if (opType->kind != TypeKind::Logicki) {
                diagnostics_.error(DiagnosticStage::Semantic, un->location,
                                   "Operator 'ne' zahtijeva tip 'logički', ali je dobijen '" + opType->toString() + "'.", "S049");
                un->inferredType = Type::error();
            } else {
                un->inferredType = Type::logicki();
            }
        } else {
            if (!opType->isNumeric()) {
                diagnostics_.error(DiagnosticStage::Semantic, un->location,
                                   "Unarni operator zahtijeva numerički tip ('cijeli' ili 'decimalni'), ali je dobijen '" + opType->toString() + "'.", "S050");
                un->inferredType = Type::error();
            } else {
                un->inferredType = opType;
            }
        }
        return un->inferredType;
    }
    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        TypePtr lt = analyzeExpr(*bin->left);
        TypePtr rt = analyzeExpr(*bin->right);
        if (lt->isError() || rt->isError()) {
            bin->inferredType = Type::error();
            return bin->inferredType;
        }

        switch (bin->op) {
            case BinaryOp::Add:
                if (lt->kind == TypeKind::Tekst || rt->kind == TypeKind::Tekst) {
                    if (lt->kind == TypeKind::Prazno || rt->kind == TypeKind::Prazno) {
                        diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                           "Nije moguće spojiti 'prazno' sa tekstom.", "S051");
                        bin->inferredType = Type::error();
                    } else {
                        bin->inferredType = Type::tekst();
                    }
                } else if (lt->isNumeric() && rt->isNumeric()) {
                    bin->inferredType = (lt->kind == TypeKind::Decimalni || rt->kind == TypeKind::Decimalni)
                        ? Type::decimalni() : Type::cijeli();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                       "Operator '+' nije podržan za tipove '" + lt->toString() + "' i '" + rt->toString() + "'.", "S052");
                    bin->inferredType = Type::error();
                }
                break;

            case BinaryOp::Sub:
            case BinaryOp::Mul:
            case BinaryOp::Div:
                if (lt->isNumeric() && rt->isNumeric()) {
                    bin->inferredType = (lt->kind == TypeKind::Decimalni || rt->kind == TypeKind::Decimalni)
                        ? Type::decimalni() : Type::cijeli();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                       "Aritmetički operator zahtijeva numeričke tipove, dobijeno '" + lt->toString() + "' i '" + rt->toString() + "'.", "S053");
                    bin->inferredType = Type::error();
                }
                break;

            case BinaryOp::Mod:
                if (lt->kind == TypeKind::Cijeli && rt->kind == TypeKind::Cijeli) {
                    bin->inferredType = Type::cijeli();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                       "Operator '%' (ostatak dijeljenja) zahtijeva dva 'cijeli' operanda.", "S054");
                    bin->inferredType = Type::error();
                }
                break;

            case BinaryOp::Eq:
            case BinaryOp::Neq:
                if (lt->equals(*rt) || (lt->isNumeric() && rt->isNumeric())) {
                    bin->inferredType = Type::logicki();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                       "Nije moguće porediti nekompatibilne tipove '" + lt->toString() + "' i '" + rt->toString() + "'.", "S055");
                    bin->inferredType = Type::error();
                }
                break;

            case BinaryOp::Lt:
            case BinaryOp::Lte:
            case BinaryOp::Gt:
            case BinaryOp::Gte:
                if ((lt->isNumeric() && rt->isNumeric()) ||
                    (lt->kind == TypeKind::Tekst && rt->kind == TypeKind::Tekst) ||
                    (lt->kind == TypeKind::Znak && rt->kind == TypeKind::Znak)) {
                    bin->inferredType = Type::logicki();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                       "Relacioni operator nije podržan za tipove '" + lt->toString() + "' i '" + rt->toString() + "'.", "S056");
                    bin->inferredType = Type::error();
                }
                break;

            case BinaryOp::And:
            case BinaryOp::Or:
                if (lt->kind == TypeKind::Logicki && rt->kind == TypeKind::Logicki) {
                    bin->inferredType = Type::logicki();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, bin->location,
                                       "Logički operator zahtijeva tip 'logički', dobijeno '" + lt->toString() + "' i '" + rt->toString() + "'.", "S057");
                    bin->inferredType = Type::error();
                }
                break;
        }
        return bin->inferredType;
    }
    if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        return analyzeCall(*call);
    }
    if (auto* mem = dynamic_cast<MemberAccessExpr*>(&expr)) {
        return analyzeMemberAccess(*mem);
    }
    if (auto* idx = dynamic_cast<IndexExpr*>(&expr)) {
        TypePtr objType = analyzeExpr(*idx->object);
        TypePtr idxType = analyzeExpr(*idx->index);
        if (objType->isError()) {
            idx->inferredType = Type::error();
            return idx->inferredType;
        }
        if (objType->kind == TypeKind::Niz || objType->kind == TypeKind::Lista) {
            if (!idxType->isError() && idxType->kind != TypeKind::Cijeli) {
                diagnostics_.error(DiagnosticStage::Semantic, idx->index->location,
                                   "Indeks kolekcije mora biti tipa 'cijeli', dobijeno '" + idxType->toString() + "'.", "S058");
            }
            idx->inferredType = objType->typeArgs.empty() ? Type::any() : objType->typeArgs[0];
        } else if (objType->kind == TypeKind::Tekst) {
            if (!idxType->isError() && idxType->kind != TypeKind::Cijeli) {
                diagnostics_.error(DiagnosticStage::Semantic, idx->index->location,
                                   "Indeks teksta mora biti tipa 'cijeli'.", "S059");
            }
            idx->inferredType = Type::znak();
        } else if (objType->kind == TypeKind::Rjecnik) {
            TypePtr keyType = objType->typeArgs.size() > 0 ? objType->typeArgs[0] : Type::any();
            TypePtr valType = objType->typeArgs.size() > 1 ? objType->typeArgs[1] : Type::any();
            if (!isAssignable(keyType, idxType)) {
                diagnostics_.error(DiagnosticStage::Semantic, idx->index->location,
                                   "Tip ključa rječnika mora biti '" + keyType->toString() + "', ali je dobijen '" + idxType->toString() + "'.", "S060");
            }
            idx->inferredType = valType;
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, idx->location,
                               "Tip '" + objType->toString() + "' ne podržava indeksiranje '[...]'.", "S061");
            idx->inferredType = Type::error();
        }
        return idx->inferredType;
    }
    if (auto* list = dynamic_cast<ListLiteralExpr*>(&expr)) {
        bool wantNiz = expectedType && expectedType->kind == TypeKind::Niz;
        list->isNiz = wantNiz;
        TypePtr expectedElem = (expectedType && !expectedType->typeArgs.empty() &&
                                (expectedType->kind == TypeKind::Niz || expectedType->kind == TypeKind::Lista))
            ? expectedType->typeArgs[0] : nullptr;

        TypePtr elemType = expectedElem;
        for (auto& el : list->elements) {
            TypePtr t = analyzeExpr(*el, expectedElem);
            if (!elemType) {
                elemType = t;
            } else if (!isAssignable(elemType, t)) {
                if (elemType->kind == TypeKind::Cijeli && t->kind == TypeKind::Decimalni && !expectedElem) {
                    elemType = Type::decimalni();
                } else {
                    diagnostics_.error(DiagnosticStage::Semantic, el->location,
                                       "Element kolekcije ima tip '" + t->toString() + "', ali se očekuje '" + elemType->toString() + "'.", "S062");
                }
            }
        }
        if (!elemType) elemType = Type::any();
        list->inferredType = wantNiz ? Type::niz(elemType) : Type::lista(elemType);
        return list->inferredType;
    }
    if (auto* dict = dynamic_cast<DictLiteralExpr*>(&expr)) {
        TypePtr expK = (expectedType && expectedType->kind == TypeKind::Rjecnik && expectedType->typeArgs.size() == 2)
            ? expectedType->typeArgs[0] : nullptr;
        TypePtr expV = (expectedType && expectedType->kind == TypeKind::Rjecnik && expectedType->typeArgs.size() == 2)
            ? expectedType->typeArgs[1] : nullptr;
        TypePtr kType = expK;
        TypePtr vType = expV;
        for (auto& kv : dict->entries) {
            TypePtr kt = analyzeExpr(*kv.first, expK);
            TypePtr vt = analyzeExpr(*kv.second, expV);
            if (!kType) kType = kt;
            else if (!isAssignable(kType, kt)) {
                diagnostics_.error(DiagnosticStage::Semantic, kv.first->location,
                                   "Ključ rječnika ima tip '" + kt->toString() + "', očekivan '" + kType->toString() + "'.", "S063");
            }
            if (!vType) vType = vt;
            else if (!isAssignable(vType, vt)) {
                diagnostics_.error(DiagnosticStage::Semantic, kv.second->location,
                                   "Vrijednost rječnika ima tip '" + vt->toString() + "', očekivana '" + vType->toString() + "'.", "S064");
            }
        }
        if (!kType) kType = Type::any();
        if (!vType) vType = Type::any();
        dict->inferredType = Type::rjecnik(kType, vType);
        return dict->inferredType;
    }

    expr.inferredType = Type::error();
    return expr.inferredType;
}

TypePtr SemanticAnalyzer::analyzeMemberAccess(MemberAccessExpr& mem) {
    TypePtr objType = analyzeExpr(*mem.object);
    if (objType->isError()) {
        mem.inferredType = Type::error();
        return mem.inferredType;
    }

    if (objType->kind == TypeKind::Modul) {
        if (objType->name == "matematika" && (mem.member == "PI" || mem.member == "E")) {
            mem.inferredType = Type::decimalni();
            return mem.inferredType;
        }
        diagnostics_.error(DiagnosticStage::Semantic, mem.location,
                           "Modul '" + objType->name + "' nema konstantni član '" + mem.member + "'.", "S065");
        mem.inferredType = Type::error();
        return mem.inferredType;
    }

    if (objType->kind == TypeKind::Greska) {
        if (mem.member == "poruka") {
            mem.inferredType = Type::tekst();
            return mem.inferredType;
        }
        if (mem.member == "kod") {
            mem.inferredType = Type::cijeli();
            return mem.inferredType;
        }
        diagnostics_.error(DiagnosticStage::Semantic, mem.location,
                           "Tip 'Greska' nema polje '" + mem.member + "' (dostupna su: poruka, kod).", "S066");
        mem.inferredType = Type::error();
        return mem.inferredType;
    }

    if (objType->kind == TypeKind::Zapis) {
        const StructInfo* st = symbols_.lookupStruct(objType->name);
        if (st) {
            if (const FieldInfo* f = st->findField(mem.member)) {
                mem.inferredType = f->type;
                return mem.inferredType;
            }
        }
        diagnostics_.error(DiagnosticStage::Semantic, mem.location,
                           "Zapis '" + objType->name + "' nema polje '" + mem.member + "'.", "S067");
        mem.inferredType = Type::error();
        return mem.inferredType;
    }

    if (objType->kind == TypeKind::Klasa) {
        const FieldInfo* f = symbols_.resolveClassField(objType->name, mem.member);
        if (f) {
            if (f->visibility == Visibility::Privatno && currentClass_ != f->ownerClass) {
                diagnostics_.error(DiagnosticStage::Semantic, mem.location,
                                   "Greška pristupa: polje '" + mem.member + "' klase '" + f->ownerClass + "' je privatno.", "S068");
            }
            mem.inferredType = f->type;
            return mem.inferredType;
        }
        diagnostics_.error(DiagnosticStage::Semantic, mem.location,
                           "Klasa '" + objType->name + "' nema polje '" + mem.member + "'.", "S069");
        mem.inferredType = Type::error();
        return mem.inferredType;
    }

    diagnostics_.error(DiagnosticStage::Semantic, mem.location,
                       "Tip '" + objType->toString() + "' nema član '" + mem.member + "'.", "S070");
    mem.inferredType = Type::error();
    return mem.inferredType;
}

TypePtr SemanticAnalyzer::analyzeCall(CallExpr& call) {
    // 1. Direct call: `foo(...)` or `Student(...)` or `Automobil(...)` or self method `predstavi()`
    if (auto* id = dynamic_cast<IdentifierExpr*>(call.callee.get())) {
        // Check class method inside same class first
        if (!currentClass_.empty()) {
            if (const FunctionSignature* m = symbols_.resolveClassMethod(currentClass_, id->name)) {
                if (m->visibility == Visibility::Privatno && m->ownerClass != currentClass_) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "Greška pristupa: metoda '" + id->name + "' klase '" + m->ownerClass + "' je privatna.", "S071");
                }
                id->resolvedAsSelfMethod = true;
                if (call.args.size() != m->paramTypes.size()) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "Metoda '" + id->name + "' očekuje " + std::to_string(m->paramTypes.size())
                                       + " argumenata, a proslijeđeno je " + std::to_string(call.args.size()) + ".", "S072");
                }
                for (size_t i = 0; i < call.args.size(); ++i) {
                    TypePtr expected = i < m->paramTypes.size() ? m->paramTypes[i] : nullptr;
                    TypePtr at = analyzeExpr(*call.args[i], expected);
                    if (expected && !isAssignable(expected, at)) {
                        diagnostics_.error(DiagnosticStage::Semantic, call.args[i]->location,
                                           "Argument " + std::to_string(i + 1) + " u pozivu '" + id->name + "' ima tip '"
                                           + at->toString() + "', očekivan '" + expected->toString() + "'.", "S073");
                    }
                }
                call.inferredType = m->returnType;
                return call.inferredType;
            }
        }

        // Struct constructor: `Student(...)` or `Student()`
        if (const StructInfo* st = symbols_.lookupStruct(id->name)) {
            if (!call.args.empty() && call.args.size() != st->fields.size()) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Konstruktor zapisa '" + st->name + "' očekuje 0 ili " + std::to_string(st->fields.size())
                                   + " argumenata, a proslijeđeno je " + std::to_string(call.args.size()) + ".", "S074");
            }
            for (size_t i = 0; i < call.args.size(); ++i) {
                TypePtr expected = i < st->fields.size() ? st->fields[i].type : nullptr;
                TypePtr at = analyzeExpr(*call.args[i], expected);
                if (expected && !isAssignable(expected, at)) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[i]->location,
                                       "Polje '" + st->fields[i].name + "' zapisa '" + st->name + "' očekuje tip '"
                                       + expected->toString() + "', dobijen '" + at->toString() + "'.", "S075");
                }
            }
            call.inferredType = Type::zapis(st->name);
            return call.inferredType;
        }

        // Class constructor: `Automobil(...)`
        if (const ClassInfo* cl = symbols_.lookupClass(id->name)) {
            if (cl->constructor.has_value()) {
                const auto& ctor = *cl->constructor;
                if (ctor.visibility == Visibility::Privatno && currentClass_ != cl->name) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "Konstruktor klase '" + cl->name + "' je privatan.", "S076");
                }
                if (call.args.size() != ctor.paramTypes.size()) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "Konstruktor klase '" + cl->name + "' očekuje " + std::to_string(ctor.paramTypes.size())
                                       + " argumenata, a proslijeđeno je " + std::to_string(call.args.size()) + ".", "S077");
                }
                for (size_t i = 0; i < call.args.size(); ++i) {
                    TypePtr expected = i < ctor.paramTypes.size() ? ctor.paramTypes[i] : nullptr;
                    TypePtr at = analyzeExpr(*call.args[i], expected);
                    if (expected && !isAssignable(expected, at)) {
                        diagnostics_.error(DiagnosticStage::Semantic, call.args[i]->location,
                                           "Argument " + std::to_string(i + 1) + " konstruktora '" + cl->name + "' ima tip '"
                                           + at->toString() + "', očekivan '" + expected->toString() + "'.", "S078");
                    }
                }
            } else if (!call.args.empty()) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Klasa '" + cl->name + "' nema definisan konstruktor sa argumentima (očekivano 0 argumenata).", "S079");
                for (auto& a : call.args) analyzeExpr(*a);
            }
            call.inferredType = Type::klasa(cl->name);
            return call.inferredType;
        }

        // Free function
        if (const FunctionSignature* fn = symbols_.lookupFunction(id->name)) {
            if (call.args.size() != fn->paramTypes.size()) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Funkcija '" + fn->name + "' očekuje " + std::to_string(fn->paramTypes.size())
                                   + " argumenata, a proslijeđeno je " + std::to_string(call.args.size()) + ".", "S080");
            }
            for (size_t i = 0; i < call.args.size(); ++i) {
                TypePtr expected = i < fn->paramTypes.size() ? fn->paramTypes[i] : nullptr;
                TypePtr at = analyzeExpr(*call.args[i], expected);
                if (expected && !isAssignable(expected, at)) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[i]->location,
                                       "Argument " + std::to_string(i + 1) + " u pozivu funkcije '" + fn->name + "' ima tip '"
                                       + at->toString() + "', očekivan '" + expected->toString() + "'.", "S081");
                }
            }
            call.inferredType = fn->returnType;
            return call.inferredType;
        }

        // Unknown function
        for (auto& a : call.args) analyzeExpr(*a);
        diagnostics_.error(DiagnosticStage::Semantic, call.location,
                           "Nepoznata funkcija ili tip '" + id->name + "'.", "S082");
        call.inferredType = Type::error();
        return call.inferredType;
    }

    // 2. Method / module call: `obj.metoda(...)`
    if (auto* mem = dynamic_cast<MemberAccessExpr*>(call.callee.get())) {
        TypePtr objType = analyzeExpr(*mem->object);
        if (objType->isError()) {
            for (auto& a : call.args) analyzeExpr(*a);
            call.inferredType = Type::error();
            return call.inferredType;
        }

        if (objType->kind == TypeKind::Modul) {
            return analyzeModuleCall(objType->name, mem->member, call);
        }

        if (objType->kind == TypeKind::Klasa) {
            const FunctionSignature* m = symbols_.resolveClassMethod(objType->name, mem->member);
            if (!m) {
                for (auto& a : call.args) analyzeExpr(*a);
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Klasa '" + objType->name + "' nema metodu '" + mem->member + "'.", "S083");
                call.inferredType = Type::error();
                return call.inferredType;
            }
            if (m->visibility == Visibility::Privatno && currentClass_ != m->ownerClass) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Greška pristupa: metoda '" + mem->member + "' klase '" + m->ownerClass + "' je privatna.", "S084");
            }
            if (call.args.size() != m->paramTypes.size()) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Metoda '" + mem->member + "' očekuje " + std::to_string(m->paramTypes.size())
                                   + " argumenata, a proslijeđeno je " + std::to_string(call.args.size()) + ".", "S085");
            }
            for (size_t i = 0; i < call.args.size(); ++i) {
                TypePtr expected = i < m->paramTypes.size() ? m->paramTypes[i] : nullptr;
                TypePtr at = analyzeExpr(*call.args[i], expected);
                if (expected && !isAssignable(expected, at)) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[i]->location,
                                       "Argument " + std::to_string(i + 1) + " metode '" + mem->member + "' ima tip '"
                                       + at->toString() + "', očekivan '" + expected->toString() + "'.", "S086");
                }
            }
            call.inferredType = m->returnType;
            return call.inferredType;
        }

        // Built-in collection / string methods: `imena.dodaj(...)`, `imena.ukloni(...)`, `imena.velicina()`
        return analyzeBuiltinMethodCall(objType, mem->member, call);
    }

    analyzeExpr(*call.callee);
    for (auto& a : call.args) analyzeExpr(*a);
    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                       "Izraz se ne može pozvati kao funkcija.", "S087");
    call.inferredType = Type::error();
    return call.inferredType;
}

TypePtr SemanticAnalyzer::analyzeModuleCall(const std::string& modName, const std::string& funcName,
                                            CallExpr& call) {
    std::vector<TypePtr> argTypes;
    for (auto& a : call.args) argTypes.push_back(analyzeExpr(*a));

    auto checkArity = [&](size_t expected) -> bool {
        if (argTypes.size() != expected) {
            diagnostics_.error(DiagnosticStage::Semantic, call.location,
                               "Funkcija '" + modName + "." + funcName + "' očekuje " + std::to_string(expected)
                               + " argumenata, a proslijeđeno je " + std::to_string(argTypes.size()) + ".", "S088");
            return false;
        }
        return true;
    };

    if (modName == "matematika") {
        if (funcName == "apsolutno") {
            if (checkArity(1)) {
                if (!argTypes[0]->isError() && !argTypes[0]->isNumeric()) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'matematika.apsolutno' očekuje numerički argument.", "S089");
                }
                call.inferredType = argTypes[0]->kind == TypeKind::Cijeli ? Type::cijeli() : Type::decimalni();
                return call.inferredType;
            }
        } else if (funcName == "korijen") {
            if (checkArity(1)) {
                if (!argTypes[0]->isError() && !argTypes[0]->isNumeric()) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'matematika.korijen' očekuje numerički argument.", "S090");
                }
                call.inferredType = Type::decimalni();
                return call.inferredType;
            }
        } else if (funcName == "stepen") {
            if (checkArity(2)) {
                if ((!argTypes[0]->isError() && !argTypes[0]->isNumeric()) ||
                    (!argTypes[1]->isError() && !argTypes[1]->isNumeric())) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "'matematika.stepen' očekuje dva numerička argumenta.", "S091");
                }
                call.inferredType = Type::decimalni();
                return call.inferredType;
            }
        } else if (funcName == "minimum" || funcName == "maksimum") {
            if (checkArity(2)) {
                if ((!argTypes[0]->isError() && !argTypes[0]->isNumeric()) ||
                    (!argTypes[1]->isError() && !argTypes[1]->isNumeric())) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "'matematika." + funcName + "' očekuje dva numerička argumenta.", "S092");
                }
                call.inferredType = (argTypes[0]->kind == TypeKind::Cijeli && argTypes[1]->kind == TypeKind::Cijeli)
                    ? Type::cijeli() : Type::decimalni();
                return call.inferredType;
            }
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, call.location,
                               "Modul 'matematika' nema funkciju '" + funcName + "'.", "S093");
        }
        call.inferredType = Type::error();
        return call.inferredType;
    }

    if (modName == "tekst") {
        if (funcName == "duzina") {
            if (checkArity(1)) {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Tekst) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'tekst.duzina' očekuje argument tipa 'tekst'.", "S094");
                }
                call.inferredType = Type::cijeli();
                return call.inferredType;
            }
        } else if (funcName == "veliko" || funcName == "malo") {
            if (checkArity(1)) {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Tekst) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'tekst." + funcName + "' očekuje argument tipa 'tekst'.", "S095");
                }
                call.inferredType = Type::tekst();
                return call.inferredType;
            }
        } else if (funcName == "sadrzi") {
            if (checkArity(2)) {
                if ((!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Tekst) ||
                    (!argTypes[1]->isError() && argTypes[1]->kind != TypeKind::Tekst)) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                       "'tekst.sadrzi' očekuje dva argumenta tipa 'tekst'.", "S096");
                }
                call.inferredType = Type::logicki();
                return call.inferredType;
            }
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, call.location,
                               "Modul 'tekst' nema funkciju '" + funcName + "'.", "S097");
        }
        call.inferredType = Type::error();
        return call.inferredType;
    }

    if (modName == "kolekcije") {
        if (funcName == "lista") {
            call.inferredType = Type::lista(Type::any());
            return call.inferredType;
        } else if (funcName == "niz") {
            call.inferredType = Type::niz(Type::any());
            return call.inferredType;
        } else if (funcName == "dodaj") {
            if (checkArity(2)) {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Lista && argTypes[0]->kind != TypeKind::Niz) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'kolekcije.dodaj' očekuje listu ili niz kao prvi argument.", "S098");
                }
                call.inferredType = Type::prazno();
                return call.inferredType;
            }
        } else if (funcName == "ukloni") {
            if (checkArity(2)) {
                if (!argTypes[1]->isError() && argTypes[1]->kind != TypeKind::Cijeli) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[1]->location,
                                       "'kolekcije.ukloni' očekuje indeks tipa 'cijeli'.", "S099");
                }
                call.inferredType = Type::prazno();
                return call.inferredType;
            }
        } else if (funcName == "velicina") {
            if (checkArity(1)) {
                call.inferredType = Type::cijeli();
                return call.inferredType;
            }
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, call.location,
                               "Modul 'kolekcije' nema funkciju '" + funcName + "'.", "S100");
        }
        call.inferredType = Type::error();
        return call.inferredType;
    }

    if (modName == "datoteke") {
        if (funcName == "otvori") {
            if (argTypes.size() < 1 || argTypes.size() > 2) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "'datoteke.otvori' očekuje 1 ili 2 argumenta (putanja, [nacin]).", "S101");
            } else {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Tekst) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "Putanja datoteke mora biti tipa 'tekst'.", "S102");
                }
                if (argTypes.size() == 2 && !argTypes[1]->isError() && argTypes[1]->kind != TypeKind::Tekst) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[1]->location,
                                       "Način otvaranja datoteke mora biti tipa 'tekst'.", "S103");
                }
            }
            call.inferredType = Type::datoteka();
            return call.inferredType;
        } else if (funcName == "procitaj") {
            if (checkArity(1)) {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Datoteka) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'datoteke.procitaj' očekuje otvorenu datoteku.", "S104");
                }
                call.inferredType = Type::tekst();
                return call.inferredType;
            }
        } else if (funcName == "zapisi") {
            if (checkArity(2)) {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Datoteka) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'datoteke.zapisi' očekuje otvorenu datoteku kao prvi argument.", "S105");
                }
                if (!argTypes[1]->isError() && argTypes[1]->kind != TypeKind::Tekst) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[1]->location,
                                       "'datoteke.zapisi' očekuje 'tekst' kao drugi argument.", "S106");
                }
                call.inferredType = Type::prazno();
                return call.inferredType;
            }
        } else if (funcName == "zatvori") {
            if (checkArity(1)) {
                if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Datoteka) {
                    diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                       "'datoteke.zatvori' očekuje otvorenu datoteku.", "S107");
                }
                call.inferredType = Type::prazno();
                return call.inferredType;
            }
        } else {
            diagnostics_.error(DiagnosticStage::Semantic, call.location,
                               "Modul 'datoteke' nema funkciju '" + funcName + "'.", "S108");
        }
        call.inferredType = Type::error();
        return call.inferredType;
    }

    call.inferredType = Type::error();
    return call.inferredType;
}

TypePtr SemanticAnalyzer::analyzeBuiltinMethodCall(const TypePtr& objType, const std::string& methodName,
                                                   CallExpr& call) {
    std::vector<TypePtr> argTypes;
    for (auto& a : call.args) argTypes.push_back(analyzeExpr(*a));

    if (objType->kind == TypeKind::Lista || objType->kind == TypeKind::Niz) {
        TypePtr elemType = objType->typeArgs.empty() ? Type::any() : objType->typeArgs[0];
        if (methodName == "dodaj") {
            if (argTypes.size() != 1) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Metoda 'dodaj' očekuje 1 argument.", "S109");
            } else if (!isAssignable(elemType, argTypes[0])) {
                diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                   "Nije moguće dodati vrijednost tipa '" + argTypes[0]->toString()
                                   + "' u kolekciju tipa '" + objType->toString() + "'.", "S110");
            }
            call.inferredType = Type::prazno();
            return call.inferredType;
        }
        if (methodName == "ukloni") {
            if (argTypes.size() != 1) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Metoda 'ukloni' očekuje 1 argument (indeks).", "S111");
            } else if (!argTypes[0]->isError() && argTypes[0]->kind != TypeKind::Cijeli) {
                diagnostics_.error(DiagnosticStage::Semantic, call.args[0]->location,
                                   "Metoda 'ukloni' očekuje indeks tipa 'cijeli'.", "S112");
            }
            call.inferredType = Type::prazno();
            return call.inferredType;
        }
        if (methodName == "velicina") {
            if (!argTypes.empty()) {
                diagnostics_.error(DiagnosticStage::Semantic, call.location,
                                   "Metoda 'velicina' ne prima argumente.", "S113");
            }
            call.inferredType = Type::cijeli();
            return call.inferredType;
        }
    }

    if (objType->kind == TypeKind::Rjecnik) {
        if (methodName == "velicina" && argTypes.empty()) {
            call.inferredType = Type::cijeli();
            return call.inferredType;
        }
        if (methodName == "sadrzi" && argTypes.size() == 1) {
            call.inferredType = Type::logicki();
            return call.inferredType;
        }
        if (methodName == "ukloni" && argTypes.size() == 1) {
            call.inferredType = Type::prazno();
            return call.inferredType;
        }
    }

    if (objType->kind == TypeKind::Tekst) {
        if (methodName == "duzina" && argTypes.empty()) {
            call.inferredType = Type::cijeli();
            return call.inferredType;
        }
        if ((methodName == "veliko" || methodName == "malo") && argTypes.empty()) {
            call.inferredType = Type::tekst();
            return call.inferredType;
        }
        if (methodName == "sadrzi" && argTypes.size() == 1 && argTypes[0]->kind == TypeKind::Tekst) {
            call.inferredType = Type::logicki();
            return call.inferredType;
        }
    }

    diagnostics_.error(DiagnosticStage::Semantic, call.location,
                       "Tip '" + objType->toString() + "' nema metodu '" + methodName + "'.", "S114");
    call.inferredType = Type::error();
    return call.inferredType;
}

} // namespace bos
