#include "ish.h"

/*
 *  [特殊文字]
 *  \x01 - \x1a : Ctrl+A - Ctrl+Z
 *  \x7f : BackSpace
 *  \x1b[3~ : Delete
 *  \x1b[A : 上方向キー
 *  \x1b[B : 下方向キー
 *  \x1b[C : 右方向キー
 *  \x1b[D : 左方向キー
 *  \x1b[1;2A : Shift + 上方向キー
 *  \x1b[1;5A : Ctrl + 上方向キー
 *  \x1bOH, \1b[1~ : Home
 *  \x1bOF, \1b[4~ : End
 *  
 *  [カーソル移動]
 *  \x1b[nA : 上に n 行移動 (n は省略可)
 */


#define HISTORY     16
#define BUF_UNIT    64

char *buf[HISTORY], *tmp = NULL;    // 履歴、作業領域
int buf_index = 0, cur_index;       // cur_index は上下キーで参照している位置
int buf_size;           // buf の領域の大きさ (実際は buf_size + 1)
int pos, len, col;      // 現在のカーソル位置、文字列の長さ、ウィンドウの横幅

// tmp の領域を拡張
void extend_tmp() {
    char *t;
    MALLOC(t, buf_size + BUF_UNIT + 1);
    memcpy(t, tmp, buf_size);
    free(tmp);
    tmp = t;
    buf_size += BUF_UNIT;
}

// カーソルを移動
void move_cursor(int target, int opt) {
    int cx = ((opt ? len : pos) + PROMPT_LEN) % col;
    int cy = ((opt ? len : pos) + PROMPT_LEN) / col;
    int tx = (target + PROMPT_LEN) % col;
    int ty = (target + PROMPT_LEN) / col;
    if (ty < cy) printf("\x1b[%dA", cy - ty);
    if (ty > cy) printf("\x1b[%dB", ty - cy);
    if (tx > cx) printf("\x1b[%dC", tx - cx);
    if (tx < cx) printf("\x1b[%dD", cx - tx);
    pos = target;
}

// 左右およびHome, Endキー
void key_left()  { if (pos > 0)   move_cursor(pos - 1, 0); }
void key_right() { if (pos < len) move_cursor(pos + 1, 0); }
void key_home()  { move_cursor(0, 0); }
void key_end()   { move_cursor(len, 0); }

// 履歴を参照
void move_history(int idx) {
    cur_index = idx;
    key_home();
    pos = len;
    memset(tmp, ' ', len);
    printf("%s", tmp);
    if ((len + PROMPT_LEN) % col == 0) printf("\n\r");
    key_home();
    if (idx == buf_index) { len = 0; tmp[0] = '\0'; return; }
    pos = len = strlen(buf[idx]);
    if (buf_size < len) {
        free(tmp);
        buf_size = (len / BUF_UNIT + 1) * BUF_UNIT;
        MALLOC(tmp, buf_size + 1);
    }
    strcpy(tmp, buf[idx]);
    printf("%s", tmp);
    if ((len + PROMPT_LEN) % col == 0) printf("\n\r");
}

// 上下キー
void key_up() {
    int idx = (cur_index - 1 + HISTORY) % HISTORY;
    if (idx == buf_index || buf[idx] == NULL) return;
    move_history(idx);
}
void key_down() {
    if (cur_index == buf_index) return;
    move_history((cur_index + 1) % HISTORY);
}

// Deleteキー
void key_delete() {
    if (pos == len) return;
    int i;
    for (i = pos; i < len; ++i) tmp[i] = tmp[i + 1];
    printf("%s ", tmp + pos);
    if ((len + PROMPT_LEN) % col == 0) printf("\n\r");
    move_cursor(pos, 1);
    --len;
}

// Backspaceキー
void key_backspace() {
    if (pos == 0) return;
    key_left();
    key_delete();
}

// Enterキー; buf[buf_index] に文字列を格納
void key_enter() {
    while (len > 0 && tmp[len - 1] == ' ') tmp[--len] = '\0';
    free(buf[buf_index]);
    MALLOC(buf[buf_index], len + 1);
    memcpy(buf[buf_index], tmp, len + 1);
    if (len > 0) buf_index = (buf_index + 1) % HISTORY;
    key_end();
    printf("\n\r");
}

// Ctrl + C; buf[buf_index] に空文字列を格納
void ctrl_c() {
    free(buf[buf_index]);
    MALLOC(buf[buf_index], 1);
    buf[buf_index][0] = '\0';
    key_end();
    printf("^C\n\r");
}

// 文字を挿入
void insert_char(int c) {
    if (len == buf_size) extend_tmp();
    int i;
    for (i = len; i >= pos; --i) tmp[i + 1] = tmp[i];
    tmp[pos] = c;
    printf("%s", tmp + pos);
    ++pos; ++len;
    if ((len + PROMPT_LEN) % col == 0) printf("\n\r");
    move_cursor(pos, 1);
}

// 標準入力から1行読み込んだ文字列のポインタを返す
// ポインタの寿命は次に readline() を呼び出すまで
// 入力では方向キーなどの使用が可能
char *readline() {
    
    buf_size = BUF_UNIT;
    free(tmp);
    MALLOC(tmp, buf_size + 1);
    
    // 非インタラクティブ時
    if (!isatty(0)) {
        while (1) {
            if (!fgets(tmp + buf_size - BUF_UNIT, BUF_UNIT + 1, stdin))
                return NULL;
            char *c = strchr(tmp, '\n');
            if (c) {
                *c = '\0'; return tmp;
            } else
                extend_tmp();
        }
    }
    
    // プロンプトを表示
    printf(PROMPT);
    
    // termios の設定
    static int init = 0;
    static struct termios original, settings;
    if (!init) {
        init = 1;
        tcgetattr(0, &original);
        tcgetattr(0, &settings);
        settings.c_iflag &= ~(PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        settings.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        settings.c_cc[VTIME] = 0;
        settings.c_cc[VMIN] = 1;
    }
    tcsetattr(0, TCSANOW, &settings);
    
    // 端末の横幅を取得
    struct winsize winsize;
    ioctl(0, TIOCGWINSZ, &winsize);
    col = winsize.ws_col;
    
    // 1バイトずつ読み込む
    int c, state = 0, ctrl_d = 0, idx = buf_index;
    tmp[0] = '\0';
    pos = len = 0;
    cur_index = buf_index;
    while ((c = getchar()) != EOF) {
        if (c == '\x1b') {
            state = 1;
        } else if (state == 1) {    // \x1b
            state = 0;
            if (c == '[') state = 2;
            if (c == 'O') state = 3;
        } else if (state == 2) {    // \x1b[
            switch (c) {
                case 'A': state = 0; key_up();    break;
                case 'B': state = 0; key_down();  break;
                case 'C': state = 0; key_right(); break;
                case 'D': state = 0; key_left();  break;
                case 'H': state = 0; key_home();  break;
                case 'F': state = 0; key_end();   break;
                case '1': state = 4; break;
                case '3': state = 5; break;
                case '4': state = 6; break;
                case '2': case '5': case '6': state = 7; break;
                default:  state = 0; break;
            }
        } else if (state == 3) {    // \x1bO
            state = 0;
            switch (c) {
                case 'A': key_up();    break;
                case 'B': key_down();  break;
                case 'C': key_right(); break;
                case 'D': key_left();  break;
                case 'H': key_home();  break;
                case 'F': key_end();   break;
            }
        } else if (state == 4) {    // \x1b[1
            state = 0;
            if (c == '~') key_home();
            if (c == ';') state = 8;
        } else if (state == 5) {    // \x1b[3
            state = 0;
            if (c == '~') key_delete();
        } else if (state == 6) {    // \x1b[4
            state = 0;
            if (c == '~') key_end();
        } else if (state == 7) {
            state = 0;
        } else if (state == 8) {    // \x1b[1;
            state = c == '5' ? 9 : 0;
        } else if (state == 9) {    // \x1b[1;5
            state = 0;
            if (c == 'C') key_end();
            if (c == 'D') key_home();
        } else if (c == '\r') {
            key_enter(); break;
        } else if (c == 127) {
            key_backspace();
        } else if (1 <= c && c <= 26) {
            if (c ==  3) { ctrl_c(); break; }
            if (c ==  4) { if (!len) { ctrl_d = 1; break; } else key_delete(); }
            if (c ==  1) key_home();
            if (c ==  2) key_left();
            if (c ==  5) key_end();
            if (c ==  6) key_right();
            if (c == 14) key_down();
            if (c == 16) key_up();
        } else if (32 <= c && c <= 126) {   
            insert_char(c);
        }
    }
    
    // termios を元に戻す
    tcsetattr(0, TCSANOW, &original);
    
    if (ctrl_d) {
        printf("exit\n");
        return NULL;
    }
    
    return buf[idx];
}

// 履歴を解放
void free_history() {
    int i;
    for (i = 0; i < HISTORY; ++i)
        free(buf[i]);
    free(tmp);
}

