#pragma once
#include <string>
#include <vector>
#include <map>
#include <cinttypes>
#include <memory>
#include <functional>

#include "Token.h"
#include "OpCommand.h"

#define VARIABLETYPE_UNDEFINED -1
#define VARIABLETYPE_NULL 0
#define VARIABLETYPE_NUMBER 1
#define VARIABLETYPE_STRPTR 2
#define VARIABLETYPE_USERPTR 3

namespace jxcode::atomscript
{
    using std::string;
    using std::wstring;
    using std::map;
    using std::vector;
    using std::shared_ptr;
    using std::function;
    using lexer::Token;

    struct Variable
    {
        int32_t type;
        union {
            float num;
            int32_t str_ptr;
            int32_t user_ptr;
        };
    };
    void SetVariableUndefined(Variable* var);
    void SetVariableNull(Variable* var);
    void SetVariableNumber(Variable* var, float num);
    void SetVariableStrPtr(Variable* var, int ptr);
    void SetVariableUserPtr(Variable* var, int user_ptr);

    void SerializeVariable(Variable* var, char out[8]);
    Variable DeserializeVariable(char value[8]);

    class InterpreterException
    {
    protected:
        std::wstring message_;
        lexer::Token token_;
    public:

    public:
        InterpreterException(const lexer::Token& token, const std::wstring& message);
    public:
        virtual std::wstring what() const;
    };

    class Interpreter
    {
    public:
        using LoadFileCallBack = function<wstring(const wstring& program_name_)>;
        //������ö���Ϊ��̬�����޷��ڱ��������ҵ�����user_type_ptrΪ0
        using FuncallCallBack = function<bool(
            const intptr_t& user_ptr, 
            const vector<Token>& domain,
            const vector<Token>& path,
            const vector<Variable>& params)>;
    protected:
        LoadFileCallBack _loadfile_;
        FuncallCallBack _funcall_;

        wstring program_name_;

        vector<OpCommand> commands_;
        int32_t exec_ptr_;
        map<wstring, size_t> labels_;

        map<wstring, Variable> variables_;
        map<int32_t, wstring> strpool_;
    public:
        int32_t line_num() const;
        size_t opcmd_count() const;
        const wstring& program_name() const;
        map<wstring, Variable>* variables();
    public:
        Interpreter(
            LoadFileCallBack _loadfile_,
            FuncallCallBack _funcall_);
    protected:
        void ResetCodeState();
        bool ExecuteLine(const OpCommand& cmd);
    public:
        bool IsExistLabel(const wstring& label);
        void SetVar(const wstring& name, const float& num);
        void SetVar(const wstring& name, const wstring& str);
        void SetVar(const wstring& name, const int& user_id);
        void SetVar(const wstring& name, const Variable& var);
        void DelVar(const wstring& name);
        Variable GetVar(const wstring& name);
    public:
        int GetStrPtr(const wstring& str);
        int NewStrPtr(const wstring& str);
        wstring* GetString(const int& strptr);
        void GCollect();
    public:
        Interpreter* ExecuteCode(const wstring& code);
        Interpreter* ExecuteProgram(const wstring& program_name_);
        //�����Ƿ����н���
        bool Next();
        
        void Reset();

        string Serialize();
        void Deserialize(const string& data);
    };
}


