
#ifndef _JXCODE_ATOMSCRIPT_VARIABLE_H
#define _JXCODE_ATOMSCRIPT_VARIABLE_H

#define VARIABLETYPE_UNDEFINED 0
#define VARIABLETYPE_NUMBER 1
#define VARIABLETYPE_STRPTR 2
#define VARIABLETYPE_USERPTR 3

typedef struct Variable
{
    int type;
    union {
        float num;
        int ptr;
    };
} Variable;

void SetVariableUndefined(Variable* var);
void SetVariableNumber(Variable* var, float num);
void SetVariableStrPtr(Variable* var, int ptr);
void SetVariableUserPtr(Variable* var, int user_ptr);

void SerializeVariable(Variable* var, char out[8]);
Variable DeserializeVariable(char value[8]);

#endif // !_JXCODE_ATOMSCRIPT_VARIABLE_H