#include "ish.h"

job_t *job_list = NULL;
int maxjid = 0;
int signaled = 0;

void do_nothing(int sig) {}

int main(int argc, char *argv[])
{
    // シグナルハンドラの設定
    struct sigaction sa1, sa2;
    sa1.sa_handler = do_nothing;
    sa2.sa_handler = SIG_IGN;
    sigemptyset(&sa1.sa_mask);
    sigemptyset(&sa2.sa_mask);
    sa1.sa_flags = sa2.sa_flags = 0;
    sigaction(SIGINT,  &sa1, NULL);
    sigaction(SIGTSTP, &sa1, NULL);
    sigaction(SIGQUIT, &sa1, NULL);
    sigaction(SIGTTIN, &sa2, NULL);
    sigaction(SIGTTOU, &sa2, NULL);
    
    char s[LINE_LEN];
    job_t *j;
    
    // メインループ
    while (get_line(s, LINE_LEN)) {
        if (strcmp(s, "exit\n") == 0) break;
        if ((j = parse_line(s)) != NULL) {
            update_status();
            if (job_list == NULL) {
                job_list = j;
            } else {
                job_t *jj = job_list;
                while (jj->next) jj = jj->next;
                jj->next = j;
            }
            execute_job(j);
        }
        print_bginfo(0);
        signaled = 0;
    }
    
    return 0;
}

