/* Pebble watchface that shows time until arrival of the next bus.
 *
 * It is a watchface, and not a watchapp, to make even very quickly
 * available. It also assumes you care about a single bus going to
 * a single direction from a single stop.
 *
 * The bus number and timetable is part of the source; making it
 * customizable via JS on the phone is too much for the first Pebble
 * app.
 *
 * Copyright Vladimir Prus 2014.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt
 * or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <pebble.h>
#include <pebble_fonts.h>

/* We need more data types. */

typedef struct { int hour; int minute; } day_time;
#define day_time(h, m) ((day_time){(h), (m)})

/* The parameters. */

static char *bus_number = "700";

static day_time schedule[] = {
        {5, 47},
        {6, 6},
        {6, 25},
        {6, 45},
        {7, 6},
        {7, 21},
        {7, 37},
        {7, 53},
        {8, 9},
        {8, 25},
        {8, 40},
        {9, 7},
        {9, 29},
        {9, 51},
        {10, 14},
        {10, 36},
        {10, 58},
        {11, 19},
        {11, 40},
        {12, 2},
        {12, 23},
        {12, 44},
        {13, 06},
        {13, 27},
        {13, 48},
        {14, 10},
        {14, 26},
        {14, 42},
        {14, 58},
        {15, 14},
        {15, 30},
        {15, 46},
        {16, 7},
        {16, 28},
        {16, 50},
        {17, 6},
        {17, 22},
        {17, 38},
        {17, 54},
        {18, 10},
        {18, 26},
        {18, 42},
        {18, 58},
        {19, 14},
        {19, 30},
        {19, 54},
        {20, 18},
        {20, 42},
        {21, 5},
        {21, 24},
        {21, 43},
        {22, 3},
        {22, 22},
        {22, 41},
        {23, 1},
        {23, 20},
        {23, 39},
        {23, 59},
        {0, 18},
        {0, 37}
};
static int schedule_size = sizeof(schedule)/sizeof(schedule[0]);


static Window *window;
static TextLayer *header;
static TextLayer *counter;
static TextLayer *details;

static int compare(day_time a, day_time b)
{
    if (a.hour < b.hour)
        return -1;
    else if (a.hour == b.hour)
        return a.minute - b.minute;
    else
        return 1;
}

/* Returns the next scheduled arrival. If there are no more
 * arrivals today, return first arrival tomorrow, with hour
 * incremented by 24.
 */
day_time next_arrival(day_time now)
{
    for (int i = 0; i < schedule_size; ++i)
    {
        if (compare(schedule[i], now) > 0)
            return schedule[i];
    }

    day_time a = schedule[0];
    a.hour += 24;
    return a;
}

/* Returns difference in minutes between 'next' and 'now'
 *
 *  Precondition: 'next' is greater than 'now'.
 */
int minutes_difference(const day_time* next, const day_time* now) {
    int mins = (next->minute - now->minute);
    int carry = 0;
    if (mins < 0) {
        mins += 60;
        carry = 1;
    }
    mins += (next->hour - now->hour - carry) * 60;
    return mins;
}

/* Function that is called every minute to update all dynamic content. */
static void update_content()
{
    static char buf[10];
    static char buf2[20];

    time_t now_ticks;
    time(&now_ticks);
    struct tm *now_tm = localtime(&now_ticks);
    day_time now = day_time(now_tm->tm_hour, now_tm->tm_min);
    day_time a = next_arrival(now);

    int mins = minutes_difference(&a, &now);
    if (mins > 60) {
        text_layer_set_text(counter, "60+m");
    } else {
        snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%dm", mins);
        text_layer_set_text(counter, buf);
    }
    layer_mark_dirty(text_layer_get_layer(counter));

    if (a.hour > 24)
        a.hour -= 24;

    snprintf(buf2, sizeof(buf2)/sizeof(buf2[0]), "%s:  %d:%02d", bus_number, a.hour, a.minute);
    text_layer_set_text(details, buf2);
    layer_mark_dirty(text_layer_get_layer(details));
}

/* Create a text layer with dimentions of rect, and configure it so
 * that a single text line of 'font_size' will have it centerline aligned
 * with the centerline of the rect.
 *
 * Here, 'font_size' may be either 42 or 28.
 *
 * Colors and alignment are set to certain default values specific for
 * this application.
 */
static TextLayer *aligned_text_layer_create(GRect rect, int font_size)
{
    TextLayer *l = text_layer_create(rect);

    GFont font;
    int height;
    /* Pebble API does not allow to get a height of GFont, so just hardcode
     * known sizes. */
    if (font_size == 42) {
        font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
        height = 30;
    } else if (font_size == 28) {
        font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
        height = 18;
    } else {
        font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
        height = 18;
    }

    // We want center line of text to be at the vertical center of rect.
    int offset = rect.size.h / 2;
    // By default, baseline of the text is already at font_size-1 y coordinate.
    offset -= font_size;
    // Further move it by half the height of the font.
    offset += height / 2;
    // Compensate apparent bug of text layer, where setting bounds move it
    // by twice the amount.
    offset /= 2;

    text_layer_set_font(l, font);
    layer_set_bounds(
            text_layer_get_layer(l),
            (GRect) { .origin = { 0, offset }, .size = rect.size } );

    // Hardcode various other properties used by this app.
    text_layer_set_text_color(l, GColorWhite);
    text_layer_set_background_color(l, GColorBlack);
    text_layer_set_text_alignment(l, GTextAlignmentCenter);

    return l;
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    GRect bounds = layer_get_bounds(window_layer);

    header = aligned_text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, 42 } }, 28);
    text_layer_set_text(header, "Next Bus");
    layer_add_child(window_layer, text_layer_get_layer(header));

    counter = aligned_text_layer_create((GRect) { .origin = { 0, 42 }, .size = { bounds.size.w, 84 } }, 42);
    layer_add_child(window_layer, text_layer_get_layer(counter));

    details = aligned_text_layer_create((GRect) { .origin = { 0, 168 - 42 }, .size = { bounds.size.w, 42 } }, 28);
    layer_add_child(window_layer, text_layer_get_layer(details));

    update_content();
}

static void window_unload(Window *window) {
    text_layer_destroy(counter);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
    update_content();
}

static void init(void) {
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
                .unload = window_unload,
    });
    window_set_background_color(window, GColorBlack);
    window_stack_push(window, true /* animated */);

    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
}

static void deinit(void) {
    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
