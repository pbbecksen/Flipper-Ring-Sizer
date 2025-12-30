#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#define TAG "RingSizer"

// -------- Fixed default scale (no calibration) --------
// Flipper LCD is 128x64 px, diagonal ~1.4".
// diag_mm = 1.4 * 25.4 = 35.56mm
// width_mm = diag_mm * 2/sqrt(5) ≈ 31.80mm  => 31.80/128 = 0.24844 mm/px
// Use integer micrometers-per-pixel for portability across firmwares.
#define UM_PER_PX_DEFAULT 248u // 0.248 mm/px (rounded)

// pi in fixed-point: 3.1415926 * 1,000,000
#define PI_X1000000 3141593u

typedef struct {
    uint8_t us_x2;      // US size * 2 (e.g., 7.5 => 15)
    uint16_t dia_x100;  // inner diameter in 0.01mm (e.g., 17.30mm => 1730)
} UsDia;

// Common US size to diameter table (approx)
static const UsDia us_table[] = {
    { 6, 1410}, { 7, 1450}, { 8, 1490}, { 9, 1530},
    {10, 1570}, {11, 1610}, {12, 1650}, {13, 1690},
    {14, 1730}, {15, 1770}, {16, 1810}, {17, 1850},
    {18, 1890}, {19, 1940}, {20, 1980}, {21, 2020},
    {22, 2060}, {23, 2100}, {24, 2140}, {25, 2180},
    {26, 2220}, {27, 2260}, {28, 2300}, {29, 2340},
    {30, 2380},
};
static const size_t us_table_len = sizeof(us_table) / sizeof(us_table[0]);

typedef enum {
    ScreenMenu = 0,
    ScreenMeasure,
    ScreenMeasureResult,
    ScreenDisplayPrompt,
    ScreenDisplayCircle,
} Screen;

typedef enum {
    DisplayModeUS = 0,
    DisplayModeEU,
} DisplayMode;

typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* input_queue;

    Screen screen;

    // menu (2 items)
    uint8_t menu_index; // 0..1

    // scale
    uint32_t um_per_px;

    // circle
    int radius_px;

    // measure result
    uint8_t last_us_x2;
    uint16_t last_eu;       // EU circumference (mm)
    uint16_t last_dia_x100; // diameter (0.01mm)

    // display prompt
    DisplayMode prompt_mode;
    uint8_t prompt_us_x2;   // 6..30 (3.0..15.0)
    uint16_t prompt_eu;     // 40..80
} App;

// -------- formatting helpers (no floats) --------
static void format_us_x2(char* out, size_t out_sz, uint8_t us_x2) {
    if(us_x2 % 2 == 0) {
        snprintf(out, out_sz, "%u", (unsigned)(us_x2 / 2));
    } else {
        snprintf(out, out_sz, "%u.5", (unsigned)(us_x2 / 2));
    }
}

static void format_mm_x100(char* out, size_t out_sz, uint16_t mm_x100) {
    snprintf(out, out_sz, "%u.%02u", (unsigned)(mm_x100 / 100), (unsigned)(mm_x100 % 100));
}

// -------- ring math (fixed-point / integer) --------
static uint16_t dia_x100_from_radius_px(const App* app, int radius_px) {
    // diameter_um = 2*r*um_per_px
    // dia_mm_x100 = diameter_um / 10 (since 0.01mm = 10um)
    uint64_t num = (uint64_t)(2 * radius_px) * (uint64_t)app->um_per_px;
    return (uint16_t)((num + 5) / 10); // rounded
}

static uint16_t eu_from_dia_x100(uint16_t dia_x100) {
    // circ_mm = PI * dia_mm = PI * (dia_x100/100)
    // using PI_X1000000: circ_mm = PI_X1000000 * dia_x100 / (100 * 1,000,000)
    uint64_t num = (uint64_t)PI_X1000000 * (uint64_t)dia_x100;
    return (uint16_t)((num + 50000000ull) / 100000000ull); // rounded
}

static uint8_t us_from_dia_x100(uint16_t dia_x100) {
    uint8_t best = us_table[0].us_x2;
    int best_err = abs((int)dia_x100 - (int)us_table[0].dia_x100);
    for(size_t i = 1; i < us_table_len; i++) {
        int err = abs((int)dia_x100 - (int)us_table[i].dia_x100);
        if(err < best_err) {
            best_err = err;
            best = us_table[i].us_x2;
        }
    }
    return best;
}

static uint16_t dia_x100_from_us_x2(uint8_t us_x2) {
    uint16_t best_d = us_table[0].dia_x100;
    int best_err = abs((int)us_x2 - (int)us_table[0].us_x2);
    for(size_t i = 1; i < us_table_len; i++) {
        int err = abs((int)us_x2 - (int)us_table[i].us_x2);
        if(err < best_err) {
            best_err = err;
            best_d = us_table[i].dia_x100;
        }
    }
    return best_d;
}

static uint16_t dia_x100_from_eu(uint16_t eu) {
    // dia_x100 = eu * 100 / pi
    uint64_t num = (uint64_t)eu * 100ull * 1000000ull;
    return (uint16_t)((num + (PI_X1000000 / 2)) / (uint64_t)PI_X1000000);
}

static int radius_from_dia_x100(const App* app, uint16_t dia_x100) {
    uint32_t dia_um = (uint32_t)dia_x100 * 10u;
    int r = (int)((dia_um + app->um_per_px) / (2u * app->um_per_px));
    if(r < 1) r = 1;
    return r;
}

// -------- UI helpers --------
static void draw_center_stack(Canvas* canvas, uint16_t dia_x100, uint8_t us_x2, uint16_t eu) {
    char d_str[12], us_str[8], eu_str[8];
    format_mm_x100(d_str, sizeof(d_str), dia_x100);
    format_us_x2(us_str, sizeof(us_str), us_x2);
    snprintf(eu_str, sizeof(eu_str), "%u", (unsigned)eu);

    char line1[20], line2[20], line3[20];
    snprintf(line1, sizeof(line1), "D %s mm", d_str);
    snprintf(line2, sizeof(line2), "US %s", us_str);
    snprintf(line3, sizeof(line3), "EU %s", eu_str);

    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 64, 22, AlignCenter, AlignTop, line1);
    elements_multiline_text_aligned(canvas, 64, 34, AlignCenter, AlignTop, line2);
    elements_multiline_text_aligned(canvas, 64, 46, AlignCenter, AlignTop, line3);
}

static void draw_menu(Canvas* canvas, const App* app) {
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Ring Sizer");

    const char* items[2] = {"Measure", "Display"};
    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < 2; i++) {
        int y = 32 + i * 14;
        if(i == app->menu_index) {
            canvas_draw_box(canvas, 26, y - 10, 76, 12);
            canvas_set_color(canvas, ColorWhite);
            elements_multiline_text_aligned(canvas, 64, y - 9, AlignCenter, AlignTop, items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            elements_multiline_text_aligned(canvas, 64, y - 9, AlignCenter, AlignTop, items[i]);
        }
    }
}

// Measure: centered circle, clean center labels
static void draw_measure(Canvas* canvas, const App* app) {
    const int cx = 64;
    const int cy = 32; // true vertical center
    canvas_draw_circle(canvas, cx, cy, app->radius_px);

    uint16_t dia_x100 = dia_x100_from_radius_px(app, app->radius_px);
    uint16_t eu = eu_from_dia_x100(dia_x100);
    uint8_t us_x2 = us_from_dia_x100(dia_x100);

    draw_center_stack(canvas, dia_x100, us_x2, eu);
}

static void draw_measure_result(Canvas* canvas, const App* app) {
    // Same look as measure, but frozen values
    const int cx = 64;
    const int cy = 32;
    canvas_draw_circle(canvas, cx, cy, radius_from_dia_x100(app, app->last_dia_x100));
    draw_center_stack(canvas, app->last_dia_x100, app->last_us_x2, app->last_eu);
}

// Display Prompt: minimal prompt first
static void draw_display_prompt(Canvas* canvas, const App* app) {
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Display");

    canvas_set_font(canvas, FontSecondary);
    if(app->prompt_mode == DisplayModeUS) {
        elements_multiline_text_aligned(canvas, 64, 26, AlignCenter, AlignTop, "US Size");
        char us_str[8];
        format_us_x2(us_str, sizeof(us_str), app->prompt_us_x2);
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 38, AlignCenter, AlignTop, us_str);
    } else {
        elements_multiline_text_aligned(canvas, 64, 26, AlignCenter, AlignTop, "EU Size");
        char eu_str[8];
        snprintf(eu_str, sizeof(eu_str), "%u", (unsigned)app->prompt_eu);
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 38, AlignCenter, AlignTop, eu_str);
    }
}

// Display circle view: circle + center stack
static void draw_display_circle(Canvas* canvas, const App* app) {
    uint16_t dia_x100 = (app->prompt_mode == DisplayModeUS) ?
        dia_x100_from_us_x2(app->prompt_us_x2) :
        dia_x100_from_eu(app->prompt_eu);

    int r = radius_from_dia_x100(app, dia_x100);
    canvas_draw_circle(canvas, 64, 32, r);

    uint16_t eu = eu_from_dia_x100(dia_x100);
    uint8_t us_x2 = us_from_dia_x100(dia_x100);

    draw_center_stack(canvas, dia_x100, us_x2, eu);
}

// -------- rendering --------
static void vp_draw(Canvas* canvas, void* ctx) {
    App* app = ctx;
    canvas_clear(canvas);

    switch(app->screen) {
    case ScreenMenu: draw_menu(canvas, app); break;
    case ScreenMeasure: draw_measure(canvas, app); break;
    case ScreenMeasureResult: draw_measure_result(canvas, app); break;
    case ScreenDisplayPrompt: draw_display_prompt(canvas, app); break;
    case ScreenDisplayCircle: draw_display_circle(canvas, app); break;
    default: break;
    }
}

// -------- input plumbing --------
static void vp_input(InputEvent* event, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->input_queue, event, 0);
}

static void go_menu(App* app) {
    app->screen = ScreenMenu;
}

static void enter_display(App* app) {
    app->screen = ScreenDisplayPrompt;
    app->prompt_mode = DisplayModeUS;
    app->prompt_us_x2 = 14; // 7.0
    app->prompt_eu = 54;
}

int32_t ring_sizer_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->um_per_px = UM_PER_PX_DEFAULT;
    app->radius_px = 22;

    app->menu_index = 0;
    app->screen = ScreenMenu;

    app->prompt_mode = DisplayModeUS;
    app->prompt_us_x2 = 14;
    app->prompt_eu = 54;

    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->gui = furi_record_open(RECORD_GUI);
    app->vp = view_port_alloc();
    view_port_draw_callback_set(app->vp, vp_draw, app);
    view_port_input_callback_set(app->vp, vp_input, app);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(app->input_queue, &event, 100) != FuriStatusOk) {
            continue;
        }
        if(event.type != InputTypeShort && event.type != InputTypeRepeat) continue;

        switch(app->screen) {
        case ScreenMenu:
            if(event.key == InputKeyUp) {
                if(app->menu_index > 0) app->menu_index--;
            } else if(event.key == InputKeyDown) {
                if(app->menu_index < 1) app->menu_index++;
            } else if(event.key == InputKeyOk) {
                if(app->menu_index == 0) app->screen = ScreenMeasure;
                else enter_display(app);
            } else if(event.key == InputKeyBack) {
                running = false; // exit app from menu
            }
            break;

        case ScreenMeasure:
            if(event.key == InputKeyUp) {
                app->radius_px++;
            } else if(event.key == InputKeyDown) {
                if(app->radius_px > 1) app->radius_px--;
            } else if(event.key == InputKeyOk) {
                app->last_dia_x100 = dia_x100_from_radius_px(app, app->radius_px);
                app->last_eu = eu_from_dia_x100(app->last_dia_x100);
                app->last_us_x2 = us_from_dia_x100(app->last_dia_x100);
                app->screen = ScreenMeasureResult;
            } else if(event.key == InputKeyBack) {
                go_menu(app);
            }
            break;

        case ScreenMeasureResult:
            if(event.key == InputKeyOk || event.key == InputKeyBack) {
                go_menu(app);
            }
            break;

        case ScreenDisplayPrompt:
            if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                app->prompt_mode = (app->prompt_mode == DisplayModeUS) ? DisplayModeEU : DisplayModeUS;
            } else if(event.key == InputKeyUp) {
                if(app->prompt_mode == DisplayModeUS) {
                    if(app->prompt_us_x2 < 30) app->prompt_us_x2++;
                } else {
                    if(app->prompt_eu < 80) app->prompt_eu++;
                }
            } else if(event.key == InputKeyDown) {
                if(app->prompt_mode == DisplayModeUS) {
                    if(app->prompt_us_x2 > 6) app->prompt_us_x2--;
                } else {
                    if(app->prompt_eu > 40) app->prompt_eu--;
                }
            } else if(event.key == InputKeyOk) {
                app->screen = ScreenDisplayCircle;
            } else if(event.key == InputKeyBack) {
                go_menu(app);
            }
            break;

        case ScreenDisplayCircle:
            // allow quick tweaks without going back
            if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                app->prompt_mode = (app->prompt_mode == DisplayModeUS) ? DisplayModeEU : DisplayModeUS;
            } else if(event.key == InputKeyUp) {
                if(app->prompt_mode == DisplayModeUS) {
                    if(app->prompt_us_x2 < 30) app->prompt_us_x2++;
                } else {
                    if(app->prompt_eu < 80) app->prompt_eu++;
                }
            } else if(event.key == InputKeyDown) {
                if(app->prompt_mode == DisplayModeUS) {
                    if(app->prompt_us_x2 > 6) app->prompt_us_x2--;
                } else {
                    if(app->prompt_eu > 40) app->prompt_eu--;
                }
            } else if(event.key == InputKeyOk || event.key == InputKeyBack) {
                go_menu(app);
            }
            break;

        default:
            break;
        }

        view_port_update(app->vp);
    }

    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->input_queue);
    free(app);
    return 0;
}
