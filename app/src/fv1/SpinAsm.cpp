#include "fv1/SpinAsm.h"
#include "fv1/Fv1Opcodes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

namespace s42::fv1 {

namespace
{
constexpr int kDelaySize = 32767; // asfv1 DELAYSIZE (not 32768 — matches reference)

struct Value
{
    double v = 0.0;
    bool isInt = true;

    long long asLL() const noexcept { return (long long) std::llround(v); }
};

// Python-style round-half-even, used for every real -> fixed-point field.
long long bankersRound(double x) noexcept
{
    const double fl = std::floor(x);
    const double frac = x - fl;
    if (frac > 0.5)
        return (long long) fl + 1;
    if (frac < 0.5)
        return (long long) fl;
    const auto below = (long long) fl;
    return (below % 2 == 0) ? below : below + 1;
}

struct Token
{
    enum Type { Ident, Number, Op, ArgSep, TargetDef, End } type = End;
    std::string text;   // uppercased identifier / operator text
    Value num;
    int line = 0;
};

class Assembler
{
public:
    explicit Assembler(std::string_view source) : src_(source) {}

    AsmResult run()
    {
        installPredefined();
        tokenizeAll();
        parseAll();
        resolveSkips();
        emit();
        return std::move(result_);
    }

private:
    // ---------------- diagnostics ----------------
    void error(const std::string& msg)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "line %d: ", curLine_);
        result_.errors.push_back(buf + msg);
    }
    void warn(const std::string& msg)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "line %d: ", curLine_);
        result_.warnings.push_back(buf + msg);
    }

    // ---------------- symbols ----------------
    void installPredefined()
    {
        static constexpr std::pair<const char*, int> kSyms[] = {
            { "SIN0_RATE", 0x00 }, { "SIN0_RANGE", 0x01 }, { "SIN1_RATE", 0x02 },
            { "SIN1_RANGE", 0x03 }, { "RMP0_RATE", 0x04 }, { "RMP0_RANGE", 0x05 },
            { "RMP1_RATE", 0x06 }, { "RMP1_RANGE", 0x07 }, { "POT0", 0x10 },
            { "POT1", 0x11 }, { "POT2", 0x12 }, { "ADCL", 0x14 }, { "ADCR", 0x15 },
            { "DACL", 0x16 }, { "DACR", 0x17 }, { "ADDR_PTR", 0x18 },
            { "SIN0", 0x00 }, { "SIN1", 0x01 }, { "RMP0", 0x02 }, { "RMP1", 0x03 },
            { "RDA", 0x00 }, { "SOF", 0x02 }, { "RDAL", 0x03 },
            { "SIN", 0x00 }, { "COS", 0x01 }, { "REG", 0x02 }, { "COMPC", 0x04 },
            { "COMPA", 0x08 }, { "RPTR2", 0x10 }, { "NA", 0x20 },
            { "RUN", 0x10 }, { "ZRC", 0x08 }, { "ZRO", 0x04 }, { "GEZ", 0x02 },
            { "NEG", 0x01 },
        };
        for (const auto& [name, val] : kSyms)
            symbols_[name] = { (double) val, true };
        for (int r = 0; r < 32; ++r)
            symbols_["REG" + std::to_string(r)] = { (double) (0x20 + r), true };
    }

    static bool isMnemonic(const std::string& s)
    {
        static const char* kMn[] = { "RDA", "RMPA", "WRA", "WRAP", "RDAX", "RDFX",
                                     "LDAX", "WRAX", "WRHX", "WRLX", "MAXX", "ABSA",
                                     "MULX", "LOG", "EXP", "SOF", "AND", "CLR", "OR",
                                     "XOR", "NOT", "SKP", "JMP", "NOP", "WLDS",
                                     "WLDR", "JAM", "CHO", "RAW" };
        return std::find_if(std::begin(kMn), std::end(kMn),
                            [&](const char* m) { return s == m; })
            != std::end(kMn);
    }

    // ---------------- tokenizer ----------------
    void tokenizeAll()
    {
        int line = 1;
        size_t i = 0;
        const size_t n = src_.size();
        auto peek = [&](size_t off = 0) -> char { return i + off < n ? src_[i + off] : '\0'; };

        while (i < n)
        {
            const char c = src_[i];
            if (c == '\n') { ++line; ++i; continue; }
            if (c == ';') { while (i < n && src_[i] != '\n') ++i; continue; }
            if (std::isspace((unsigned char) c)) { ++i; continue; }

            Token t;
            t.line = line;

            if (std::isalpha((unsigned char) c) || c == '_')
            {
                std::string s;
                while (i < n && (std::isalnum((unsigned char) peek()) || peek() == '_'))
                    s += (char) std::toupper((unsigned char) src_[i++]);
                // 0x-style hex is caught in the number branch; here handle
                // delay-address modifiers and target definitions.
                while (peek() == '#' || peek() == '^')
                    s += src_[i++];
                if (peek() == ':')
                {
                    ++i;
                    t.type = Token::TargetDef;
                    t.text = s;
                }
                else
                {
                    t.type = Token::Ident;
                    t.text = s;
                }
                tokens_.push_back(t);
                continue;
            }
            if (std::isdigit((unsigned char) c) || c == '$' || c == '%'
                || (c == '.' && std::isdigit((unsigned char) peek(1))))
            {
                t.type = Token::Number;
                t.num = lexNumber(i, line);
                tokens_.push_back(t);
                continue;
            }
            if (c == ',') { t.type = Token::ArgSep; ++i; tokens_.push_back(t); continue; }

            // operators
            std::string op(1, c);
            ++i;
            if ((c == '<' || c == '>') && peek() == c) { op += src_[i++]; }
            t.type = Token::Op;
            t.text = op;
            tokens_.push_back(t);
        }
        Token end;
        end.type = Token::End;
        end.line = line;
        tokens_.push_back(end);
    }

    Value lexNumber(size_t& i, int line)
    {
        const size_t n = src_.size();
        curLine_ = line;
        auto peek = [&](size_t off = 0) -> char { return i + off < n ? src_[i + off] : '\0'; };
        Value out;

        if (peek() == '$' || (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')))
        {
            i += peek() == '$' ? 1 : 2;
            long long v = 0;
            bool any = false;
            while (std::isxdigit((unsigned char) peek()) || peek() == '_')
            {
                if (peek() != '_')
                {
                    v = v * 16 + (std::isdigit((unsigned char) peek())
                                      ? peek() - '0'
                                      : (std::tolower((unsigned char) peek()) - 'a' + 10));
                    any = true;
                }
                ++i;
            }
            if (! any)
                error("malformed hex literal");
            out = { (double) v, true };
            return out;
        }
        if (peek() == '%')
        {
            ++i;
            long long v = 0;
            bool any = false;
            while (peek() == '0' || peek() == '1' || peek() == '_')
            {
                if (peek() != '_')
                {
                    v = v * 2 + (peek() - '0');
                    any = true;
                }
                ++i;
            }
            if (! any)
                error("malformed binary literal");
            out = { (double) v, true };
            return out;
        }

        std::string s;
        bool isFloat = false;
        while (std::isdigit((unsigned char) peek()))
            s += src_[i++];
        if (peek() == '.')
        {
            isFloat = true;
            s += src_[i++];
            while (std::isdigit((unsigned char) peek()))
                s += src_[i++];
        }
        if (peek() == 'E' || peek() == 'e')
        {
            const char sign = peek(1);
            if (std::isdigit((unsigned char) sign)
                || ((sign == '+' || sign == '-') && std::isdigit((unsigned char) peek(2))))
            {
                isFloat = true;
                s += src_[i++];
                if (peek() == '+' || peek() == '-')
                    s += src_[i++];
                while (std::isdigit((unsigned char) peek()))
                    s += src_[i++];
            }
        }
        out.v = std::strtod(s.c_str(), nullptr);
        out.isInt = ! isFloat;
        return out;
    }

    // ---------------- token cursor ----------------
    const Token& cur() const { return tokens_[(size_t) pos_]; }
    void advance()
    {
        if (cur().type != Token::End)
            ++pos_;
        curLine_ = cur().line;
    }
    // ---------------- expressions ----------------
    Value expression() { return orExpr(); }

    Value orExpr()
    {
        Value a = xorExpr();
        while (cur().type == Token::Op && cur().text == "|")
        {
            advance();
            Value b = xorExpr();
            a = { (double) (a.asLL() | b.asLL()), a.isInt && b.isInt };
        }
        return a;
    }
    Value xorExpr()
    {
        Value a = andExpr();
        while (cur().type == Token::Op && cur().text == "^")
        {
            advance();
            Value b = andExpr();
            a = { (double) (a.asLL() ^ b.asLL()), a.isInt && b.isInt };
        }
        return a;
    }
    Value andExpr()
    {
        Value a = shiftExpr();
        while (cur().type == Token::Op && cur().text == "&")
        {
            advance();
            Value b = shiftExpr();
            a = { (double) (a.asLL() & b.asLL()), a.isInt && b.isInt };
        }
        return a;
    }
    Value shiftExpr()
    {
        Value a = addExpr();
        while (cur().type == Token::Op && (cur().text == "<<" || cur().text == ">>"))
        {
            const bool left = cur().text == "<<";
            advance();
            Value b = addExpr();
            const long long r = left ? (a.asLL() << b.asLL()) : (a.asLL() >> b.asLL());
            a = { (double) r, true };
        }
        return a;
    }
    Value addExpr()
    {
        Value a = mulExpr();
        while (cur().type == Token::Op && (cur().text == "+" || cur().text == "-"))
        {
            const bool add = cur().text == "+";
            advance();
            Value b = mulExpr();
            a = { add ? a.v + b.v : a.v - b.v, a.isInt && b.isInt };
        }
        return a;
    }
    Value mulExpr()
    {
        Value a = unaryExpr();
        while (cur().type == Token::Op && (cur().text == "*" || cur().text == "/"))
        {
            const bool mul = cur().text == "*";
            advance();
            Value b = unaryExpr();
            if (mul)
                a = { a.v * b.v, a.isInt && b.isInt };
            else
            {
                if (b.v == 0.0)
                {
                    error("division by zero");
                    a = { 0.0, true };
                }
                else
                {
                    const double q = a.v / b.v;
                    a = { q, a.isInt && b.isInt && q == std::floor(q) };
                }
            }
        }
        return a;
    }
    Value unaryExpr()
    {
        if (cur().type == Token::Op && cur().text == "-")
        {
            advance();
            Value a = unaryExpr();
            return { -a.v, a.isInt };
        }
        if (cur().type == Token::Op && cur().text == "+")
        {
            advance();
            return unaryExpr();
        }
        if (cur().type == Token::Op && cur().text == "~")
        {
            advance();
            Value a = unaryExpr();
            return { (double) (~a.asLL()), true };
        }
        return atom();
    }
    Value atom()
    {
        if (cur().type == Token::Op && cur().text == "(")
        {
            advance();
            Value v = expression();
            if (cur().type == Token::Op && cur().text == ")")
                advance();
            else
                error("expected ')'");
            return v;
        }
        if (cur().type == Token::Number)
        {
            Value v = cur().num;
            advance();
            return v;
        }
        if (cur().type == Token::Ident)
        {
            const auto it = symbols_.find(cur().text);
            if (it == symbols_.end())
            {
                error("undefined label " + cur().text);
                advance();
                return { 0.0, true };
            }
            Value v = it->second;
            advance();
            return v;
        }
        error("unexpected token in expression");
        return { 0.0, true };
    }

    void expectArgSep(const char* mnemonic)
    {
        if (cur().type == Token::ArgSep)
            advance();
        else
            error(std::string("missing operand for ") + mnemonic);
    }

    // ---------------- operand converters (asfv1-identical clamping) --------
    long long clampField(double v, double lo, double hi, double ref, const char* what)
    {
        if (v < lo || v > hi)
        {
            warn(std::string(what) + " clamped");
            v = std::clamp(v, lo, hi);
        }
        return bankersRound(v * ref);
    }
    int fS1_14(const Value& x) { return (int) (clampField(x.v, -2.0, 32767.0 / 16384.0, 16384.0, "S1.14") & 0xFFFF); }
    int fS1_9(const Value& x) { return (int) (clampField(x.v, -2.0, 1023.0 / 512.0, 512.0, "S1.9") & 0x7FF); }
    int fS_10(const Value& x) { return (int) (clampField(x.v, -1.0, 1023.0 / 1024.0, 1024.0, "S.10") & 0x7FF); }

    int fRegister(const char* mn)
    {
        const Value v = expression();
        if (! v.isInt || v.v < 0 || v.v > 63)
        {
            error(std::string("invalid register for ") + mn);
            return 0;
        }
        return (int) v.asLL();
    }
    int fDelay15(const char* mn) // integer preferred, real in [-1,1) scaled
    {
        const Value x = expression();
        double v = x.v;
        if (v < -1.0 || v > 32767.0 / 32768.0)
        {
            long long a = (long long) std::llround(v);
            if (a < -0x8000 || a > 0x7FFF)
            {
                warn(std::string("address clamped for ") + mn);
                a = std::clamp<long long>(a, -0x8000, 0x7FFF);
            }
            return (int) (a & 0x7FFF);
        }
        return (int) (bankersRound(v * 32768.0) & 0x7FFF);
    }
    int fAddr16(const char* mn) // CHO address: type-tracked like asfv1 s_15a
    {
        const Value x = expression();
        if (x.isInt)
        {
            long long a = x.asLL();
            if (a < 0 || a > 0xFFFF)
            {
                warn(std::string("S.15 arg clamped for ") + mn);
                a = std::clamp<long long>(a, 0, 0xFFFF);
            }
            return (int) a;
        }
        double v = x.v;
        if (v < -1.0 || v > 32767.0 / 32768.0)
        {
            warn(std::string("S.15 arg clamped for ") + mn);
            v = std::clamp(v, -1.0, 32767.0 / 32768.0);
        }
        return (int) (bankersRound(v * 32768.0) & 0xFFFF);
    }
    uint32_t fMask24()
    {
        const Value x = expression();
        if (x.isInt)
        {
            long long m = x.asLL();
            if (m < 0 || m > 0xFFFFFF)
            {
                warn("mask clamped");
                m = std::clamp<long long>(m, 0, 0xFFFFFF);
            }
            return (uint32_t) m;
        }
        return (uint32_t) (clampField(x.v, -1.0, 8388607.0 / 8388608.0, 8388608.0, "S.23")
                           & 0xFFFFFF);
    }

    // ---------------- statements ----------------
    void parseAll()
    {
        while (cur().type != Token::End)
        {
            curLine_ = cur().line;
            if (cur().type == Token::TargetDef)
            {
                const std::string name = cur().text;
                if (targets_.count(name) != 0)
                    error("duplicate target " + name);
                targets_[name] = (int) instructions_.size();
                advance();
                continue;
            }
            if (cur().type == Token::Ident)
            {
                if (cur().text == "EQU" || cur().text == "MEM")
                {
                    parseAssemblerDirective("", cur().text);
                    continue;
                }
                if (isMnemonic(cur().text))
                {
                    parseInstruction();
                    continue;
                }
                // 'name EQU value' / 'name MEM size'
                const std::string name = cur().text;
                advance();
                if (cur().type == Token::Ident && (cur().text == "EQU" || cur().text == "MEM"))
                {
                    parseAssemblerDirective(name, cur().text);
                    continue;
                }
                error("unexpected token " + name);
                continue;
            }
            error("unexpected token");
            advance();
        }
    }

    void parseAssemblerDirective(std::string name, const std::string& kind)
    {
        advance(); // consume EQU/MEM
        if (name.empty())
        {
            if (cur().type == Token::Ident)
            {
                name = cur().text;
                advance();
            }
            else
            {
                error("expected label for " + kind);
                return;
            }
        }
        while (! name.empty() && (name.back() == '#' || name.back() == '^'))
            name.pop_back();
        if (name == "RDA" || name == "SOF" || name == "RDAL")
        {
            error("reserved label " + name + " cannot be re-defined");
            return;
        }
        if (symbols_.count(name) != 0)
            warn("label " + name + " re-defined");

        const Value v = expression();
        if (kind == "EQU")
        {
            symbols_[name] = v;
            return;
        }
        // MEM: 'name' = start, 'name#' = start+N, 'name^' = start+N/2;
        // the next block starts at start+N+1 (asfv1 allocation).
        if (! v.isInt)
        {
            error("memory length for " + name + " not integer");
            return;
        }
        long long len = v.asLL();
        if (len < 0 || len > kDelaySize)
        {
            warn("memory size clamped for " + name);
            len = std::clamp<long long>(len, 0, kDelaySize);
        }
        const long long top = delayMem_ + len;
        if (delayMem_ > kDelaySize)
            error("delay memory exhausted");
        else if (top > kDelaySize)
            error("delay exhausted: requested " + std::to_string(len));
        symbols_[name] = { (double) delayMem_, true };
        symbols_[name + "#"] = { (double) top, true };
        const long long mid = delayMem_ + len / 2; // floor division, as asfv1
        symbols_[name + "^"] = { (double) mid, true };
        delayMem_ = top + 1;
    }

    struct PendingInstruction
    {
        uint32_t word = 0;
        std::string skpTarget; // non-empty: patch the N field on resolve
        uint32_t skpCond = 0;
        int line = 0;
    };

    void pushWord(uint32_t w)
    {
        PendingInstruction pi;
        pi.word = w;
        pi.line = curLine_;
        instructions_.push_back(pi);
    }

    void parseInstruction()
    {
        const std::string mn = cur().text;
        advance();
        if ((int) instructions_.size() >= Fv1Vm::kProgramWords)
        {
            error("max program length exceeded by " + mn);
            return;
        }
        const char* m = mn.c_str();

        if (mn == "AND" || mn == "OR" || mn == "XOR")
        {
            const uint32_t mask = fMask24();
            pushWord(encMask(mn == "AND" ? Op::And : mn == "OR" ? Op::Or : Op::Xor, mask));
        }
        else if (mn == "SOF" || mn == "EXP" || mn == "LOG")
        {
            const int c = fS1_14(expression());
            expectArgSep(m);
            const int d = fS_10(expression());
            pushWord(encSofLogExp(mn == "SOF" ? Op::Sof : mn == "EXP" ? Op::Exp : Op::Log, c, d));
        }
        else if (mn == "RDAX" || mn == "WRAX" || mn == "MAXX" || mn == "RDFX"
                 || mn == "WRLX" || mn == "WRHX")
        {
            const int r = fRegister(m);
            expectArgSep(m);
            const int c = fS1_14(expression());
            const Op op = mn == "RDAX" ? Op::Rdax
                        : mn == "WRAX" ? Op::Wrax
                        : mn == "MAXX" ? Op::Maxx
                        : mn == "RDFX" ? Op::Rdfx
                        : mn == "WRLX" ? Op::Wrlx
                                       : Op::Wrhx;
            pushWord(encRegOp(op, r, c));
        }
        else if (mn == "MULX")
        {
            pushWord(encMulx(fRegister(m)));
        }
        else if (mn == "LDAX")
        {
            pushWord(encRegOp(Op::Rdfx, fRegister(m), 0));
        }
        else if (mn == "ABSA")
        {
            pushWord(encRegOp(Op::Maxx, 0, 0));
        }
        else if (mn == "CLR")
        {
            pushWord(encMask(Op::And, 0));
        }
        else if (mn == "NOT")
        {
            pushWord(encMask(Op::Xor, 0xFFFFFF));
        }
        else if (mn == "NOP")
        {
            pushWord(encSkp(0, 0));
        }
        else if (mn == "SKP" || mn == "JMP")
        {
            uint32_t cond = 0;
            if (mn == "SKP")
            {
                const Value c = expression();
                if (! c.isInt || c.v < 0 || c.v > 31)
                    error("invalid condition for SKP");
                else
                    cond = (uint32_t) c.asLL();
                expectArgSep(m);
            }
            // Any identifier here is a jump target (asfv1 rule), never an
            // EQU-defined offset.
            if (cur().type == Token::Ident)
            {
                PendingInstruction pi;
                pi.word = encSkp(cond, 0);
                pi.skpTarget = cur().text;
                pi.skpCond = cond;
                pi.line = curLine_;
                instructions_.push_back(pi);
                advance();
            }
            else
            {
                const Value n = expression();
                if (! n.isInt || n.v < 0 || n.v > 63)
                {
                    error("skip offset out of range");
                    pushWord(encSkp(cond, 0));
                }
                else
                    pushWord(encSkp(cond, (int) n.asLL()));
            }
        }
        else if (mn == "RDA" || mn == "WRA" || mn == "WRAP")
        {
            const int addr = fDelay15(m);
            expectArgSep(m);
            const int c = fS1_9(expression());
            const Op op = mn == "RDA" ? Op::Rda : mn == "WRA" ? Op::Wra : Op::Wrap;
            pushWord(encDelay(op, addr, c));
        }
        else if (mn == "RMPA")
        {
            pushWord(encRmpa(fS1_9(expression())));
        }
        else if (mn == "WLDS")
        {
            const int lfo = fLfoSelect(m) & 0x1;
            expectArgSep(m);
            const Value f = expression();
            long long freq = f.asLL();
            if (! f.isInt || freq < 0 || freq > 0x1FF)
            {
                warn("sine frequency clamped");
                freq = std::clamp<long long>(freq, 0, 0x1FF);
            }
            expectArgSep(m);
            const int amp = fDelay15(m);
            pushWord(encWlds(lfo, (int) freq, amp));
        }
        else if (mn == "WLDR")
        {
            const int lfo = fLfoSelect(m) | 0x2;
            expectArgSep(m);
            const Value f = expression();
            long long freq;
            if (f.v >= -0.5 && f.v <= 32767.0 / 32768.0 && ! f.isInt)
                freq = bankersRound(f.v * 32768.0);
            else
            {
                freq = f.asLL();
                if (freq < -0x8000 || freq > 0x7FFF)
                {
                    warn("ramp frequency clamped");
                    freq = std::clamp<long long>(freq, -0x8000, 0x7FFF);
                }
            }
            expectArgSep(m);
            const Value a = expression();
            int ampCode = 0;
            const long long av = a.asLL();
            if (a.isInt && (av == 4096 || av == 2048 || av == 1024 || av == 512))
                ampCode = av == 4096 ? 0 : av == 2048 ? 1 : av == 1024 ? 2 : 3;
            else if (a.isInt && av >= 0 && av <= 3)
                ampCode = (int) av;
            else
                error("invalid ramp amplitude (use 4096/2048/1024/512)");
            pushWord(encWldr(lfo, (int) (freq & 0xFFFF), ampCode));
        }
        else if (mn == "JAM")
        {
            pushWord(encJam(fLfoSelect(m) | 0x2));
        }
        else if (mn == "CHO")
        {
            parseCho();
        }
        else if (mn == "RAW")
        {
            const Value v = expression();
            pushWord((uint32_t) (v.asLL() & 0xFFFFFFFFll));
        }
        else
        {
            error("unhandled mnemonic " + mn);
        }
    }

    int fLfoSelect(const char* mn)
    {
        const Value v = expression();
        if (! v.isInt || v.v < 0 || v.v > 3)
        {
            error(std::string("invalid LFO for ") + mn);
            return 0;
        }
        return (int) v.asLL();
    }

    void parseCho()
    {
        uint32_t type = cho::TypeRda;
        if (cur().type == Token::Ident
            && (cur().text == "RDA" || cur().text == "SOF" || cur().text == "RDAL"))
        {
            type = cur().text == "RDA" ? cho::TypeRda
                 : cur().text == "SOF" ? cho::TypeSof
                                       : cho::TypeRdal;
            advance();
        }
        else
        {
            error("invalid CHO type");
        }
        expectArgSep("CHO");
        const int lfo = fLfoSelect("CHO");
        uint32_t flags = 0b000010; // asfv1 default (REG) when omitted for RDAL
        int addr = 0;
        if (type != cho::TypeRdal)
        {
            expectArgSep("CHO");
            flags = fChoFlags(lfo);
            expectArgSep("CHO");
            addr = fAddr16("CHO");
        }
        else if (cur().type == Token::ArgSep)
        {
            advance();
            flags = fChoFlags(lfo);
        }
        pushWord(encCho(type, lfo, flags, addr));
    }

    uint32_t fChoFlags(int lfo)
    {
        const Value v = expression();
        if (! v.isInt || v.v < 0 || v.v > 0x3F)
        {
            error("invalid CHO flags");
            return 0;
        }
        auto flags = (uint32_t) v.asLL();
        const uint32_t masked = (lfo & 0x2) != 0 ? (flags & 0x3E) : (flags & 0x0F);
        if (masked != flags)
            warn((lfo & 0x2) != 0 ? "RMP flags masked for CHO" : "SIN flags masked for CHO");
        return masked;
    }

    // ---------------- fixups + emit ----------------
    void resolveSkips()
    {
        for (size_t i = 0; i < instructions_.size(); ++i)
        {
            auto& pi = instructions_[i];
            if (pi.skpTarget.empty())
                continue;
            curLine_ = pi.line;
            const auto it = targets_.find(pi.skpTarget);
            if (it == targets_.end())
            {
                error("undefined skip target " + pi.skpTarget);
                continue;
            }
            const int offset = it->second - (int) i - 1;
            if (offset < 0 || offset > 63)
            {
                error("skip target " + pi.skpTarget + " out of range");
                continue;
            }
            pi.word = encSkp(pi.skpCond, offset);
        }
    }

    void emit()
    {
        result_.instructionCount = (int) instructions_.size();
        for (int i = 0; i < Fv1Vm::kProgramWords; ++i)
            result_.words[(size_t) i] = i < result_.instructionCount
                                            ? instructions_[(size_t) i].word
                                            : encSkp(0, 0);
    }

    std::string_view src_;
    std::vector<Token> tokens_;
    int pos_ = 0;
    int curLine_ = 1;

    std::map<std::string, Value> symbols_;
    std::map<std::string, int> targets_;
    long long delayMem_ = 0;
    std::vector<PendingInstruction> instructions_;
    AsmResult result_;
};
} // namespace

AsmResult assembleSpinAsm(std::string_view source)
{
    return Assembler(source).run();
}

} // namespace s42::fv1
