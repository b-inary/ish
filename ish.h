#ifndef ISH_H_
#define ISH_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// 定数
#define PROMPT      "\x1b[1;34mish$\x1b[0;39m "
#define LINE_LEN    256
#define ARGS_LEN    32

// mallocマクロ
#define MALLOC(p, size) \
    do if (!(p = malloc(size))) { perror("malloc()"); exit(1); } while (0)

// プロセスおよびジョブの状態
typedef enum proc_status_ {
    RUNNING,
    STOPPED,
    DONE
} proc_status;

// 出力モード
typedef enum write_opt_ {
    TRUNC,
    APPEND
} write_opt;

// ジョブのモード
typedef enum job_mode_ {
    FOREGROUND,
    BACKGROUND
} job_mode;

// プロセス構造体
typedef struct proc_t_ {
    struct proc_t_ *next;           // 次のプロセス; 無ければNULL
    pid_t pid;                      // プロセスID
    int argc;                       // 引数の個数 (ARGS_LEN個が最大)
    char *arg_list[ARGS_LEN + 1];   // 引数リスト
    char *infile;                   // 入力リダイレクト; 無ければNULL
    char *outfile;                  // 出力リダイレクト; 無ければNULL
    write_opt outopt;               // 出力モード
    proc_status status;             // プロセスの状態
} proc_t;

// ジョブ構造体
typedef struct job_t_ {
    struct job_t_ *next;    // 次のジョブ; 無ければNULL
    pid_t pgid;             // プロセスグループID
    char cmd[LINE_LEN];     // 入力されたコマンド (LINE_LEN-1文字が最大)
    int proc_cnt;           // ジョブに含まれるプロセス数
    proc_t *proc_list;      // プロセスリスト
    job_mode mode;          // フォアグラウンド/バックグラウンド
    int jobid;              // バックグラウンド管理用に割り当てるジョブ番号
    int printed;            // 状態変化を既に表示したかどうか
    proc_status status;     // ジョブの状態
} job_t;

// グローバル変数
extern job_t *job_list;     // ジョブリスト
extern int maxjid;          // jobidの最大値
extern int signaled;        // 子プロセスがシグナルを受けた (改行の制御用)

// exec.c
void execute_job(job_t *j, char *envp[], char *path[]);     // ジョブの実行

// job.c
proc_t *new_proc();         // プロセス構造体を作成
job_t  *new_job();          // ジョブ構造体を作成
void free_job(job_t *j);    // ジョブを解放 (ジョブリストではない)
void wait_job(job_t *j);    // フォアグラウンドジョブのwaitを行う
void continue_bg(int jid);  // バックグラウンドで再開
void continue_fg(int jid);  // フォアグラウンドで再開
void update_status();       // ジョブリストの状態を更新
void print_bginfo(int print_all);   // バックグラウンドの状態変化を表示し、
                                    // 完了したジョブを削除する
// parse.c
char *get_line(char *s, int size);      // size - 1 文字を s に読み込む
job_t *parse_line(char *s);             // パーサー; エラー時はNULLを返す
char **parse_env_path(char *envp[]);    // 環境変数PATHのパーサー
void free_env_path(char *path[]);       // パースしたPATHの解放


#endif // ISH_H_

