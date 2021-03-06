#include "util.h"

typedef enum {
    PPTK_SHARP  = '#',
    PPTK_LPAREN = '(',
    PPTK_RPAREN = ')',
    PPTK_COMMA  = ',',
    PPTK_NUM    = 256,
    PPTK_DSHARP,    //##
    PPTK_STRING,    //"文字列"
    PPTK_IDENT,     //識別子
    PPTK_PPTOKEN,   //
    PPTK_SPACE,     //SPACE+TAB
    PPTK_NEWLINE,   //'\n'
    PPTK_IF,        //#if
    PPTK_IFDEF,     //#ifdef
    PPTK_IFNDEF,    //#ifndef
    PPTK_ELIF,      //#elif
    PPTK_ELSE,      //#else
    PPTK_ENDIF,     //#endif
    PPTK_INCLUDE,   //#include
    PPTK_DEFINE,    //#define
    PPTK_UNDEF,     //#undef
    PPTK_LINE,      //#line
    PPTK_ERROR,     //#error
    PPTK_PRAGMA,    //#pragma
    PPTK_DEFARG,    //defineの引数に置換される要素
    PPTK_EOF,       //入力の終わり
} PPTKtype;

typedef struct {
    int start;          //pptokensの開始インデックスと長さ
    int len;            //
} PPTKrange;

typedef struct {
    const char *name;   //マクロ名
    Vector *args;       //引数のリスト（char*）
    PPTKrange range;    //マクロの本体：pptokensの開始インデックスと長さ
    char in_use;        //多重展開防止フラグ
} PPMacro;

typedef struct {
    PPTKtype type;      //トークンの型
    char *ident;        //PPTK_IDENTの場合の識別子
    int len;            //トークンの長さ
    long val;           //PPTK_NUMの場合の値、PPTK_DEFARGの場合の引数のindex
    PPMacro *macro;     //PPTK_DEFINEの場合のマクロ定義
    SrcInfo info;       //ソースファイルの情報
} PPToken;

void preprocessing_file(void);
const char *get_PPTKtype_str(PPTKtype type);
void dump_token(PPToken *token, const char *str);
void dump_tokens(void);
void dump_token_range(PPTKrange *range, const char *str);
void cpp_tokenize(char *p);
PPMacro *new_macro(const char*name);

EXTERN Vector *pptoken_vec;
EXTERN PPToken **pptokens;  //token_vec->data;
EXTERN int pptoken_pos;     //tokensの現在位置

#define next_token_type()   (pptokens[pptoken_pos+1]->type)
#define next_token_is(_tp)  (next_token_type()==(_tp))

EXTERN Map *define_map;

//デバッグオプション
EXTERN int g_dump_token;
EXTERN FILE *g_fp;

//現在のトークン
#define cur_token() (pptokens[pptoken_pos])
//現在のトークンのsソースファイル情報
#define cur_token_info() (pptokens[pptoken_pos]->info)
//現在のトークン（エラー箇所）の入力文字列
#define input_str() (pptokens[pptoken_pos]->input)
