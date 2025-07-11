/*
*   parser.c
*
*   An execution engine for a Basic language interpreter.
*
*   This module accepts a parsed internal representation of a Basic Language
*   command and executes the command(s).
*
*/

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "symtab.h"
#include "lexer.h"
#include "expr.h"
#include "parser.h"
#include "runtime.h"
#include "main.h"

#define STRING_LEN 80
#define TABLE_LEN 100
#define STACK_SIZE 20
#define ARG_MAX 10

#define BEEP_TONE 440
#define BEEP_DURATION 300

#define ALL_COMMANDS false

extern void PrintExprTree(Node *root);

bool RunProgram(void);
bool ListProgram(void);
bool NewProgram(void);
bool ExecCommand(Command *command, bool cmdListOnly);
bool ExecPrint(PrintCommand *cmd);
bool ExecAssign(AssignCommand *cmd);
bool ExecFor(ForCommand *cmd);
bool ExecNext(NextCommand *cmd);
bool ExecGoto(GotoCommand *cmd);
bool ExecIf(IfCommand *cmd);
bool ExecGosub(GosubCommand *cmd);
bool ExecReturn(void);
bool ExecEnd(void);
bool ExecInput(InputCommand *cmd);
bool ExecPoke(PlatformCommand *cmd);
bool ExecTone(PlatformCommand *cmd);
bool ExecBeep(PlatformCommand *cmd);
bool ExecLeds(PlatformCommand *cmd);
bool ExecDisplay(PlatformCommand *cmd);
bool ExecPutchar(PlatformCommand *cmd);
bool ExecPutDB(PlatformCommand *cmd);
bool ExecLoadFB(PlatformCommand *cmd);
bool ExecClear(PlatformCommand *cmd);
bool ExecClearDB(PlatformCommand *cmd);
bool ExecText(PlatformCommand *cmd);
bool ExecGr(PlatformCommand *cmd);
bool ExecOutchar(PlatformCommand *cmd);
bool ExecRseed(PlatformCommand *cmd);
bool ExecDelay(PlatformCommand *cmd);
bool ExecDim(DimCommand *cmd);
bool ExecBreak(PlatformCommand *cmd);
bool ExecBuiltinFct(const char *name);

bool EvaluateNumExpr(Node *exprTreeRoot, float *pValue);
bool EvaluateStrExpr(Node *exprTreeRoot, char **pValue);
bool TraverseSyntaxTree(Node *node);

float NumStackPush(float a);
float NumStackPop(void);
float NumStackTop(void);
float NumStackPut(float a);
char *StrStackPush(char *a);
char *StrStackPop(void);
char *StrStackTop(void);
char *StrStackPut(char *a);

void FreeCommand(Command *cmd);
void FreeCommandList(Command *commandList);
void FreeProgram(void);
Command *IterateCmdPtr(bool cmdListOnly);
bool LineNum2CmdLineIdx(int lineNum);
void SwapProgLines(int progIdxA, int progIdxB);
void SortProgramByLineNum(void);
void PrintResult(void);
bool InstallBuiltinFcts(void);

// runtime strings and flags
bool ready = true;
bool textMode = true;
char resultStr[STRING_LEN];


// ***stacks and queues***

// command queue aka "the program"
CommandLine Program[MAX_PROGRAM_LEN];   // list of command lines all of which share the same line number
int programSize = 0;                    // program index used to add commands to the program

// index into the program of the current command list
int cmdListIdx = 0;

// command pointer is used to point to the next command to be executed
Command *cmdPtr = NULL;

// empty command line with which to reset the program
CommandLine emptyCommandLine = {0};

// for command stack needed by the next command
ForCommand *fortab[TABLE_LEN];
int fortabSize = 0, lastForIdx;

// call stack used for subroutines/returns
Command *callStack[STACK_SIZE];
unsigned callSP = 0;

// floating point stack and its index, i.e. real stack pointer
float numStack[STACK_SIZE];
unsigned numSP = 0;

// string stack and its index, i.e. string stack pointer
char *strStack[STACK_SIZE];
unsigned strSP = 0;

// built-in function list
typedef struct FctList {
    char *name;
    int arity;
} FctList;


// *** main entry point from the main UI into the interpreter ***

bool ProcessCommand(char *commandStr)
{
    // init the parser and error string
    CommandLine commandLine = {0};
    bool isImmediate = true;
    int i;
    char tempStr[80];
    char commandBuf[80];
    char *filename;
    
    // default parser error
    strcpy(errorStr, "syntax error");
    
    // execute any directive - use strtok instead of the parsing infrastructure
    // command-line : directive
    strcpy(commandBuf, commandStr);
    if (!strcmp(commandStr, ""))
    {
        return true;
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "run"))
    {
        return RunProgram();
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "list"))
    {
        return ListProgram();
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "new"))
    {
        return NewProgram();
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "reboot"))
    {
        InitDisplay();
        return NewProgram();
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "mount"))
    {
        return FsMount();
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "unmount"))
    {
        return FsUnmount();
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "files"))
    {
        if ((filename = strtok(NULL, " ")) != NULL)
            return false;
        return FsList();
    }    
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "delete"))
    {
        filename = strtok(NULL, " ");
        return FsDelete(filename);
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "load"))
    {
        NewProgram();
        filename = strtok(NULL, " ");
        return FsLoad(filename);
    }
    strcpy(commandBuf, commandStr);
    if (!strcmp(strtok(commandBuf, " "), "save"))
    {
        filename = strtok(NULL, " ");
        return FsSave(filename);
    }
    
    // init the lexer and parse the command to create the IR
    // command-line : [Constant] command-list
    if (GetNextToken(commandStr))
    {
        // check for a line number to determine whether this is an immediate command
        if (token == Constant)
        {
            // if there's a line number set the command-line's line number and save the command string for later printing
            commandLine.lineNum = atoi(lexval.lexeme);
            strcpy(commandLine.commandStr, commandStr);
            isImmediate = false;
            if (!GetNextToken(NULL))
            {
                return false;
            }
        }

        if (IsCommandList(&commandLine.commandList, commandLine.lineNum))
        {
            if (isImmediate)
            {
                bool success = true;
                
                // the command list has no line number so execute it immediately
                cmdPtr = commandLine.commandList;
                while (success && cmdPtr)
                {
                    success = ExecCommand(cmdPtr, isImmediate);
                    PrintResult();
                }
                
                // free the command line and all expressions immediately
                FreeCommandList(commandLine.commandList);
                commandLine = emptyCommandLine;
                FreeExprTrees();
                return success;
            }
            else
            {
                // check to see if this is a replacement of an existing line by number
                for (i = 0; i < programSize; i++)
                {
                    if (commandLine.lineNum == Program[i].lineNum)
                    {
                        break;
                    }
                }
                Program[i] = commandLine;
                if (i == programSize)
                {
                    //Program[i].lineNum = command.lineNum;
                    if (programSize < MAX_PROGRAM_LEN-1)
                    {
                        programSize++;
                    }
                    else
                    {
                        sprintf(errorStr, "no more program space");
                        return false;
                    }
                }
                SortProgramByLineNum();
                sprintf(message, "%d nodes in use\n", gNodeQty);
                MESSAGE(message);
                ready = false;
                return true;
            }
        }
        else
        {
            sprintf(tempStr, ": %s", commandStr);
            strcat(errorStr, tempStr);
        }
    }
    
    return false;
}

// execute commands in the program based on the command pointer, cmdPtr, which 
// will be iterated unless a command is executed that explicitly changes it
bool RunProgram(void)
{
    char tempStr[STRING_LEN];
    
    if (programSize != 0)
    {
        // clear error string
        strcpy(errorStr, "");
        
        // ready for more commands whether or not the program executes correctly
        ready = true;    

        // init all stacks
        callSP = 0;
        numSP = 0;
        strSP = 0;
                  
        // init the command pointer to the first command in the first command line
        cmdPtr = Program[0].commandList;
        cmdListIdx = 0;
        while (cmdPtr != NULL)
        {
            // execute the command
            if (ExecCommand(cmdPtr, ALL_COMMANDS))
            {
                PrintResult();
            }
            else
            {
                sprintf(tempStr, " at line %d", cmdPtr->lineNum);
                strcat(errorStr, tempStr);
                return false;
            }
        }
    }
    else
    {
        strcpy(errorStr, "no program present");
        return false;
    }
          
    return true;
}

bool ListProgram(void)
{
    for (int i = 0; i < programSize; i++)
    {
        if (Program[i].commandList->type != CT_NOP || strstr(Program[i].commandStr, "rem"))
        {
            strcpy(resultStr, Program[i].commandStr);
            PrintResult();
        }
    }
    ready = true;      
      
    return true;
}

bool NewProgram(void)
{
    fortabSize = 0;
    programSize = 0;
    cmdListIdx = 0;
    callSP = 0;
    numSP = 0;
    strSP = 0;
    FreeExprTrees();
    FreeSymtab();
    InstallBuiltinFcts();
    FreeProgram();
    
    return true;
}

// execute a possible list of commands
bool ExecCommand(Command *command, bool cmdListOnly)
{
    Command *lastCmdPtr = cmdPtr;
    
    switch (command->type)
    {
        case CT_NOP: 
            break;
                
        case CT_PRINT: 
            if (!ExecPrint(&command->cmd.printCmd))
                return false;
            break;
                
        case CT_ASSIGN: 
            if (!ExecAssign(&command->cmd.assignCmd))
                return false;
            break;
                
        case CT_FOR: 
            if (!ExecFor(&command->cmd.forCmd))
                return false;
            break;
                
        case CT_NEXT: 
            if (!ExecNext(&command->cmd.nextCmd))
                return false;
            break;
                
        case CT_GOTO: 
            if (!ExecGoto(&command->cmd.gotoCmd))
                return false;
            break;                
                
        case CT_IF: 
            if (!ExecIf(&command->cmd.ifCmd))
                return false;
            break;                
                
        case CT_GOSUB: 
            if (!ExecGosub(&command->cmd.gosubCmd))
                return false;
            break;                
                
        case CT_RETURN: 
            if (!ExecReturn())
                return false;
            break;                
                
        case CT_END: 
            if (!ExecEnd())
                return false;
            break;                
                
        case CT_INPUT: 
            if (!ExecInput(&command->cmd.inputCmd))
                return false;
            break;                
                
        case CT_POKE: 
            if (!ExecPoke(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_TONE: 
            if (!ExecTone(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_BEEP: 
            if (!ExecBeep(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_LEDS: 
            if (!ExecLeds(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_DISPLAY: 
            if (!ExecDisplay(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_PUTCHAR: 
            if (!ExecPutchar(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_PUTDB: 
            if (!ExecPutDB(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_LOADFB: 
            if (!ExecLoadFB(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_CLEAR: 
            if (!ExecClear(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_CLEARDB: 
            if (!ExecClearDB(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_TEXT: 
            if (!ExecText(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_GR: 
            if (!ExecGr(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_OUTCHAR: 
            if (!ExecOutchar(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_RSEED: 
            if (!ExecRseed(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_DELAY: 
            if (!ExecDelay(&command->cmd.platformCmd))
                return false;
            break;                
                
        case CT_DIM: 
            if (!ExecDim(&command->cmd.dimCmd))
                return false;
            break;                
        case CT_BREAK: 
            if (!ExecBreak(&command->cmd.platformCmd))
                return false;
            break;                
    }
    if (cmdPtr == lastCmdPtr)
    {
        // if a command didn't change the command pointer, logically increment it
        cmdPtr = IterateCmdPtr(cmdListOnly);                      
    }
    
    return true;
}

// expr-list : expr  [{';' | ','} expr-list]
bool ExecPrint(PrintCommand *cmd)
{
    char exprStr[80];
    float numval;
    char *strval;
    int intval, decval;
    
    for (int i = 0; i < cmd->printListIdx; i++)
    {
        strcat(resultStr, "");
        
        if (cmd->printList[i].separator == ',')
        {
            strcat(resultStr, "    ");
        }
        
        switch (NODE_TYPE(cmd->printList[i].expr))
        {
            case NT_BINOP:
            case NT_UNOP:
            case NT_NUMVAR:
            case NT_FCT:
            case NT_CONSTANT:
                if (!EvaluateNumExpr(cmd->printList[i].expr, &numval))
                {
                    if (!strcmp(errorStr, ""))
                    {
                        // default error
                        strcpy(errorStr, "invalid print expression");
                    }
                    return false;
                }
                switch (cmd->style)
                {
                    case PS_DECIMAL:
                        sprintf(exprStr, "%f", numval);
                        sscanf(exprStr, "%d.%d", &intval, &decval);
                        if (decval == 0)
                        {
                            sprintf(exprStr, "%.f", numval);
                        }
                        break;
                    case PS_HEX:
                        sprintf(exprStr, "0x%x", (unsigned int)numval);
                        break;
                    case PS_ASCII:
                        sprintf(exprStr, "%c", (int)numval);
                        break;
                }
                if (exprStr == NULL)
                {
                    strcpy(errorStr, "invalid print expression");
                    return false;
                }
                strcat(resultStr, exprStr);
                break;
                
            case NT_STRVAR:
            case NT_STRING:
                if (!EvaluateStrExpr(cmd->printList[i].expr, &strval))
                {
                    if (!strcmp(errorStr, ""))
                    {
                        // default error
                        strcpy(errorStr, "invalid print expression");
                    }
                    return false;
                }
                if (strval == NULL)
                {
                    strcpy(errorStr, "invalid print expression");
                    return false;
                }
                strcat(resultStr, strval);
                break;

            default:
                break;
        }
    }
    
    return true;
}

// assignment : {Intvar | Strvar} ['(' expr [',' expr]* ')'] '=' {expr | string}
// [let] Intvar ['(' expr [',' expr]* ')'] '=' expr
// [let] Strvar ['(' expr [',' expr]* ')'] '=' String | postfixExpr
bool ExecAssign(AssignCommand *cmd)
{
    float indeces[DIM_MAX] = {0};
    float numRhs;
    char *strRhs;
    int i;
    
    if (SYM_DIM(cmd->varsym) > 0)
    {
        // evaluate the LHS index values from their nodes padding with 0's for unused higher order dimensions
        for (i = 0; i < DIM_MAX - SYM_DIM(cmd->varsym); i++)
        {
            SYM_DIMSIZES(cmd->varsym, i) = 0;
        }
        for (int j = 0; j < SYM_DIM(cmd->varsym); j++, i++)
        {
            if (!EvaluateNumExpr(cmd->indexNodes[j], &indeces[i]))
            {
                strcpy(errorStr, "invalid array index expression");
                return false;
            }
        }
    }
            
    // perform the assignment, varsym = expr, can only assign values to variables
    if (SYM_TYPE(cmd->varsym) == ST_NUMVAR)
    {
        switch (NODE_TYPE(cmd->expr))
        {
            case NT_BINOP:
            case NT_UNOP:
            case NT_NUMVAR:
            case NT_FCT:
            case NT_CONSTANT:
                if (EvaluateNumExpr(cmd->expr, &numRhs))
                {
                    if (SymWriteNumvar(cmd->varsym, indeces, numRhs))
                    {
                        return true;
                    }
                }
                break;
            default:
                break;
        }
    }        
    else if (SYM_TYPE(cmd->varsym) == ST_STRVAR)
    {
        switch (NODE_TYPE(cmd->expr))
        {
            case NT_STRVAR:
            case NT_STRING:
                if (EvaluateStrExpr(cmd->expr, &strRhs))
                {
                    if (SymWriteStrvar(cmd->varsym, indeces, strRhs))
                    {
                        return true;
                    }
                }
                break;
            default:
                break;
        }
        
    }

    if (!strcmp(errorStr, ""))
    {
        strcpy(errorStr, "incompatible types");
    }
    return false;
}

// for : FOR Intvar '=' init TO to [STEP step]
bool ExecFor(ForCommand *cmd)
{
    float value;
    
    //Console("ExecFor()\n");
    
    // perform initial variable assignment
    if (EvaluateNumExpr(cmd->init, &value))
    {
        if (SymWriteNumvar(cmd->symbol, NULL, value))
        {
            // push for-command onto FOR stack if it isn't already there
            for (int i = 0; i < fortabSize; i++)
            {
                if (fortab[i] && fortab[i]->lineNum == cmd->lineNum)
                {
                    lastForIdx = i;
                    return true;
                }
            }
            lastForIdx = fortabSize;
            fortab[fortabSize++] = cmd;
            return true;
        }
    }
    
    return false;
}

// next : NEXT [Intvar]
bool ExecNext(NextCommand *cmd)
{
    ForCommand *forInstr = NULL;
    float symval, to, step = 1;
    
    //Console("ExecNext()\n");
    
    // associate the next instruction with its matching for command
    if (fortabSize > 0)
    {
        if (cmd->symbol)
        {
            // find the explicit for instruction associated with the next instruction's var name at a line number less than command
            for (int i = 0; i < fortabSize; i++)
            {
                if ((fortab[i]->symbol == cmd->symbol) && (fortab[i]->lineNum < cmd->lineNum))
                {
                    forInstr = fortab[i];
                }
            }
            if (forInstr == NULL)
            {
                strcpy(errorStr, "no matching FOR");
                return false;
            }
        }
        else
        {
            // use the last FOR executed for annonymous next
            forInstr = fortab[lastForIdx];
        }
    }
    else
    {
        strcpy(errorStr, "no matching FOR");
        return false;
    }
        
    // modify the associated variable and perform a goto if needed        
    if (forInstr->step)
    {
        if (!EvaluateNumExpr(forInstr->step, &step))
        {
            return false;
        }
    }
    if (SymReadNumvar(forInstr->symbol, NULL, &symval))
    {
        symval += step;
        if (SymWriteNumvar(forInstr->symbol, NULL, symval))
        {
            // check that the variable's value is in the range of the for instruction
            if (EvaluateNumExpr(forInstr->to, &to))
            {
                if ((step >= 0 && symval <= to) || (step < 0 && symval >= to))
                {
                    // goto the first command in the command line following the for command
                    if (LineNum2CmdLineIdx(forInstr->lineNum))
                    {
                        cmdListIdx++;
                        cmdPtr = Program[cmdListIdx].commandList;
                    }
                    else
                    {
                        return false;
                    }
                }
                return true;
            }
        }
    }
    
    return false;
}

// goto : GOTO Constant
bool ExecGoto(GotoCommand *cmd)
{
    float dest;
    
    if (EvaluateNumExpr(cmd->dest, &dest))
    {
        if (dest > 0)
        {
            // goto the first command in the command line of the goto destination
            if (LineNum2CmdLineIdx((int)dest))
            {
                cmdPtr = Program[cmdListIdx].commandList;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    return false;
}

// if : IF expr THEN [assign | print | goto]
bool ExecIf(IfCommand *cmd)
{
    float predicate;
    char tempStr[STRING_LEN];
    
    if (EvaluateNumExpr(cmd->expr, &predicate))
    {
        // if the predicate expr is true then execute the command list, else simply return
        if (predicate)
        {
            cmdPtr = cmd->commandList;
            while (cmdPtr != NULL)
            {
                // execute the command
                if (ExecCommand(cmdPtr, ALL_COMMANDS))
                {
                    PrintResult();
                }
                else
                {
                    sprintf(tempStr, " at line %d", cmdPtr->lineNum);
                    strcat(errorStr, tempStr);
                    return false;
                }
            }
        }
        return true;
    }

    return false;
}

// gosub : GOSUB Constant
bool ExecGosub(GosubCommand *cmd)
{
    float dest;
    
    //Console("ExecGosub()\n");
    
    if (EvaluateNumExpr(cmd->dest, &dest))
    {
        if (dest > 0)
        {
            // push the pointer to the next command to be executed after the subroutine return onto the call stack
            callStack[callSP++] = IterateCmdPtr(false);
            
            // goto the first command in the command line of the gosub destination
            if (LineNum2CmdLineIdx((int)dest))
            {
                cmdPtr = Program[cmdListIdx].commandList;
                return true;
            }
        }
    }

    return false;
}

// return : RETURN
bool ExecReturn(void)
{
    
    //Console("ExecReturn()\n");
    
    // ensure the call stack isn't empty
    if (callSP == 0)
    {
        strcpy(errorStr, "return without gosub");
        return false;
    }
    // pop the next command to be executed from the call stack
    cmdPtr = callStack[--callSP];
    if (LineNum2CmdLineIdx(cmdPtr->lineNum))
    {
        return true;
    }
    
    return false;
}

// end : END
bool ExecEnd(void)
{
    // end the program by setting the command point to NULL
    cmdPtr = NULL;
    return true;
}

// input : INPUT {Intvar | Strvar} ['(' expr [',' expr]* ')']
bool ExecInput(InputCommand *cmd)
{
    char buffer[80];
    float indeces[4];
    Node *expr;
    float numInput;
    char *strInput;
    
    // evaluate index values for an LHS array
    for (int i = 0; i < SYM_DIM(cmd->varsym); i++)
    {
        if (!EvaluateNumExpr(cmd->indexNodes[i], &indeces[i]))
        {
            strcpy(errorStr, "invalid array index expression");
            return false;
        }
    }
            
    //prompt with symbol name, space, question mark
    sprintf(buffer, "? ");
    PutString(buffer);
    
    // read and tokenize input text
    GetString(buffer);
    buffer[strlen(buffer)-1] = '\0';
    if (GetNextToken(buffer))
    {
        if (SYM_TYPE(cmd->varsym) == ST_NUMVAR)
        {
            // input can only be a constant
            if (token == Constant && IsExpr(&expr))
            {
                if (EvaluateNumExpr(expr, &numInput))
                {
                    if (!SymWriteNumvar(cmd->varsym, indeces, numInput))
                    {
                        return false;
                    }
                }
            }
            else
            {
                strcpy(errorStr, "invalid input expression");
                return false;
            }
        }        
        else if (SYM_TYPE(cmd->varsym) == ST_STRVAR)
        {
            // input can only be a string
            if (token == String && IsExpr(&expr))
            {
                if (EvaluateStrExpr(expr, &strInput))
                {
                    if (!SymWriteStrvar(cmd->varsym, indeces, strInput))
                    {
                        return false;
                    }
                }
            }
            else
            {
                strcpy(errorStr, "invalid input string");
                return false;
            }
        }
    }

    return true;
}

bool ExecPoke(PlatformCommand *cmd)
{
    float numval;
    int addr, data;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        addr = (int)numval;
        if (EvaluateNumExpr(cmd->arg2, &numval))
        {
            data = (int)numval;
            MemWrite((uint16_t)addr, (uint8_t)data);
            return true;
        }
    }
    
    return false;
}

bool ExecTone(PlatformCommand *cmd)
{
    float numval;
    int freq, duration;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        freq = (int)numval;
        if (EvaluateNumExpr(cmd->arg2, &numval))
        {
            duration = (int)numval;
            Tone((uint16_t)freq, (uint16_t)duration);
            return true;
        }
    }
    
    return false;
}

bool ExecBeep(PlatformCommand *cmd)
{
    Tone((uint16_t)BEEP_TONE, (uint16_t)BEEP_DURATION);
    return true;
}

bool ExecLeds(PlatformCommand *cmd)
{
    float numval;
    int value;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        value = (int)numval;
        Leds((uint16_t)value);
        return true;
    }
    
    return false;
}

bool ExecDisplay(PlatformCommand *cmd)
{
    float numval;
    int value, displayQty;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        value = (int)numval;
        if (EvaluateNumExpr(cmd->arg2, &numval))
        {
            displayQty = (int)numval;
            Display7((uint16_t)value, (uint8_t)displayQty);
            return true;
        }
    }
    
    return false;
}

// putchar row,col,value
bool ExecPutchar(PlatformCommand *cmd)
{
    float numval;
    int row, col, value;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        row = (int)numval;
        if (EvaluateNumExpr(cmd->arg2, &numval))
        {
            col = (int)numval;
            if (EvaluateNumExpr(cmd->arg3, &numval))
            {
                value = (int)numval;
                GfxPutChar((uint8_t)row, (uint8_t)col, (uint8_t)value);
                return true;
            }
        }
    }
    
    return false;
}

// putdb id,row,col,value
bool ExecPutDB(PlatformCommand *cmd)
{
    float numval;
    int row, col, value;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        row = (int)numval;
        if (EvaluateNumExpr(cmd->arg2, &numval))
        {
            col = (int)numval;
            if (EvaluateNumExpr(cmd->arg3, &numval))
            {
                value = (int)numval;
                GfxPutDB((uint8_t)row, (uint8_t)col, (uint8_t)value);
                return true;
            }
        }
    }
    
    return false;
}

bool ExecLoadFB(PlatformCommand *cmd)
{
    GfxLoadFB();
    return true;
}

bool ExecClear(PlatformCommand *cmd)
{
    GfxClearScreen();
    return true;
}

bool ExecClearDB(PlatformCommand *cmd)
{
    GfxClearDB();
    return true;
}

bool ExecText(PlatformCommand *cmd)
{
    textMode = true;
    GfxTextMode(1);
    return true;
}

bool ExecGr(PlatformCommand *cmd)
{
    textMode = false;
    GfxTextMode(0);
    return true;
}

bool ExecOutchar(PlatformCommand *cmd)
{
    float numval;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        sprintf(message, "%c", (int)numval);
        Console(message);
        return true;
    }
    
    return false;
}

bool ExecRseed(PlatformCommand *cmd)
{
    float numval;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        srand((unsigned int)numval);
        return true;
    }
    
    return false;
}

bool ExecDelay(PlatformCommand *cmd)
{
    float numval;
    int duration;
    
    if (EvaluateNumExpr(cmd->arg1, &numval))
    {
        duration = (int)numval;
        Delay((uint16_t)duration);
        return true;
    }
    
    return false;
}

// dim : DIM {Numvar | Strvar} '(' expr [',' expr]+ ')'
bool ExecDim(DimCommand *cmd)
{
    int size = 1, i;
    
    // load the dim size array, padding with 0's for unused higher order dimensions
    for (i = 0; i < DIM_MAX - SYM_DIM(cmd->varsym); i++)
    {
        SYM_DIMSIZES(cmd->varsym, i) = 0;
    }
    for (int j = 0; j < SYM_DIM(cmd->varsym); j++, i++)
    {
        if (!EvaluateNumExpr(cmd->dimSizeNodes[j], &SYM_DIMSIZES(cmd->varsym, i)))
        {
            strcpy(errorStr, "invalid dim expression");
            return false;
        }
        size *= SYM_DIMSIZES(cmd->varsym, i);
    }
    
    // ensure total number of array elements will fit into a variable
    if (size > ARRAY_MAX)
    {
        strcpy(errorStr, "too many array elements");
        return false;
    }
    
    return true;
}

bool ExecBreak(PlatformCommand *cmd)
{
    return true;
}

bool ExecBuiltinFct(const char *name)
{
    if (!strcmp(name, "peek"))
    {
        // 1 unsigned int arg
        NumStackPush(MemRead((uint16_t)NumStackPop()));
    } 
    else if (!strcmp(name, "rnd"))
    {
        // 1 unsigned int arg
        NumStackPush(rand() % ((uint16_t)NumStackPop()));
    } 
    else if (!strcmp(name, "abs"))
    {
        // 1 float arg
        NumStackPush(fabsf(NumStackPop()));
    } 
    else if (!strcmp(name, "switches"))
    {
        // no args
        NumStackPush(Switches());
    } 
    else if (!strcmp(name, "buttons"))
    {
        // no args
        NumStackPush(Buttons());
    } 
    else if (!strcmp(name, "getchar"))
    {
        // 2 unsigned int args, row (TOS), col
        uint16_t col = (uint16_t)NumStackPop();
        uint16_t row = (uint16_t)NumStackPop();
        NumStackPush(GfxGetChar(row, col));
    }
    else if (!strcmp(name, "getdb"))
    {
        // 2 unsigned int args, row (TOS), col
        uint16_t col = (uint16_t)NumStackPop();
        uint16_t row = (uint16_t)NumStackPop();
        NumStackPush(GfxGetDB(row, col));
    }
    else
    {
        strcpy(errorStr, "unknown builtin function");
        return false;
    } 
        
    return true;       
}

// return the value of a numberic expression based on the traversal of its expr tree
bool EvaluateNumExpr(Node *exprTreeRoot, float *pValue)
{
    if (TraverseSyntaxTree(exprTreeRoot))
    {
        *pValue = NumStackPop();
        return true;
    }

    return false;
}

// return the value of a string expression based on the traversal of its expr tree
bool EvaluateStrExpr(Node *exprTreeRoot, char **pValue)
{
    if (TraverseSyntaxTree(exprTreeRoot))
    {
        *pValue = StrStackPop();
        return true;
    }

    return false;
}

bool TraverseSyntaxTree(Node *node)
{
    bool retval = true;
    float indeces[4] = {0};
    int intval;
    float numval;
    char *strval;
    
    if (node != NULL)
    {
        switch (NODE_TYPE(node))
        {
            case NT_BINOP:
                // binop
                //   opndL opndR
                // push the operands onto the num stack
                retval &= TraverseSyntaxTree(node->son);
                retval &= TraverseSyntaxTree(node->bro);
                switch (NODE_VAL_OP(node))
                {    
                    case AND_OP:
                        // put the logical AND of the top 2 expr stack entries onto the top of the stack
                        NumStackPut(NumStackPop() && NumStackTop());
                        break;
                        
    
                    case OR_OP:
                        // put the logical OR of the top 2 expr stack entries onto the top of the stack
                        NumStackPut(NumStackPop() || NumStackTop());
                        break;
                        
    
                    case XOR_OP:
                        // put the exclusive OR of the top 2 expr stack entries onto the top of the stack
                        NumStackPut((float)((int)NumStackPop() ^ (int)NumStackTop()));
                        break;
                                                
                    case '=':
                        // put the equality relation of the top 2 expr stack entries onto the top of the stack
                        NumStackPut(NumStackPop() == NumStackTop());
                        break;                        
                        
                    case NE_OP:
                        // put the non-equality relation of the top 2 expr stack entries onto the top of the stack
                        NumStackPut(NumStackPop() != NumStackTop());
                        break;     
                                           
                    case '>':
                        // put the > relation of the top 2 expr stack entries onto the top of the stack
                        numval = NumStackPop();
                        NumStackPut(NumStackTop() > numval);
                        break;
                            
                    case GE_OP:
                        // put the >= relation of the top 2 expr stack entries onto the top of the stack
                        numval = NumStackPop();
                        NumStackPut(NumStackTop() >= numval);
                        break;
                        
                    case '<':
                        // put the < relation of the top 2 expr stack entries onto the top of the stack
                        numval = NumStackPop();
                        NumStackPut(NumStackTop() < numval);
                        break;
                        
    
                    case LE_OP:
                        // put the <= relation of the top 2 expr stack entries onto the top of the stack
                        numval = NumStackPop();
                        NumStackPut(NumStackTop() <= numval);
                        break;
                        
                    case '+':
                        // put the sum of the top 2 expr stack entries onto the top of the stack
                        NumStackPut(NumStackPop() + NumStackTop());
                        break;
                        
                    case '-':
                        // put the difference of the top 2 expr stack entries onto the top of the stack
                        numval = NumStackPop();
                        NumStackPut(NumStackTop() - numval);
                        break;
                        
                    case '*':
                        // put the product of the top 2 expr stack entries onto the top of the stack
                        NumStackPut(NumStackPop() * NumStackTop());
                        break;
                        
                    case '/':
                        // put the quotient of the top 2 expr stack entries onto the top of the stack
                        numval = NumStackPop();
                        NumStackPut(NumStackTop() / numval);
                        break;
                        
                    // NOTE: the following operators can only operate on
                    case '%':
                    case MOD_OP:
                        // put the modulus of the next to top of stack value by the top of stack value onto the top of the stack
                        intval = (int)NumStackPop();
                        NumStackPut((int)NumStackTop() % intval);
                        break;
                        
                    case SL_OP:
                        // put the left shift of the next to top of stack value by the top of stack onto the top of the stack
                        intval = (int)NumStackPop();
                        NumStackPut((int)NumStackTop() << intval);
                        break;
                        
                    case SR_OP:
                        // put the right shift of the next to top of stack value by the top of stack onto the top of the stack
                        intval = (int)NumStackPop();
                        NumStackPut((int)NumStackTop() >> intval);
                        break;
                    default:
                        break;
                }
                break;
                
            case NT_UNOP:
                // unop
                //   opnd
                // push the operand onto the num stack
                retval &= TraverseSyntaxTree(node->son);
                switch (NODE_VAL_OP(node))
                {
                    case '+':
                        // do nothing to make something positive
                        break;
                        
                    case '-':
                        // negate the top of the expression
                        NumStackPut(-NumStackTop());
                        break;
                        
                    case '~':
                        // negate the top of the expression
                        NumStackPut((float)(~(int)NumStackTop()));
                        break;
                        
                    case NOT_OP:
                        // logically invert the top of the expression stack
                        NumStackPut(!NumStackTop());
                        break;
                }
                break;
                
            case NT_CONSTANT:
                NumStackPush(NODE_VAL_CONST(node));
                break;
                
            case NT_STRING:
                StrStackPush(NODE_VAL_STRING(node));
                break;
                
            // note: for arrays, indeces are the decendents pop the indeces into an index array then read the value, 
            // the popped values will be the reverse of the needed indeces so reverse them in the indeces array
            case NT_NUMVAR:
                // process vector
                if (SYM_DIM(NODE_VAL_VARSYM(node)) > 0)
                {
                    int indexQty = 0;
                    
                    // push the indeces onto the stack
                    for (Node *next = node->bro; next; next = next->bro)
                    {
                        // args are expressions which could be empty so only process non-empty arg expressions
                        if (next->son)
                        {
                            TraverseSyntaxTree(next);
                            indexQty++;
                        }
                    }
                
                    // check that the dim of the array equals the qty of indeces parsed
                    if (SYM_DIM(NODE_VAL_VARSYM(node)) == indexQty)
                    {                
                        // build the index list from the num stack in reverse order
                        for (int i = 0; i < SYM_DIM(NODE_VAL_VARSYM(node)); i++)
                        {
                            indeces[DIM_MAX-1 - i] = (int)NumStackPop();
                        }
                        if ((retval = SymReadNumvar(NODE_VAL_VARSYM(node), indeces, &numval)))
                        {
                            NumStackPush(numval);
                        }
                        else
                        {
                            strcpy(errorStr, "index out of range");
                            retval &= false;
                        }
                    }
                    else
                    {
                        strcpy(errorStr, "subscript error: dim of the number array does not equal the qty of indeces parsed");
                        retval &= false;
                    }
                }
                
                // process scalar
                else if ((retval = SymReadNumvar(NODE_VAL_VARSYM(node), indeces, &numval)))
                {
                    NumStackPush(numval);
                }

                break;

            case NT_STRVAR:         
                // process vector
                if (SYM_DIM(NODE_VAL_VARSYM(node)) > 0)
                {
                    int indexQty = 0;
                    
                    // push the indeces onto the stack
                    for (Node *next = node->bro; next; next = next->bro)
                    {
                        // args are expressions which could be empty so only process non-empty arg expressions
                        if (next->son)
                        {
                            TraverseSyntaxTree(next);
                            indexQty++;
                        }
                    }
                
                    // check that the dim of the array equals the qty of indeces parsed
                    if (SYM_DIM(NODE_VAL_VARSYM(node)) == indexQty)
                    {                
                        // build the index list from the num stack in reverse order
                        for (int i = 0; i < SYM_DIM(NODE_VAL_VARSYM(node)); i++)
                        {
                            indeces[DIM_MAX-1 - i] = (int)NumStackPop();
                        }
                        if ((retval = SymReadStrvar(NODE_VAL_VARSYM(node), indeces, &strval)))
                        {
                            StrStackPush(strval);
                        }
                        else
                        {
                            strcpy(errorStr, "index out of range");
                            retval &= false;
                        }
                    }
                    else
                    {
                        strcpy(errorStr, "subscript error: dim of the string array does not equal the qty of indeces parsed");
                        retval &= false;
                    }
                }

                // process scalar
                else if ((retval = SymReadStrvar(NODE_VAL_VARSYM(node), indeces, &strval)))
                {
                    StrStackPush(strval);
                }

                break;
                
            case NT_FCT:
                {
                    int indexQty = 0;
                    
                    // push the args onto the stack
                    for (Node *next = node->bro; next; next = next->bro)
                    {
                        // args are expressions which could be empty so only process non-empty arg expressions
                        if (next->son)
                        {
                            TraverseSyntaxTree(next);
                            indexQty++;
                        }
                    }
                    
                    // check that the arity of the function equals the qty of args parsed
                    if (SYM_DIM(NODE_VAL_VARSYM(node)) == indexQty)
                    {                
                        // exec the builtin fct which will put the result on the stack
                        retval = ExecBuiltinFct(SYM_NAME(NODE_VAL_VARSYM(node)));
                    }
                    else
                    {
                        strcpy(errorStr, "incorrect number of arguments for builtin function");
                        retval &= false;
                    }
                }
                break;
                
            case NT_EXPR:
                TraverseSyntaxTree(node->son);
                break;

            default:
                break;
        }
    }
    
    return retval;
}


// STACKS
// TODO: add call stack push/pop functions

// number stack functions
float NumStackPush(float a)
{
    if (numSP < STACK_SIZE)
    {
        numStack[numSP++] = a;
        return a;
    }
    Panic("num stack overflow in NumStackPush\n");
    return 0;
}
float NumStackPop(void)
{
    if (numSP > 0)
    {
        return numStack[--numSP];
    }
    Panic("num stack underflow in NumStackPop\n");
    return 0;
} 
float NumStackTop(void)
{
    if (numSP > 0)
    {
        return numStack[numSP-1];
    }
    Panic("num stack underflow in NumStackTop()\n");
    return 0;
} 
float NumStackPut(float a)
{
    if (numSP > 0)
    {
        numStack[numSP-1] = a;
        return a;
    }
    Panic("num stack underflow in NumStackPut()\n");
    return 0;
}

// string stack functions
char *StrStackPush(char *a)
{
    if (strSP < STACK_SIZE)
    {
        strStack[strSP++] = a;
        return a;
    }
    Panic("num stack overflow in StrStackPush\n");
    return 0;
}
char *StrStackPop(void)
{
    if (strSP > 0)
    {
        return strStack[--strSP];
    }
    Panic("num stack underflow in StrStackPop\n");
    return NULL;
} 
char *StrStackTop(void)
{
    if (strSP > 0)
    {
        return strStack[strSP-1];
    }
    Panic("num stack underflow in StrStackTop()\n");
    return NULL;
} 
char *StrStackPut(char *a)
{
    if (strSP > 0)
    {
        strStack[strSP-1] = a;
        return a;
    }
    Panic("num stack underflow in StrStackPut()\n");
    return NULL;
}


// HELPER FUNCTIONS

// free a command list
void FreeCommand(Command *cmd)
{
    if (cmd)
    {
        if (cmd->next)
        {
            FreeCommand(cmd->next);
        }
        free(cmd);
    }
}

// free all allocated commands in a command line
void FreeCommandList(Command *commandList)
{
    if (commandList)
    {
        if (commandList->next)
        {
            FreeCommand(commandList->next);
        }
        commandList->next = NULL;
    }
}

// free all allocated commands in the program
void FreeProgram(void)
{
    for (int i = 0; i < MAX_PROGRAM_LEN; i++)
    {
        FreeCommandList(Program[i].commandList);
        Program[i] = emptyCommandLine;
    }
}

Command *IterateCmdPtr(bool cmdListOnly)
{
    if (cmdPtr->next)
    {
        // next command in the command line
        return cmdPtr->next;
    }
    else if ((!cmdListOnly) && (cmdListIdx < programSize))
    {
        // first command in the next command line
        return Program[++cmdListIdx].commandList;
    }
    
    return NULL;
}

// convert a line number to a program index
bool LineNum2CmdLineIdx(int lineNum)
{
    for (int i = 0; i < programSize; i++)
    {
        if (Program[i].lineNum == lineNum)
        {
            cmdListIdx = i;
            return true;
        }
    }
    
    return false;
}

void SwapProgLines(int progIdxA, int progIdxB) 
{ 
    // copy a to temp, b to a, then temp to b
    CommandLine temp = Program[progIdxA]; 
    Program[progIdxA] = Program[progIdxB]; 
    Program[progIdxB] = temp; 
} 

void SortProgramByLineNum(void) 
{ 
    int i, j, min_idx; 
  
    // One by one move boundary of unsorted subarray 
    for (i = 0; i < programSize - 1; i++) 
    { 
        // Find the minimum element in unsorted array 
        min_idx = i; 
        for (j = i + 1; j < programSize; j++) 
            if (Program[j].lineNum < Program[min_idx].lineNum) 
                min_idx = j; 
  
        // Swap the found minimum element with the first element 
        SwapProgLines(min_idx, i); 
    } 
} 

void PrintResult(void)
{
    if (resultStr[0] != '\0')
    {
        PutString(resultStr);
        PutString("\n");
        resultStr[0] = '\0';
    }
}

// install builtin functions
bool InstallBuiltinFcts(void)
{
    for (int i = 0; i < builtinFctTableSize; i++)
    {
        strcpy(tokenStr, builtinFctTab[i].name);
        if (SymLookup(Function))
        {
            // for a function symbol the arity is stored as the dimension of the variable
            SYM_DIM(lexval.lexsym) = builtinFctTab[i].arity;
        }
        else
        {
            return false;
        }
    }
    
    return true;
}








// end of runtime.c

