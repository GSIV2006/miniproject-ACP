#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <curses.h>

#define CANVAS_WIDTH 50
#define CANVAS_HEIGHT 20
#define MAX_SHAPES 100

typedef enum {
    SHAPE_LINE,
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_TRIANGLE
} ShapeType;

typedef struct {
    int x1, y1, x2, y2;
} LineProps;

typedef struct {
    int x1, y1, x2, y2;
    int filled;
} RectProps;

typedef struct {
    int cx, cy, r;
    int filled;
} CircleProps;

typedef struct {
    int x1, y1, x2, y2, x3, y3;
    int filled;
} TriProps;

typedef struct {
    int id;
    ShapeType type;
    char draw_char;
    int color_idx;
    union {
        LineProps line;
        RectProps rect;
        CircleProps circle;
        TriProps triangle;
    } data;
} Shape;

Shape shapes[MAX_SHAPES];
int num_shapes = 0;
int next_shape_id = 1;
int selected_shape_idx = -1;

double zoom_scale = 1.0;
#define ZOOM_CX 25.0
#define ZOOM_CY 10.0

int scale_x(int x) {
    return (int)round(ZOOM_CX + (x - ZOOM_CX) * zoom_scale);
}

int scale_y(int y) {
    return (int)round(ZOOM_CY + (y - ZOOM_CY) * zoom_scale);
}

int scale_r(int r) {
    int tr = (int)round(r * zoom_scale);
    if (tr < 1 && r > 0) tr = 1;
    return tr;
}

char canvas[CANVAS_HEIGHT][CANVAS_WIDTH];
int shape_map[CANVAS_HEIGHT][CANVAS_WIDTH];

WINDOW *canvas_win;
WINDOW *sidebar_win;
WINDOW *status_win;

void set_pixel(int x, int y, char c, int shape_idx) {
    if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT) {
        canvas[y][x] = c;
        shape_map[y][x] = shape_idx;
    }
}

// 1. Line drawing using Bresenham's algorithm
void draw_line(int x1, int y1, int x2, int y2, char c, int idx) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        set_pixel(x1, y1, c, idx);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

// 2. Rectangle drawing
void draw_rect(int x1, int y1, int x2, int y2, int filled, char c, int idx) {
    int min_x = x1 < x2 ? x1 : x2;
    int max_x = x1 > x2 ? x1 : x2;
    int min_y = y1 < y2 ? y1 : y2;
    int max_y = y1 > y2 ? y1 : y2;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            if (filled || x == min_x || x == max_x || y == min_y || y == max_y) {
                set_pixel(x, y, c, idx);
            }
        }
    }
}

// 3. Circle drawing (highly robust entire-canvas scan with font aspect ratio correction)
void draw_circle(int cx, int cy, int r, int filled, char c, int idx) {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            double dx = (x - cx) / 2.0; // Correct for non-square font aspect ratio in terminals
            double dy = y - cy;
            double dist_sq = dx * dx + dy * dy;
            if (filled) {
                if (dist_sq <= (r + 0.5) * (r + 0.5)) {
                    set_pixel(x, y, c, idx);
                }
            } else {
                double r_inner = r - 0.75;
                double r_outer = r + 0.75;
                if (r_inner < 0) r_inner = 0;
                if (dist_sq >= r_inner * r_inner && dist_sq <= r_outer * r_outer) {
                    set_pixel(x, y, c, idx);
                }
            }
        }
    }
}

// 4. Triangle drawing (outline or barycentric edge sign fill checks)
void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int filled, char c, int idx) {
    if (!filled) {
        draw_line(x1, y1, x2, y2, c, idx);
        draw_line(x2, y2, x3, y3, c, idx);
        draw_line(x3, y3, x1, y1, c, idx);
    } else {
        int min_x = x1; if (x2 < min_x) min_x = x2; if (x3 < min_x) min_x = x3;
        int max_x = x1; if (x2 > max_x) max_x = x2; if (x3 > max_x) max_x = x3;
        int min_y = y1; if (y2 < min_y) min_y = y2; if (y3 < min_y) min_y = y3;
        int max_y = y1; if (y2 > max_y) max_y = y2; if (y3 > max_y) max_y = y3;

        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                int d1 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2);
                int d2 = (x - x3) * (y2 - y3) - (x2 - x3) * (y - y3);
                int d3 = (x - x1) * (y3 - y1) - (x3 - x1) * (y - y1);
                int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                if (!(has_neg && has_pos)) {
                    set_pixel(x, y, c, idx);
                }
            }
        }
        // Draw outlines to ensure fully solid margins
        draw_line(x1, y1, x2, y2, c, idx);
        draw_line(x2, y2, x3, y3, c, idx);
        draw_line(x3, y3, x1, y1, c, idx);
    }
}

// 2D Array Rendering Pipeline
void render_all_shapes() {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            canvas[y][x] = '_';
            shape_map[y][x] = -1;
        }
    }
    for (int i = 0; i < num_shapes; i++) {
        Shape s = shapes[i];
        char c = s.draw_char;
        switch (s.type) {
            case SHAPE_LINE:
                draw_line(
                    scale_x(s.data.line.x1), scale_y(s.data.line.y1),
                    scale_x(s.data.line.x2), scale_y(s.data.line.y2),
                    c, i
                );
                break;
            case SHAPE_RECTANGLE:
                draw_rect(
                    scale_x(s.data.rect.x1), scale_y(s.data.rect.y1),
                    scale_x(s.data.rect.x2), scale_y(s.data.rect.y2),
                    s.data.rect.filled, c, i
                );
                break;
            case SHAPE_CIRCLE:
                draw_circle(
                    scale_x(s.data.circle.cx), scale_y(s.data.circle.cy),
                    scale_r(s.data.circle.r),
                    s.data.circle.filled, c, i
                );
                break;
            case SHAPE_TRIANGLE:
                draw_triangle(
                    scale_x(s.data.triangle.x1), scale_y(s.data.triangle.y1),
                    scale_x(s.data.triangle.x2), scale_y(s.data.triangle.y2),
                    scale_x(s.data.triangle.x3), scale_y(s.data.triangle.y3),
                    s.data.triangle.filled, c, i
                );
                break;
        }
    }
}

// Display Subsystem
void display_picture() {
    wclear(canvas_win);
    box(canvas_win, 0, 0);
    wattron(canvas_win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(canvas_win, 0, 2, " CANVAS (50x20) [Zoom: %d%%] ", (int)round(zoom_scale * 100));
    wattroff(canvas_win, COLOR_PAIR(2) | A_BOLD);
    
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        wmove(canvas_win, y + 1, 1);
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            int idx = shape_map[y][x];
            char c = canvas[y][x];
            if (idx == -1) {
                wattron(canvas_win, COLOR_PAIR(2) | A_DIM);
                waddch(canvas_win, c);
                wattroff(canvas_win, COLOR_PAIR(2) | A_DIM);
            } else {
                int col = shapes[idx].color_idx;
                if (idx == selected_shape_idx) {
                    wattron(canvas_win, COLOR_PAIR(col) | A_REVERSE | A_BOLD);
                    waddch(canvas_win, c);
                    wattroff(canvas_win, COLOR_PAIR(col) | A_REVERSE | A_BOLD);
                } else {
                    wattron(canvas_win, COLOR_PAIR(col) | A_BOLD);
                    waddch(canvas_win, c);
                    wattroff(canvas_win, COLOR_PAIR(col) | A_BOLD);
                }
            }
        }
    }
    wrefresh(canvas_win);
}

void display_sidebar() {
    wclear(sidebar_win);
    box(sidebar_win, 0, 0);
    wattron(sidebar_win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(sidebar_win, 0, 2, " SHAPES LIST ");
    wattroff(sidebar_win, COLOR_PAIR(2) | A_BOLD);
    
    if (num_shapes == 0) {
        wattron(sidebar_win, COLOR_PAIR(1));
        mvwprintw(sidebar_win, 2, 2, "No shapes added.");
        mvwprintw(sidebar_win, 4, 2, "Press 'A' to add!");
        wattroff(sidebar_win, COLOR_PAIR(1));
    } else {
        for (int i = 0; i < num_shapes && i < 20; i++) {
            Shape s = shapes[i];
            const char *type_str = "";
            switch (s.type) {
                case SHAPE_LINE: type_str = "Line"; break;
                case SHAPE_RECTANGLE: type_str = "Rect"; break;
                case SHAPE_CIRCLE: type_str = "Circle"; break;
                case SHAPE_TRIANGLE: type_str = "Tri"; break;
            }
            const char *col_str = "";
            switch (s.color_idx) {
                case 1: col_str = "Grn"; break;
                case 2: col_str = "Cyn"; break;
                case 3: col_str = "Ylw"; break;
                case 4: col_str = "Red"; break;
                case 5: col_str = "Blu"; break;
                case 6: col_str = "Mag"; break;
                case 7: col_str = "Wht"; break;
                default: col_str = "Ylw"; break;
            }
            if (i == selected_shape_idx) {
                wattron(sidebar_win, COLOR_PAIR(4) | A_REVERSE | A_BOLD);
                mvwprintw(sidebar_win, 2 + i, 2, " > ID %d: %-4s [%c] %s ", s.id, type_str, s.draw_char, col_str);
                wattroff(sidebar_win, COLOR_PAIR(4) | A_REVERSE | A_BOLD);
            } else {
                wattron(sidebar_win, COLOR_PAIR(s.color_idx));
                mvwprintw(sidebar_win, 2 + i, 2, "   ID %d: %-4s [%c] %s ", s.id, type_str, s.draw_char, col_str);
                wattroff(sidebar_win, COLOR_PAIR(s.color_idx));
            }
        }
    }
    wrefresh(sidebar_win);
}

void display_status(const char *msg) {
    wclear(status_win);
    box(status_win, 0, 0);
    if (msg && strlen(msg) > 0) {
        wattron(status_win, COLOR_PAIR(4) | A_BOLD);
        mvwprintw(status_win, 1, 2, "%s", msg);
        wattroff(status_win, COLOR_PAIR(4) | A_BOLD);
    } else {
        wattron(status_win, COLOR_PAIR(1) | A_BOLD);
        mvwprintw(status_win, 1, 2, "[A] Add [D] Del [M] Mod [+/-] Zoom [Q] Quit [Up/Dn] Select");
        wattroff(status_win, COLOR_PAIR(1) | A_BOLD);
    }
    wrefresh(status_win);
}

// User Input Utility Functions
void get_input(const char *prompt, char *buf, int max) {
    wclear(status_win);
    box(status_win, 0, 0);
    wattron(status_win, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(status_win, 1, 2, "%s: ", prompt);
    wattroff(status_win, COLOR_PAIR(4) | A_BOLD);
    wrefresh(status_win);
    echo();
    curs_set(1);
    wgetnstr(status_win, buf, max);
    noecho();
    curs_set(0);
}

int get_int(const char *prompt, int def) {
    char buf[32];
    get_input(prompt, buf, 30);
    if (buf[0] == '\0') return def;
    return atoi(buf);
}

char get_char(const char *prompt, char def) {
    char buf[16];
    get_input(prompt, buf, 14);
    if (buf[0] == '\0') return def;
    return buf[0];
}

// Add Shape Menu
void add_shape_menu() {
    wclear(status_win);
    box(status_win, 0, 0);
    wattron(status_win, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(status_win, 1, 2, "Add: 1:Line  2:Rect  3:Circle  4:Triangle  (other to cancel)");
    wattroff(status_win, COLOR_PAIR(4) | A_BOLD);
    wrefresh(status_win);
    
    int choice = getch();
    if (choice < '1' || choice > '4') {
        display_status("Cancelled.");
        return;
    }
    
    Shape s;
    s.id = next_shape_id++;
    
    if (choice == '1') {
        s.type = SHAPE_LINE;
        s.data.line.x1 = get_int("Enter X1 (0-49)", 0);
        s.data.line.y1 = get_int("Enter Y1 (0-19)", 0);
        s.data.line.x2 = get_int("Enter X2 (0-49)", 10);
        s.data.line.y2 = get_int("Enter Y2 (0-19)", 10);
    } else if (choice == '2') {
        s.type = SHAPE_RECTANGLE;
        s.data.rect.x1 = get_int("Enter Corner X1 (0-49)", 0);
        s.data.rect.y1 = get_int("Enter Corner Y1 (0-19)", 0);
        s.data.rect.x2 = get_int("Enter Corner X2 (0-49)", 15);
        s.data.rect.y2 = get_int("Enter Corner Y2 (0-19)", 8);
        s.data.rect.filled = get_int("Filled? (1=Yes, 0=No)", 0);
    } else if (choice == '3') {
        s.type = SHAPE_CIRCLE;
        s.data.circle.cx = get_int("Enter Center X (0-49)", 25);
        s.data.circle.cy = get_int("Enter Center Y (0-19)", 10);
        s.data.circle.r = get_int("Enter Radius (1-15)", 5);
        s.data.circle.filled = get_int("Filled? (1=Yes, 0=No)", 0);
    } else if (choice == '4') {
        s.type = SHAPE_TRIANGLE;
        s.data.triangle.x1 = get_int("Enter V1 X (0-49)", 10);
        s.data.triangle.y1 = get_int("Enter V1 Y (0-19)", 5);
        s.data.triangle.x2 = get_int("Enter V2 X (0-49)", 20);
        s.data.triangle.y2 = get_int("Enter V2 Y (0-19)", 15);
        s.data.triangle.x3 = get_int("Enter V3 X (0-49)", 30);
        s.data.triangle.y3 = get_int("Enter V3 Y (0-19)", 5);
        s.data.triangle.filled = get_int("Filled? (1=Yes, 0=No)", 0);
    }
    
    s.draw_char = get_char("Enter draw character", '*');
    s.color_idx = get_int("Color (1:Grn 2:Cyn 3:Ylw 4:Red 5:Blu 6:Mag 7:Wht)", 3);
    if (s.color_idx < 1 || s.color_idx > 7) s.color_idx = 3;
    
    if (num_shapes < MAX_SHAPES) {
        shapes[num_shapes++] = s;
        selected_shape_idx = num_shapes - 1;
        render_all_shapes();
        display_status("Shape added successfully!");
    } else {
        display_status("Error: Max shape limit reached!");
    }
}

// Delete Selected Shape
void delete_selected_shape() {
    if (selected_shape_idx < 0 || selected_shape_idx >= num_shapes) {
        display_status("No shape selected to delete!");
        return;
    }
    for (int i = selected_shape_idx; i < num_shapes - 1; i++) {
        shapes[i] = shapes[i + 1];
    }
    num_shapes--;
    if (selected_shape_idx >= num_shapes) {
        selected_shape_idx = num_shapes - 1;
    }
    render_all_shapes();
    display_status("Shape deleted successfully.");
}

// Modify Selected Shape
void modify_selected_shape() {
    if (selected_shape_idx < 0 || selected_shape_idx >= num_shapes) {
        display_status("No shape selected to modify!");
        return;
    }
    
    Shape *s = &shapes[selected_shape_idx];
    wclear(status_win);
    box(status_win, 0, 0);
    wattron(status_win, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(status_win, 1, 2, "Modify: 1:Coords/Size  2:Draw Char  3:Toggle Fill  4:Color  (other to cancel)");
    wattroff(status_win, COLOR_PAIR(4) | A_BOLD);
    wrefresh(status_win);
    
    int choice = getch();
    if (choice == '1') {
        switch (s->type) {
            case SHAPE_LINE:
                s->data.line.x1 = get_int("New X1", s->data.line.x1);
                s->data.line.y1 = get_int("New Y1", s->data.line.y1);
                s->data.line.x2 = get_int("New X2", s->data.line.x2);
                s->data.line.y2 = get_int("New Y2", s->data.line.y2);
                break;
            case SHAPE_RECTANGLE:
                s->data.rect.x1 = get_int("New X1", s->data.rect.x1);
                s->data.rect.y1 = get_int("New Y1", s->data.rect.y1);
                s->data.rect.x2 = get_int("New X2", s->data.rect.x2);
                s->data.rect.y2 = get_int("New Y2", s->data.rect.y2);
                break;
            case SHAPE_CIRCLE:
                s->data.circle.cx = get_int("New Center X", s->data.circle.cx);
                s->data.circle.cy = get_int("New Center Y", s->data.circle.cy);
                s->data.circle.r = get_int("New Radius", s->data.circle.r);
                break;
            case SHAPE_TRIANGLE:
                s->data.triangle.x1 = get_int("New Vertex 1 X", s->data.triangle.x1);
                s->data.triangle.y1 = get_int("New Vertex 1 Y", s->data.triangle.y1);
                s->data.triangle.x2 = get_int("New Vertex 2 X", s->data.triangle.x2);
                s->data.triangle.y2 = get_int("New Vertex 2 Y", s->data.triangle.y2);
                s->data.triangle.x3 = get_int("New Vertex 3 X", s->data.triangle.x3);
                s->data.triangle.y3 = get_int("New Vertex 3 Y", s->data.triangle.y3);
                break;
        }
        display_status("Shape parameters updated.");
    } else if (choice == '2') {
        s->draw_char = get_char("New draw character", s->draw_char);
        display_status("Draw character updated.");
    } else if (choice == '3') {
        switch (s->type) {
            case SHAPE_LINE:
                display_status("Lines cannot be filled!");
                return;
            case SHAPE_RECTANGLE:
                s->data.rect.filled = !s->data.rect.filled;
                break;
            case SHAPE_CIRCLE:
                s->data.circle.filled = !s->data.circle.filled;
                break;
            case SHAPE_TRIANGLE:
                s->data.triangle.filled = !s->data.triangle.filled;
                break;
        }
        display_status("Filled state toggled.");
    } else if (choice == '4') {
        s->color_idx = get_int("New Color (1:Grn 2:Cyn 3:Ylw 4:Red 5:Blu 6:Mag 7:Wht)", s->color_idx);
        if (s->color_idx < 1 || s->color_idx > 7) s->color_idx = 3;
        display_status("Shape color updated.");
    } else {
        display_status("Cancelled.");
        return;
    }
    render_all_shapes();
}

int main() {
    // Initialise Curses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    // Set colors
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    
    // Layout creation (Fits exactly within standard 80x24 macOS terminals)
    canvas_win = newwin(CANVAS_HEIGHT + 2, CANVAS_WIDTH + 2, 0, 0);
    sidebar_win = newwin(CANVAS_HEIGHT + 2, 26, 0, 52);
    status_win = newwin(2, 78, 22, 0);
    
    render_all_shapes();
    
    int ch;
    display_status("");
    
    while (1) {
        // Safe resize and terminal size guard
        int term_width, term_height;
        getmaxyx(stdscr, term_height, term_width);
        if (term_width < 78 || term_height < 24) {
            clear();
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(term_height / 2 - 1, (term_width - 24) / 2 > 0 ? (term_width - 24) / 2 : 0, "TERMINAL TOO SMALL");
            attroff(COLOR_PAIR(4) | A_BOLD);
            mvprintw(term_height / 2, (term_width - 32) / 2 > 0 ? (term_width - 32) / 2 : 0, "Please resize to at least 80x24");
            refresh();
            
            // Wait for next key (e.g. resize event)
            ch = getch();
            if (ch == 'q' || ch == 'Q') {
                break;
            }
            continue;
        }
        
        display_picture();
        display_sidebar();
        
        ch = getch();
        if (ch == 'q' || ch == 'Q') {
            break;
        }
        
        switch (ch) {
            case 'a':
            case 'A':
                add_shape_menu();
                break;
            case 'd':
            case 'D':
                delete_selected_shape();
                break;
            case 'm':
            case 'M':
                modify_selected_shape();
                break;
            case '+':
            case '=':
                zoom_scale += 0.1;
                if (zoom_scale > 4.0) zoom_scale = 4.0;
                render_all_shapes();
                display_status("");
                break;
            case '-':
            case '_':
                zoom_scale -= 0.1;
                if (zoom_scale < 0.2) zoom_scale = 0.2;
                render_all_shapes();
                display_status("");
                break;
            case KEY_UP:
                if (num_shapes > 0) {
                    selected_shape_idx--;
                    if (selected_shape_idx < 0) selected_shape_idx = num_shapes - 1;
                    display_status("");
                }
                break;
            case KEY_DOWN:
                if (num_shapes > 0) {
                    selected_shape_idx++;
                    if (selected_shape_idx >= num_shapes) selected_shape_idx = 0;
                    display_status("");
                }
                break;
            default:
                display_status("");
                break;
        }
    }
    
    // Windows Cleanup
    delwin(canvas_win);
    delwin(sidebar_win);
    delwin(status_win);
    endwin();
    
    // Print the final ASCII output directly to standard stdout upon exiting
    printf("\nFinal 2D ASCII Graphics Art:\n");
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            putchar(canvas[y][x]);
        }
        putchar('\n');
    }
    printf("\nThank you for using the 2D Graphics Editor!\n");
    
    return 0;
}
