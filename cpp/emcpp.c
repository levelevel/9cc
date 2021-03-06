#define EXTERN

#include "emcpp.h"

char *cpp(const char *fname) {
    char *p=NULL;
    return p;
}

static void usage(void) {
    fprintf(stderr,"Usage: emcpp [option] file\n");
    fprintf(stderr,"  -dt: dump tokens\n");
    exit(1);
}

static void read_opt(int argc, char*argv[]) {
    if (argc<=1) usage();
    error_ctrl   = ERC_EXIT;
    warning_ctrl = ERC_CONTINUE;
    note_ctrl    = ERC_CONTINUE;
    g_dump_token = 0;
    g_fp = stdout;

    for (; argc>1;  argc--, argv++) {
        if (strcmp(argv[1], "-dt")==0) {
            g_dump_token = 1;
        } else if (argc==2) {
            g_filename = argv[1];
            g_cur_filename = g_filename;
            g_cur_line = 0;
            g_user_input = read_file(g_filename);
            break;
        } else {
            usage();
        }
    }
    //puts(g_user_input);
}

int main(int argc, char**argv) {
    g_user_input = "nothing";
    read_opt(argc, argv);

    pptoken_vec = new_vector();
    define_map = new_map();
    new_macro("_emcpp");

    cpp_tokenize(g_user_input);
    pptokens             = (PPToken**)pptoken_vec->data;
    pptoken_pos          = 0;

    preprocessing_file();

    return 0;
}

void test_error(void){}
