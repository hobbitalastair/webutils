/* comic-viewer.c
 *
 * Experimental low-memory comic viewer.
 *
 * Author:  Alastair Hughes
 * Contact: hobbitalastair at yandex dot com
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include <libnsfb.h>
#include <libnsfb_event.h>
#include <libnsfb_plot.h>

#define SURFACE_TYPE NSFB_SURFACE_SDL /* Default surface type */
#define BACKGROUND_COLOUR 0xFF000000 /* Black (ABGR) */
#define ERROR_COLOUR 0xFF0000FF /* Bright red (ABGR) */
#define PAGE_MULT 0.7 /* Fraction of page to move for a PageUp or PageDown */
#define ARROW_MULT 0.1 /* Fraction of page to move for arrow keys */
#define READ_BUF_SIZE 4096 /* Read buffer size (pipe buffer, so <= 4K) */
#define FALLBACK_HEIGHT 200 /* Height for images which failed to load */
#define SCALE_FACTOR 0.7 /* Scale factor used for +/- scaling (<1) */
#define TO_FARBFELD "/usr/bin/2ff" /* Helper program to convert to farbfeld */


typedef struct {
    /* Current (scrolled) offsets */
    int offset_y;
    int offset_x;

    /* Current image scaling factor */
    float scale_factor;

    /* Visible display width and height */
    int width;
    int height;

    /* Display format, buffer and stride */
    enum nsfb_format_e format;
    uint8_t* buf;
    int stride;

    /* Shared libnsfb context */
    nsfb_t *nsfb;
} display_t;

typedef struct {
    /* Information needed for rendering the images onto the display */

    int image_count; /* Number of images in the array */
    char** images; /* Array of image files to open */
    int* heights; /* An array of image heights */
    int max_width; /* Maximum image width */

    /* We keep a global read buffer.
     *
     * This allows us to read images in (more) quickly without storing huge
     * amounts of data in memory - it turns out that a system call for each
     * pixel is *very* expensive.
     */
    unsigned char readbuf[READ_BUF_SIZE];
} content_t;


void initialise_display(char* name, display_t *d) {
    /* Initialise the given display struct.
     *
     * This exits on failure.
     */

    d->offset_y = 0;
    d->offset_x = 0;
    d->scale_factor = 1;

    d->nsfb = nsfb_new(SURFACE_TYPE);
    if (d->nsfb == NULL || nsfb_init(d->nsfb) != 0) {
        fprintf(stderr, "%s: failed to initialise libnsfb\n", name);
        exit(EXIT_FAILURE);
    }

    if (nsfb_get_geometry(d->nsfb, &d->width, &d->height, &d->format) != 0) {
        fprintf(stderr, "%s: failed to get window geometry\n", name);
        exit(EXIT_FAILURE);
    }
    if (nsfb_get_buffer(d->nsfb, &d->buf, &d->stride) != 0) {
        fprintf(stderr, "%s: failed to get window buffer\n", name);
        exit(EXIT_FAILURE);
    }
}

void resize_display(char* name, display_t *d, int width, int height) {
    /* Resize the display, updating the display struct as required.
     *
     * This (may) call exit on failure.
     */

    if (nsfb_set_geometry(d->nsfb, width, height, NSFB_FMT_ANY) != 0) {
        fprintf(stderr, "%s: failed to resize window region\n", name);
        return;
    }

    if (nsfb_get_geometry(d->nsfb, &d->width, &d->height, &d->format) != 0) {
        fprintf(stderr, "%s: failed to get window geometry\n", name);
        exit(EXIT_FAILURE);
    }
    if (nsfb_get_buffer(d->nsfb, &d->buf, &d->stride) != 0) {
        fprintf(stderr, "%s: failed to get window buffer\n", name);
        exit(EXIT_FAILURE);
    }
}

void initialise_content(char* name, content_t *content,
        int image_count, char** images) {
    /* Initialise the given content struct.
     *
     * This exits on failure.
     */

    content->image_count = image_count;
    content->images = images;

    content->heights = calloc(image_count, sizeof(unsigned int));
    if (content->heights == NULL) {
        fprintf(stderr, "%s: calloc(): %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    content->max_width = 0;
}


bool render_image(char* name, display_t *d, content_t *content,
        int img, int offset) {
    /* Render the given image onto the display.
     *
     * "img" is the index of the image in the context arrays.
     * "offset" is the offset in pixels from the top of the screen to render
     * the image at.
     *
     * If we haven't loaded the image before this will also set the image
     * heights.
     *
     * Return false on failure, true on success.
     */

    /* To render images we have to load them.
     * For this we utilize a helper program which is assumed to take a handle
     * and print the corresponding images to stdout in farbfeld format
     * (tools.suckless.org/farbfeld).
     * Unfortunately most images will be quite a bit too large to store in
     * memory, so we need to reload them each time we go to use them,
     * and loading takes time so we can't preload the images either!
     * Instead, stream the result to the screen as we load the image, using a
     * temporary buffer to avoid calling read() too many times.
     * 
     * FIXME: Loading the same image many times is quite inefficient!
     */

    /* Start the helper process */
    int pipes[2];
    if (pipe(pipes) == -1) {
        fprintf(stderr, "%s: pipe(): %s\n", name, strerror(errno));
        return false;
    }
    pid_t child = fork();
    if (child == -1) {
        fprintf(stderr, "%s: fork(): %s\n", name, strerror(errno));
        return false;
    } else if (child == 0) {
        /* Redirect stdout to the pipe */
        if (dup2(pipes[1], 1) == -1) {
            fprintf(stderr, "%s: dup2(): %s\n", name, strerror(errno));
            exit(EXIT_FAILURE);
        }
        close(pipes[0]);
        close(pipes[1]);

        /* Open the file and pass it to the child as stdin */
        int input = open(content->images[img], 0);
        if (input == -1) {
            fprintf(stderr, "%s: open(%s): %s\n", name, content->images[img],
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (dup2(input, 0) == -1) {
            fprintf(stderr, "%s: dup2(): %s\n", name, strerror(errno));
            exit(EXIT_FAILURE);
        }
        close(input);

        execl(TO_FARBFELD, TO_FARBFELD, NULL);
        fprintf(stderr, "%s: execl(%s): %s\n", name, TO_FARBFELD,
                strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        close(pipes[1]);
    }

    /* Load the header */
    uint32_t header[4];
    // TODO: Retry on transient failure...
    if (read(pipes[0], header, sizeof(header)) != sizeof(header)) {
        fprintf(stderr, "%s: read(): %s\n", name, strerror(errno));
        return false;
    }
    if (memcmp("farbfeld", header, 8) != 0) {
        fprintf(stderr, "%s: bad header magic\n", name);
        return false;
    }
    uint32_t width = ntohl(header[2]);
    uint32_t height = ntohl(header[3]);
    /* We need to save the height and width when we find it, so do that here */
    content->heights[img] = height;
    if (content->max_width < width) {
        content->max_width = width;
    }

    int x = 0; /* Current x position in the image */
    int y = 0; /* Current y position in the image */
    ssize_t count = 1; /* Number of bytes read */
    while (count > 0 || (count == -1 && errno == EINTR)) {
        count = read(pipes[0], &content->readbuf, sizeof(content->readbuf));
        for (int i = 0; i < (count / 8); i++) {
            /* Current x, y positions on the display */
            int display_y = (y / d->scale_factor) + offset;
            int display_x = (x / d->scale_factor) - d->offset_x;

            /* Plot the actual point.
             *
             * Because libnsfb doesn't provide a "set-pixel" function, we
             * plot into the buffer directly. For this to work we need the
             * buffer to be set to NSFB_FMT_RGB888, and we have to be *very*
             * careful not to draw either outside the screen or outside of the
             * area declared by our image (eg if we get extra data from the
             * helper).
             */
            if (y < height &&
                    display_x >= 0 && display_x < d->width &&
                    display_y >= 0 && display_y < d->height) {
                unsigned char blue = content->readbuf[8*i + 0];
                unsigned char green = content->readbuf[8*i + 2];
                unsigned char red = content->readbuf[8*i + 4];

                if (d->format == NSFB_FMT_RGB888) {
                    int buf_offset = display_x * 4 + display_y * d->stride;
                    d->buf[buf_offset + 0] = red;
                    d->buf[buf_offset + 1] = green;
                    d->buf[buf_offset + 2] = blue;
                }
                if (d->format == NSFB_FMT_ARGB8888 ||
                    d->format == NSFB_FMT_XRGB8888) {
                    int buf_offset = display_x * 4 + display_y * d->stride;
                    d->buf[buf_offset + 0] = red;
                    d->buf[buf_offset + 1] = green;
                    d->buf[buf_offset + 2] = blue;
                }
                if (d->format == NSFB_FMT_ABGR8888 ||
                    d->format == NSFB_FMT_XBGR8888) {
                    int buf_offset = display_x * 4 + display_y * d->stride;
                    d->buf[buf_offset + 0] = blue;
                    d->buf[buf_offset + 1] = green;
                    d->buf[buf_offset + 2] = red;
                }
            }

            x++;
            if (x >= width) {
                x = 0;
                y++;
            }
        }
    }
    if (x != 0 && y != height) {
        fprintf(stderr, "%s: image %s seems corrupted\n", content->images[img],
                name);
    }

    /* Wait for the child */
    int child_status;
    if (waitpid(child, &child_status, 0) == child) {
        if (!(WIFEXITED(child_status) &&
              WEXITSTATUS(child_status) == EXIT_SUCCESS)) {
            fprintf(stderr, "%s: helper failed\n", name);
        }
    } else {
        fprintf(stderr, "%s: waitpid(): %s\n", name, strerror(errno));
    }

    return true;
}


void render(char* name, display_t *d, content_t *c) {
    /* Render the visible screen content */

    // FIXME: Don't redraw the entire window.

    nsfb_bbox_t display_box = {0, 0, d->width, d->height};
    if (nsfb_claim(d->nsfb, &display_box) != 0) {
        fprintf(stderr, "%s: failed to claim window region\n", name);
        return;
    }

    nsfb_plot_clg(d->nsfb, BACKGROUND_COLOUR);

    /* Display the images */
    int start_height = 0;
    for (int i = 0; i < c->image_count; i++) {
        if (start_height + (c->heights[i] / d->scale_factor) >= d->offset_y &&
                start_height - d->height < d->offset_y) {
            if (!render_image(name, d, c, i, start_height - d->offset_y)) {
                /* If we can't render, then fallback and just fill with the
                 * error colour.
                 */
                if (c->heights[i] == 0) {
                    /* Use a placeholder height */
                    c->heights[i] = FALLBACK_HEIGHT;
                }
                nsfb_bbox_t rect = {
                    0, start_height - d->offset_y,
                    d->width, start_height - d->offset_y + 
                              (c->heights[i] / d->scale_factor),
                };
                if (!nsfb_plot_rectangle_fill(d->nsfb, &rect, ERROR_COLOUR)) {
                    fprintf(stderr, "%s: fallback plot failed\n", name);
                }
            }
        }
        start_height += c->heights[i] / d->scale_factor;
    }

    /* If our offset is too large, clamp it and re-render; we shouldn't
     * allow the user to go beyond the end of the images anyway, and will
     * have tried to render all the images at this point so we know the actual
     * total height.
     *
     * TODO: What is the effect of this on performance??
     */
    int max_offset_y = start_height - d->height;
    if (max_offset_y < 0) max_offset_y = 0;
    if (d->offset_y > max_offset_y) {
        d->offset_y = max_offset_y;
        render(name, d, c);
    }
    /* We don't know the maximum width, but we do know the maximum width found
     * so far, which includes the current width. We can use this to clamp to
     * visible width.
     */
    int max_offset_x = (c->max_width / d->scale_factor) - d->width;
    if (max_offset_x < 0) max_offset_x = 0;
    if (d->offset_x > max_offset_x) {
        d->offset_x = max_offset_x;
        render(name, d, c);
    }

    if (nsfb_update(d->nsfb, &display_box) != 0) {
        fprintf(stderr, "%s: failed to update window\n", name);
    }
}


int main(int argc, char** argv) {
    char* name = __FILE__;
    if (argc > 0) name = argv[0];
    if (argc < 2) {
        fprintf(stderr, "usage: %s <ids> ...\n", name);
        exit(EINVAL);
    }

    content_t content;
    initialise_content(name, &content, argc - 1, &(argv[1]));

    display_t d;
    initialise_display(name, &d);

    render(name, &d, &content);

    /* Handle events */
    while (1) {
        nsfb_event_t event;
        if (nsfb_event(d.nsfb, &event, -1)) {

            if (event.type == NSFB_EVENT_CONTROL) {
                if (event.value.controlcode == NSFB_CONTROL_QUIT) exit(0);
            } else if (event.type == NSFB_EVENT_KEY_DOWN) {
                enum nsfb_key_code_e code = event.value.keycode;
                if (code == NSFB_KEY_q) exit(0);

                if (code == NSFB_KEY_PAGEDOWN) {
                    d.offset_y += d.height * PAGE_MULT;
                    render(name, &d, &content);
                }
                if (code == NSFB_KEY_PAGEUP) {
                    d.offset_y -= d.height * PAGE_MULT;
                    if (d.offset_y < 0) d.offset_y = 0;
                    render(name, &d, &content);
                }

                if (code == NSFB_KEY_DOWN) {
                    d.offset_y += d.height * ARROW_MULT;
                    render(name, &d, &content);
                }
                if (code == NSFB_KEY_UP) {
                    d.offset_y -= d.height * ARROW_MULT;
                    if (d.offset_y < 0) d.offset_y = 0;
                    render(name, &d, &content);
                }

                if (code == NSFB_KEY_RIGHT) {
                    d.offset_x += d.width * ARROW_MULT;
                    render(name, &d, &content);
                }
                if (code == NSFB_KEY_LEFT) {
                    d.offset_x -= d.width * ARROW_MULT;
                    if (d.offset_x < 0) d.offset_x = 0;
                    render(name, &d, &content);
                }

                if (code == NSFB_KEY_HOME) {
                    d.offset_y = 0;
                    render(name, &d, &content);
                }

                if (code == NSFB_KEY_EQUALS || code == NSFB_KEY_KP_PLUS) {
                    d.scale_factor *= SCALE_FACTOR;
                    if (d.scale_factor <= 1) d.scale_factor = 1;
                    render(name, &d, &content);
                }
                if (code == NSFB_KEY_MINUS) {
                    d.scale_factor /= SCALE_FACTOR;
                    render(name, &d, &content);
                }
                if (code == NSFB_KEY_f) {
                    d.scale_factor = (float)content.max_width / (float)d.width;
                    if (d.scale_factor <= 1) d.scale_factor = 1;
                    render(name, &d, &content);
                }
            } else if (event.type == NSFB_EVENT_RESIZE) {
                resize_display(name, &d,
                        event.value.resize.w, event.value.resize.h);
                render(name, &d, &content);
            }
        }
    }
}
