#include "ish.h"

// プロセス構造体を作成
proc_t *new_proc() {
    proc_t *p;
    MALLOC(p, sizeof(proc_t));
    p->next = NULL;
    p->pid = 0;
    p->argc = 0;
    int i;
    for (i = 0; i < ARGS_LEN + 1; ++i)
        p->arg_list[i] = NULL;
    p->infile  = NULL;
    p->outfile = NULL;
    p->outopt = TRUNC;
    p->status = RUNNING;
    return p;
}

// ジョブ構造体を作成
job_t *new_job() {
    job_t *j;
    MALLOC(j, sizeof(job_t));
    j->next = NULL;
    j->pgid = 0;
    j->cmd[0] = '\0';
    j->proc_cnt = 1;
    j->proc_list = new_proc();
    j->mode = FOREGROUND;
    j->jobid = 0;
    j->printed = 0;
    j->status = RUNNING;
    return j;
}

// プロセスリストを解放
void free_proc_list(proc_t *p) {
    if (p == NULL) return;
    free_proc_list(p->next);
    int i;
    for (i = 0; i < p->argc; ++i)
        free(p->arg_list[i]);
    free(p->infile);
    free(p->outfile);
}

// ジョブを解放 (ジョブリストではない)
void free_job(job_t *j) {
    free_proc_list(j->proc_list);
    free(j);
}

// ジョブが実行中かどうか
int is_running(job_t *j) {
    proc_t *p;
    for (p = j->proc_list; p; p = p->next)
        if (p->status == RUNNING)
            return 1;
    return 0;
}

// ジョブが終了したかどうか
int is_terminated(job_t *j) {
    proc_t *p;
    for (p = j->proc_list; p; p = p->next)
        if (p->status == RUNNING || p->status == STOPPED)
            return 0;
    return 1;
}

// waitpid() で返ってきた PID を指定してステータスを更新; 成功したら1を返す
int mark_status(job_t *j, pid_t pid, int status) {
    if (pid > 0) {
        for (; j; j = j->next) {
            proc_t *p;
            for (p = j->proc_list; p; p = p->next) {
                if (pid == p->pid) {
                    if (WIFSTOPPED(status))
                        p->status = STOPPED;
                    else if (WIFEXITED(status))
                        p->status = WEXITSTATUS(status) ? EXIT : DONE;
                    else
                        p->status = TERMINATED;
                    if (j->mode == FOREGROUND && signaled == 0)
                        if (WIFSTOPPED(status) || WIFSIGNALED(status))
                            printf("\n"), signaled = 1;
                    return 1;
                }
            }
        }
        return 0;   // PID が見つからない
    } else if (pid == 0 || errno == ECHILD) {
        return 0;
    } else {
        perror("waitpid()");
        exit(1);
    }
}

// フォアグラウンドジョブのwaitを行う
void wait_job(job_t *j) {
    while (is_running(j)) {
        int status;
        errno = 0;
        pid_t pid = waitpid(-j->pgid, &status, WUNTRACED);
        mark_status(j, pid, status);
    }
    tcsetpgrp(0, getpid());
    if (is_terminated(j)) {
        proc_t *p = j->proc_list;
        while (p->next) p = p->next;
        j->status = p->status;
    } else {
        j->mode = BACKGROUND;
        j->printed = 0;
        j->status = STOPPED;
    }
}

// バックグラウンドで再開
void continue_bg(int jobid) {
    job_t *j;
    if (jobid == 0)
        for (j = job_list; j; j = j->next)
            if (j->status == STOPPED && jobid < j->jobid)
                jobid = j->jobid;
    if (jobid == 0)
        jobid = maxjid ? maxjid : -1;
    for (j = job_list; j; j = j->next) {
        if (jobid == j->jobid) {
            if (j->status == RUNNING) {
                printf("bg: job %d already in background\n", jobid);
            } else if (j->status == STOPPED) {
                proc_t *p;
                for (p = j->proc_list; p; p = p->next)
                    if (p->status == STOPPED)
                        p->status = RUNNING;
                if (kill(-j->pgid, SIGCONT) == -1)
                    perror("kill (SIGCONT)");
                j->printed = 0;
                j->status = RUNNING;
            } else {
                printf("bg: job has terminated\n");
            }
            return;
        }
    }
    printf("bg: no such job\n");
}

// フォアグラウンドで再開
void continue_fg(int jobid) {
    job_t *j;
    if (jobid == 0)
        jobid = maxjid ? maxjid : -1;
    for (j = job_list; j; j = j->next) {
        if (jobid == j->jobid) {
            if (j->status == RUNNING || j->status == STOPPED) {
                proc_t *p;
                for (p = j->proc_list; p; p = p->next)
                    if (p->status == STOPPED)
                        p->status = RUNNING;
                printf("%s\n", j->cmd);
                if (j->status == STOPPED && kill(-j->pgid, SIGCONT) == -1)
                    perror("kill (SIGCONT)");
                j->mode = FOREGROUND;
                j->status = RUNNING;
                tcsetpgrp(0, j->pgid);
                wait_job(j);
            } else {
                printf("fg: job has terminated\n");
            }
            return;
        }
    }
    printf("fg: no such job\n");
}

// ジョブリストの状態を更新
void update_status() {
    int status;
    pid_t pid;
    do {
        errno = 0;
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
    } while (mark_status(job_list, pid, status));
    job_t *j;
    for (j = job_list; j; j = j->next) {
        proc_status s;
        if (is_terminated(j)) {
            proc_t *p = j->proc_list;
            while (p->next) p = p->next;
            s = p->status;
        } else {
            s = is_running(j) ? RUNNING : STOPPED;
        }
        if (j->status != s) {
            j->printed = 0;
            j->status = s;
        }
        if (j->mode == BACKGROUND && !j->jobid)
            j->jobid = ++maxjid;
    }
}

// バックグラウンドの状態変化を表示し、完了したジョブを削除する
void print_bginfo(int print_all) {
    update_status();
    job_t *j = job_list, *prv = NULL;
    job_list = NULL;
    while (j) {
        if (j->mode == BACKGROUND && (!j->printed || print_all)) {
            j->printed = 1;
            char buf[20];
            sprintf(buf, "[%d] (%d) ", j->jobid, j->pgid);
            printf("%-14s", buf);
            switch (j->status) {
                case RUNNING:    printf("Running       "); break;
                case STOPPED:    printf("Stopped       "); break;
                case DONE:       printf("Done          "); break;
                case EXIT:       printf("Exit          "); break;
                case TERMINATED: printf("Terminated    "); break;
            }
            if (strlen(j->cmd) > 41) printf("%.40s...\n", j->cmd);
            else                     printf("%s\n", j->cmd);
        }
        if (j->status == RUNNING || j->status == STOPPED) {
            if (!job_list) job_list = j;
            prv = j;
            j = j->next;
        } else {
            job_t *jj = j;
            j = j->next;
            if (prv) prv->next = j;
            free_job(jj);
        }
    }
    maxjid = 0;
    for (j = job_list; j; j = j->next)
        if (maxjid < j->jobid) maxjid = j->jobid; 
}

