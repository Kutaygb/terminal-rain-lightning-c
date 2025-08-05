#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define UPDATE_INTERVAL 15000 // (15ms)
#define MAX_RAIN 2000
#define MAX_BOLTS 5

typedef struct {
    int x;
    float y;
    float speed;
    char ch;
} Raindrop;

typedef struct {
    int y, x;
    double created;
} Segment;

typedef struct {
    Segment segments[1000];
    int seg_count;
    int growing;
    int target_len;
    int max_y, max_x;
    double last_growth;
} LightningBolt;

int rows, cols;
int is_thunderstorm = 0;
int rain_count = 0;
int bolt_count = 0;

Raindrop raindrops[MAX_RAIN];
LightningBolt bolts[MAX_BOLTS];

int COLOR_PAIR_RAIN_NORMAL = 1;
int COLOR_PAIR_LIGHTNING   = 2;

double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void init_colors(short rain_fg, short lightning_fg) {
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(COLOR_PAIR_RAIN_NORMAL, rain_fg, -1);
        init_pair(COLOR_PAIR_LIGHTNING, lightning_fg, -1);
    }
}

short parse_color(const char *name) {
    if (strcasecmp(name, "black") == 0) return COLOR_BLACK;
    if (strcasecmp(name, "red") == 0) return COLOR_RED;
    if (strcasecmp(name, "green") == 0) return COLOR_GREEN;
    if (strcasecmp(name, "yellow") == 0) return COLOR_YELLOW;
    if (strcasecmp(name, "blue") == 0) return COLOR_BLUE;
    if (strcasecmp(name, "magenta") == 0) return COLOR_MAGENTA;
    if (strcasecmp(name, "cyan") == 0) return COLOR_CYAN;
    if (strcasecmp(name, "white") == 0) return COLOR_WHITE;
    return COLOR_CYAN; // default
}

void add_raindrop() {
    if (rain_count >= MAX_RAIN) return;
    Raindrop d;
    d.x = rand() % cols;
    d.y = 0;
    float min_speed = is_thunderstorm ? 0.3f : 0.3f;
    float max_speed = is_thunderstorm ? 1.0f : 0.6f;
    d.speed = ((float)rand() / RAND_MAX) * (max_speed - min_speed) + min_speed;
    char rain_chars[] = {'|', '.', '`'};
    d.ch = rain_chars[rand() % 3];
    raindrops[rain_count++] = d;
}

void update_rain() {
    int i, j = 0;
    for (i = 0; i < rain_count; i++) {
        raindrops[i].y += raindrops[i].speed;
        if ((int)raindrops[i].y < rows) {
            raindrops[j++] = raindrops[i];
        }
    }
    rain_count = j;
}

void draw_rain() {
    for (int i = 0; i < rain_count; i++) {
        int ry = (int)raindrops[i].y;
        if (ry >= rows) continue;
        attr_t attr = COLOR_PAIR(COLOR_PAIR_RAIN_NORMAL);
        if (is_thunderstorm) attr |= A_BOLD;
        else if (raindrops[i].speed < 0.8) attr |= A_DIM;
        mvaddch(ry, raindrops[i].x % cols, raindrops[i].ch | attr);
    }
}

void start_bolt() {
    if (bolt_count >= MAX_BOLTS) return;
    LightningBolt b;
    b.max_y = rows;
    b.max_x = cols;
    b.seg_count = 1;
    b.segments[0].y = rand() % (rows / 5);
    b.segments[0].x = cols / 4 + rand() % (cols / 2);
    b.segments[0].created = get_time_sec();
    b.growing = 1;
    b.target_len = rows / 2 + rand() % (rows / 2);
    b.last_growth = get_time_sec();
    bolts[bolt_count++] = b;
}

void update_bolts() {
    double now = get_time_sec();
    int j = 0;
    for (int i = 0; i < bolt_count; i++) {
        
        LightningBolt *b = &bolts[i];
        if (b->growing && now - b->last_growth > 0.002) {
            b->last_growth = now;

            if (b->seg_count < b->target_len) {
                Segment last = b->segments[b->seg_count - 1];
                int offset = (rand() % 5) - 2;
                int nx = last.x + offset;
                if (nx < 0) nx = 0;
                if (nx >= cols) nx = cols - 1;
                int ny = last.y + 1;
                if (ny < rows) {
                    Segment s = {ny, nx, now};
                    b->segments[b->seg_count++] = s;
                } else {
                    b->growing = 0;
                }
            } else {
                b->growing = 0;
            }
        }

        int keep = 0;
        for (int k = 0; k < b->seg_count; k++) {
            if (now - b->segments[k].created <= 0.8) {
                keep = 1; break;
            }
        }
        if (keep) bolts[j++] = *b;
    }
    bolt_count = j;
}

void draw_bolts() {
    double now = get_time_sec();
    for (int i = 0; i < bolt_count; i++) {
        LightningBolt *b = &bolts[i];
        for (int k = 0; k < b->seg_count; k++) {
            double age = now - b->segments[k].created;
            if (age > 0.8) continue;
            char ch;
            if (age < 0.26) ch = '#';
            else if (age < 0.53) ch = '+';
            else ch = '*';
            mvaddch(b->segments[k].y, b->segments[k].x,
                    ch | COLOR_PAIR(COLOR_PAIR_LIGHTNING) | A_BOLD);
        }
    }
}

int main(int argc, char *argv[] ) {
    srand(time(NULL));
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    short rain_color = COLOR_CYAN;
    short lightning_color = COLOR_YELLOW;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--rain-color=", 13) == 0) {
            rain_color = parse_color(argv[i] + 13);
        }
        else if (strncmp(argv[i], "--lightning-color=", 18) == 0) {
            lightning_color = parse_color(argv[i] + 18);
        }
        else if (strcmp(argv[i], "--thunderstorm") == 0) {
            is_thunderstorm = 1;
        }
    }

    init_colors(rain_color, lightning_color);
    getmaxyx(stdscr, rows, cols);

    while (1) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) break;
        if (ch == 't' || ch == 'T') is_thunderstorm = !is_thunderstorm;
        if (ch == KEY_RESIZE) {
            getmaxyx(stdscr, rows, cols);
            clear();
            rain_count = 0;
            bolt_count = 0;
        }

        float generation_chance = is_thunderstorm ? 0.5f : 0.3f;
        int max_new_drops = is_thunderstorm ? cols / 8 : cols / 15;

        if ((float)rand()/RAND_MAX < generation_chance) {
            int num_new = 1 + rand() % (max_new_drops > 0 ? max_new_drops : 1);
            for (int i = 0; i < num_new; i++) add_raindrop();
        }

        if (is_thunderstorm && bolt_count < 3 && ((float)rand()/RAND_MAX < 0.005f)) {
            start_bolt();
        }

        update_rain();
        update_bolts();

        clear();
        draw_bolts();
        draw_rain();
        refresh();

        usleep(UPDATE_INTERVAL);
    }

    endwin();

    return 0;
}