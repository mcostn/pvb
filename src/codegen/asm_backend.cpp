#include <algorithm>
#include <cstdint>
#include <utility>

#include "codegen/asm_backend.hpp"

static std::string Sanitize(const std::string &name);
static bool IsFloatKind(AsmKind k);
static bool IsIntDomain(AsmKind k);

// Unused hooks (required by the base Emitter interface)
std::string_view AsmEmitter::BinaryOperator(BinaryOp) { return {}; }
std::string_view AsmEmitter::UnaryOperator(UnaryOp) { return {}; }

// Type inference
AsmKind AsmEmitter::ValueToKind(Value v)
{
    if (v & VAL_STRING) return AsmKind::Str;
    if (v & VAL_BOOL)   return AsmKind::Bool;
    if (v & VAL_FLOAT)  return AsmKind::Float;
    if (v & VAL_INT)    return AsmKind::Int;
    return AsmKind::Int; // VAL_NONE / VAL_ANY with nothing else to go on
}

const AsmVarInfo *AsmEmitter::LookupVar(const std::string &name) const
{
    auto lit = CurrentLocals.find(name);
    if (lit != CurrentLocals.end())
        return &lit->second;

    auto git = Globals.find(name);
    if (git != Globals.end())
        return &git->second;

    return nullptr;
}

AsmKind AsmEmitter::ResolveKind(const Expr &e) const
{
    switch (e.Kind) {
        case AstNodeKind::LiteralExpr:
            return ValueToKind(e.Type);

        case AstNodeKind::VariableExpr: {
            const auto &v = static_cast<const VariableExpr&>(e);
            if (const AsmVarInfo *info = LookupVar(v.Name))
                return info->Kind;
            return ValueToKind(v.Type);
        }

        case AstNodeKind::AssignExpr: {
            const auto &a = static_cast<const AssignExpr&>(e);
            if (const AsmVarInfo *info = LookupVar(a.Name))
                return info->Kind;
            return a.ValueExpr ? ResolveKind(*a.ValueExpr) : AsmKind::Int;
        }

        case AstNodeKind::UnaryExpr: {
            const auto &u = static_cast<const UnaryExpr&>(e);
            if (u.Op == UnaryOp::Not) return AsmKind::Bool;
            return u.Data ? ResolveKind(*u.Data) : AsmKind::Int;
        }

        case AstNodeKind::BinaryExpr: {
            const auto &b = static_cast<const BinaryExpr&>(e);
            switch (b.Op) {
                case BinaryOp::Less: case BinaryOp::Greater:
                case BinaryOp::LessEqual: case BinaryOp::GreaterEqual:
                case BinaryOp::Equal: case BinaryOp::NotEqual:
                case BinaryOp::And: case BinaryOp::Or:
                    return AsmKind::Bool;
                default:
                    break;
            }

            AsmKind l = b.Left ? ResolveKind(*b.Left) : AsmKind::Int;
            AsmKind r = b.Right ? ResolveKind(*b.Right) : AsmKind::Int;
            if (l == AsmKind::Str || r == AsmKind::Str) return AsmKind::Str;
            if (IsFloatKind(l) || IsFloatKind(r)) return AsmKind::Float;
            return AsmKind::Int;
        }

        case AstNodeKind::CallExpr: {
            const auto &c = static_cast<const CallExpr&>(e);
            if (c.BuiltinKind != Builtin::None) {
                switch (c.BuiltinKind) {
                    case Builtin::Sqrt: case Builtin::Sin: case Builtin::Cos:
                    case Builtin::Tan:  case Builtin::Atan:
                        return AsmKind::Float;

                    case Builtin::Round: case Builtin::Floor: case Builtin::Ceil:
                    case Builtin::RandomRange: case Builtin::Length:
                        return AsmKind::Int;

                    case Builtin::Max: case Builtin::Min: case Builtin::Abs: {
                        AsmKind k = AsmKind::Int;
                        for (auto &arg : c.Args) {
                            if (arg && IsFloatKind(ResolveKind(*arg)))
                                k = AsmKind::Float;
                        }
                        return k;
                    }

                    case Builtin::CharAt: case Builtin::Join:
                        return AsmKind::Str;

                    case Builtin::Contains:
                        return AsmKind::Bool;

                    default:
                        return AsmKind::Int;
                }
            }

            auto it = Functions_.find(c.Function);
            if (it != Functions_.end())
                return it->second.ReturnKind == AsmKind::Void ? AsmKind::Int : it->second.ReturnKind;
            return AsmKind::Int;
        }

        default:
            return AsmKind::Int;
    }
}

// Labels / interned strings / externs
std::string AsmEmitter::NewLabel(const std::string &prefix)
{
    return "." + prefix + "_" + std::to_string(LabelCounter++);
}

std::string AsmEmitter::InternString(const std::string &s)
{
    auto it = StringLiteralIndex.find(s);
    if (it != StringLiteralIndex.end())
        return it->second;

    std::string label = "pvb_str_" + std::to_string(StringLiteralCounter++);
    StringLiterals.push_back({ label, s });
    StringLiteralIndex.emplace(s, label);
    return label;
}

void AsmEmitter::RequireExtern(const std::string &name)
{
    Externs.insert(name);
}

void AsmEmitter::RequireRodata(const std::string &name)
{
    RequiredRodata.insert(name);
}

// Stack bookkeeping
void AsmEmitter::EmitPush(const char *reg)
{
    Out() << "    push " << reg << "\n";
    PendingSpillBytes += 8;
}

void AsmEmitter::EmitPop(const char *reg)
{
    Out() << "    pop " << reg << "\n";
    PendingSpillBytes -= 8;
}

void AsmEmitter::EmitDropSlots(int count)
{
    if (count <= 0) return;
    Out() << "    add rsp, " << (8 * count) << "\n";
    PendingSpillBytes -= 8 * count;
}

void AsmEmitter::EmitSpill(AsmKind kind)
{
    if (IsFloatKind(kind)) {
        Out() << "    sub rsp, 8\n"
                 "    movsd [rsp], xmm0\n";
        PendingSpillBytes += 8;
    } else {
        EmitPush("rax");
    }
}

void AsmEmitter::EmitUnspillInt(const char *reg)
{
    EmitPop(reg);
}

void AsmEmitter::EmitUnspillFloat(const char *xmm)
{
    Out() << "    movsd " << xmm << ", [rsp]\n"
             "    add rsp, 8\n";
    PendingSpillBytes -= 8;
}

void AsmEmitter::EmitCall(const std::string &label, bool ensureExtern)
{
    if (ensureExtern)
        RequireExtern(label);

    bool pad = (PendingSpillBytes % 16) != 0;
    if (pad)
        Out() << "    sub rsp, 8\n";
    Out() << "    call " << label << "\n";
    if (pad)
        Out() << "    add rsp, 8\n";
}

// Value coercion (in place, RAX <-> XMM0)
void AsmEmitter::EmitCoerce(AsmKind from, AsmKind to)
{
    if (from == to) return;

    if (IsIntDomain(from) && to == AsmKind::Float) {
        Out() << "    cvtsi2sd xmm0, rax\n";
    } else if (from == AsmKind::Float && IsIntDomain(to)) {
        Out() << "    cvttsd2si rax, xmm0\n";
    }
}

// Variable storage
std::string AsmEmitter::OperandOf(const AsmVarInfo &info) const
{
    if (info.IsGlobal)
        return "[rel " + info.Label + "]";

    std::string sign = info.Offset < 0 ? "-" : "+";
    return "[rbp" + sign + std::to_string(std::abs(info.Offset)) + "]";
}

void AsmEmitter::EmitLoadVar(const AsmVarInfo &info)
{
    std::string operand = OperandOf(info);
    if (IsFloatKind(info.Kind))
        Out() << "    movsd xmm0, " << operand << "\n";
    else
        Out() << "    mov rax, " << operand << "\n";
}

void AsmEmitter::EmitStoreVar(const AsmVarInfo &info, AsmKind valueKind)
{
    EmitCoerce(valueKind, info.Kind);
    std::string operand = OperandOf(info);
    if (IsFloatKind(info.Kind))
        Out() << "    movsd " << operand << ", xmm0\n";
    else
        Out() << "    mov " << operand << ", rax\n";
}

// Scratch slots
std::string AsmEmitter::AllocScratch()
{
    int idx = ScratchUsed++;
    if (idx >= kMaxScratchSlots)
        idx = kMaxScratchSlots - 1; // best effort clamp for pathological nesting
    int offset = ScratchBaseOffset + 8 * (idx + 1);
    return "[rbp-" + std::to_string(offset) + "]";
}

void AsmEmitter::FreeScratch(int count)
{
    ScratchUsed -= count;
    if (ScratchUsed < 0)
        ScratchUsed = 0;
}

// Local frame construction
void AsmEmitter::CollectLocalNames(
        const Stmt *stmt,
        std::vector<std::pair<std::string, AsmKind>> &order,
        std::unordered_set<std::string> &seen) const
{
    if (!stmt) return;

    switch (stmt->Kind) {
        case AstNodeKind::BlockStmt: {
            const auto &b = static_cast<const BlockStmt&>(*stmt);
            for (auto &s : b.Statements)
                CollectLocalNames(s.get(), order, seen);
            break;
        }

        case AstNodeKind::IfStmt: {
            const auto &s = static_cast<const IfStmt&>(*stmt);
            CollectLocalNames(s.ThenBranch.get(), order, seen);
            CollectLocalNames(s.ElseBranch.get(), order, seen);
            break;
        }

        case AstNodeKind::WhileStmt: {
            const auto &s = static_cast<const WhileStmt&>(*stmt);
            CollectLocalNames(s.Body.get(), order, seen);
            break;
        }

        case AstNodeKind::ForStmt: {
            const auto &s = static_cast<const ForStmt&>(*stmt);
            CollectLocalNames(s.Init.get(), order, seen);
            CollectLocalNames(s.Body.get(), order, seen);
            break;
        }

        case AstNodeKind::DeclVarStmt: {
            const auto &d = static_cast<const DeclVarStmt&>(*stmt);
            if (d.Scope != VarScope::Local) break;
            if (seen.count(d.Name)) break;

            AsmKind kind;
            if (d.Type != VAL_ANY) {
                kind = ValueToKind(d.Type);
            } else if (d.Initializer) {
                kind = ResolveKind(*d.Initializer);
            } else {
                kind = AsmKind::Int;
            }

            seen.insert(d.Name);
            order.push_back({ d.Name, kind });
            break;
        }

        default:
            break;
    }
}

int AsmEmitter::BuildLocalFrame(const std::vector<const Stmt*> &bodyStatements, const std::vector<Param> *params)
{
    CurrentLocals.clear();
    ScratchUsed = 0;

    if (params) {
        for (size_t i = 0; i < params->size(); ++i) {
            const Param &p = (*params)[i];
            AsmVarInfo info;
            info.Kind = ValueToKind(p.Type);
            info.IsGlobal = false;
            info.Offset = static_cast<int>(16 + 8 * i);
            CurrentLocals[p.Name] = info;
        }
    }

    std::vector<std::pair<std::string, AsmKind>> order;
    std::unordered_set<std::string> seen;
    for (const Stmt *s : bodyStatements)
        CollectLocalNames(s, order, seen);

    int offset = 0;
    for (auto &[name, kind] : order) {
        offset += 8;
        AsmVarInfo info;
        info.Kind = kind;
        info.IsGlobal = false;
        info.Offset = -offset;
        CurrentLocals[name] = info; // locals shadow same-named params
    }

    ScratchBaseOffset = offset;
    int totalBytes = offset + 8 * kMaxScratchSlots;
    FrameSize = ((totalBytes + 15) / 16) * 16;
    if (FrameSize == 0)
        FrameSize = 16;

    return FrameSize;
}

// Pre-passes
Error AsmEmitter::CollectGlobals(const Program &program)
{
    PushOut(&GlobalInit);

    for (auto &stmt : program.Statements) {
        if (stmt->Kind != AstNodeKind::DeclVarStmt)
            continue;

        auto *decl = static_cast<DeclVarStmt*>(stmt.get());
        if (decl->Scope != VarScope::Global)
            continue;

        AsmKind kind;
        if (decl->Type != VAL_ANY) {
            kind = ValueToKind(decl->Type);
        } else if (decl->Initializer) {
            kind = ResolveKind(*decl->Initializer);
        } else {
            kind = AsmKind::Int;
        }

        AsmVarInfo info;
        info.Kind = kind;
        info.IsGlobal = true;
        info.Label = "pvb_var_" + Sanitize(decl->Name);
        Globals[decl->Name] = info;

        Bss << "    " << info.Label << ": resq 1\n";

        if (decl->Initializer) {
            AsmKind valKind;
            Error err = EmitValue(*decl->Initializer, valKind);
            if (err != Error::Ok) { PopOut(); return err; }
            EmitStoreVar(info, valKind);
        }
    }

    PopOut();
    return Error::Ok;
}

void AsmEmitter::CollectFunctionSignatures(const Program &program)
{
    for (auto &stmt : program.Statements) {
        if (stmt->Kind != AstNodeKind::FunctionStmt)
            continue;

        auto *fn = static_cast<FunctionStmt*>(stmt.get());

        AsmFunctionInfo info;
        info.Label = "pvb_fn_" + Sanitize(fn->Name);
        info.ReturnKind = (fn->ReturnType == VAL_NONE) ? AsmKind::Void : ValueToKind(fn->ReturnType);
        for (auto &p : fn->Params)
            info.ParamKinds.push_back(ValueToKind(p.Type));

        Functions_[fn->Name] = info;
    }
}

// Program
Error AsmEmitter::Visit(const Program &program)
{
    CollectFunctionSignatures(program);
    TRY(CollectGlobals(program));

    std::vector<const Stmt*> mainStmts;
    std::vector<const FunctionStmt*> functions;

    for (auto &stmt : program.Statements) {
        if (stmt->Kind == AstNodeKind::FunctionStmt) {
            functions.push_back(static_cast<const FunctionStmt*>(stmt.get()));
            continue;
        }

        if (stmt->Kind == AstNodeKind::DeclVarStmt) {
            auto *decl = static_cast<const DeclVarStmt*>(stmt.get());
            if (decl->Scope == VarScope::Global)
                continue;
        }

        mainStmts.push_back(stmt.get());
    }

    for (const FunctionStmt *fn : functions)
        TRY(Emit(*fn));

    BuildLocalFrame(mainStmts, nullptr);
    int mainFrameSize = FrameSize;

    PushOut(&MainText);
    PendingSpillBytes = 0;

    for (const Stmt *s : mainStmts)
        TRY(Emit(*s));

    PopOut();

    Out() << "BITS 64\n"
             "default rel\n\n";

    static const std::pair<const char *, const char *> kRodataConsts[] = {
        { "pvb_fmt_int",      "    pvb_fmt_int:      db \"%ld\", 0\n" },
        { "pvb_fmt_int_nl",   "    pvb_fmt_int_nl:   db \"%ld\", 10, 0\n" },
        { "pvb_fmt_float",    "    pvb_fmt_float:    db \"%g\", 0\n" },
        { "pvb_fmt_float_nl", "    pvb_fmt_float_nl: db \"%g\", 10, 0\n" },
        { "pvb_fmt_str",      "    pvb_fmt_str:      db \"%s\", 0\n" },
        { "pvb_fmt_str_nl",   "    pvb_fmt_str_nl:   db \"%s\", 10, 0\n" },
        { "pvb_str_true",     "    pvb_str_true:     db \"true\", 0\n" },
        { "pvb_str_true_nl",  "    pvb_str_true_nl:  db \"true\", 10, 0\n" },
        { "pvb_str_false",    "    pvb_str_false:    db \"false\", 0\n" },
        { "pvb_str_false_nl", "    pvb_str_false_nl: db \"false\", 10, 0\n" },
        { "pvb_scan_int",     "    pvb_scan_int:     db \"%ld\", 0\n" },
        { "pvb_scan_float",   "    pvb_scan_float:   db \"%lf\", 0\n" },
        { "pvb_scan_str",     "    pvb_scan_str:     db \"%255s\", 0\n" },
        { "pvb_scan_tok",     "    pvb_scan_tok:     db \"%63s\", 0\n" },
        { "pvb_lit_true",     "    pvb_lit_true:     db \"true\", 0\n" },
        { "pvb_abs_mask",     "    pvb_abs_mask:     dq 0x7FFFFFFFFFFFFFFF\n" },
        { "pvb_sign_mask",    "    pvb_sign_mask:    dq 0x8000000000000000\n" },
    };

    std::ostringstream rodataBody;
    for (auto &[name, def] : kRodataConsts) {
        if (RequiredRodata.count(name))
            rodataBody << def;
    }
    for (auto &[label, text] : StringLiterals) {
        rodataBody << "    " << label << ": db \"";
        for (char c : text) {
            if (c == '"')       rodataBody << "\", 34, \"";
            else if (c == '\\') rodataBody << "\", 92, \"";
            else if (c == '\n') rodataBody << "\", 10, \"";
            else if (c == '\t') rodataBody << "\", 9, \"";
            else                rodataBody << c;
        }
        rodataBody << "\", 0\n";
    }

    const std::string rodataText = rodataBody.str();
    if (!rodataText.empty())
        Out() << "section .rodata\n" << rodataText << "\n";

    const bool bssHasGlobals = !Bss.str().empty();
    if (NeedsReadBuf || bssHasGlobals) {
        Out() << "section .bss\n";
        if (NeedsReadBuf)
            Out() << "    pvb_readbuf: resb 256\n";
        AppendStream(Bss);
        Out() << "\n";
    }

    Out() << "section .text\n";
    for (auto &name : { std::string("printf"), std::string("scanf"), std::string("malloc") })
        Externs.insert(name);
    std::vector<std::string> sortedExterns(Externs.begin(), Externs.end());
    std::sort(sortedExterns.begin(), sortedExterns.end());
    for (auto &e : sortedExterns)
        Out() << "extern " << e << "\n";
    Out() << "global main\n\n";

    AppendStream(FuncsText);

    Out() << "main:\n"
             "    push rbp\n"
             "    mov rbp, rsp\n"
             "    sub rsp, " << mainFrameSize << "\n";

    if (NeedsRandomSeed) {
        Out() << "    xor edi, edi\n";
        EmitCall("time", true);
        Out() << "    mov edi, eax\n";
        EmitCall("srand", true);
    }

    AppendStream(GlobalInit);
    AppendStream(MainText);

    Out() << "    xor eax, eax\n"
             "    mov rsp, rbp\n"
             "    pop rbp\n"
             "    ret\n";

    return Error::Ok;
}

// Statements
Error AsmEmitter::Visit(const FunctionStmt &fn)
{
    auto it = Functions_.find(fn.Name);
    FAIL_COND_V(it == Functions_.end(), Error::Failed);
    const AsmFunctionInfo &sig = it->second;

    std::vector<const Stmt*> bodyStmts;
    if (fn.Body) {
        for (auto &s : fn.Body->Statements)
            bodyStmts.push_back(s.get());
    }
    BuildLocalFrame(bodyStmts, &fn.Params);
    int frameSize = FrameSize;

    PushOut(&FuncsText);
    int savedPending = PendingSpillBytes;
    PendingSpillBytes = 0;

    Out() << sig.Label << ":\n"
             "    push rbp\n"
             "    mov rbp, rsp\n"
             "    sub rsp, " << frameSize << "\n"
             "    xor eax, eax\n"; // deterministic default value (no `return` statement exists in this AST)

    if (fn.Body) {
        for (auto &s : fn.Body->Statements)
            TRY(Emit(*s));
    }

    Out() << "    mov rsp, rbp\n"
             "    pop rbp\n"
             "    ret\n\n";

    PendingSpillBytes = savedPending;
    PopOut();

    return Error::Ok;
}

Error AsmEmitter::Visit(const BlockStmt &stmt)
{
    for (auto &s : stmt.Statements)
        TRY(Emit(*s));
    return Error::Ok;
}

Error AsmEmitter::Visit(const ExprStmt &stmt)
{
    AsmKind kind;
    return EmitValue(*stmt.Expression, kind);
}

Error AsmEmitter::Visit(const PrintStmt &stmt)
{
    AsmKind kind;
    TRY(EmitValue(*stmt.Data, kind));

    switch (kind) {
        case AsmKind::Bool: {
            std::string endLabel = NewLabel("print_bool_end");
            std::string falseLabel = NewLabel("print_bool_false");
            const std::string trueSym = stmt.Newline ? "pvb_str_true_nl" : "pvb_str_true";
            const std::string falseSym = stmt.Newline ? "pvb_str_false_nl" : "pvb_str_false";
            RequireRodata(trueSym);
            RequireRodata(falseSym);
            Out() << "    cmp rax, 0\n"
                     "    je " << falseLabel << "\n"
                     "    lea rdi, [rel " << trueSym << "]\n"
                     "    jmp " << endLabel << "\n"
                  << falseLabel << ":\n"
                     "    lea rdi, [rel " << falseSym << "]\n"
                  << endLabel << ":\n"
                     "    xor eax, eax\n";
            EmitCall("printf");
            break;
        }

        case AsmKind::Float:
            RequireRodata(stmt.Newline ? "pvb_fmt_float_nl" : "pvb_fmt_float");
            Out() << "    lea rdi, [rel " << (stmt.Newline ? "pvb_fmt_float_nl" : "pvb_fmt_float") << "]\n"
                     "    mov al, 1\n";
            EmitCall("printf");
            break;

        case AsmKind::Str:
            RequireRodata(stmt.Newline ? "pvb_fmt_str_nl" : "pvb_fmt_str");
            Out() << "    mov rsi, rax\n"
                     "    lea rdi, [rel " << (stmt.Newline ? "pvb_fmt_str_nl" : "pvb_fmt_str") << "]\n"
                     "    xor eax, eax\n";
            EmitCall("printf");
            break;

        case AsmKind::Int:
        default:
            RequireRodata(stmt.Newline ? "pvb_fmt_int_nl" : "pvb_fmt_int");
            Out() << "    mov rsi, rax\n"
                     "    lea rdi, [rel " << (stmt.Newline ? "pvb_fmt_int_nl" : "pvb_fmt_int") << "]\n"
                     "    xor eax, eax\n";
            EmitCall("printf");
            break;
    }

    return Error::Ok;
}

Error AsmEmitter::Visit(const ReadStmt &stmt)
{
    const std::string &name = stmt.Variable->Name;
    const AsmVarInfo *found = LookupVar(name);
    AsmVarInfo info = found ? *found : AsmVarInfo{ ValueToKind(stmt.Variable->Type), false, "", 0 };

    switch (info.Kind) {
        case AsmKind::Int:
            RequireRodata("pvb_scan_int");
            Out() << "    lea rsi, " << OperandOf(info) << "\n"
                     "    lea rdi, [rel pvb_scan_int]\n"
                     "    xor eax, eax\n";
            EmitCall("scanf");
            break;

        case AsmKind::Float:
            RequireRodata("pvb_scan_float");
            Out() << "    lea rsi, " << OperandOf(info) << "\n"
                     "    lea rdi, [rel pvb_scan_float]\n"
                     "    xor eax, eax\n";
            EmitCall("scanf");
            break;

        case AsmKind::Bool: {
            NeedsReadBuf = true;
            RequireRodata("pvb_scan_tok");
            RequireRodata("pvb_lit_true");
            Out() << "    lea rsi, [rel pvb_readbuf]\n"
                     "    lea rdi, [rel pvb_scan_tok]\n"
                     "    xor eax, eax\n";
            EmitCall("scanf");
            RequireExtern("strcmp");
            Out() << "    lea rdi, [rel pvb_readbuf]\n"
                     "    lea rsi, [rel pvb_lit_true]\n";
            EmitCall("strcmp", true);
            Out() << "    cmp eax, 0\n"
                     "    sete al\n"
                     "    movzx rax, al\n";
            EmitStoreVar(info, AsmKind::Bool);
            return Error::Ok;
        }

        case AsmKind::Str:
        default: {
            NeedsReadBuf = true;
            RequireRodata("pvb_scan_str");
            Out() << "    mov rdi, 256\n";
            EmitCall("malloc", true);
            Out() << "    mov [rel pvb_readbuf], rax\n"
                     "    mov rsi, rax\n"
                     "    lea rdi, [rel pvb_scan_str]\n"
                     "    xor eax, eax\n";
            EmitCall("scanf");
            Out() << "    mov rax, [rel pvb_readbuf]\n";
            EmitStoreVar(info, AsmKind::Str);
            return Error::Ok;
        }
    }

    return Error::Ok;
}

Error AsmEmitter::Visit(const ExitStmt &stmt)
{
    AsmKind kind;
    TRY(EmitValue(*stmt.Code, kind));
    EmitCoerce(kind, AsmKind::Int);
    Out() << "    mov edi, eax\n";
    EmitCall("exit", true);
    return Error::Ok;
}

Error AsmEmitter::Visit(const DeclVarStmt &stmt)
{
    if (stmt.Scope == VarScope::Global)
        return Error::Ok;

    const AsmVarInfo *info = LookupVar(stmt.Name);
    FAIL_COND_V_MSG(!info, Error::Failed, "Local variable '{}' was not allocated a frame slot", stmt.Name);

    if (stmt.Initializer) {
        AsmKind kind;
        TRY(EmitValue(*stmt.Initializer, kind));
        EmitStoreVar(*info, kind);
    }

    return Error::Ok;
}

Error AsmEmitter::Visit(const IfStmt &stmt)
{
    AsmKind kind;
    TRY(EmitValue(*stmt.Condition, kind));
    EmitCoerce(kind, AsmKind::Bool);

    std::string elseLabel = NewLabel("if_else");
    std::string endLabel = stmt.ElseBranch ? NewLabel("if_end") : elseLabel;

    Out() << "    cmp rax, 0\n"
             "    je " << elseLabel << "\n";
    TRY(Emit(*stmt.ThenBranch));

    if (stmt.ElseBranch) {
        Out() << "    jmp " << endLabel << "\n"
              << elseLabel << ":\n";
        TRY(Emit(*stmt.ElseBranch));
        Out() << endLabel << ":\n";
    } else {
        Out() << elseLabel << ":\n";
    }

    return Error::Ok;
}

Error AsmEmitter::Visit(const WhileStmt &stmt)
{
    std::string startLabel = NewLabel("while_start");
    std::string endLabel = NewLabel("while_end");

    Out() << startLabel << ":\n";
    AsmKind kind;
    TRY(EmitValue(*stmt.Condition, kind));
    EmitCoerce(kind, AsmKind::Bool);
    Out() << "    cmp rax, 0\n"
             "    je " << endLabel << "\n";

    LoopStack.push_back({ startLabel, endLabel });
    TRY(Emit(*stmt.Body));
    LoopStack.pop_back();

    Out() << "    jmp " << startLabel << "\n"
          << endLabel << ":\n";

    return Error::Ok;
}

Error AsmEmitter::Visit(const ForStmt &stmt)
{
    if (stmt.Init)
        TRY(Emit(*stmt.Init));

    std::string condLabel = NewLabel("for_cond");
    std::string updateLabel = NewLabel("for_update");
    std::string endLabel = NewLabel("for_end");

    Out() << condLabel << ":\n";
    if (stmt.Condition) {
        AsmKind kind;
        TRY(EmitValue(*stmt.Condition, kind));
        EmitCoerce(kind, AsmKind::Bool);
        Out() << "    cmp rax, 0\n"
                 "    je " << endLabel << "\n";
    }

    LoopStack.push_back({ updateLabel, endLabel });
    if (stmt.Body)
        TRY(Emit(*stmt.Body));
    LoopStack.pop_back();

    Out() << updateLabel << ":\n";
    if (stmt.Update) {
        AsmKind kind;
        TRY(EmitValue(*stmt.Update, kind));
    }
    Out() << "    jmp " << condLabel << "\n"
          << endLabel << ":\n";

    return Error::Ok;
}

Error AsmEmitter::Visit(const LoopStmt &stmt)
{
    FAIL_COND_V_MSG(LoopStack.empty(), Error::Failed, "break/continue used outside of a loop");

    switch (stmt.LoopKind) {
        case LoopStmtKind::Continue:
            Out() << "    jmp " << LoopStack.back().ContinueLabel << "\n";
            break;
        case LoopStmtKind::Break:
            Out() << "    jmp " << LoopStack.back().BreakLabel << "\n";
            break;
    }

    return Error::Ok;
}

// Expressions
Error AsmEmitter::EmitValue(const Expr &e, AsmKind &outKind)
{
    TRY(Emit(e));
    outKind = LastExprKind;
    return Error::Ok;
}

Error AsmEmitter::Visit(const LiteralExpr &expr)
{
    std::visit([this](auto &&value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
            Out() << "    mov rax, " << (value ? 1 : 0) << "\n";
            LastExprKind = AsmKind::Bool;
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::string label = InternString(value);
            Out() << "    lea rax, [rel " << label << "]\n";
            LastExprKind = AsmKind::Str;
        } else if constexpr (std::is_same_v<T, float>) {
            std::string label = "pvb_flt_" + std::to_string(LabelCounter++);
            RoData << "    " << label << ": dq " << static_cast<double>(value) << "\n";
            Out() << "    movsd xmm0, [rel " << label << "]\n";
            LastExprKind = AsmKind::Float;
        } else { // int
            Out() << "    mov rax, " << value << "\n";
            LastExprKind = AsmKind::Int;
        }
    }, expr.Data);

    return Error::Ok;
}

Error AsmEmitter::Visit(const VariableExpr &expr)
{
    const AsmVarInfo *info = LookupVar(expr.Name);
    FAIL_COND_V_MSG(!info, Error::Failed, "Unknown variable '{}'", expr.Name);

    EmitLoadVar(*info);
    LastExprKind = info->Kind;
    return Error::Ok;
}

Error AsmEmitter::Visit(const AssignExpr &expr)
{
    const AsmVarInfo *info = LookupVar(expr.Name);
    FAIL_COND_V_MSG(!info, Error::Failed, "Unknown variable '{}'", expr.Name);

    AsmKind kind;
    TRY(EmitValue(*expr.ValueExpr, kind));
    EmitStoreVar(*info, kind);

    LastExprKind = info->Kind;
    return Error::Ok;
}

Error AsmEmitter::Visit(const UnaryExpr &expr)
{
    AsmKind kind;
    TRY(EmitValue(*expr.Data, kind));

    if (expr.Op == UnaryOp::Not) {
        EmitCoerce(kind, AsmKind::Bool);
        Out() << "    xor rax, 1\n";
        LastExprKind = AsmKind::Bool;
        return Error::Ok;
    }

    // Negate
    if (IsFloatKind(kind)) {
        RequireRodata("pvb_sign_mask");
        Out() << "    movq xmm1, [rel pvb_sign_mask]\n"
                 "    xorpd xmm0, xmm1\n";
    } else {
        Out() << "    neg rax\n";
    }
    LastExprKind = kind;
    return Error::Ok;
}

Error AsmEmitter::Visit(const BinaryExpr &expr)
{
    // Logical operators short-circuit and never touch the FP domain.
    if (expr.Op == BinaryOp::And || expr.Op == BinaryOp::Or) {
        AsmKind lk;
        TRY(EmitValue(*expr.Left, lk));
        EmitCoerce(lk, AsmKind::Bool);

        std::string shortLabel = NewLabel(expr.Op == BinaryOp::And ? "and_false" : "or_true");
        std::string endLabel = NewLabel("logic_end");

        Out() << "    cmp rax, 0\n";
        if (expr.Op == BinaryOp::And)
            Out() << "    je " << shortLabel << "\n";
        else
            Out() << "    jne " << shortLabel << "\n";

        AsmKind rk;
        TRY(EmitValue(*expr.Right, rk));
        EmitCoerce(rk, AsmKind::Bool);
        Out() << "    jmp " << endLabel << "\n"
              << shortLabel << ":\n"
                 "    mov rax, " << (expr.Op == BinaryOp::And ? 0 : 1) << "\n"
              << endLabel << ":\n";

        LastExprKind = AsmKind::Bool;
        return Error::Ok;
    }

    AsmKind lk = ResolveKind(*expr.Left);
    AsmKind rk = ResolveKind(*expr.Right);
    bool isComparison =
        expr.Op == BinaryOp::Less || expr.Op == BinaryOp::Greater ||
        expr.Op == BinaryOp::LessEqual || expr.Op == BinaryOp::GreaterEqual ||
        expr.Op == BinaryOp::Equal || expr.Op == BinaryOp::NotEqual;

    // String comparison via strcmp.
    if (isComparison && (lk == AsmKind::Str || rk == AsmKind::Str)) {
        AsmKind actualLk;
        TRY(EmitValue(*expr.Left, actualLk));
        EmitSpill(AsmKind::Int);
        AsmKind actualRk;
        TRY(EmitValue(*expr.Right, actualRk));
        Out() << "    mov rsi, rax\n";
        EmitUnspillInt("rdi");
        EmitCall("strcmp", true);
        Out() << "    cmp eax, 0\n";
        switch (expr.Op) {
            case BinaryOp::Less:         Out() << "    setl al\n"; break;
            case BinaryOp::Greater:      Out() << "    setg al\n"; break;
            case BinaryOp::LessEqual:    Out() << "    setle al\n"; break;
            case BinaryOp::GreaterEqual: Out() << "    setge al\n"; break;
            case BinaryOp::Equal:        Out() << "    sete al\n"; break;
            case BinaryOp::NotEqual:     Out() << "    setne al\n"; break;
            default: break;
        }
        Out() << "    movzx rax, al\n";
        LastExprKind = AsmKind::Bool;
        return Error::Ok;
    }

    bool useFloat = IsFloatKind(lk) || IsFloatKind(rk);

    // Left operand
    AsmKind actualLk;
    TRY(EmitValue(*expr.Left, actualLk));
    EmitSpill(actualLk);

    // Right operand
    AsmKind actualRk;
    TRY(EmitValue(*expr.Right, actualRk));

    if (useFloat) {
        EmitCoerce(actualRk, AsmKind::Float); // right -> xmm0
        if (IsFloatKind(actualLk)) {
            EmitUnspillFloat("xmm1");
        } else {
            EmitUnspillInt("rcx");
            Out() << "    cvtsi2sd xmm1, rcx\n";
        }
        // xmm1 = left, xmm0 = right

        if (isComparison) {
            Out() << "    ucomisd xmm1, xmm0\n";
            switch (expr.Op) {
                case BinaryOp::Less:         Out() << "    setb al\n"; break;
                case BinaryOp::Greater:      Out() << "    seta al\n"; break;
                case BinaryOp::LessEqual:    Out() << "    setbe al\n"; break;
                case BinaryOp::GreaterEqual: Out() << "    setae al\n"; break;
                case BinaryOp::Equal:        Out() << "    sete al\n"; break;
                case BinaryOp::NotEqual:     Out() << "    setne al\n"; break;
                default: break;
            }
            Out() << "    movzx rax, al\n";
            LastExprKind = AsmKind::Bool;
        } else {
            switch (expr.Op) {
                case BinaryOp::Add: Out() << "    addsd xmm1, xmm0\n"; break;
                case BinaryOp::Sub: Out() << "    subsd xmm1, xmm0\n"; break;
                case BinaryOp::Mul: Out() << "    mulsd xmm1, xmm0\n"; break;
                case BinaryOp::Div: Out() << "    divsd xmm1, xmm0\n"; break;
                default: break;
            }
            Out() << "    movsd xmm0, xmm1\n";
            LastExprKind = AsmKind::Float;
        }
        return Error::Ok;
    }

    // Integer / bool domain
    EmitCoerce(actualRk, AsmKind::Int);
    Out() << "    mov r8, rax\n"; // save right
    EmitUnspillInt("rcx");        // rcx = left
    EmitCoerce(actualLk, AsmKind::Int);

    if (isComparison) {
        Out() << "    cmp rcx, r8\n";
        switch (expr.Op) {
            case BinaryOp::Less:         Out() << "    setl al\n"; break;
            case BinaryOp::Greater:      Out() << "    setg al\n"; break;
            case BinaryOp::LessEqual:    Out() << "    setle al\n"; break;
            case BinaryOp::GreaterEqual: Out() << "    setge al\n"; break;
            case BinaryOp::Equal:        Out() << "    sete al\n"; break;
            case BinaryOp::NotEqual:     Out() << "    setne al\n"; break;
            default: break;
        }
        Out() << "    movzx rax, al\n";
        LastExprKind = AsmKind::Bool;
        return Error::Ok;
    }

    switch (expr.Op) {
        case BinaryOp::Add:
            Out() << "    mov rax, rcx\n"
                     "    add rax, r8\n";
            break;
        case BinaryOp::Sub:
            Out() << "    mov rax, rcx\n"
                     "    sub rax, r8\n";
            break;
        case BinaryOp::Mul:
            Out() << "    mov rax, rcx\n"
                     "    imul rax, r8\n";
            break;
        case BinaryOp::Div:
            Out() << "    mov rax, rcx\n"
                     "    cqo\n"
                     "    idiv r8\n";
            break;
        case BinaryOp::Mod:
            Out() << "    mov rax, rcx\n"
                     "    cqo\n"
                     "    idiv r8\n"
                     "    mov rax, rdx\n";
            break;
        default:
            break;
    }

    LastExprKind = AsmKind::Int;
    return Error::Ok;
}

Error AsmEmitter::Visit(const CallExpr &expr)
{
    if (expr.BuiltinKind != Builtin::None) {
        switch (expr.BuiltinKind) {
            case Builtin::Sqrt: case Builtin::Sin: case Builtin::Cos:
            case Builtin::Tan:  case Builtin::Atan: {
                AsmKind k;
                TRY(EmitValue(*expr.Args[0], k));
                EmitCoerce(k, AsmKind::Float);
                if (expr.BuiltinKind == Builtin::Sqrt) {
                    Out() << "    sqrtsd xmm0, xmm0\n";
                } else {
                    const char *name =
                        expr.BuiltinKind == Builtin::Sin ? "sin" :
                        expr.BuiltinKind == Builtin::Cos ? "cos" :
                        expr.BuiltinKind == Builtin::Tan ? "tan" : "atan";
                    EmitCall(name, true);
                }
                LastExprKind = AsmKind::Float;
                return Error::Ok;
            }

            case Builtin::Max:
            case Builtin::Min: {
                AsmKind lk, rk;
                TRY(EmitValue(*expr.Args[0], lk));
                EmitSpill(lk);
                TRY(EmitValue(*expr.Args[1], rk));

                bool useFloat = IsFloatKind(lk) || IsFloatKind(rk);
                if (useFloat) {
                    EmitCoerce(rk, AsmKind::Float);
                    if (IsFloatKind(lk)) {
                        EmitUnspillFloat("xmm1");
                    } else {
                        EmitUnspillInt("rcx");
                        Out() << "    cvtsi2sd xmm1, rcx\n";
                    }
                    if (expr.BuiltinKind == Builtin::Max)
                        Out() << "    maxsd xmm1, xmm0\n";
                    else
                        Out() << "    minsd xmm1, xmm0\n";
                    Out() << "    movsd xmm0, xmm1\n";
                    LastExprKind = AsmKind::Float;
                } else {
                    EmitCoerce(rk, AsmKind::Int);
                    Out() << "    mov r8, rax\n";
                    EmitUnspillInt("rax");
                    Out() << "    cmp rax, r8\n";
                    if (expr.BuiltinKind == Builtin::Max)
                        Out() << "    cmovl rax, r8\n";
                    else
                        Out() << "    cmovg rax, r8\n";
                    LastExprKind = AsmKind::Int;
                }
                return Error::Ok;
            }

            case Builtin::Round: {
                AsmKind k;
                TRY(EmitValue(*expr.Args[0], k));
                EmitCoerce(k, AsmKind::Float);
                Out() << "    cvtsd2si rax, xmm0\n";
                LastExprKind = AsmKind::Int;
                return Error::Ok;
            }

            case Builtin::Floor:
            case Builtin::Ceil: {
                AsmKind k;
                TRY(EmitValue(*expr.Args[0], k));
                EmitCoerce(k, AsmKind::Float);
                int mode = (expr.BuiltinKind == Builtin::Floor) ? 1 : 2;
                Out() << "    roundsd xmm0, xmm0, " << mode << "\n"
                         "    cvttsd2si rax, xmm0\n";
                LastExprKind = AsmKind::Int;
                return Error::Ok;
            }

            case Builtin::Abs: {
                AsmKind k;
                TRY(EmitValue(*expr.Args[0], k));
                if (IsFloatKind(k)) {
                    RequireRodata("pvb_abs_mask");
                    Out() << "    movq xmm1, [rel pvb_abs_mask]\n"
                             "    andpd xmm0, xmm1\n";
                    LastExprKind = AsmKind::Float;
                } else {
                    std::string doneLabel = NewLabel("abs_done");
                    Out() << "    cmp rax, 0\n"
                             "    jge " << doneLabel << "\n"
                             "    neg rax\n"
                          << doneLabel << ":\n";
                    LastExprKind = AsmKind::Int;
                }
                return Error::Ok;
            }

            case Builtin::RandomRange: {
                // MarkNeedsRandomSeed();
                AsmKind mk, xk;
                TRY(EmitValue(*expr.Args[0], mk));
                EmitCoerce(mk, AsmKind::Int);
                EmitSpill(AsmKind::Int); // save min
                TRY(EmitValue(*expr.Args[1], xk));
                EmitCoerce(xk, AsmKind::Int);
                EmitUnspillInt("rcx"); // rcx = min

                Out() << "    sub rax, rcx\n"
                         "    add rax, 1\n"; // rax = range
                EmitSpill(AsmKind::Int); // save range
                EmitPush("rcx");         // save min

                EmitCall("rand", true);

                EmitPop("rcx");          // rcx = min
                EmitUnspillInt("r8");    // r8 = range
                Out() << "    xor rdx, rdx\n"
                         "    div r8\n"
                         "    add rdx, rcx\n"
                         "    mov rax, rdx\n";
                LastExprKind = AsmKind::Int;
                return Error::Ok;
            }

            case Builtin::Length: {
                AsmKind k;
                TRY(EmitValue(*expr.Args[0], k));
                Out() << "    mov rdi, rax\n";
                EmitCall("strlen", true);
                LastExprKind = AsmKind::Int;
                return Error::Ok;
            }

            case Builtin::CharAt: {
                AsmKind sk, ik;
                TRY(EmitValue(*expr.Args[0], sk));
                EmitSpill(AsmKind::Int);
                TRY(EmitValue(*expr.Args[1], ik));
                EmitCoerce(ik, AsmKind::Int);
                EmitUnspillInt("rcx"); // rcx = string ptr
                Out() << "    add rcx, rax\n"
                         "    movzx eax, byte [rcx]\n";
                EmitSpill(AsmKind::Int); // save the char
                Out() << "    mov rdi, 2\n";
                EmitCall("malloc", true);
                EmitUnspillInt("rcx");
                Out() << "    mov [rax], cl\n"
                         "    mov byte [rax+1], 0\n";
                LastExprKind = AsmKind::Str;
                return Error::Ok;
            }

            case Builtin::Join: {
                std::string sA = AllocScratch();
                std::string sB = AllocScratch();
                std::string sLenA = AllocScratch();
                std::string sBuf = AllocScratch();

                AsmKind ak, bk;
                TRY(EmitValue(*expr.Args[0], ak));
                Out() << "    mov " << sA << ", rax\n";
                TRY(EmitValue(*expr.Args[1], bk));
                Out() << "    mov " << sB << ", rax\n";

                Out() << "    mov rdi, " << sA << "\n";
                EmitCall("strlen", true);
                Out() << "    mov " << sLenA << ", rax\n";

                Out() << "    mov rdi, " << sB << "\n";
                EmitCall("strlen", true);
                Out() << "    add rax, " << sLenA << "\n"
                         "    add rax, 1\n"
                         "    mov rdi, rax\n";
                EmitCall("malloc", true);
                Out() << "    mov " << sBuf << ", rax\n";

                Out() << "    mov rdi, " << sBuf << "\n"
                         "    mov rsi, " << sA << "\n";
                EmitCall("strcpy", true);
                Out() << "    mov rdi, " << sBuf << "\n"
                         "    mov rsi, " << sB << "\n";
                EmitCall("strcat", true);
                Out() << "    mov rax, " << sBuf << "\n";

                FreeScratch(4);
                LastExprKind = AsmKind::Str;
                return Error::Ok;
            }

            case Builtin::Contains: {
                std::string sHay = AllocScratch();
                std::string sNeedle = AllocScratch();

                AsmKind hk, nk;
                TRY(EmitValue(*expr.Args[0], hk));
                Out() << "    mov " << sHay << ", rax\n";
                TRY(EmitValue(*expr.Args[1], nk));
                Out() << "    mov " << sNeedle << ", rax\n";

                Out() << "    mov rdi, " << sHay << "\n"
                         "    mov rsi, " << sNeedle << "\n";
                EmitCall("strstr", true);
                Out() << "    cmp rax, 0\n"
                         "    setne al\n"
                         "    movzx rax, al\n";

                FreeScratch(2);
                LastExprKind = AsmKind::Bool;
                return Error::Ok;
            }

            case Builtin::None:
            default:
                return Error::Failed;
        }
    }

    // User-defined (or external) function call.
    auto it = Functions_.find(expr.Function);
    bool known = it != Functions_.end();

    int n = static_cast<int>(expr.Args.size());
    for (int i = n - 1; i >= 0; --i) {
        AsmKind argKind;
        TRY(EmitValue(*expr.Args[i], argKind));
        if (known && i < static_cast<int>(it->second.ParamKinds.size()))
            EmitCoerce(argKind, it->second.ParamKinds[i]);
        EmitSpill(known && i < static_cast<int>(it->second.ParamKinds.size()) ? it->second.ParamKinds[i] : argKind);
    }

    EmitCall(known ? it->second.Label : expr.Function);
    EmitDropSlots(n);

    if (known && it->second.ReturnKind == AsmKind::Float)
        Out() << "    movq xmm0, rax\n";

    LastExprKind = known ? (it->second.ReturnKind == AsmKind::Void ? AsmKind::Int : it->second.ReturnKind) : AsmKind::Int;
    return Error::Ok;
}

static std::string Sanitize(const std::string &name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
            out += c;
        else
            out += '_';
    }
    if (out.empty())
        out = "_";
    return out;
}

static bool IsFloatKind(AsmKind k)
{
    return k == AsmKind::Float;
}

static bool IsIntDomain(AsmKind k)
{
    return k == AsmKind::Int || k == AsmKind::Bool;
}
