#include "ish.h"

int getjid(char *arg) {
    if (arg == NULL) return 0;
    char *c;
    errno = 0;
    int jobid = strtol(arg, &c, 10);
    if (jobid < 1 || *c != '\0' || errno == ERANGE)
        return -1;
    return jobid;
}

// ジョブの実行
void execute_job(job_t *j, char *envp[], char *path[]) {
    
    struct sigaction dfl;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_handler = SIG_DFL;
    dfl.sa_flags   = 0;
    
    int fd_stdin = dup(0), fd_stdout = dup(1);
    int fd[2], infile = fd_stdin, outfile;
    pid_t pid;
    proc_t *p = j->proc_list;
    
    for (; p; p = p->next) {
        // パイプの作成
        if (p->next) {
            if (pipe(fd) == -1) {
                perror("pipe()");
                exit(1);
            }
            outfile = fd[1];
        } else
            outfile = fd_stdout;
        
        // 入出力の設定
        if (p->infile) {
            if (infile != fd_stdin) close(infile);
            infile = open(p->infile, O_RDONLY);
        }
        if (p->outfile) {
            if (outfile != fd_stdout) close(outfile);
            int flags = O_WRONLY | O_CREAT;
            flags |= p->outopt == TRUNC ? O_TRUNC : O_APPEND;
            outfile = open(p->outfile, flags, 0664);
        }
        dup2(infile, 0);
        dup2(outfile, 1);
        if (infile  != fd_stdin)  close(infile);
        if (outfile != fd_stdout) close(outfile);
        
        // 組み込みコマンドの処理 (jobs, bg, fg)
        // bg, fg は引数に正整数を取ってジョブの指定ができる
        if (strcmp(p->arg_list[0], "jobs") == 0) {
            print_bginfo(1);
            p->status = DONE;
        } else if (strcmp(p->arg_list[0], "bg") == 0) {
            int jobid = getjid(p->arg_list[1]);
            if (jobid != -1) continue_bg(jobid);
            else printf("bg: invalid argument\n");
            p->status = DONE;
        } else if (strcmp(p->arg_list[0], "fg") == 0) {
            int jobid = getjid(p->arg_list[1]);
            if (jobid != -1) continue_fg(jobid);
            else printf("fg: invalid argument\n");
            p->status = DONE;
        }
        // 組み込みコマンドでなければforkを行う
        else if ((pid = fork()) == 0) {
            // プロセスグループを変更
            if (!j->pgid) {
                setpgid(0, 0);
                if (j->mode == FOREGROUND) tcsetpgrp(0, getpid());
            } else
                setpgid(0, j->pgid);
            
            // シグナルの処理をデフォルトに戻す
            sigaction(SIGINT,  &dfl, NULL);
            sigaction(SIGTSTP, &dfl, NULL);
            sigaction(SIGTTIN, &dfl, NULL);
            sigaction(SIGTTOU, &dfl, NULL);
            
            // 処理呼び出し
            if (strchr(p->arg_list[0], '/')) {
                execve(p->arg_list[0], p->arg_list, envp);
            } else {
                int i;
                char buf[LINE_LEN];
                for (i = 0; path[i]; ++i) {
                    strcpy(buf, path[i]);
                    strcat(buf, p->arg_list[0]);
                    execve(buf, p->arg_list, envp);
                }
            }
            perror(p->arg_list[0]);
            exit(1);
        } else if (pid == -1) {
            perror("fork()");
            exit(1);
        } else {
            // PIDなどの設定
            p->pid = pid;
            if (!j->pgid) j->pgid = pid;
            setpgid(pid, j->pgid);
        }
        
        infile = fd[0];
    }
    
    // 入出力の後処理
    dup2(fd_stdin, 0);
    dup2(fd_stdout, 1);
    close(fd_stdin);
    close(fd_stdout);
    
    // フォアグラウンドジョブの場合はwaitを行う
    if (j->mode == FOREGROUND)
        wait_job(j);
}

