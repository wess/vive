#include "vive.h"
#include <ncurses.h>
#include <signal.h>

static int tui_running = 1;
static int daemon_start = 0;

typedef struct { char id[17]; char name[64]; int total_steps, done_steps, elapsed, token_budget, tokens_used; } RunningTask;
typedef struct { char id[17]; char role[32]; char state[8]; char task_id[17]; } AgentInfo;

static void on_resize(int sig) { (void)sig; endwin(); refresh(); clear(); }

static void draw_box(WINDOW *w, const char *title) {
    box(w, 0, 0);
    if (title) { wattron(w, A_BOLD|COLOR_PAIR(1)); mvwprintw(w,0,2," %s ",title); wattroff(w, A_BOLD|COLOR_PAIR(1)); }
}

static void draw_bar(WINDOW *w, int y, int x, int width, int used, int total) {
    if (total <= 0) total = 1;
    int filled = (used * width) / total;
    if (filled > width) filled = width;
    wmove(w, y, x);
    wattron(w, COLOR_PAIR(2));
    for (int i = 0; i < filled; i++) waddch(w, ACS_BLOCK);
    wattroff(w, COLOR_PAIR(2));
    for (int i = filled; i < width; i++) waddch(w, ' ');
}

static void fmt_dur(int s, char *b, int sz) {
    if (s>=3600) snprintf(b,sz,"%dh %dm",s/3600,(s%3600)/60);
    else if (s>=60) snprintf(b,sz,"%dm %ds",s/60,s%60);
    else snprintf(b,sz,"%ds",s);
}

static void fmt_count(int n, char *b, int sz) {
    if (n>=1000000) snprintf(b,sz,"%.1fM",(float)n/1e6f);
    else if (n>=1000) snprintf(b,sz,"%.1fk",(float)n/1e3f);
    else snprintf(b,sz,"%d",n);
}

static int fetch_running(sqlite3 *db, RunningTask *out, int max) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT t.id,t.name,t.token_budget,t.tokens_used,t.updated_at,"
        "(SELECT COUNT(*) FROM task_steps WHERE task_id=t.id),"
        "(SELECT COUNT(*) FROM task_steps WHERE task_id=t.id AND state='completed')"
        " FROM tasks t WHERE t.state='running' LIMIT ?", -1, &s, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(s,1,max);
    int now_t=(int)time(NULL), count=0;
    while (sqlite3_step(s)==SQLITE_ROW && count<max) {
        RunningTask *r = &out[count]; memset(r,0,sizeof(RunningTask));
        const char *tid=(const char*)sqlite3_column_text(s,0);
        const char *nm=(const char*)sqlite3_column_text(s,1);
        if (tid) strncpy(r->id,tid,16); if (nm) strncpy(r->name,nm,63);
        r->token_budget=sqlite3_column_int(s,2); r->tokens_used=sqlite3_column_int(s,3);
        r->elapsed=now_t-sqlite3_column_int(s,4);
        r->total_steps=sqlite3_column_int(s,5); r->done_steps=sqlite3_column_int(s,6);
        count++;
    }
    sqlite3_finalize(s);
    return count;
}

static int fetch_agents(sqlite3 *db, AgentInfo *out, int max) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,"SELECT id,role,state,current_task_id FROM agents LIMIT ?",
        -1, &s, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(s,1,max);
    int count=0;
    while (sqlite3_step(s)==SQLITE_ROW && count<max) {
        AgentInfo *a = &out[count]; memset(a,0,sizeof(AgentInfo));
        const char *ai=(const char*)sqlite3_column_text(s,0);
        const char *ar=(const char*)sqlite3_column_text(s,1);
        const char *as=(const char*)sqlite3_column_text(s,2);
        const char *at=(const char*)sqlite3_column_text(s,3);
        if (ai) strncpy(a->id,ai,16); if (ar) strncpy(a->role,ar,31);
        if (as) strncpy(a->state,as,7); if (at) strncpy(a->task_id,at,16);
        count++;
    }
    sqlite3_finalize(s);
    return count;
}

static void render_tasks(WINDOW *w, Stats *st, RunningTask *rt, int rtc) {
    werase(w); draw_box(w,"Tasks");
    int row=1;
    wattron(w,COLOR_PAIR(2)); mvwprintw(w,row,2,"* %d running",st->tasks_running); wattroff(w,COLOR_PAIR(2));
    wprintw(w,"  %d queued",st->tasks_pending); row++;
    wattron(w,COLOR_PAIR(4)); mvwprintw(w,row,2,"+ %d completed",st->tasks_completed); wattroff(w,COLOR_PAIR(4));
    if (st->tasks_failed>0) { wattron(w,COLOR_PAIR(5)); wprintw(w,"  x %d failed",st->tasks_failed); wattroff(w,COLOR_PAIR(5)); }
    row+=2;
    for (int i=0; i<rtc && row<getmaxy(w)-2; i++) {
        mvwprintw(w,row++,2,"[%.8s] %s",rt[i].id,rt[i].name);
        if (rt[i].total_steps>0) {
            mvwprintw(w,row,4,"step %d/%d ",rt[i].done_steps,rt[i].total_steps);
            int bw=getmaxx(w)-18; if (bw>2) draw_bar(w,row,14,bw,rt[i].done_steps,rt[i].total_steps);
            row++;
        }
        char dur[32]; fmt_dur(rt[i].elapsed,dur,sizeof(dur));
        mvwprintw(w,row++,4,"elapsed: %s",dur);
    }
    wrefresh(w);
}

static void render_agents(WINDOW *w, Stats *st, AgentInfo *ai, int aic) {
    (void)st; werase(w); draw_box(w,"Agents");
    for (int i=0,row=1; i<aic && row<getmaxy(w)-1; i++,row++) {
        if (strcmp(ai[i].state,"busy")==0) {
            wattron(w,COLOR_PAIR(2)); mvwprintw(w,row,2,"* %-12s busy [%.8s]",ai[i].role,ai[i].task_id); wattroff(w,COLOR_PAIR(2));
        } else if (strcmp(ai[i].state,"error")==0) {
            wattron(w,COLOR_PAIR(5)); mvwprintw(w,row,2,"! %-12s error",ai[i].role); wattroff(w,COLOR_PAIR(5));
        } else mvwprintw(w,row,2,"  %-12s idle",ai[i].role);
    }
    if (aic==0) mvwprintw(w,1,2,"no agents registered");
    wrefresh(w);
}

static void render_tokens(WINDOW *w, Stats *st, RunningTask *rt, int rtc) {
    werase(w); draw_box(w,"Tokens");
    char b1[16],b2[16],b3[16];
    fmt_count(st->total_token_budget,b1,sizeof(b1));
    fmt_count(st->total_tokens_used,b2,sizeof(b2));
    fmt_count(st->total_compressed,b3,sizeof(b3));
    mvwprintw(w,1,2,"budget: %s   used: %s",b1,b2);
    mvwprintw(w,2,2,"compressed: %s saved",b3);
    for (int i=0,row=4; i<rtc && row<getmaxy(w)-1; i++,row++) {
        char tb[16],tu[16];
        fmt_count(rt[i].token_budget,tb,sizeof(tb));
        fmt_count(rt[i].tokens_used,tu,sizeof(tu));
        mvwprintw(w,row,2,"%.8s: %s/%s ",rt[i].id,tu,tb);
        int bw=getmaxx(w)-24; if (bw>2) draw_bar(w,row,getmaxx(w)-bw-2,bw,rt[i].tokens_used,rt[i].token_budget);
    }
    wrefresh(w);
}

static void render_memory(WINDOW *w, Stats *st) {
    werase(w); draw_box(w,"Memory");
    mvwprintw(w,1,2,"total: %d memories",st->total_memories);
    mvwprintw(w,2,2,"semantic: %d  structured: %d",st->semantic_memories,st->structured_memories);
    mvwprintw(w,3,2,"session: %d",st->session_memories);
    wrefresh(w);
}

static void render_system(WINDOW *w, Stats *st) {
    werase(w); draw_box(w,"System");
    char up[32],rq[16];
    fmt_dur((int)time(NULL)-daemon_start,up,sizeof(up));
    fmt_count(st->total_requests,rq,sizeof(rq));
    mvwprintw(w,1,2,"uptime: %s  |  requests: %s  |  avg latency: %.0fms",up,rq,st->avg_latency);
    mvwprintw(w,2,2,"clients: %d connected  |  errors: %d",st->connected_clients,st->total_errors);
    wrefresh(w);
}

void tui_run(sqlite3 *db) {
    daemon_start = (int)time(NULL);
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,"SELECT MIN(connected_at) FROM sessions",-1,&s,NULL)==SQLITE_OK) {
        if (sqlite3_step(s)==SQLITE_ROW) { int t=sqlite3_column_int(s,0); if (t>0) daemon_start=t; }
        sqlite3_finalize(s);
    }

    initscr(); cbreak(); noecho(); curs_set(0); timeout(500); keypad(stdscr,TRUE);
    signal(SIGWINCH, on_resize);
    if (has_colors()) {
        start_color(); use_default_colors();
        init_pair(1,COLOR_CYAN,-1); init_pair(2,COLOR_GREEN,-1);
        init_pair(3,COLOR_BLACK,-1); init_pair(4,COLOR_GREEN,-1);
        init_pair(5,COLOR_RED,-1); init_pair(6,COLOR_YELLOW,-1);
    }

    while (tui_running) {
        int h=LINES, w=COLS;
        int top_h=(h-4)/2, mid_h=(h-4)/2, bot_h=4;
        int left_w=w/2, right_w=w-left_w;
        if (top_h<4) top_h=4; if (mid_h<4) mid_h=4;
        if (top_h+mid_h+bot_h>h) mid_h=h-top_h-bot_h;

        WINDOW *wt=newwin(top_h,left_w,0,0);
        WINDOW *wa=newwin(top_h,right_w,0,left_w);
        WINDOW *wk=newwin(mid_h,left_w,top_h,0);
        WINDOW *wm=newwin(mid_h,right_w,top_h,left_w);
        WINDOW *ws=newwin(bot_h,w,top_h+mid_h,0);

        Stats stats={0}; db_query_stats(db,&stats);
        RunningTask rt[10]; int rtc=fetch_running(db,rt,10);
        AgentInfo ai[20]; int aic=fetch_agents(db,ai,20);

        render_tasks(wt,&stats,rt,rtc);
        render_agents(wa,&stats,ai,aic);
        render_tokens(wk,&stats,rt,rtc);
        render_memory(wm,&stats);
        render_system(ws,&stats);

        int ch=getch();
        if (ch=='q'||ch=='Q') tui_running=0;

        delwin(wt); delwin(wa); delwin(wk); delwin(wm); delwin(ws);
    }
    endwin();
}

void print_status(sqlite3 *db) {
    Stats s={0};
    if (db_query_stats(db,&s)!=0) { fprintf(stderr,"error: failed to query stats\n"); return; }
    printf("=== Vive Status ===\n\n");
    printf("Tasks:\n  pending: %d  running: %d  completed: %d  failed: %d\n\n",
        s.tasks_pending,s.tasks_running,s.tasks_completed,s.tasks_failed);
    printf("Agents:\n  total: %d  busy: %d  idle: %d\n\n",s.total_agents,s.agents_busy,s.agents_idle);
    printf("Tokens:\n  budget: %d  used: %d  compressed: %d\n\n",
        s.total_token_budget,s.total_tokens_used,s.total_compressed);
    printf("Memory:\n  total: %d  (semantic: %d  structured: %d  session: %d)\n\n",
        s.total_memories,s.semantic_memories,s.structured_memories,s.session_memories);
    printf("System:\n  requests: %d  errors: %d  avg latency: %.0fms  clients: %d\n",
        s.total_requests,s.total_errors,s.avg_latency,s.connected_clients);
}
