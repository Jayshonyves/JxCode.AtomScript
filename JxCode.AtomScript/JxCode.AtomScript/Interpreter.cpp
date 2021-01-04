#include <stdexcept>
#include "Interpreter.h"
#include "Lexer.h"

namespace jxcode::atomscript
{

    using std::wstring;
    using namespace lexer;

    wstring InterpreterException::ParserException = wstring(L"ParserException");
    wstring InterpreterException::RuntimeException = wstring(L"RunetimeException");

    InterpreterException::InterpreterException(const lexer::Token& token, const std::wstring& message)
        : token_(token), message_(message) {}

    std::wstring InterpreterException::what() const
    {
        return this->token_.to_string();
    }


    int32_t Interpreter::line_num() const
    {
        return this->exec_ptr_;
    }
    size_t Interpreter::opcmd_count() const
    {
        return this->commands_.size();
    }
    map<wstring, Variable*>& Interpreter::variables()
    {
        return this->variables_;
    }
    Interpreter::Interpreter(LoadFileCallBack _loadfile_, FuncallCallBack _funcall_, ErrorInfoCallBack _errorcall_)
        : exec_ptr_(-1), _loadfile_(_loadfile_), _funcall_(_funcall_), _errorcall_(_errorcall_)
    {
    }

    bool Interpreter::IsExistLabel(const wstring& label)
    {
        return this->labels_.find(label) != this->labels_.end();
    }

    void Interpreter::SetVar(const wstring& name, const float& num)
    {
        Variable* var = this->GetVar(name);
        if (var == nullptr) {
            this->variables_[name] = new Variable(num);
        }
        else {
            this->variables_[name]->SetNumber(num);
        }
    }

    void Interpreter::SetVar(const wstring& name, const wstring& str)
    {
        Variable* var = this->GetVar(name);
        if (var == nullptr) {
            this->variables_[name] = new Variable(str);
        }
        else {
            this->variables_[name]->SetString(str);
        }
    }

    void Interpreter::SetVar(const wstring& name, const intptr_t& user_id)
    {
        Variable* var = this->GetVar(name);
        if (var == nullptr) {
            this->variables_[name] = new Variable(user_id);
        }
        else {
            this->variables_[name]->SetUserVarId(user_id);
        }
    }

    void Interpreter::DelVar(const wstring& name)
    {
        auto it = this->variables_.find(name);
        if (it != this->variables_.end()) {
            //�����ڴ�
            delete it->second;
            this->variables_.erase(it);
        }
    }

    Variable* Interpreter::GetVar(const wstring& name)
    {
        auto it = this->variables_.find(name);
        if (it == this->variables_.end()) {
            return nullptr;
        }
        return it->second;
    }

    void Interpreter::ResetCodeState()
    {
        //��� ��������ִ��ָ�룬��ǩ��
        vector<OpCommand>().swap(this->commands_);
        this->exec_ptr_ = -1;
        decltype(this->labels_)().swap(this->labels_);
    }

    bool Interpreter::ExecuteLine(const OpCommand& cmd)
    {
        if (cmd.code == OpCode::Label) {
            //ig;
        }
        else if (cmd.code == OpCode::Goto) {
            wstring label;
            if (cmd.targets.size() == 2 && *cmd.targets[0].value == L"var") {
                //goto var a
                Variable* var = this->GetVar(*cmd.targets[1].value);
                if (var == nullptr) {
                    throw InterpreterException(cmd.targets[1], L"����δ�ҵ�");
                }
                else if (var->type != VariableType::String) {
                    throw InterpreterException(cmd.targets[1], L"�������ʹ���");
                }
                label = var->str;
            }
            else if (cmd.targets.size() == 1) {
                //goto label
                label = *cmd.targets[0].value;
            }
            else {
                throw InterpreterException(cmd.op_token, L"goto������");
            }
            //Check
            if (this->labels_.find(label) == this->labels_.end()) {
                throw InterpreterException(cmd.op_token, L"Label������");
            }
            //jump
            size_t pos = this->labels_[label];
            this->exec_ptr_ = pos;
        }
        else if (cmd.code == OpCode::Set) {
            //��ʱ���ñ��ʽ��ֻʹ��һ��ֵ
            this->SetVar(*cmd.targets[0].value, *cmd.targets[2].value);
        }
        else if (cmd.code == OpCode::Del) {
            this->DelVar(*cmd.targets[0].value);
        }
        else if (cmd.code == OpCode::JumpFile) {
            wstring code = this->_loadfile_(*cmd.targets[0].value);
            this->ExecuteCode(code);
        }
        else if (cmd.code == OpCode::If) {
            //��ʱ�������ʽ��Ŀǰֻ����==���ж�

        }
        else if (cmd.code == OpCode::ClearSubVar) {
            //�����ӱ���
            //�ӱ�������Obj__subvar
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
            Variable* var = this->GetVar(*cmd.targets[0].value);

            vector<Token> domain;
            vector<Token> path;
            vector<Token> params;

            intptr_t user_type_ptr = 0;

            int32_t first = 0;
            //inst
            if (var != nullptr) {
                if (var->type != VariableType::UserVarId) {
                    throw InterpreterException(cmd.targets[0], L"������user����");
                }
                user_type_ptr = 0;
                first = 1;
            }

            bool is_symbol = false;
            bool is_last_domain = false;
            bool is_last_path = false;

            if (var != nullptr) {
                is_last_path = true; //����ֱ�ӻ�ȡ�Ӷ���
            }
            else {
                is_last_domain = true; //��̬�Ӷ�����ʼ����
            }

            for (; first < cmd.targets.size(); first++) {
                const Token& token = cmd.targets[first];

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
                        first++;
                        break;
                    }
                    else {
                        throw InterpreterException(token, L"parser error");
                    }
                    is_symbol = false;
                }
                else {
                    if (is_last_domain) {
                        domain.push_back(cmd.targets[first]);
                        is_last_domain = false;
                    }
                    if (is_last_path) {
                        path.push_back(cmd.targets[first]);
                        is_last_path = false;
                    }

                    is_symbol = true;
                }
            }

            for (; first < cmd.targets.size(); first++) {
                const Token& token = cmd.targets[first];
                if (token.token_type == TokenType::Comma) {
                    continue;
                }
                params.push_back(token);
            }

            return this->_funcall_(user_type_ptr, domain, path, params);
        }

        return true;
    }

    Interpreter* Interpreter::ExecuteCode(const wstring& code)
    {
        this->ResetCodeState();

        auto tokens = lexer::Scanner(&const_cast<wstring&>(code),
            &lexer::get_std_operator_map(),
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

    Interpreter* Interpreter::Next()
    {
        //check
        if (this->exec_ptr_ + 1 >= (int32_t)this->opcmd_count()) {
            this->ResetCodeState();
            return this;
        }

        bool is_next = false;
        //execute
        ++this->exec_ptr_;

        while (is_next = ExecuteLine(this->commands_[this->exec_ptr_])) {
            ++this->exec_ptr_;
            //check range
            if (this->exec_ptr_ >= (int32_t)this->opcmd_count()) {
                this->ResetCodeState();
                return this;
            }
            else {
                //���
                int line = this->commands_[this->exec_ptr_].op_token.line;
            }
        }

        return this;
    }

    std::wstring Interpreter::Serialize()
    {
        return std::wstring();
    }

    void Interpreter::Deserialize(const wstring& text)
    {
    }

    Variable::Variable()
        : type(VariableType::Null), num(0.0f), str(wstring()), user_type_ptr(0)
    {
    }

    Variable::Variable(const float& num) : Variable()
    {
        this->SetNumber(num);
    }

    Variable::Variable(const wstring& str) : Variable()
    {
        this->SetString(str);
    }

    Variable::Variable(const intptr_t& user_type_ptr) : Variable()
    {
        this->SetUserVarId(user_type_ptr);
    }

    void Variable::SetNumber(const float& num)
    {
        this->num = num;
        this->type = VariableType::Numeric;
    }

    void Variable::SetString(const wstring& str)
    {
        this->str = str;
        this->type = VariableType::String;
    }

    void Variable::SetUserVarId(const intptr_t& user_type_ptr)
    {
        this->user_type_ptr = user_type_ptr;
        this->type = VariableType::UserVarId;
    }

}

