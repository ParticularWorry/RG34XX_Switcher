#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "engine.h"
#include "timing.h"

static const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const uint8_t font[36][7] = {
 {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
 {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
 {31,4,4,4,4,4,31}, {7,2,2,2,18,18,12},
 {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
 {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
 {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static void draw_text(SDL_Renderer *r, int x, int y,
                      const char *s, int scale,
                      uint8_t red, uint8_t green, uint8_t blue) {
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    for (; *s; s++, x += 6 * scale) {
        const char *p = strchr(chars, *s);
        if (!p) continue;
        int index = (int)(p - chars);
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (font[index][row] & (1 << (4 - col))) {
                    SDL_Rect pixel = {
                        x + col * scale, y + row * scale,
                        scale, scale
                    };
                    SDL_RenderFillRect(r, &pixel);
                }
    }
}

static void rectangle(SDL_Renderer *r, int x, int y, int w, int h,
                      int red, int green, int blue) {
    SDL_Rect box = {x, y, w, h};
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    SDL_RenderFillRect(r, &box);
}


/* Retro PC turbo display, drawn entirely with SDL2. */
static void turbo_rect(SDL_Renderer *r,
                       int x, int y, int w, int h,
                       int red, int green, int blue) {
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void turbo_digit(SDL_Renderer *r,
                        int x, int y, int digit) {
    static const unsigned char masks[10] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66,
        0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };

    static const SDL_Rect segments[7] = {
        { 8,  0, 28, 6}, /* top */
        {36,  7,  6, 25}, /* upper right */
        {36, 39,  6, 25}, /* lower right */
        { 8, 65, 28, 6}, /* bottom */
        { 2, 39,  6, 25}, /* lower left */
        { 2,  7,  6, 25}, /* upper left */
        { 8, 32, 28, 6}  /* middle */
    };

    unsigned char mask =
        digit >= 0 && digit <= 9 ? masks[digit] :
        digit == -2 ? 0x40 : 0;

    for (int i = 0; i < 7; i++) {
        int active = (mask & (1 << i)) != 0;

        turbo_rect(r,
                   x + segments[i].x,
                   y + segments[i].y,
                   segments[i].w,
                   segments[i].h,
                   active ? 35 : 10,
                   active ? 255 : 43,
                   active ? 75 : 17);
    }
}

static void draw_turbo_display(SDL_Renderer *r,
                               double hz) {
    int digits[3] = {-2, -2, -2};
    int count = 3;

    if (hz >= 1.0 && hz <= 999.0) {
        int number = (int)(hz + 0.5);

        count = number >= 100 ? 3 :
                number >= 10 ? 2 : 1;

        for (int i = count - 1; i >= 0; i--) {
            digits[i] = number % 10;
            number /= 10;
        }
    }

    /*
     * Dedicated right-hand column.
     * Covers the old full-width menu bars without
     * covering their text.
     */
    turbo_rect(r, 429, 105, 285, 293, 15, 20, 30);

    /* Subtle column separator */
    turbo_rect(r, 430, 113, 2, 276, 49, 62, 76);

    draw_text(r, 451, 115,
              "CURRENT MODE",
              2, 130, 235, 175);

    /* Vintage gray bezel */
    turbo_rect(r, 443, 137, 235, 165, 110, 114, 108);
    turbo_rect(r, 447, 141, 227, 157, 173, 174, 163);
    turbo_rect(r, 453, 165, 215, 111, 43, 48, 43);

    /* Black glass */
    turbo_rect(r, 459, 171, 203, 99, 8, 19, 10);

    /* Fake mounting screws */
    turbo_rect(r, 453, 146, 5, 5, 55, 57, 55);
    turbo_rect(r, 664, 146, 5, 5, 55, 57, 55);
    turbo_rect(r, 453, 287, 5, 5, 55, 57, 55);
    turbo_rect(r, 664, 287, 5, 5, 55, 57, 55);

    draw_text(r, 517, 146, "TURBO",
              2, 25, 28, 25);

    int start = count == 3 ? 479 : count == 2 ? 511 : 538;

    for (int i = 0; i < count; i++)
        turbo_digit(r, start + i * 53, 185, digits[i]);

    draw_text(r, 609, 280, "HZ",
              2, 25, 28, 25);




    char timing_label[48];

    if (hz > 0.0)
        snprintf(timing_label, sizeof(timing_label),
                 "CALC %.2f HZ", hz);
    else
        snprintf(timing_label, sizeof(timing_label),
                 "TIMING READ FAILED");

    draw_text(r, 452, 310, timing_label,
              2, 155, 190, 205);

    draw_text(r, 452, 330,
              "NOT MEASURED LIVE",
              2, 155, 190, 205);

    draw_text(r, 452, 350,
              "CHECK RETROARCH FOR",
              2, 155, 190, 205);

    draw_text(r, 452, 370,
              "ACCURATE HZ READING",
              2, 155, 190, 205);

}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
                 SDL_INIT_GAMECONTROLLER) != 0)
        return 1;

    SDL_Window *window = SDL_CreateWindow(
        "RG34XX Refresh Switcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        720, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);

    if (!window) {
        SDL_Quit();
        return 2;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    SDL_RenderSetLogicalSize(renderer, 720, 480);

    SDL_GameController *controller = NULL;
    SDL_Joystick *joystick = NULL;

    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0))
            controller = SDL_GameControllerOpen(0);
        else
            joystick = SDL_JoystickOpen(0);
    }

    const char *items[4] = {
        "BACKUP ORIGINAL",
        "SET 60 HZ",
        "SET 120 HZ",
        "RESTORE BACKUP"
    };

    int pending_write_popup = 0;
    int flash_success_popup = 0;
    Uint32 write_popup_opened = 0;
    const char *status = "";
    double refresh_hz = timing_read_current_hz();
    if (backup_ok()) {
        items[1] = "SET 60 HZ";
        items[2] = "SET 120 HZ";
    }
    int selected = flash_backup_exists() ? 1 : 0;
    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            /* Disabled backup must never be activated. */
            if (flash_backup_exists() && selected == 0) {
                selected = 1;

                if (event.type == SDL_KEYDOWN ||
                    event.type == SDL_CONTROLLERBUTTONDOWN ||
                    event.type == SDL_JOYBUTTONDOWN)
                    continue;
            }

            if (flash_success_popup && event.type != SDL_QUIT) {
                int close_popup = 0;

                if (event.type == SDL_KEYDOWN) {
                    SDL_Keycode key = event.key.keysym.sym;
                    close_popup =
                        key == SDLK_ESCAPE ||
                        key == SDLK_BACKSPACE ||
                        key == SDLK_RETURN ||
                        key == SDLK_x ||
                        key == SDLK_z;
                }

                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    close_popup =
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_A;
                }

                if (event.type == SDL_JOYBUTTONDOWN) {
                    close_popup =
                        event.jbutton.button == 0 ||
                        event.jbutton.button == 1;
                }

                if (close_popup)
                    flash_success_popup = 0;

                continue;
            }

            if (pending_write_popup && event.type != SDL_QUIT) {
                int confirm = 0;
                int cancel = 0;

                if (event.type == SDL_KEYDOWN) {
                    SDL_Keycode key = event.key.keysym.sym;
                    confirm = key == SDLK_RETURN ||
                              key == SDLK_KP_ENTER ||
                              key == SDLK_z;
                    cancel = key == SDLK_ESCAPE ||
                             key == SDLK_BACKSPACE ||
                             key == SDLK_x;
                }

                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    confirm = event.cbutton.button ==
                              SDL_CONTROLLER_BUTTON_A;
                    cancel = event.cbutton.button ==
                             SDL_CONTROLLER_BUTTON_B;
                }

                if (event.type == SDL_JOYBUTTONDOWN) {
                    confirm = event.jbutton.button == 0;
                    cancel = event.jbutton.button == 1;
                }

                if (SDL_GetTicks() - write_popup_opened >= 250) {
                    if (confirm) {
                        status = engine_action(pending_write_popup);
                        flash_success_popup =
                            strcmp(status,
                                   "FLASH VERIFIED REBOOT REQUIRED") == 0;

                refresh_hz = timing_read_current_hz();
                        pending_write_popup = 0;
                    } else if (cancel) {
                        status = "OPERATION CANCELLED";
                        pending_write_popup = 0;
                    }
                }

                continue;
            }

            int action = 0;

            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    action = 1; break;
                    case SDLK_DOWN:  action = 2; break;
                    case SDLK_RETURN:
                    case SDLK_z:     action = 3; break;
                    case SDLK_ESCAPE:
                    case SDLK_BACKSPACE:
                    case SDLK_x:     action = 4; break;
                    default: break;
                }
            }

            if (controller &&
                event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        action = 1; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        action = 2; break;
                    case SDL_CONTROLLER_BUTTON_A:
                        action = 3; break;
                    case SDL_CONTROLLER_BUTTON_B:
                        action = 4; break;
                    default: break;
                }
            }

            if (joystick && event.type == SDL_JOYHATMOTION) {
                if (event.jhat.value & SDL_HAT_UP)
                    action = 1;
                else if (event.jhat.value & SDL_HAT_DOWN)
                    action = 2;
            }

            if (joystick && event.type == SDL_JOYBUTTONDOWN) {
                if (event.jbutton.button == 0) action = 3;
                if (event.jbutton.button == 1) action = 4;
            }

            if (action == 1)
                selected = flash_backup_exists()
                ? (selected == 1 ? 3 : selected - 1)
                : (selected + 3) % 4;
            else if (action == 2)
                selected = flash_backup_exists()
                ? (selected == 3 ? 1 : selected + 1)
                : (selected + 1) % 4;
            else if (action == 3) {
                if (selected >= 1 && selected <= 3) {
                    pending_unknown_backup = 0;
                    pending_write_popup = selected;
                    write_popup_opened = SDL_GetTicks();
                    status = "CONFIRM DISPLAY TIMINGS";
                } else {
                    status = engine_action(selected);
                refresh_hz = timing_read_current_hz();
                }
                if (backup_ok()) {
                    items[1] = "SET 60 HZ";
                    items[2] = "SET 120 HZ";
                }
            }
            else if (action == 4)
                running = false;
        }

        SDL_SetRenderDrawColor(renderer, 15, 20, 30, 255);
        if (flash_backup_exists() && selected == 0)
            selected = 1;

        SDL_RenderClear(renderer);

        draw_text(renderer, 65, 35,
                  "RG34XX REFRESH SWITCHER", 3, 240, 245, 255);
        draw_text(renderer, 65, 77,
                  "TURBO MODE - PIXEL POWER", 2, 140, 165, 190);

        for (int i = 0; i < 4; i++) {
            int y = 112 + i * 65;

            if (i == selected)
                rectangle(renderer, 55, y, 610, 55, 45, 105, 120);
            else
                rectangle(renderer, 55, y, 610, 55, 40, 48, 62);

            draw_text(renderer, 78, y + 17, items[i], 3,
                      i == selected ? 255 : 190,
                      i == selected ? 255 : 200,
                      i == selected ? 255 : 215);
        }

        
        if (flash_backup_exists()) {
            SDL_SetRenderDrawColor(
                renderer, 35, 40, 49, 255);

            SDL_Rect disabled_bar = {55, 112, 610, 55};
            SDL_RenderFillRect(renderer, &disabled_bar);

            draw_text(renderer, 77, 128,
                      "BACKUP ORIGINAL",
                      3, 105, 112, 125);
        }

        draw_turbo_display(renderer, refresh_hz);
        draw_text(renderer, 65, 390, status, 2, 255, 190, 100);
        draw_text(renderer, 65, 414,
                  "THANKS TO THEGAMMASQUEEZE",
                  2, 120, 200, 175);
        draw_text(renderer, 65, 438,
                  "DPAD SELECT   A INFO   B EXIT",
                  2, 160, 180, 200);

        
        if (pending_unknown_backup) {
            SDL_SetRenderDrawBlendMode(
                renderer, SDL_BLENDMODE_BLEND);

            /* Darken the menu behind the popup */
            SDL_SetRenderDrawColor(
                renderer, 0, 0, 0, 195);
            SDL_Rect shade = {0, 0, 720, 480};
            SDL_RenderFillRect(renderer, &shade);

            /* Popup background */
            SDL_SetRenderDrawColor(
                renderer, 23, 32, 46, 255);
            SDL_Rect panel = {85, 145, 550, 190};
            SDL_RenderFillRect(renderer, &panel);

            /* Gold border */
            SDL_SetRenderDrawColor(
                renderer, 245, 185, 80, 255);
            SDL_RenderDrawRect(renderer, &panel);

            SDL_Rect inner = {89, 149, 542, 182};
            SDL_RenderDrawRect(renderer, &inner);

            draw_text(renderer, 157, 177,
                      "UNRECOGNIZED BOOT PACKAGE",
                      2, 255, 190, 90);

            draw_text(renderer, 222, 226,
                      "CONTINUE BACKUP?",
                      2, 255, 255, 255);

            draw_text(renderer, 208, 280,
                      "A YES     B NO",
                      2, 120, 225, 165);
        }

        
        if (pending_write_popup) {
            SDL_SetRenderDrawBlendMode(
                renderer, SDL_BLENDMODE_BLEND);

            SDL_SetRenderDrawColor(
                renderer, 0, 0, 0, 210);
            SDL_Rect shade = {0, 0, 720, 480};
            SDL_RenderFillRect(renderer, &shade);

            SDL_SetRenderDrawColor(
                renderer, 23, 32, 46, 255);
            SDL_Rect panel = {65, 88, 590, 310};
            SDL_RenderFillRect(renderer, &panel);

            SDL_SetRenderDrawColor(
                renderer, 245, 185, 80, 255);
            SDL_RenderDrawRect(renderer, &panel);

            SDL_Rect inner = {69, 92, 582, 302};
            SDL_RenderDrawRect(renderer, &inner);

            draw_text(renderer, 105, 116,
                      pending_write_popup == 3
                          ? "RESTORE BACKUP WARNING"
                          : "DISPLAY TIMINGS WARNING",
                      2, 255, 190, 90);

            draw_text(renderer, 105, 151,
                      "BOOT AREA: /DEV/MMCBLK0",
                      2, 255, 255, 255);

            draw_text(renderer, 105, 182,
                      pending_write_popup == 3
                          ? "SAVED BOOT PACKAGE"
                          : "PIXEL CLOCK: 54 MHZ",
                      2, 200, 220, 255);

            draw_text(renderer, 105, 205,
                      pending_write_popup == 3
                          ? "EXACT BACKUP BYTES"
                          : pending_write_popup == 1
                          ? "HORIZONTAL TOTAL: 1679"
                          : "HORIZONTAL TOTAL: 840",
                      2, 200, 220, 255);

            draw_text(renderer, 105, 228,
                      pending_write_popup == 3
                          ? "RESTORE SAVED IMAGE"
                          : "VERTICAL TOTAL: 536",
                      2, 200, 220, 255);

            draw_text(renderer, 105, 251,
                      pending_write_popup == 1
                          ? "TARGET: 60 HZ"
                          : pending_write_popup == 2
                          ? "TARGET: 120 HZ EXPERIMENTAL"
                          : "TARGET: SAVED BACKUP",
                      2, 255, 220, 120);

            draw_text(renderer, 105, 292,
                      "THIS WILL WRITE BOOT AREA",
                      2, 255, 145, 120);

            draw_text(renderer, 105, 345,
                      "A FLASH    B CANCEL",
                      2, 120, 225, 165);
        }

        
        if (flash_success_popup) {
            SDL_SetRenderDrawBlendMode(
                renderer, SDL_BLENDMODE_BLEND);

            /* Dark overlay */
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 215);
            SDL_Rect shade = {0, 0, 720, 480};
            SDL_RenderFillRect(renderer, &shade);

            /* Popup panel */
            SDL_SetRenderDrawColor(renderer, 23, 32, 46, 255);
            SDL_Rect panel = {90, 105, 540, 285};
            SDL_RenderFillRect(renderer, &panel);

            /* Golden double border */
            SDL_SetRenderDrawColor(renderer, 245, 185, 80, 255);
            SDL_RenderDrawRect(renderer, &panel);

            SDL_Rect inner = {94, 109, 532, 277};
            SDL_RenderDrawRect(renderer, &inner);

            draw_text(renderer, 210, 133,
                      "FLASH SUCCESSFUL!",
                      2, 130, 235, 175);

            /* Happy pixel robot */
            SDL_SetRenderDrawColor(renderer, 245, 185, 80, 255);

            SDL_Rect head = {318, 186, 84, 74};
            SDL_RenderFillRect(renderer, &head);

            SDL_SetRenderDrawColor(renderer, 23, 32, 46, 255);
            SDL_Rect face = {324, 192, 72, 62};
            SDL_RenderFillRect(renderer, &face);

            /* Antenna */
            SDL_SetRenderDrawColor(renderer, 245, 185, 80, 255);
            SDL_Rect antenna = {357, 175, 6, 11};
            SDL_RenderFillRect(renderer, &antenna);

            SDL_SetRenderDrawColor(renderer, 130, 235, 175, 255);
            SDL_Rect antenna_tip = {354, 169, 12, 8};
            SDL_RenderFillRect(renderer, &antenna_tip);

            /* Eyes */
            SDL_Rect eye1 = {338, 211, 10, 10};
            SDL_Rect eye2 = {372, 211, 10, 10};
            SDL_RenderFillRect(renderer, &eye1);
            SDL_RenderFillRect(renderer, &eye2);

            /* Pixel smile */
            SDL_Rect mouth1 = {340, 233, 6, 7};
            SDL_Rect mouth2 = {346, 240, 28, 6};
            SDL_Rect mouth3 = {374, 233, 6, 7};

            SDL_RenderFillRect(renderer, &mouth1);
            SDL_RenderFillRect(renderer, &mouth2);
            SDL_RenderFillRect(renderer, &mouth3);

            draw_text(renderer, 264, 287,
                      "PLEASE REBOOT!",
                      2, 255, 255, 255);

            draw_text(renderer, 310, 343,
                      "B CLOSE",
                      2, 130, 235, 175);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (controller) SDL_GameControllerClose(controller);
    if (joystick) SDL_JoystickClose(joystick);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
