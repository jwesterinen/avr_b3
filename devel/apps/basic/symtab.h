/*
 * symtab.h -- definitions for symbol table
 */

#define DIM_MAX 4
#define ARRAY_MAX 100

#define SYM_NAME(symbol)                ((symbol)->name)
#define SYM_TYPE(symbol)                ((symbol)->type)
#define SYM_DIM(symbol)                 ((symbol)->dim)
#define SYM_DIMSIZES(symbol, index)     ((symbol)->dimSizes[(index)])

enum SYMTYPE {ST_NUMVAR, ST_STRVAR, ST_FCT};
typedef struct Symbol
{
    char *name;
    enum SYMTYPE type;
    union
    {
        float numvals[ARRAY_MAX];
        char *strvals[ARRAY_MAX];
    } value;
    float dim;                  // the dimension of an array, e.g. dim a(2,3,4) dim = 3, or arity of a fct
    float dimSizes[DIM_MAX];    // the size of each array dimension, e.g. dim a(2,3,4) dimSizes = {2,3,4,0}, or fct arg values
    struct Symbol *next;
} Symbol;

bool SymLookup(int token);
Symbol *SymFind(const char *name);
bool SymConvertToArray(Symbol *varsym, int size);
void FreeSymtab(void);
bool SymReadNumvar(Symbol *varsym, float indeces[4], float *value);
bool SymWriteNumvar(Symbol *varsym, float indeces[4], float value);
bool SymReadStrvar(Symbol *varsym, float indeces[4], char **value);
bool SymWriteStrvar(Symbol *varsym, float indeces[4], char *value);

#ifdef DEBUG_ALLOCS
void *LocalCalloc(size_t nmemb, size_t size);
void LocalFree(void *ptr);
unsigned GetAllocMemQty(void);
void ClearAllocMemQty(void);
#endif

