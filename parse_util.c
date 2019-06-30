#define _PARSE_C_

#include "9cc.h"

//型のサイズ
// - 配列の場合、要素のサイズ*要素数
// - 構造体の場合、アラインメントで単位切り上げ
long size_of(const Type *tp) {
    assert(tp);
    switch (tp->type) {
    case VOID:     return sizeof(void);
    case CHAR:     return sizeof(char);
    case SHORT:    return sizeof(short);
    case INT:      return sizeof(int);
    case LONG:     return sizeof(long);
    case LONGLONG: return sizeof(long long);
    case ENUM:     return sizeof(int);
    case PTR:      return sizeof(void*);
    case ARRAY:
        if (tp->array_size<0) return sizeof(void*);
        else return tp->array_size * size_of(tp->ptr_of);
    case FUNC:     return sizeof(void*);
    default:    //CONST,NEST
        assert(0);
    }
    _ERROR_;
    return -1;
}

//型のアラインメント
// - 配列の場合、要素のアラインメント
// - 構造体の場合、メンバー内の最大のアラインメント
int align_of(const Type *tp) {
    assert(tp);
    if (tp->type==ARRAY) return align_of(tp->ptr_of);
    return size_of(tp);
}

static long calc_node(Node *node, long val1, long val2) {
    long val;
    switch ((int)node->type) {
    case ND_LAND:   val = val1 && val2; break;
    case ND_LOR:    val = val1 || val2; break;
    case '&':       val = val1 &  val2; break;
    case '^':       val = val1 ^  val2; break;
    case '|':       val = val1 |  val2; break;
    case ND_EQ:     val = val1 == val2; break;
    case ND_NE:     val = val1 != val2; break;
    case '<':       val = val1 <  val2; break;
    case ND_LE:     val = val1 <= val2; break;
    case ND_SHIFTL: val = val1 << val2; break;
    case ND_SHIFTR: val = val1 >> val2; break;
    case '+':       val = val1 +  val2; break;
    case '-':       val = val1 -  val2; break;
    case '*':       val = val1 *  val2; break;
    case '/':       val = val1 /  val2; break;
    case '%':       val = val1 %  val2; break;
    case '!':       val = !val1;        break;
    case '~':       val = ~val1;        break;
    default:
        assert(0);
    }
    return val;
}

//nodeが定数になっているかどうかを調べる。valpに定数を返す
int node_is_const(Node *node, long *valp) {
    long val, val1, val2;

    switch (node->type) {
    case ND_NUM:
    case ND_ENUM:
        val = node->val;
        break;
    case ND_TRI_COND:
        if (!node_is_const(node->lhs, &val)) return 0;
        if (val) {
            if (!node_is_const(node->rhs->lhs, &val)) return 0;
        } else {
            if (!node_is_const(node->rhs->rhs, &val)) return 0;
        }
        if (valp) *valp = val;
        return 1;
    case ND_CAST:
        return node_is_const(node->rhs, valp);
    case ND_LIST:
        //if (vec_len(node->lst))
        //    return node_is_const(vec_data(node->lst, vec_len(node->lst)-1), valp);
        //return 0;
    case ND_LOCAL_VAR:
    case ND_GLOBAL_VAR:
    case ND_STRING:
        return 0;
    default:
        if (node->lhs && !node_is_const(node->lhs, &val1)) return 0;
        if (node->rhs && !node_is_const(node->rhs, &val2)) return 0;
        val = calc_node(node, val1, val2);
    }
    if (valp) *valp = val;
    return 1;
}

//nodeが定数またはアドレス+定数の形式になっているかどうかを調べる。varpにND_ADDRESS(&var)のノード、valpに定数を返す
int node_is_const_or_address(Node *node, long *valp, Node **varp) {
    long val, val1, val2;
    Node *var1=NULL, *var2=NULL;

    if ((node->type==ND_GLOBAL_VAR || type_is_static(node->tp))
        && node->tp->type==ARRAY) {
        if (varp) *varp = new_node(ND_ADDRESS, NULL, node, node->tp->ptr_of, node->input);
        if (valp) *valp = 0;
        return 1;
    }

    switch (node->type) {
    case ND_NUM:
    case ND_ENUM:
        val = node->val;
        break;
    case ND_LIST:
        if (vec_len(node->lst))
            return node_is_const_or_address(vec_data(node->lst, vec_len(node->lst)-1), valp, varp);
        return 0;
    case ND_LOCAL_VAR:
    case ND_GLOBAL_VAR:
    case ND_STRING:
        return 0;
    case ND_ADDRESS:
        if (node->rhs->type==ND_GLOBAL_VAR || type_is_static(node->rhs->tp)) {
            if (varp) *varp = node;
            if (valp) *valp = 0;
            return 1;
        } else {
            return 0;
        }
    case ND_TRI_COND:
        if (!node_is_const_or_address(node->lhs, &val, varp)) return 0;
        if (val) {
            if (!node_is_const_or_address(node->rhs->lhs, &val, varp)) return 0;
        } else {
            if (!node_is_const_or_address(node->rhs->rhs, &val, varp)) return 0;
        }
        if (valp) *valp = val;
        return 1;
    case ND_CAST:
        return node_is_const_or_address(node->rhs, valp, varp);
    default:
        if (node->lhs && !node_is_const_or_address(node->lhs, &val1, &var1)) return 0;
        if (node->rhs && !node_is_const_or_address(node->rhs, &val2, &var2)) return 0;
        if (var1 && var2) {
            return 0;
        } else if (var1) {
            if (node->type=='+' || node->type=='-') {
                if (varp) *varp = var1;
            } else {
                return 0;
            }
        } else if (var2) {
            if (node->type=='+') {
                if (varp) *varp = var2;
            } else {
                return 0;
            }
        }
        val = calc_node(node, val1, val2);
    }

    if (valp) *valp = val;
    return 1;
}

// static char*p; のような宣言ではPTRにはsclassが設定されず、
// charだけに設定されているのでcharのところまで見に行く
int type_is_static(Type *tp) {
    if (tp->ptr_of) return type_is_static(tp->ptr_of);
    return tp->sclass==SC_STATIC;
}

int type_is_extern(Type *tp) {
    if (tp->ptr_of) return type_is_extern(tp->ptr_of);
    return tp->sclass==SC_EXTERN;
}

// ==================================================
// 以下はparse.cローカル

//未登録の変数であればローカル変数またはグローバル変数として登録する
void regist_var_def(Node *node) {
    char *name = node->name;
    if (cur_funcdef && !type_is_extern(node->tp)) {  //関数内かつexternでなければローカル変数
        Map *symbol_map = stack_top(symbol_stack); 
        if (map_get(symbol_map, name, NULL)==0) {
            if (type_is_static(node->tp)) {
                node->offset = global_index++;
            } else {
                node->offset = get_var_offset(node->tp);
            }
            if (node->type==ND_UNDEF) node->type = ND_LOCAL_VAR_DEF;
            map_put(symbol_map, name, node);

            if (type_is_static(node->tp)) {
                vec_push(static_var_vec, node);
            }
        } else {
            error_at(node->input, "'%s'はローカル変数の重複定義です", name);
        }
    } else {            //グローバル変数
        if (node->type==ND_UNDEF) node->type = ND_GLOBAL_VAR_DEF;
        if (map_get(global_symbol_map, name, NULL)==0) {
            map_put(global_symbol_map, name, node);
        } else if (!type_is_extern(node->tp)) {
            error_at(node->input, "'%s'はグローバル変数の重複定義です", name);
        }
    }
}

//enum値を登録
void regist_symbol(Node *node) {
    char *name = node->name;
    Node *node2;
    Map *symbol_map = stack_top(symbol_stack); 
    if (map_get(symbol_map, name, (void**)&node2)==0) {
        map_put(symbol_map, name, node);
    } else {
        error_at(node->input, "'%s'はシンボルの重複定義です", name);
    }
}

//タグ名を登録
void regist_tagname(Node *node) {
    char *name = node->name;
    Node *node2;
    Map *tagname_map = stack_top(tagname_stack); 
    if (map_get(tagname_map, name, (void**)&node2)==0) {
        map_put(tagname_map, name, node);
    } else if (node2->type==ND_ENUM_DEF && node->type==ND_ENUM_DEF) {
        if (node2->lst!=NULL && node->lst!=NULL)
            error_at(node->input, "enumの重複定義です");
    } else {
        error_at(node->input, "'%s'はシンボルの重複定義です", name);
    }
}

//ラベルを登録
//ラベル定義より前にgotoが先にあった場合、gotoを登録する。その後ラベル定義があればそれで置き換える。
//ラベル定義のないgotoはcodegen時にチェックする。
void regist_label(Node *node) {
    Node *node2;
    if (map_get(cur_funcdef->label_map, node->name, (void**)&node2)!=0) {
        if (node->type==ND_GOTO) return;
        if (node2->type==ND_LABEL) error_at(node->input, "ラベルが重複しています");
    }
    map_put(cur_funcdef->label_map, node->name, node);
}

//重複をチェックしてcase,defaultをcur_switch->mapに登録する。
void regist_case(Node *node) {
    if (cur_switch==NULL) error_at(node->input, "switch文の中ではありません");
    char *name;
    if (node->type==ND_DEFAULT) {
        name = "Default";
    } else {
        char buf[64];
        if (node->val>=0) sprintf(buf, "Case%ld", node->val);
        else              sprintf(buf, "CaseM%ld", -node->val);
        name = malloc(strlen(buf)+1);
        strcpy(name, buf);
    }
    node->name = name;
    if (map_get(cur_switch->map, name, NULL)!=0)
        error_at(node->input, "%sは重複しています", name);
    map_put(cur_switch->map, name, node);
}

//storage classを外したTypeの複製を返す。
Type *get_typeof(Type *tp) {
    Type *ret = malloc(sizeof(Type));
    memcpy(ret, tp, sizeof(Type));
    ret->sclass = SC_UNDEF;
    if (tp->ptr_of) ret->ptr_of = get_typeof(tp->ptr_of);
    return ret;
}

//戻り値の妥当性をチェック
void check_return(Node *node) {
    assert(node->type==ND_RETURN);
    assert(cur_funcdef);
    assert(cur_funcdef->tp->type==FUNC);
    Type *func_tp = cur_funcdef->tp->ptr_of;
    Type *ret_tp = node->tp;
    if (func_tp->type==VOID) {
        if (ret_tp==NULL) return;
        if (ret_tp->type==VOID) return;
        warning_at(node->input, "void型関数が値を返しています");
    } else if (ret_tp==NULL || ret_tp->type==VOID) {
        warning_at(node->input, "非void関数%sが値を返していません", cur_funcdef->name);
    } else if (!type_eq_assign(func_tp, ret_tp)) {
        warning_at(node->input, "%s型の関数%sが%s型を返しています",
            get_type_str(func_tp), cur_funcdef->name, get_type_str(ret_tp));
    }
}

//関数が戻り値を返しているかをチェック（簡易版）
void check_func_return(Funcdef *funcdef) {
    Type *func_tp = cur_funcdef->tp->ptr_of;
    if (func_tp->type==VOID) return;    //void関数が値を返す場合はreturn側でチェックする
    Node **nodes = (Node**)funcdef->node->rhs->lst->data;
    int len = funcdef->node->rhs->lst->len;
    if (len==0 || nodes[len-1]->type!=ND_RETURN) {
        warning_at(input_str()-1, "関数が戻り値を返していません");
    }

}

//引数リスト(ND_LIST)の妥当性を確認
//関数定義(def_mode=1)の場合は名前付き宣言(declaration)であることを確認
void check_funcargs(Node *node, int def_mode) {
    int size = node->lst->len;
    Node **arg_nodes = (Node**)node->lst->data;
    for (int i=0; i < size; i++) {
        Node *arg = arg_nodes[i];
        if (arg->type==ND_VARARGS) continue;
        if (arg->tp->type==VOID) {
            if (i>0) error_at(arg->input, "ここではvoidを指定できません");
            continue;
        }
        if (def_mode && arg->name==NULL)
            error_at(arg->input, "関数定義の引数には名前が必要です");
    }
}

//タイプが等しいかどうかを判定する（storage classは無視。constも今は無視）
int type_eq(const Type *tp1, const Type *tp2) {
    if (tp1->type != tp2->type) return 0;
    if (tp1->is_unsigned != tp2->is_unsigned) return 0;
    if (tp1->ptr_of) return type_eq(tp1->ptr_of, tp2->ptr_of);
    return 1;
}

//node1=node2の代入の観点で型の一致を判定する
int type_eq_assign(const Type *tp1, const Type *tp2) {
    if ((tp1->type==PTR && tp2->type==ARRAY) ||
        (type_is_integer(tp1) && type_is_integer(tp2))) {
        ;
    } else if (tp1->type==PTR && tp1->ptr_of->type==FUNC &&
               tp2->type==FUNC) {   //関数ポインタに対する関数名の代入
        tp1 = tp1->ptr_of;
    } else {
        if (tp1->type != tp2->type) return 0;
    }  
    if (tp1->ptr_of) return type_eq_assign(tp1->ptr_of, tp2->ptr_of);
    return 1;
}

//ローカル変数のRBPからのoffset（バイト数）を返し、var_stack_sizeを更新する。
int get_var_offset(const Type *tp) {
    int size = size_of(tp);
    int align_size = align_of(tp);
    cur_funcdef->var_stack_size += size;
    // アラインメント（align_sizeバイト単位に切り上げ）
    if (size>0) cur_funcdef->var_stack_size = (cur_funcdef->var_stack_size + (size-1))/align_size * align_size;
    return cur_funcdef->var_stack_size;
}

//関数定義のroot生成
Funcdef *new_funcdef(void) {
    Funcdef * funcdef;
    funcdef = calloc(1, sizeof(Funcdef));
    funcdef->symbol_map  = new_map();
    funcdef->tagname_map = new_map();
    funcdef->label_map   = new_map();
    return funcdef;
}

//型情報の生成
Type *new_type_ptr(Type*ptr) {
    Type *tp = calloc(1, sizeof(Type));
    tp->type = PTR;
    tp->ptr_of = ptr;
    return tp;
}

Type *new_type_func(Type*ptr) {
    Type *tp = calloc(1, sizeof(Type));
    tp->type = FUNC;
    tp->ptr_of = ptr;
    return tp;
}

Type *new_type_array(Type*ptr, size_t size) {
    Type *tp = calloc(1, sizeof(Type));
    tp->type = ARRAY;
    tp->ptr_of = ptr;
    tp->array_size = size;
    return tp;
}

Type *new_type(int type, int is_unsigned) {
    Type *tp = calloc(1, sizeof(Type));
    tp->type = type;
    tp->is_unsigned = is_unsigned;
    return tp;
}

//抽象構文木の生成（演算子）
Node *new_node(int type, Node *lhs, Node *rhs, Type *tp, char *input) {
    Node *node = calloc(1, sizeof(Node));
    node->type = type;
    node->lhs  = lhs;
    node->rhs  = rhs;
    node->tp   = tp;
    node->input = input;
    return node;
}

//抽象構文木の生成（数値）
Node *new_node_num(long val, char *input) {
    NDtype type = (val>UINT_MAX || val<INT_MIN) ? LONG : INT;
    Node *node = new_node(ND_NUM, NULL, NULL, new_type(type, 0), input);
    node->val = val;
//    fprintf(stderr, "val=%ld\n", val);
    return node;
}

//抽象構文木の生成（変数定義）
Node *new_node_var_def(char *name, Type*tp, char *input) {
    Node *node = new_node(ND_UNDEF, NULL, NULL, tp, input);
    node->name = name;
    if (name) regist_var_def(node);
    return node;
}

//抽象構文木の生成（文字列リテラル）
Node *new_node_string(char *string, char *input) {
    Type *tp = new_type_array(new_type(CHAR, 0), strlen(string)+1);
    Node *node = new_node(ND_STRING, NULL, NULL, tp, input);
    node->val = string_vec->len;    //インデックス
    vec_push(string_vec, string);

    return node;
}

//nameをスコープの内側から検索してNodeを返す
static Node *search_symbol(const char *name) {
    Node *node = NULL;
    for (int i=stack_len(symbol_stack)-1; i>=0; i--) {
        Map *symbol_map = (Map*)stack_get(symbol_stack, i);
        if (map_get(symbol_map, name, (void**)&node)!=0) {
            return node;
        }
    }
    return NULL;
}

//抽象構文木の生成（識別子：ローカル変数・グローバル変数）
Node *new_node_ident(char *name, char *input) {
    Node *node, *var_def;
    NDtype type;
    Type *tp;
    long offset = 0;

    //定義済みの変数であるかをスコープの内側からチェック
    var_def = search_symbol(name);
    if (var_def) {
        switch (var_def->type) {
        case ND_LOCAL_VAR_DEF:
            type = ND_LOCAL_VAR;
            break;
        case ND_GLOBAL_VAR_DEF:
        case ND_FUNC_DEF:       
        case ND_FUNC_DECL:
            type = ND_GLOBAL_VAR;
            break;
        case ND_ENUM:
            return var_def;
        default:
            assert(0);
        }
        tp = var_def->tp;
        offset = var_def->offset;
    } else {
        type = ND_IDENT;
        tp = NULL;
    }

    node = new_node(type, NULL, NULL, tp, input);
    node->name   = name;
    node->offset = offset;
    //dump_node(node, __func__);

    return node;
}

//抽象構文木の生成（関数コール）
Node *new_node_func_call(char *name, char *input) {
    Node *node, *func_node;
    Type *tp;
    func_node = search_symbol(name);
    if (func_node) {
        switch (func_node->type) {
        case ND_LOCAL_VAR_DEF:  //関数ポインタ
        case ND_GLOBAL_VAR_DEF: //関数ポインタ
            if (func_node->tp->type!=PTR || func_node->tp->ptr_of->type!=FUNC)
                error_at(input, "%sは関数ではありません", name);
            node = new_node(func_node->type==ND_LOCAL_VAR_DEF?ND_LOCAL_VAR:ND_GLOBAL_VAR,
                            NULL, NULL, func_node->tp, func_node->input);
            node->name = name;
            node->offset = func_node->offset;
            func_node = node;
            // PTR->FUNC->int
            //            ^---ここを指す
            tp = func_node->tp->ptr_of->ptr_of;
            break;
        case ND_FUNC_DEF:       
        case ND_FUNC_DECL:
            assert(func_node->tp->type==FUNC);
            tp = func_node->tp->ptr_of;
            break;
        default:
            error_at(input, "不正な関数コールです。");
        }
    } else {
        warning_at(input, "未宣言の関数コールです。");
        tp = new_type(INT, 0);
    }
    node = new_node(ND_FUNC_CALL, NULL, func_node, tp, input);
    node->name = name;
//  node->lhs   //引数リスト
//  node->rhs   //コール先を示すノード

    return node;
}

//抽象構文木の生成（関数定義）
Node *new_node_func_def(char *name, Type *tp, char *input) {
    Node *node = new_node(ND_FUNC_DEF, NULL, NULL, tp, input);
    node->name = name;
//  node->lhs       //引数リスト(ND_LIST)
//  node->rhs       //ブロック(ND_BLOCK)

    cur_funcdef = new_funcdef();
    cur_funcdef->tp = tp;
    cur_funcdef->node = node;
    cur_funcdef->name = node->name;
    stack_push(symbol_stack,  cur_funcdef->symbol_map);     //popはfunction_definition()で行う
    stack_push(tagname_stack, cur_funcdef->tagname_map);    //popはfunction_definition()で行う

    //関数をシンボルとして登録する
    Node *def_node;
    if (map_get(global_symbol_map, name, (void**)&def_node)==0)  {
        map_put(global_symbol_map, name, node);
    } else if (def_node->type==ND_FUNC_DEF) {
        error_at(input, "関数が再定義されています");
    } else if (def_node->type==ND_GLOBAL_VAR_DEF) {
        error_at(input, "'%s'は異なる種類のシンボルとして再定義されています", name);
    }
    return node;
}

//抽象構文木の生成（空文）
Node *new_node_empty(char *input) {
    Node *node = new_node(ND_EMPTY, NULL, NULL, NULL, input);
    return node;
}

//抽象構文木の生成（ブロック）
Node *new_node_block(char *input) {
    Node *node = new_node(ND_BLOCK, NULL, NULL, NULL, input);
    node->lst  = new_vector();
    return node;
}

//抽象構文木の生成（コンマリスト）
//型は最後の要素の型であるべきだがここでは設定しない
Node *new_node_list(Node *item, char *input) {
    Node *node = new_node(ND_LIST, NULL, NULL, NULL, input);
    node->lst  = new_vector();
    if (item) vec_push(node->lst, item);
    return node;
}
