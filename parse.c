#define _PARSE_C_

#include "9cc.h"

/*  文法：
- A.2.4 External Definitions
    translation_unit        = external_declaration*
    external_declaration    = function_definition | declaration
    function_definition     = declaration_specifiers declarator compound_statement
- A.2.2 Declarations
    declaration             = declaration_specifiers init_declarator ( "," init_declarator )* ";"
    declaration_specifiers  = "typeof" "(" identifier ")"
                            | type_specifier          declaration_specifiers*
                            | storage_class_specifier declaration_specifiers*
                            | type_qualifier          declaration_specifiers*
    init_declarator         = declarator ( "=" initializer )?
    storage_class_specifier  = "static" | "extern"
    type_specifier          = "void" | "char" | "short" | "int" | "long" | "signed" | "unsigned"
    type_qualifier          = "const"
    declarator              = pointer? direct_declarator
    direct_declarator       = identifier | "(" declarator ")"
                            | direct_declarator "[" assign? "]"
                            | direct_declarator "(" parameter_type_list? ")"    //関数
    parameter_type_list     = parameter_declaration ( "," parameter_declaration )* ( "," "..." )?
    parameter_declaration   = declaration_specifiers ( declarator | abstract_declarator )?
    initializer             = assign
                            | "{" init_list "}"
                            | "{" init_list "," "}"
    init_list               = initializer
                            | init_list "," initializer
    array_def   = "[" expr? "]" ( "[" expr "]" )*
    pointer                 = ( "*" type_qualifier* )*
    type_name               = "typeof" "(" identifier ")"
                            | type_specifier abstract_declarator*
                            | type_qualifier abstract_declarator*
    abstract_declarator     = pointer? direct_abstract_declarator
    direct_abstract_declarator = "(" abstract_declarator ")"
                            | direct_abstract_declarator? "[" assign? "]"
                            | direct_abstract_declarator? "(" parameter_type_list? ")"  //関数
- A.2.3 Statements
    statement   = compound_statement
                | declaration
                | return" expr ";"
                | "if" "(" expr ")" ( "else" expr )?
                | "while" "(" expr ")"
                | "for" "(" expr? ";" expr? ";" expr? ")" statement
                | "break"
                | "continue"
    compound_statement      = "{" declaration | statement* "}"
- A.2.1 Expressions
    expr        = assign ( "," assign )* 
    assign      = tri_cond ( "=" assign | "+=" assign | "-=" assign )*
    tri_cond    = logical_or "?" expr ":" tri_cond
    logical_or  = logical_and ( "||" logical_and )*
    logical_and = bitwise_or ( "&&" bitwise_or )*
    bitwise_or  = ex_or ( "&" ex_or )*
    ex_or       = bitwise_and ( "^" bitwise_and )*
    bitwise_and = equality ( "&" equality )*
    equality    = relational ( "==" relational | "!=" relational )*
    relational  = add ( "<" add | "<=" add | ">" add | ">=" add )*
    add         = mul ( "+" mul | "-" mul )*
    mul         = unary ( "*" unary | "/" unary | "%" unary )*
    unary       = post_unary
                | ( "+" | "-" |  "!" | "*" | "&" ) unary
                | ( "++" | "--" )? unary
                | "sizeof" unary
                | "sizeof" "(" type_name ")"
                | "_Alignof" "(" type_name ")"
    post_unary  = term ( "++" | "--" | "[" expr "]")?
    term        = num
                | string
                | identifier
                | identifier "(" expr? ")"   //関数コール
                |  "(" expr ")"
*/
static Node *external_declaration(void);
static Node *function_definition(Type *tp, char *name);
static Node *declaration(Type *tp, char *name);
static Node *init_declarator(Type *decl_spec, Type *tp, char *name);
static Node *declarator(Type *decl_spec, Type *tp, char *name);
static Node *direct_declarator(Type *tp, char *name);
static Node *parameter_type_list(void);
static Node *parameter_declaration(void);
static Node *initializer(void);
static Node *init_list(void);
static Type *pointer(Type *tp);
static Type *type_name(void);
static Type *abstract_declarator(Type *tp);
static Type *direct_abstract_declarator(Type *tp);
static Type *declaration_specifiers(void);
static Type *array_def(Type *tp);

static Node *statement(void);
static Node *compound_statement(void);
static Node *expr(void);
static Node *assign(void);
static Node *tri_cond(void);
static Node *equality(void);
static Node *logical_or(void);
static Node *logical_and(void);
static Node *bitwise_or(void);
static Node *ex_or(void);
static Node *bitwise_and(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void); 
static Node *unary(void);
static Node *post_unary(void);
static Node *term(void); 

void translation_unit(void) {
    while (!token_is(TK_EOF)) {
        external_declaration();
        cur_funcdef = NULL;
    }
}

//    external_declaration    = function_definition | declaration
//    function_definition     = declaration_specifiers declarator compound_statement
//    declaration             = declaration_specifiers init_declarator ( "," init_declarator )* ";"
static Node *external_declaration(void) {
    Node *node;
    Type *tp;
    char *name;
    //          <-> declaration_specifiers
    //             <-> pointer* (declarator|init_declarator)
    //                <-> identifier (declarator|init_declarator)
    // 関数定義：int * foo (int a){
    // 関数宣言：int * foo (int a);
    //          int * foo (int);
    // 変数定義：int * ptr ;
    //          int   abc = 1;
    //          int   ary [4];
    //                    ^ここまで読まないと区別がつかない
    // ネスト：　int * ( )
    //                ^ここでdeclarationが確定
    if (token_is_type_spec()) {
        tp = declaration_specifiers();
        tp = pointer(tp);
        if (token_is('(')) {    //int * ()
            node = declaration(tp, NULL);
        } else if (!consume_ident(&name)) {
            error_at(input_str(), "型名の後に識別名がありません");
        } else if (token_is('(')) {
            node = function_definition(tp, name);
        } else {
            node = declaration(tp, name);
            //if (!consume(';')) error_at(input_str(), "; がありません");
        }
    } else {
        error_at(input_str(), "関数・変数の定義がありません");
    }
    return node;
}

//関数の定義: lhs=引数(ND_LIST)、rhs=ブロック(ND_BLOCK)
//トップレベルにある関数定義もここで処理する。
//    function_definition     = declaration_specifiers declarator compound_statement
static Node *function_definition(Type *tp, char *name) {
    Node *node;
    Type *decl_spec = tp;
    assert(name!=NULL);
    while (decl_spec->ptr_of) decl_spec = decl_spec->ptr_of;
    node = declarator(decl_spec, tp, name);
    if (token_is('{')) {
        check_funcargs(node->lhs, 1);   //引数リストの妥当性を確認（定義モード）
        node->rhs = compound_statement();
        map_put(funcdef_map, node->name, cur_funcdef);
    } else {
        consume(';');
        check_funcargs(node->lhs, 0);   //引数リストの妥当性を確認（宣言モード）
        node->type = ND_FUNC_DECL;
    }

    return node;
}

//    declaration             = declaration_specifiers init_declarator ( "," init_declarator )* ";"
//    init_declarator         = declarator ( "=" initializer )?
//    declarator              = pointer? direct_declarator
//declaration_specifiers, pointer, identifierまで先読み済み
static Node *declaration(Type *tp, char *name) {
    Node *node;
    Type *decl_spec;

    if (tp==NULL) {
        //declaration_specifiers, pointer, identifierを先読みしていなければdecl_specを読む
        decl_spec = declaration_specifiers();
    } else {
        // 関数定義：int * foo (){}
        // 変数定義：int * ptr ;
        //          int   abc = 1;
        //          int   ary [4];
        //                   ^ここまで先読み済みであるので、ポインタを含まないdecl_spec（int）を取り出す
        // int * ( *x )
        //       ^ここまで先読み済み
        decl_spec = tp;
        while (decl_spec->ptr_of) decl_spec = decl_spec->ptr_of;
    }

    node = init_declarator(decl_spec, tp, name);

    if (consume(',')) {
        Node *last_node;
        node = new_node_list(node, node->input);
        Vector *lists = node->lst;
        vec_push(lists, last_node=init_declarator(decl_spec, NULL, NULL));
        while (consume(',')) {
            vec_push(lists, last_node=init_declarator(decl_spec, NULL, NULL));
        }
        node->tp = last_node->tp;
    }
    if (!consume(';')) error_at(input_str(), "; がありません");
    return node;
}

//    init_declarator         = declarator ( "=" initializer )?
//    declarator              = pointer? direct_declarator
//declaration_specifiers, pointer, identifierまで先読みの可能性あり
static Node *init_declarator(Type *decl_spec, Type *tp, char *name) {
    Node *node, *rhs=NULL;
    char *input = input_str();

    Node *tmp_node = declarator(decl_spec, tp, name);
    tp   = tmp_node->tp;
    name = tmp_node->name;

    //初期値: rhsに初期値を設定する
    if (consume('=')) {
        if (type_is_extern(tp)) error_at(input, "extern変数は初期化できません");
        //node->rhsに変数=初期値の形のノードを設定する。->そのまま初期値設定のコード生成に用いる
        rhs = initializer();
        long val;
        if (tp->type==ARRAY) {  //左辺が配列
            if (rhs->tp->type==ARRAY) {
                //文字列リテラルで初期化できるのはcharの配列だけ
                if (rhs->type==ND_STRING && tp->ptr_of->type!=CHAR)
                    error_at(rhs->input, "%sを文字列リテラルで初期化できません", get_type_str(tp));
                if (tp->array_size<0) {
                    tp->array_size = rhs->tp->array_size;
                }
            } else if (rhs->type==ND_LIST) {
                if (tp->array_size<0) {
                    tp->array_size = rhs->lst->len;
                }
            } else {
                error_at(rhs->input, "配列の初期値が配列形式になっていません");
            }
        }
        if (node_is_const(rhs, &val)) {
            rhs = new_node_num(val, rhs->input);
        }
    }

    node = new_node_var_def(name, tp, input); //ND_(LOCAL|GLOBAL)_VAR_DEF
    if (rhs) node->rhs = new_node('=', new_node_var(name, input), rhs, tp, input);

    //初期値のないサイズ未定義のARRAYはエラー
    //externの場合はOK
    if (node->tp->type==ARRAY && node->tp->array_size<0 && !type_is_extern(node->tp) &&
        (node->rhs==NULL || node->rhs->type!='='))
        error_at(input_str(), "配列のサイズが未定義です");

    //グローバルスカラー変数の初期値は定数または固定アドレスでなければならない
    if ((node->type==ND_GLOBAL_VAR_DEF || type_is_static(node->tp)) && 
        node->tp->type!=ARRAY && node->rhs &&
        node->rhs->rhs->type!=ND_STRING) {  //文字列リテラルはここではチェックしない
        long val;
        Node *var=NULL;
        if (!node_is_const_or_address(node->rhs->rhs, &val, &var))
            error_at(node->rhs->rhs->input, "グローバル変数の初期値が定数ではありません");
        if (var) {
            if (val) {
                node->rhs->rhs = new_node('+', var, new_node_num(val, input), var->tp, input);
            } else {
                node->rhs->rhs = var;
            }
        }
    }

    if (node->type==ND_LOCAL_VAR_DEF && type_is_static(node->tp)) {
        char *name = node->name;
        //ローカルstatic変数は初期値を外してreturnする。初期化はvarderのリストから実施される。
        node = new_node(ND_LOCAL_VAR_DEF, NULL, NULL, node->tp, node->input);
        node->name = name;
    }
    return node;
}

//    declarator              = pointer* direct_declarator
//    direct_declarator       = identifier | "(" declarator ")"
//                            = direct_declarator "[" assign? "]"
//                            | direct_abstract_declarator? "(" parameter_type_list? ")"  //関数宣言
//declaration_specifiers, pointer, identifierまで先読み済みの可能性あり
//戻り値のnodeはname、lhs（関数の場合の引数）、tp以外未設定
static Node *declarator(Type *decl_spec, Type *tp, char *name) {
    Node *node;
    if (tp==NULL) {
        tp = pointer(decl_spec);
        name = NULL;
    }
    node = direct_declarator(tp, name);

    return node;
}
static Node *direct_declarator(Type *tp, char *name) {
    Node *node, *lhs=NULL;
    char *input = input_str();
    if (name == NULL) {
        if (consume('(')) {
            node = declarator(tp, NULL, name);
            tp   = node->tp;
            name = node->name;
            if (!consume(')')) error_at(input_str(), "閉じかっこがありません");
        } else {
            if (!consume_ident(&name)) error_at(input_str(), "型名の後に識別名がありません");
        }
    }
    if (consume('(')) { //関数定義・宣言確定
        Funcdef *org_funcdef = cur_funcdef;
        node = new_node_func_def(name, tp, input);
        node->lhs = parameter_type_list();
        if (!consume(')')) error_at(input_str(), "閉じかっこがありません");
        if (org_funcdef) cur_funcdef = org_funcdef;
        return node;
    } else if (tp->type==VOID) {
        error_at(input_str(), "不正なvoid指定です"); 
    }
    
    if (token_is('[')) tp = array_def(tp);
    node = new_node(ND_LOCAL_VAR_DEF, lhs, NULL, tp, NULL);
    node->name = name;
    node->input = input;
    return node;
}

//    parameter_type_list     = parameter_declaration ( "," parameter_declaration )* ( "," "..." )?
//    parameter_declaration   = declaration_specifiers ( declarator | abstract_declarator )?
static Node *parameter_type_list(void) {
    Node *node = new_node_list(NULL, input_str());
    Node *last_node;
    char *vararg_ptr = NULL;
    if (!token_is_type_spec()) return node; //空のリスト

    vec_push(node->lst, last_node=parameter_declaration());
    while (consume(',')) {
        char *input = input_str();
        if (token_is('}')){
            break;
        } else if (consume(TK_3DOTS)) { //...（可変長引数）
            last_node = new_node(ND_VARARGS, NULL, NULL, NULL, input);
            vararg_ptr = input;
        } else {
            last_node = parameter_declaration();
            last_node->input = input;
        }
        vec_push(node->lst, last_node);
    }
    if (vararg_ptr && last_node->type!=ND_VARARGS)
        error_at(vararg_ptr, "...の位置が不正です");
    return node;
}
static Node *parameter_declaration(void) {
    Node *node;
    char *name, *input = input_str();
    Type *tp = declaration_specifiers();
    tp = pointer(tp);
    if (consume_ident(&name)) {
        node = declarator(tp, tp, name);
        regist_var_def(node);
    } else {
        tp = abstract_declarator(tp);
        node = new_node_var_def(NULL, tp, input);
    }
    return node;
}

//    initializer = assign
//                | "{" init_list "}"
//                | "{" init_list "," "}"
static Node *initializer(void) {
    Node *node;

    if (consume('{')) {
        node = init_list();
        consume(',');
        if (!consume('}')) error_at(input_str(),"初期化式の閉じカッコ } がありません");
    } else {
        node = assign();
    }
    return node;
}

//    init_list   = initializer
//                | init_list "," initializer
static Node *init_list(void) {
    Node *node = new_node_list(NULL, input_str());
    Node *last_node;

    vec_push(node->lst, last_node=initializer());
    while (consume(',') && !token_is('}')) {
        vec_push(node->lst, last_node=initializer());
    }
    node->tp = last_node->tp;

    return node;
}

//    array_def   = "[" assign? "]"
static Type *array_def(Type *tp) {
    Type *ret_tp = tp;
    // int *a[10][2][3]
    if (consume('[')) {
        if (consume(']')) { //char *argv[];
            tp = new_type_array(tp, -1);    //最初だけ省略できる（初期化が必要）
        } else {
            char *input = input_str();
            Node *node = expr();
            long val;
            if (!node_is_const(node, &val)) error_at(input, "配列サイズが定数ではありません");
            if (val==0) error_at(input, "配列のサイズが0です");
            tp = new_type_array(tp, val);
            if (!consume(']')) error_at(input_str(), "配列サイズの閉じかっこ ] がありません"); 
        }
        ret_tp = tp;
        // ret_tp=tp=ARRAY[10] -> PTR -> INT 
    }

    while (consume('[')) {
        char *input = input_str();
        Node *node = expr();
        long val;
        if (!node_is_const(node, &val)) error_at(input, "配列サイズが定数ではありません");
        if (val==0) error_at(input, "配列のサイズが0です");
        tp->ptr_of = new_type_array(tp->ptr_of, val);
        tp = tp->ptr_of;
        if (!consume(']')) error_at(input_str(), "配列サイズの閉じかっこ ] がありません"); 
        // ARRAYのリストの最後に挿入してゆく
        // ret_tp=tp=ARRAY[10]                               -> PTR -> INT 
        // ret_tp=   ARRAY[10] -> tp=ARRAY[2]                -> PTR -> INT 
        // ret_tp=   ARRAY[10] ->    ARRAY[2] -> tp=ARRAY[3] -> PTR -> INT 
    }

    return ret_tp;
}

//    pointer                 = ( "*" type_qualifier* )*
static Type *pointer(Type *tp) {
    int is_const = 0;
    if (tp->type==CONST) {
        is_const = 1;
        tp = tp->ptr_of;
    }
    while (consume('*')) {
        tp = new_type_ptr(tp);
        if (is_const) tp->is_const = 1;
        is_const = 0;
        while (consume(TK_CONST)) is_const = 1;
    }
    if (is_const) {
        Type *tmp = tp;
        while (tmp->ptr_of) tmp = tmp->ptr_of;
        tmp->is_const = 1;       
    }

    return tp;
}

//    type_name               = "typeof" "(" identifier ")"
//                            | type_specifier abstract_declarator*
//                            | type_qualifier abstract_declarator*
//    abstract_declarator     = pointer? direct_abstract_declarator
//    direct_abstract_declarator = "(" abstract_declarator ")"
//                            | direct_abstract_declarator? "[" assign "]"
//                            | direct_abstract_declarator? "(" parameter_type_list? ")"  //関数
static Type *type_name(void) {
    Type *tp = declaration_specifiers();
    if (tp->is_static) error_at(input_str(), "staticは指定できません");
    if (tp->is_extern) error_at(input_str(), "externは指定できません");
    tp = abstract_declarator(tp);
    return tp;
}
static Type *abstract_declarator(Type *tp) {
    tp = pointer(tp);
    tp = direct_abstract_declarator(tp);
    return tp;
}
static Type *direct_abstract_declarator(Type *tp) {
    if (consume('(')){
        tp = abstract_declarator(tp);
        if (!consume(')')) error_at(input_str(), "閉じかっこがありません");
    }
    if (token_is('[')) tp = array_def(tp);
    return tp;
}

//    declaration_specifiers  = "typeof" "(" identifier ")"
//                            | type_specifier         declaration_specifiers*
//                            | storage_class_specifier declaration_specifiers*
//                            | type_qualifier         declaration_specifiers*
static Type *declaration_specifiers(void) {
    Type *tp;

    if (consume(TK_TYPEOF)) {
        if (!consume('(')) error_at(input_str(), "typeofの後に開きカッコがありません");
        char *name;
        if (!consume_ident(&name)) error_at(input_str(), "識別子がありません");
        Node *node = new_node_var(name, NULL);
        tp = get_typeof(node->tp);
        if (!consume(')')) error_at(input_str(), "typeofの後に閉じカッコがありません");
    	return tp;
    }

    char *input;
    Typ type = 0;
    int is_unsigned = -1;
    int is_static = -1; //0:extern
    int pre_const = 0;  //型の前にconstがある：const int
    int post_const = 0; //型の後にconstがある：int const

    while (1) {
        input = input_str();
        if (consume(TK_VOID)) {
            if (type) error_at(input, "型指定が不正です\n");
            type = VOID;
        } else if (consume(TK_CHAR)) {
            if (type) error_at(input, "型指定が不正です\n");
            type = CHAR;
        } else if (consume(TK_SHORT)) {
            if (type && type!=INT) error_at(input, "型指定が不正です\n");
            type = SHORT;
        } else if (consume(TK_INT)) {
            if (type && type!=SHORT && type!=LONG && type!=LONGLONG) error_at(input, "型指定が不正です\n");
            type = INT;
        } else if (consume(TK_LONG)) {
            if (type==LONG) type = LONGLONG;
            else if (type && type!=INT) error_at(input, "型指定が不正です\n");
            else type = LONG;
        } else if (consume(TK_SIGNED)) {
            if (is_unsigned>=0) error_at(input, "型指定が不正です\n");
            is_unsigned = 1;
        } else if (consume(TK_UNSIGNED)) {
            if (is_unsigned>=0) error_at(input, "型指定が不正です\n");
            is_unsigned = 1;
        } else if (consume(TK_STATIC)) {
            if (is_static>=0) error_at(input, "strage classが重複しています\n");
            is_static = 1;
        } else if (consume(TK_EXTERN)) {
            if (is_static>=0) error_at(input, "strage classが重複しています\n");
            is_static = 0;
        } else if (consume(TK_CONST)) {
            if (type == 0) pre_const = 1;
            else           post_const = 1;
        } else {
            if (!type && is_unsigned>=0) type = INT;
            if (!type) error_at(input, "型名がありません\n");
            break;
        }
        if (type==VOID && is_unsigned>=0)
            error_at(input, "void型の指定が不正です\n");
    }

    if (is_unsigned<0) is_unsigned = 0;
    tp = new_type(type, is_unsigned);
    if (is_static==1) tp->is_static = 1;
    if (is_static==0) tp->is_extern = 1;
    if (pre_const)    tp->is_const = 1;
    if (post_const) {
        Type *tmp = tp;
        tp = new_type(CONST, 0);
        tp->ptr_of = tmp;
    }
    return tp;
}

static Node *statement(void) {
    Node *node;
    char *input = input_str();

    if (token_is('{')) {
        return compound_statement();    //{ ブロック }
    } else if (consume(';')) {
        return new_node_empty(input_str());
    } else if (consume(TK_RETURN)) {
        node = expr();
        node = new_node(ND_RETURN, node, NULL, node->tp, input);
    } else if (consume(TK_IF)) {    //if(A)B else C
        Node *node_A, *node_B;
        if (!consume('(')) error_at(input_str(), "ifの後に開きカッコがありません");
        node_A = expr();
        if (!consume(')')) error_at(input_str(), "ifの開きカッコに対応する閉じカッコがありません");
        input = input_str();
        node_B = statement();
        node = new_node(0, node_A, node_B, NULL, input); //lhs
        input = input_str();
        if (consume(TK_ELSE)) {
            node = new_node(ND_IF, node, statement(), NULL, input);
        } else {
            node = new_node(ND_IF, node, NULL, NULL, input);
        }
        return node;
    } else if (consume(TK_WHILE)) {
        if (!consume('(')) error_at(input_str(), "whileの後に開きカッコがありません");
        node = expr();
        if (!consume(')')) error_at(input_str(), "whileの開きカッコに対応する閉じカッコがありません");
        node = new_node(ND_WHILE, node, statement(), NULL, input);
        return node;
    } else if (consume(TK_FOR)) {   //for(A;B;C)D
        Node *node1, *node2;
        if (!consume('(')) error_at(input_str(), "forの後に開きカッコがありません");
        if (consume(';')) {
            node1 = new_node_empty(input_str());
        } else {
            node1 = expr();         //A
            if (!consume(';')) error_at(input_str(), "forの1個目の;がありません");
        }
        if (consume(';')) {
            node2 = new_node_empty(input_str());
        } else {
            node2 = expr();         //B
            if (!consume(';')) error_at(input_str(), "forの2個目の;がありません");
        }
        node = new_node(0, node1, node2, NULL, input);       //A,B
        if (consume(')')) {
            node1 = new_node_empty(input_str());
        } else {
            node1 = expr();         //C
            if (!consume(')')) error_at(input_str(), "forの開きカッコに対応する閉じカッコがありません");
        }
        node2 = new_node(0, node1, statement(), NULL, input);     //C,D
        node = new_node(ND_FOR, node, node2, NULL, input);   //(A,B),(C,D)
        return node;
    } else if (consume(TK_BREAK)) {     //break
        node = new_node(ND_BREAK, NULL, NULL, NULL, input);
    } else if (consume(TK_CONTINUE)) {  //continue
        node = new_node(ND_CONTINUE, NULL, NULL, NULL, input);
    } else {
        node = expr();
    }

    if (!consume(';')) {
        error_at(input_str(), ";でないトークンです");
    }
    return node;
}

static Node *compound_statement(void) {
    Node *node;

    if (!consume('{')) error_at(input_str(), "{がありません");

    node = new_node_block(input_str());
    while (!consume('}')) {
        Node *block;
        if (token_is_type_spec()) {
            block = declaration(NULL, NULL);
        } else {
            block = statement();
        }
        vec_push(node->lst, block);
    }
    return node;
}

//expr：単なる式またはそのコンマリスト（左結合）
//リストであればリスト(ND_LIST)を作成する
//    expr        = assign ( "," assign )* 
static Node *expr(void) {
    char *input = input_str();
    Node *node = assign();
    Node *last_node = node;
    if (consume(',')) {
        node = new_node_list(node, input);
        Vector *lists = node->lst;
        vec_push(lists, last_node=assign());
        while (consume(',')) {
            vec_push(lists, last_node=assign());
        }
    } else {
        return node;
    }
    node->tp = last_node->tp;
    return node;
}

//代入（右結合）
static Node *assign(void) {
    Node *node = tri_cond(), *rhs;
    char *input = input_str();
    if (consume('=')) {
        if (node->tp->type==ARRAY) error_at(node->input, "左辺値ではありません");
        rhs = assign(); 
        if (!(rhs->type==ND_NUM && rhs->val==0) &&  //右辺が0の場合は無条件にOK
            !node_type_eq(node->tp, rhs->tp))
            warning_at(input, "=の左右の型(%s:%s)が異なります", 
                get_type_str(node->tp), get_type_str(rhs->tp));
        node = new_node('=', node, rhs, node->tp, input); //ND_ASIGN
    } else if (consume(TK_PLUS_ASSIGN)) { //+=
        if (node->tp->type==ARRAY) error_at(node->input, "左辺値ではありません");
        rhs = assign(); 
        if (node_is_ptr(node) && node_is_ptr(rhs))
            error_at(node->input, "ポインタ同士の加算です");
        node = new_node(ND_PLUS_ASSIGN, node, rhs, node->tp, input);
    } else if (consume(TK_MINUS_ASSIGN)) { //-=
        if (node->tp->type==ARRAY) error_at(node->input, "左辺値ではありません");
        rhs = assign(); 
        if node_is_ptr(rhs)
            error_at(node->input, "ポインタによる減算です");
        node = new_node(ND_MINUS_ASSIGN, node, rhs, node->tp, input);
    }
    return node;
}

//三項演算子（右結合）
//    tri_cond    = logical_or "?" expr ":" tri_cond
static Node *tri_cond(void) {
    Node *node = logical_or(), *sub_node, *lhs, *rhs;
    char *input = input_str();
    if (consume('?')) {
        lhs = expr();
        if (!consume(':'))
            error_at(node->input, "三項演算に : がありません");
        rhs = tri_cond();
        sub_node = new_node(0, lhs, rhs, lhs->tp, input);
        node = new_node(ND_TRI_COND, node, sub_node, lhs->tp, node->input);
    }
    return node;
}

//論理和（左結合）
//    logical_or  = logical_and ( "||" logical_and )*
static Node *logical_or(void) {
    Node *node = logical_and();
    for (;;) {
        char *input = input_str();
        if (consume(TK_LOR)) {
            node = new_node(ND_LOR, node, logical_and(), new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//論理積（左結合）
//    logical_and = bitwise_or ( "&&" bitwise_or )*
static Node *logical_and(void) {
    Node *node = bitwise_or();
    for (;;) {
        char *input = input_str();
        if (consume(TK_LAND)) {
            node = new_node(ND_LAND, node, bitwise_or(), new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//OR（左結合）
//    bitwise_or = ex_or ( "|" ex_or )*
static Node *bitwise_or(void) {
    Node *node = ex_or();
    for (;;) {
        char *input = input_str();
        if (consume('|')) {
            node = new_node('|', node, ex_or(), new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//EX-OR（左結合）
//    bitwise_or = bitwise_and ( "|" bitwise_and )*
static Node *ex_or(void) {
    Node *node = bitwise_and();
    for (;;) {
        char *input = input_str();
        if (consume('^')) {
            node = new_node('^', node, bitwise_and(), new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//AND（左結合）
//    bitwise_and = equality ( "&" equality )*
static Node *bitwise_and(void) {
    Node *node = equality();
    for (;;) {
        char *input = input_str();
        if (consume('&')) {
            node = new_node('&', node, equality(), new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//等価演算（左結合）
//    equality    = relational ( "==" relational | "!=" relational )*
static Node *equality(void) {
    Node *node = relational();
    for (;;) {
        char *input = input_str();
        if (consume(TK_EQ)) {
            node = new_node(ND_EQ, node, relational(), new_type(INT, 0), input);
        } else if (consume(TK_NE)) {
            node = new_node(ND_NE, node, relational(), new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//関係演算（左結合）
//    relational  = add ( "<" add | "<=" add | ">" add | ">=" add )*
static Node *relational(void) {
    Node *node = add();
    for (;;) {
        char *input = input_str();
        if (consume('<')) {
            node = new_node('<',   node, add(), new_type(INT, 0), input);
        } else if (consume(TK_LE)) {
            node = new_node(ND_LE, node, add(), new_type(INT, 0), input);
        } else if (consume('>')) {
            node = new_node('<',   add(), node, new_type(INT, 0), input);
        } else if (consume(TK_GE)) {
            node = new_node(ND_LE, add(), node, new_type(INT, 0), input);
        } else {
            break;
        }
    }
    return node;
}

//加減算（左結合）
//    add         = mul ( "+" mul | "-" mul )*
static Node *add(void) {
    Node *node = mul(), *rhs;
    for (;;) {
        char *input = input_str();
        if (consume('+')) {
            rhs = mul();
            if (node_is_ptr(node) && node_is_ptr(rhs))
                error_at(node->input, "ポインタ同士の加算です");
            Type *tp = node_is_ptr(node) ? node->tp : rhs->tp;
            node = new_node('+', node, rhs, tp, input);
        } else if (consume('-')) {
            rhs = mul();
            if (node_is_ptr(rhs))
                error_at(input, "ポインタによる減算です");
            node = new_node('-', node, rhs, rhs->tp, input);
        } else {
            break;
        }
    }
    return node;
}

//乗除算、剰余（左結合）
//    mul         = unary ( "*" unary | "/" unary | "%" unary )*
static Node *mul(void) {
    Node *node = unary();
    for (;;) {
        char *input = input_str();
        if (consume('*')) {
            node = new_node('*', node, unary(), node->tp, input);
        } else if (consume('/')) {
            node = new_node('/', node, unary(), node->tp, input);
        } else if (consume('%')) {
            node = new_node('%', node, unary(), node->tp, input);
        } else {
            break;
        }
    }
    return node;
}

//前置単項演算子（右結合）
//    unary       = post_unary
//                | ( "+" | "-" |  "!" | "*" | "&" ) unary
//                | ( "++" | "--" )? unary
//                | "sizeof" unary
//                | "sizeof"   "(" type_name ")"
//                | "_Alignof" "(" type_name ")"
static Node *unary(void) {
    Node *node;
    char *input = input_str();
    if (consume('+')) {
        node = unary();
    } else if (consume('-')) {
        node = unary();
        node = new_node('-', new_node_num(0, input), node, node->tp, input);
    } else if (consume('!')) {
        node = new_node('!', NULL, unary(), new_type(INT, 0), input);
    } else if (consume(TK_INC)) {
        node = unary();
        node = new_node(ND_INC_PRE, NULL, node, node->tp, input);
    } else if (consume(TK_DEC)) {
        node = unary();
        node = new_node(ND_DEC_PRE, NULL, node, node->tp, input);
    } else if (consume('*')) {
        node = unary();
        if (!node_is_ptr(node)) 
            error_at(node->input, "'*'は非ポインタ型(%s)を参照しています", get_type_str(node->tp));
        node = new_node(ND_INDIRECT, NULL, node, node->tp->ptr_of, input);
    } else if (consume('&')) {
        node = unary();
        node = new_node(ND_ADDRESS, NULL, node, new_type_ptr(node->tp), input);
    } else if (consume(TK_SIZEOF)) {
        Type *tp;
        if (token_is('(')) {
            if (next_token_is_type_spec()) {
                consume('(');
                tp = type_name();
                if (!consume(')')) error_at(input_str(), "開きカッコに対応する閉じカッコがありません");
            } else {
                node = unary();
                tp = node->tp;
            }
        } else {
            node = unary();
            tp = node->tp;
        }
        node = new_node_num(size_of(tp), input);
    } else if (consume(TK_ALIGNOF)) {
        Type *tp;
        if (!consume('(')) error_at(input_str(), "開きカッコがありません");
        tp = type_name();
        if (!consume(')')) error_at(input_str(), "開きカッコに対応する閉じカッコがありません");
        node = new_node_num(align_of(tp), input);
    } else {
        node = post_unary();
    }
    return node;
}

//後置単項演算子（左結合）
//    post_unary  = term ( "++" | "--" | "[" expr "]")?
static Node *post_unary(void) {
    Node *node = term();
    for (;;) {
        char *input = input_str();
        Type *tp = node->tp;
        if (consume(TK_INC)) {
            node = new_node(ND_INC, node, NULL, node->tp, input);
        } else if (consume(TK_DEC)) {
            node = new_node(ND_DEC, node, NULL, node->tp, input);
        } else if (consume('[')) {
            // a[3] => *(a+3)
            // a[3][2] => *(*(a+3)+2)
            input = input_str();
            Node *rhs = expr();
            node = new_node('+', node, rhs, tp ,input);
            tp = node->tp->ptr_of ? node->tp->ptr_of : rhs->tp->ptr_of;
            if (tp==NULL) error_at(input_str(), "ここでは配列を指定できません");
            node = new_node(ND_INDIRECT, NULL, node, tp, input);
            if (!consume(']')) {
                error_at(input_str(), "配列の開きカッコに対応する閉じカッコがありません");
            }
        } else {
            break;
        }
    }
    return node;
}

//終端記号：数値、識別子（変数、関数）、カッコ
static Node *term(void) {
    Node *node;
    char *name;
    char *input = input_str();
    if (consume('(')) {
        node = expr();
        if (!consume(')')) {
            error_at(input_str(), "開きカッコに対応する閉じカッコがありません");
        }
    } else if (consume(TK_NUM)) {
        node = new_node_num(tokens[token_pos-1]->val, input);
    } else if (consume(TK_STRING)) {
        node = new_node_string(tokens[token_pos-1]->str, input);
    } else if (consume_ident(&name)) {
        if (consume('(')) { //関数コール
            node = new_node_func_call(name, input);
            if (consume(')')) return node;
            node->lhs = expr();
            if (node->lhs->type != ND_LIST) {
                node->lhs = new_node_list(node->lhs, input);
            }
            if (!consume(')')) {
                error_at(input_str(), "関数コールの開きカッコに対応する閉じカッコがありません");
            }
        } else {
            node = new_node_var(name, input);
        }
    } else {
        error_at(input, "終端記号でないトークンです");
    }

    return node;
}
