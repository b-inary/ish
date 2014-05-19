#include "ish.h"

#define PARSEERR(msg) \
    do { printf("parse_line(): %s\n", msg); free_job(j); return NULL; } while (0)

typedef enum parse_state_ {
    ARGUMENT,
    INFILE,
    OUTFILE
} parse_state;

// パーサー; ジョブが無い場合はNULLを返す
job_t *parse_line(char *buf) {
    
    // 空文字列、空白文字のみの場合を除外
    buf += strspn(buf, " \t");
    if (*buf == '\0')
        return NULL;
    
    job_t *j = new_job();
    MALLOC(j->cmd, strlen(buf) + 1);
    strcpy(j->cmd, buf);
    
    proc_t *proc = j->proc_list;
    parse_state state = ARGUMENT;
    
    // 終端文字まで解析
    while (*buf != '\0') {
        // 空白文字
        if (*buf == ' ' || *buf == '\t')
            buf += strspn(buf, " \t");
        // 状態遷移
        else if (*buf == '<') {
            if (state != ARGUMENT || proc->infile)
                PARSEERR("syntax error");
            ++buf;
            state = INFILE;
        } else if (*buf == '>') {
            if (state != ARGUMENT || proc->outfile)
                PARSEERR("syntax error");
            ++buf;
            state = OUTFILE;
            if (*buf == '>')
                ++buf, proc->outopt = APPEND;
        } else if (*buf == '|') {
            if (state != ARGUMENT)
                PARSEERR("syntax error");
            ++buf;
            ++j->proc_cnt;
            proc = proc->next = new_proc();
        }
        // バックグラウンドの設定
        else if (*buf == '&') {
            j->mode = BACKGROUND;
            ++buf;
            buf += strspn(buf, " \t");
            if (*buf != '\0')
                PARSEERR("syntax error");
            break;
        }
        // 文字列の読み込み
        else {
            int len, quot = 0;
            if (*buf == '\'' || *buf == '\"') {
                char *c = strchr(buf + 1, *buf);
                if (c == NULL)
                    PARSEERR("syntax error");
                ++buf;
                len = c - buf;
                quot = 1;
            } else
                len = strcspn(buf, " \t\n<>|&");
            char *s;
            MALLOC(s, sizeof(char) * (len + 1));
            strncpy(s, buf, len);
            s[len] = '\0';
            if (state == ARGUMENT) {
                if (proc->argc == ARGS_LEN)
                    PARSEERR("too many arguments");
                proc->arg_list[proc->argc++] = s;
            } else if (state == INFILE) {
                proc->infile = s;
            } else if (state == OUTFILE) {
                proc->outfile = s;
            }
            buf += len + quot;
            state = ARGUMENT;
        }
    }
    
    // エラーチェック
    if (state != ARGUMENT)
        PARSEERR("syntax error");
    proc = j->proc_list;
    for (; proc; proc = proc->next)
        if (proc->argc == 0)
            PARSEERR("syntax error");

    return j;
}

