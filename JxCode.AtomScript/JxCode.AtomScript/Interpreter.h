#pragma once
#include <string>
#include <vector>
#include <map>
#include <cinttypes>
#include <memory>
#include <functional>

#include "Token.h"
#include "OpCommand.h"
#include "Variable.h"

namespace jxcode::atomscript
{
    using std::string;
    using std::wstring;
    using std::map;
    using std::vector;
    using std::shared_ptr;
    using std::function;
    using lexer::Token;

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
            const int32_t& user_ptr, 
            const vector<Token>& domain,
            const vector<Token>& path,
            const vector<Variable>& params)>;
    protected:
        LoadFileCallBack _loadfile_;
        FuncallCallBack _funcall_;

        wstring program_name_; //ser

        vector<OpCommand> commands_;
        int32_t exec_ptr_; //ser
        map<wstring, size_t> labels_;

        map<wstring, Variable> variables_; //ser
        map<int32_t, wstring> strpool_; //ser
        int32_t ptr_alloc_index_; //ser
    public:
        int32_t line_num() const;
        size_t opcmd_count() const;
        const wstring& program_name() const;
        const map<wstring, Variable>& variables();
        const map<int32_t, wstring>& strpool();
    public:
        Interpreter(
            LoadFileCallBack _loadfile_,
            FuncallCallBack _funcall_);
    protected:
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
        Interpreter* ExecuteProgram(const wstring& program_name_);
        //�����Ƿ����н���
        bool Next();

        void ResetState();
        void ResetMemory();

        string Serialize();
        void Deserialize(const string& data);
    };
}


