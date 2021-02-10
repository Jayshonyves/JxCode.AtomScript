#include <stdexcept>
#include "Interpreter.h"
#include "Lexer.h"
#include <regex>
#include <codecvt>
#include <sstream>

namespace jxcode::atomscript
{
    using namespace std;
    using namespace lexer;

#pragma region InterpreterException
    InterpreterException::InterpreterException(const lexer::Token& token, const std::wstring& message)
        : token_(token), message_(message) {}

    std::wstring InterpreterException::what() const
    {
        return this->token_.to_string();
    }
#pragma endregion


#pragma region Interpreter

    int32_t Interpreter::line_num() const
    {
        return this->exec_ptr_;
    }
    size_t Interpreter::opcmd_count() const
    {
        return this->commands_.size();
    }
    const wstring& Interpreter::program_name() const
    {
        return this->program_name_;
    }
    map<wstring, Variable>* Interpreter::variables()
    {
        return &this->variables_;
    }
    Interpreter::Interpreter(LoadFileCallBack _loadfile_, FuncallCallBack _funcall_)
        : exec_ptr_(-1), _loadfile_(_loadfile_), _funcall_(_funcall_)
    {
    }

    bool Interpreter::IsExistLabel(const wstring& label)
    {
        return this->labels_.find(label) != this->labels_.end();
    }

    void Interpreter::SetVar(const wstring& name, const float& num)
    {
        Variable var = this->GetVar(name);
        if (var.type == VARIABLETYPE_UNDEFINED) {
            SetVariableNumber(&var, num);
            this->variables_[name] = var;
        }
        else {
            SetVariableNumber(&this->variables_[name], num);
        }
    }

    void Interpreter::SetVar(const wstring& name, const wstring& str)
    {
        Variable var = this->GetVar(name);
        if (var.type == VARIABLETYPE_UNDEFINED) {
            this->variables_[name] = var;
        }
        int id = this->NewStrPtr(str);
        SetVariableStrPtr(&this->variables_[name], id);
    }

    void Interpreter::SetVar(const wstring& name, const int& user_id)
    {
        Variable var = this->GetVar(name);
        if (var.type == VARIABLETYPE_UNDEFINED) {
            this->variables_[name] = var;
        }
        SetVariableUserPtr(&this->variables_[name], user_id);
    }

    void Interpreter::SetVar(const wstring& name, const Variable& _var)
    {
        if (_var.type == VARIABLETYPE_UNDEFINED) {
            return;
        }
        this->variables_[name] = _var;
    }

    void Interpreter::DelVar(const wstring& name)
    {
        auto it = this->variables_.find(name);
        if (it != this->variables_.end()) {
            this->variables_.erase(it);
        }
    }

    Variable Interpreter::GetVar(const wstring& name)
    {
        auto it = this->variables_.find(name);
        if (it == this->variables_.end()) {
            Variable var;
            SetVariableUndefined(&var);
            return var;
        }
        return it->second;
    }

    int Interpreter::GetStrPtr(const wstring& str)
    {
        int id = 0;
        for (auto& item : this->strpool_) {
            if (item.second == str) {
                id = item.first;
            }
        }
        return id;
    }

    int Interpreter::NewStrPtr(const wstring& str)
    {
        static int id = 0;

        int _id = this->GetStrPtr(str);
        if (_id != 0) {
            return _id;
        }

        ++id;
        this->strpool_[id] = str;
        return 0;
    }

    wstring* Interpreter::GetString(const int& strptr)
    {
        return &this->strpool_[strptr];
    }
    void Interpreter::GCollect()
    {
        //���������û�г����ַ������������ʱ�����ַ���
        vector<int> delay_remove_list;
        for (auto& item : this->strpool_) {
            bool has_var = false;
            for (auto& var_item : this->variables_) {
                const Variable& var = var_item.second;
                if (var.type == VARIABLETYPE_STRPTR && var.str_ptr == item.first) {
                    has_var = true;
                    break;
                }
            }
            if (!has_var) {
                delay_remove_list.push_back(item.first);
            }
        }
        for (auto& item : delay_remove_list) {
            this->strpool_.erase(item);
        }
    }

    inline static bool IsLiteralToken(const Token& token) {
        return token.token_type == TokenType::Number || token.token_type == TokenType::String;
    }
    inline static bool IsLiteralOrVarStrToken(Interpreter* inter, const Token& token) {
        if (token.token_type == TokenType::String) {
            return true;
        }
        if (token.token_type == TokenType::Ident) {
            auto var = inter->GetVar(*token.value);
            if (var.type == VARIABLETYPE_STRPTR) {
                return true;
            }
        }
        return false;
    }
    inline static void CheckValidLength(const OpCommand& cmd, int length) {
        auto size = cmd.targets.size();
        if (size != length) {
            throw InterpreterException(cmd.op_token, L"arguments error");
        }
    }
    inline static void CheckValidMinLength(const OpCommand& cmd, int min_length) {
        auto size = cmd.targets.size();
        if (size < min_length) {
            throw InterpreterException(cmd.op_token, L"arguments error");
        }
    }
    inline static void CheckValidIdent(const Token& token) {
        if (token.token_type != TokenType::Ident) {
            throw InterpreterException(token, L"argument not is ident");
        }
    }
    inline static void CheckValidTokenType(const Token& token, const TokenType_t& type) {
        if (token.token_type != type) {
            throw InterpreterException(token, L"token type error");
        }
    }
    inline static void CheckValidStrVarOrStrLiteral(Interpreter* inter, const Token& token) {
        if (!IsLiteralOrVarStrToken(inter, token)) {
            throw InterpreterException(token, L"type error");
        }
    }
    inline static void CheckValidVariable(Interpreter* inter, const Token& token) {
        auto var = inter->GetVar(*token.value);
        if (var.type == VARIABLETYPE_UNDEFINED) {
            throw InterpreterException(token, L"variable undefined");
        }
    }
    inline static void CheckValidVariableOrLiteral(Interpreter* inter, const Token& token) {
        //��������ֵ ���� ����������
        if (!IsLiteralToken(token)
            && inter->GetVar(*token.value).type == VARIABLETYPE_UNDEFINED)
        {
            throw InterpreterException(token, L"variable undefined");
        }
    }
    inline static void CheckValidVariableType(const Token& token, const Variable& var, int type) {
        if (var.type != type) {
            throw InterpreterException(token, L"variable type error");
        }
    }


    void Interpreter::ResetCodeState()
    {
        //��� ��������ִ��ָ�룬��ǩ��
        vector<OpCommand>().swap(this->commands_);
        this->exec_ptr_ = -1;
        decltype(this->labels_)().swap(this->labels_);
        this->program_name_.clear();
    }

    bool Interpreter::ExecuteLine(const OpCommand& cmd)
    {
        if (cmd.code == OpCode::Label) {
            //ignore;
        }
        else if (cmd.code == OpCode::Goto) {
            wstring* label;
            if (cmd.targets.size() == 2 && *cmd.targets[0].value == L"var") {
                //goto var a
                CheckValidIdent(cmd.targets[0]);
                CheckValidStrVarOrStrLiteral(this, cmd.targets[1]);

                Variable var = this->GetVar(*cmd.targets[1].value);
                label = this->GetString(var.str_ptr);
            }
            else if (cmd.targets.size() == 1) {
                //goto label
                CheckValidStrVarOrStrLiteral(this, cmd.targets[0]);
                label = cmd.targets[0].value.get();
            }
            else {
                throw InterpreterException(cmd.op_token, L"goto������");
            }
            //Check
            if (this->labels_.find(*label) == this->labels_.end()) {
                throw InterpreterException(cmd.op_token, L"Label������");
            }
            //jump
            size_t pos = this->labels_[*label];
            this->exec_ptr_ = pos;
        }
        else if (cmd.code == OpCode::Set) {
            //��ʱ���ñ��ʽ��ֻʹ��һ��ֵ ��ʱ������
            CheckValidLength(cmd, 3);
            CheckValidIdent(cmd.targets[0]);
            CheckValidTokenType(cmd.targets[1], TokenType::Equal);

            wstring& varname = *cmd.targets[0].value;

            if (cmd.targets[2].token_type == TokenType::Number) {
                this->SetVar(varname, std::stof(*cmd.targets[2].value));
            }
            else if (cmd.targets[2].token_type == TokenType::String)
            {
                this->SetVar(varname, *cmd.targets[2].value);
            }
            else if (cmd.targets[2].token_type == TokenType::Ident) {
                Variable v = this->GetVar(*cmd.targets[2].value);
                if (v.type == VARIABLETYPE_UNDEFINED) {
                    throw InterpreterException(cmd.targets[2], L"variable not found");
                }
                this->SetVar(varname, v);
            }
        }
        else if (cmd.code == OpCode::Del) {
            CheckValidLength(cmd, 1);
            CheckValidIdent(cmd.targets[0]);
            this->DelVar(*cmd.targets[0].value);
        }
        else if (cmd.code == OpCode::JumpFile) {
            wstring* pfilestr;
            CheckValidLength(cmd, 1);
            Token token = cmd.targets[0];
            CheckValidStrVarOrStrLiteral(this, token);

            if (token.token_type == TokenType::Ident) {
                auto var = this->GetVar(*token.value);
                pfilestr = this->GetString(var.str_ptr);
            }
            else {
                pfilestr = token.value.get();
            }

            wstring code = this->_loadfile_(*pfilestr);
            this->ExecuteCode(code);
        }
        else if (cmd.code == OpCode::If) {
            //��ʱ�������ʽ��Ŀǰֻ����==���ж�

        }
        else if (cmd.code == OpCode::ClearSubVar) {
            //�����ӱ���
            //�ӱ�������Obj__subvar
            CheckValidLength(cmd, 1);
            CheckValidIdent(cmd.targets[0]);

            auto target = cmd.targets[0].value;

            auto it = this->variables_.begin();
            while (it != this->variables_.end()) {
                wstring name = it->first;
                if (name.length() > target->length() + 2 && name.substr(0, target->length() + 2) == *target + L"__") {
                    this->variables_.erase(it++);
                }
                else {
                    it++;
                }
            }
        }
        else if (cmd.code == OpCode::Call) {
            //
            Variable var = this->GetVar(*cmd.targets[0].value);

            vector<Token> domain;
            vector<Token> path;
            vector<Variable> params;

            int var_userptr = 0;

            int32_t index = 0;

            //instance
            if (var.type != VARIABLETYPE_UNDEFINED) {
                CheckValidVariableType(cmd.targets[0], var, VARIABLETYPE_USERPTR);
                var_userptr = var.user_ptr;
                index = 1;
            }

            bool is_symbol = false;
            bool is_last_domain = false;
            bool is_last_path = false;

            if (var.type != VARIABLETYPE_UNDEFINED) {
                is_symbol = true; //����һ��������ʲô
                //is_last_path = true; //����ֱ�ӻ�ȡ�Ӷ���
            }
            else {
                is_last_domain = true; //��̬�Ӷ�����ʼ����
            }

            for (; index < cmd.targets.size(); index++) {
                const Token& token = cmd.targets[index];

                if (is_symbol) {
                    if (token.token_type == TokenType::DoubleColon) {
                        //�������
                        is_last_domain = true;
                    }
                    else if (token.token_type == TokenType::Dot) {
                        //�Ӷ��������
                        is_last_path = true;
                    }
                    else if (token.token_type == TokenType::Colon) {
                        //������������˳�
                        index++;
                        break;
                    }
                    else {
                        throw InterpreterException(token, L"parser error");
                    }
                    is_symbol = false;
                }
                else {
                    if (is_last_domain) {
                        domain.push_back(cmd.targets[index]);
                        is_last_domain = false;
                    }
                    if (is_last_path) {
                        path.push_back(cmd.targets[index]);
                        is_last_path = false;
                    }

                    is_symbol = true;
                }
            }

            for (; index < cmd.targets.size(); index++) {
                const Token& token = cmd.targets[index];

                //��ֱ��ʡ�Զ���
                if (token.token_type == TokenType::Comma) {
                    continue;
                }

                Variable temp_var;

                CheckValidVariableOrLiteral(this, token);
                if (IsLiteralToken(token)) {
                    if (token.token_type == TokenType::Number) {
                        SetVariableNumber(&temp_var, stof(*token.value));
                    }
                    else if (token.token_type == TokenType::String) {
                        auto strptr = this->NewStrPtr(*token.value);
                        SetVariableStrPtr(&temp_var, strptr);
                    }
                }
                else {
                    temp_var = this->GetVar(*token.value);
                }

                params.push_back(temp_var);

            }

            return this->_funcall_(var_userptr, domain, path, params);
        }

        return true;
    }

    static map<wstring, TokenType_t> get_atom_operator_map() {
        map<wstring, TokenType_t> mp;
        mp[L"=="] = TokenType::DoubleEqual;
        mp[L"="] = TokenType::Equal;
        mp[L"("] = TokenType::LBracket;
        mp[L")"] = TokenType::RBracket;
        mp[L"&&"] = TokenType::And;
        mp[L"||"] = TokenType::Or;
        mp[L"*"] = TokenType::Multiple;
        mp[L"-"] = TokenType::Minus;
        mp[L"+"] = TokenType::Plus;
        mp[L"/"] = TokenType::Division;
        mp[L"!="] = TokenType::ExclamatoryAndEqual;
        mp[L"::"] = TokenType::DoubleColon;
        mp[L":"] = TokenType::Colon;
        mp[L","] = TokenType::Comma;
        mp[L"."] = TokenType::Dot;
        mp[L">"] = TokenType::GreaterThan;
        mp[L"<"] = TokenType::LessThan;

        mp[L"~"] = TokenType::Tilde;
        mp[L"!"] = TokenType::Exclamatory;
        mp[L"@"] = TokenType::At;
        mp[L"#"] = TokenType::Pound;
        mp[L"$"] = TokenType::Doller;
        mp[L"%"] = TokenType::Precent;

        mp[L"->"] = TokenType::SingleArrow;
        mp[L"=>"] = TokenType::DoubleArrow;

        return mp;
    }
    Interpreter* Interpreter::ExecuteCode(const wstring& code)
    {
        this->ResetCodeState();

        auto tokens = lexer::Scanner(&const_cast<wstring&>(code),
            &get_atom_operator_map(),
            &lexer::get_std_esc_char_map());

        this->commands_ = ParseOpList(tokens);

        //��ȡ���б�ǩ
        for (size_t i = 0; i < this->commands_.size(); i++) {
            const OpCommand& item = this->commands_[i];
            const wstring& token_value = *item.targets[0].value;
            if (item.code == OpCode::Label) {
                this->labels_[token_value] = i;
            }
        }

        return this;
    }

    Interpreter* Interpreter::ExecuteProgram(const wstring& program_name_)
    {
        this->program_name_ = program_name_;
        wstring code = this->_loadfile_(program_name_);
        this->ExecuteCode(code);
        return this;
    }

    bool Interpreter::Next()
    {
        //check
        if (this->exec_ptr_ + 1 >= (int32_t)this->opcmd_count()) {
            this->ResetCodeState();
            return false;
        }

        bool is_next = false;
        //execute
        ++this->exec_ptr_;

        while (is_next = this->ExecuteLine(this->commands_[this->exec_ptr_])) {
            ++this->exec_ptr_;
            //ÿ��128��ִ��һ��GC
            if (this->exec_ptr_ % 128 == 0) {
                this->GCollect();
            }
            //check range
            if (this->exec_ptr_ >= (int32_t)this->opcmd_count()) {
                this->ResetCodeState();
                return false;
            }
            else {
                //���
                int line = this->commands_[this->exec_ptr_].op_token.line;
            }
        }

        return true;
    }

    void Interpreter::Reset()
    {
        this->ResetCodeState();
        decltype(this->variables_)().swap(this->variables_);
        decltype(this->strpool_)().swap(this->strpool_);
    }

    inline static void StreamWriteInt32(ostream* stream, int32_t i)
    {
        stream->write((char*)&i, sizeof(int32_t));
    }
    inline static int32_t StreamReadInt32(istream* stream)
    {
        char bytes[sizeof(int32_t)];
        stream->read(bytes, sizeof(int32_t));
        int32_t i = *(int32_t*)&bytes;
        return i;
    }

    string Interpreter::Serialize()
    {
        this->GCollect();

        std::wstring_convert<std::codecvt_utf8<wchar_t>> c;
        stringstream ss;

        //variables

        StreamWriteInt32(&ss, (int32_t)this->variables_.size());

        for (auto& item : this->variables_) {

            auto name = c.to_bytes(item.first);
            int32_t _length = (int32_t)name.size();

            ss.write((char*)&_length, sizeof(int32_t));

            ss.write(name.c_str(), _length);

            char varser[sizeof(Variable)];
            SerializeVariable(&item.second, varser);

            ss.write(varser, sizeof(Variable));
        }
        //strpool

        StreamWriteInt32(&ss, (int32_t)this->strpool_.size());

        for (auto& item : this->strpool_) {

            StreamWriteInt32(&ss, item.first);

            string encode_str = c.to_bytes(item.second);
            int32_t str_len = (int32_t)encode_str.size();
            StreamWriteInt32(&ss, str_len);

            ss.write(encode_str.c_str(), str_len);
        }

        return ss.str();
    }


    void Interpreter::Deserialize(const string& data)
    {
        stringstream ss(data);
        std::wstring_convert<std::codecvt_utf8<wchar_t>> c;
        
        //variables
        int32_t _length = StreamReadInt32(&ss);

        for (int32_t i = 0; i < _length; i++)
        {
            int32_t var_name_len = StreamReadInt32(&ss);

            //���������Ȳ��ܳ���64���ַ�
            char name_buf[64] = { 0 };
            ss.read(name_buf, var_name_len);
            wstring name = c.from_bytes(name_buf);
            
            char var_buf[sizeof(Variable)];
            ss.read(var_buf, sizeof(Variable));
            Variable var = DeserializeVariable(var_buf);

            this->SetVar(name, var);
        }

        //strpool
        
        int32_t strpool_len = StreamReadInt32(&ss);

        for (int32_t i = 0; i < strpool_len; i++)
        {
            int32_t str_ptr = StreamReadInt32(&ss);

            int32_t str_len = StreamReadInt32(&ss);
            char* str_buf = new char[str_len];
            ss.read(str_buf, str_len);
            wstring str = c.from_bytes(str_buf);
            delete[] str_buf;

            this->strpool_[str_ptr] = str;
        }
    }


#pragma endregion

#pragma region Variable
    void SetVariableUndefined(Variable* var)
    {
        var->type = VARIABLETYPE_UNDEFINED;
    }
    void SetVariableNull(Variable* var)
    {
        var->type = VARIABLETYPE_NULL;
        var->num = 0;
    }

    void SetVariableNumber(Variable* var, float num)
    {
        var->type = VARIABLETYPE_NUMBER;
        var->num = num;
    }

    void SetVariableStrPtr(Variable* var, int ptr)
    {
        var->type = VARIABLETYPE_STRPTR;
        var->str_ptr = ptr;
    }

    void SetVariableUserPtr(Variable* var, int user_ptr)
    {
        var->type = VARIABLETYPE_USERPTR;
        var->user_ptr = user_ptr;
    }
    void SerializeVariable(Variable* var, char out[8])
    {
        memcpy(out, &var, sizeof(Variable));
    }
    Variable DeserializeVariable(char value[8])
    {
        Variable var;
        memcpy(&var, value, 8);
        return var;
    }
#pragma endregion

}

