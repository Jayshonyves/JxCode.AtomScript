#include "OpCommand.h"
#include <vector>
#include <stdexcept>
#include <sstream>

namespace jxcode::atomscript
{
    using namespace std;
    using namespace lexer;

    OpCommand::OpCommand() : code(OpCode::Unknow), op_token(nullptr), targets(vector<shared_ptr<Token>>()) {

    }
    OpCommand::OpCommand(const OpCode& code, const shared_ptr<Token>& optoken, const vector<shared_ptr<Token>>& targets)
        : code(code), op_token(optoken), targets(targets)
    {

    }

    static int cur_ptr;
    static std::wstring* program_name;
    static vector<shared_ptr<Token>>* tokens;

    inline static void Reset()
    {
        cur_ptr = -1;
        tokens = nullptr;
    }

    inline static bool is_next()
    {
        return (tokens != nullptr) && (cur_ptr < (int)tokens->size() - 1);
    }
    inline static shared_ptr<Token> Peek(size_t count)
    {
        if (cur_ptr + count >= tokens->size()) {
            return nullptr;
        }
        return tokens->at(cur_ptr + count);
    }
    inline static bool PeekExist(int count)
    {
        return (tokens != nullptr) && (cur_ptr + count < (int)tokens->size());
    }
    inline static shared_ptr<Token> cur_token()
    {
        return tokens->at(cur_ptr);
    }
    inline static shared_ptr<Token> NextToken(int offset = 1)
    {
        if (!is_next()) {
            return nullptr;
        }
        cur_ptr += offset;
        return tokens->at(cur_ptr);
    }
    inline static void NextLine()
    {
        while (is_next() && Peek(1)->token_type != TokenType::LF) {
            NextToken();
        }
    }
    inline static void ThrowParameterException(bool bol) {
        if (!bol) {
            throw CommandParserException(cur_token(), L"OpParseParameterException");
        }
    }
    inline static void AssertAfterLength(int min, int max)
    {
        int count = 0;

        for (int i = 1; i <= max; i++)
        {
            if (Peek(i) != nullptr && Peek(i)->token_type != TokenType::LF) {
                ++count;
            }
        }

        if (count < min || count > max) {
            throw CommandParserException(cur_token(), L"OpParseTargetRangeOut");
        }
    }
    inline static void AssertAfterLength(int count)
    {
        AssertAfterLength(count, count);
    }

    inline static bool CheckValidPeek(int i, TokenType type)
    {
        auto p = Peek(i);
        if (p == nullptr) {
            return false;
        }
        return p->token_type == type;
    }
    inline static bool CheckValidPeek(int i, const wstring& str)
    {
        auto p = Peek(i);
        if (p == nullptr) {
            return false;
        }
        return *p->value == str;
    }

    inline static void AddRange(vector<shared_ptr<Token>>* tokens, int length) {
        for (int i = 0; i < length; i++)
        {
            tokens->push_back(NextToken());
        }
    }
    inline static void AddRangeByEndType(vector<shared_ptr<Token>>* tokens, bool(*cb)(const shared_ptr<Token>& token))
    {
        shared_ptr<Token> token;
        for (int i = 1;; i++)
        {
            token = Peek(1);
            if (token == nullptr || cb(token)) {
                break;
            }
            tokens->push_back(token);
            NextToken();
        }
    }
    inline static void AddRangeToLF(vector<shared_ptr<Token>>* tokens)
    {
        AddRangeByEndType(tokens, [](const shared_ptr<Token>& token)->bool { return token->token_type == TokenType::LF; });
    }

    std::shared_ptr<std::vector<OpCommand>> ParseOpList(std::wstring* _program_name, vector<shared_ptr<Token>>* _tokens)
    {
        Reset();
        program_name = _program_name;
        tokens = _tokens;

        using namespace jxcode::lexer;

        static wstring opcode_call = L"call";
        static wstring opcode_goto = L"goto";
        static wstring opcode_if = L"if";
        static wstring opcode_set = L"set";
        static wstring opcode_clear = L"clearsub";
        static wstring opcode_del = L"del";
        static wstring opcode_jumpfile = L"toprog";
        static wstring opcode_label = L"label";

        shared_ptr<vector<OpCommand>> list = std::make_shared<vector<OpCommand>>();

        while (is_next()) {

            OpCommand cmd = OpCommand();

            shared_ptr<Token> token = Peek(1);
            cmd.op_token = token;

            if (token->token_type == TokenType::LF) {
                NextToken();
                continue;
            }
            else if (*token->value == opcode_call || token->token_type == TokenType::At) {
                // call @
                NextToken();
                cmd.code = OpCode::Call;
                AddRangeToLF(&cmd.targets);
                NextToken(); //吞换行符
            }
            else if (*token->value == opcode_goto || token->token_type == TokenType::DoubleGreaterThan) {
                // goto >>
                NextToken();
                AssertAfterLength(1, 2);
                static wstring _var = L"var";
                ThrowParameterException(
                    (
                    CheckValidPeek(1, TokenType::Ident) &&
                    CheckValidPeek(1, _var) &&
                    CheckValidPeek(2, TokenType::Ident)
                ) ||
                    CheckValidPeek(1, TokenType::Ident)
                );

                cmd.code = OpCode::Goto;

                auto var_token = NextToken();
                cmd.targets.push_back(var_token);
                //如果有var在添加一个
                if (*var_token->value == _var) {
                    cmd.targets.push_back(NextToken());
                }
                
            }
            else if (*token->value == opcode_if || token->token_type == TokenType::Question) {
                // if ?
                NextToken();
                cmd.code = OpCode::If;
                AssertAfterLength(4);
                ThrowParameterException(
                    Peek(4)->token_type == TokenType::Ident &&
                    *Peek(4)->value == L"then"
                );
                AddRange(&cmd.targets, 3);
                NextToken(); // 吞then
            }
            else if (*token->value == opcode_set || token->token_type == TokenType::Doller) {
                // set $
                NextToken();
                cmd.code = OpCode::Set;
                AddRangeToLF(&cmd.targets);
                NextToken(); //吞换行符
            }
            else if (*token->value == opcode_clear || token->token_type == TokenType::Tilde) {
                // clear ~
                NextToken();
                AssertAfterLength(1);
                ThrowParameterException(
                    (CheckValidPeek(1, TokenType::Ident))
                );
                cmd.code = OpCode::ClearSub;
                cmd.targets.push_back(NextToken());
            }
            else if (*token->value == opcode_del || token->token_type == TokenType::Division) {
                // del -
                NextToken();
                AssertAfterLength(1);
                ThrowParameterException(
                    (CheckValidPeek(1, TokenType::Ident))
                );
                cmd.code = OpCode::Del;
                cmd.targets.push_back(NextToken());
            }
            else if (*token->value == opcode_jumpfile || token->token_type == TokenType::TripleGreaterThan) {
                // jumpfile >>>
                NextToken();
                AssertAfterLength(1);
                cmd.code = OpCode::ToProg;
                cmd.targets.push_back(NextToken());
            }
            else if (*token->value == opcode_label || token->token_type == TokenType::DoubleColon) {
                // ::
                NextToken();
                AssertAfterLength(1);
                ThrowParameterException(
                    (CheckValidPeek(1, TokenType::Ident))
                );
                cmd.code = OpCode::Label;
                cmd.targets.push_back(NextToken());
            }
            else {
                throw CommandParserException(token, L"Unknow");
            }

            list->push_back(cmd);
        }
        return list;
    }
    std::wstring OpCommand::to_string() const
    {
        wstringstream ss;
        ss.width(8);
        ss << (int)code;
        for (const auto& item : targets) {
            ss.width(18);
            ss << *item->value;
        }
        return ss.str();
    }

    std::wstring CommandParserException::get_name()
    {
        return L"CommandParserException";
    }

    CommandParserException::CommandParserException(const std::shared_ptr<Token>& token, const std::wstring& message)
        : TokenException(token, message)
    {
    }
}