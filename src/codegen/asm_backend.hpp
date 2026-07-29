#pragma once

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "codegen/backend.hpp"

enum class AsmKind
{
    Int,
    Float,
    Bool,
    Str,
    Void,
};

struct AsmVarInfo
{
    AsmKind Kind = AsmKind::Int;
    bool IsGlobal = false;
    std::string Label; // valid when IsGlobal
    int Offset = 0; // valid when !IsGlobal, relative to RBP
};

struct AsmFunctionInfo
{
    std::string Label;
    AsmKind ReturnKind = AsmKind::Void;
    std::vector<AsmKind> ParamKinds;
};

class AsmEmitter : public Emitter
{
public:
    using Emitter::Emitter;

    std::string_view BinaryOperator(BinaryOp) override;
    std::string_view UnaryOperator(UnaryOp) override;

    // Visitors
    Error Visit(const Program&) override;

    Error Visit(const LiteralExpr&) override;
    Error Visit(const VariableExpr&) override;
    Error Visit(const AssignExpr&) override;
    Error Visit(const UnaryExpr&) override;
    Error Visit(const BinaryExpr&) override;
    Error Visit(const CallExpr&) override;

    Error Visit(const PrintStmt&) override;
    Error Visit(const ReadStmt&) override;
    Error Visit(const ExitStmt&) override;
    Error Visit(const ExprStmt&) override;
    Error Visit(const BlockStmt&) override;
    Error Visit(const FunctionStmt&) override;
    Error Visit(const IfStmt&) override;
    Error Visit(const WhileStmt&) override;
    Error Visit(const ForStmt&) override;
    Error Visit(const LoopStmt&) override;
    Error Visit(const DeclVarStmt&) override;

private:
    static AsmKind ValueToKind(Value v);
    AsmKind ResolveKind(const Expr &e) const;
    const AsmVarInfo *LookupVar(const std::string &name) const;

    std::unordered_map<std::string, AsmVarInfo> Globals;
    std::unordered_map<std::string, AsmFunctionInfo> Functions_;
    std::unordered_map<std::string, AsmVarInfo> CurrentLocals;

    Error CollectGlobals(const Program &program);
    void CollectFunctionSignatures(const Program &program);
    void CollectLocalNames(
            const Stmt *stmt,
            std::vector<std::pair<std::string, AsmKind>> &order,
            std::unordered_set<std::string> &seen) const;

    static constexpr int kMaxScratchSlots = 64;
    int BuildLocalFrame(const std::vector<const Stmt*> &bodyStatements, const std::vector<Param> *params);
    int FrameSize = 0;
    int ScratchBaseOffset = 0;
    int ScratchUsed = 0;
    std::string AllocScratch();
    void FreeScratch(int count);

    AsmKind LastExprKind = AsmKind::Int;
    Error EmitValue(const Expr &e, AsmKind &outKind);

    int PendingSpillBytes = 0;
    void EmitPush(const char *reg); // push a GPR, tracks alignment
    void EmitPop(const char *reg); // pop into a GPR, tracks alignment
    void EmitSpill(AsmKind kind); // push RAX or XMM0
    void EmitUnspillInt(const char *reg); // pop into a GPR
    void EmitUnspillFloat(const char *xmm); // pop into an XMM register
    void EmitDropSlots(int count); // add rsp, 8*count

    void EmitCoerce(AsmKind from, AsmKind to);
    void EmitCall(const std::string &label, bool ensureExtern = false);

    void EmitLoadVar(const AsmVarInfo &info);
    void EmitStoreVar(const AsmVarInfo &info, AsmKind valueKind);
    std::string OperandOf(const AsmVarInfo &info) const;

    std::string InternString(const std::string &s);
    std::string NewLabel(const std::string &prefix);
    int LabelCounter = 0;

    struct LoopCtx { std::string ContinueLabel; std::string BreakLabel; };
    std::vector<LoopCtx> LoopStack;

    void RequireExtern(const std::string &name);
    bool NeedsRandomSeed = false;

    void RequireRodata(const std::string &name);
    std::unordered_set<std::string> RequiredRodata;

    bool NeedsReadBuf = false;

    std::ostringstream RoData;
    std::ostringstream Bss;
    std::ostringstream FuncsText;
    std::ostringstream GlobalInit;
    std::ostringstream MainText;

    std::vector<std::pair<std::string, std::string>> StringLiterals;
    std::unordered_map<std::string, std::string> StringLiteralIndex;
    int StringLiteralCounter = 0;

    std::unordered_set<std::string> Externs;
};
