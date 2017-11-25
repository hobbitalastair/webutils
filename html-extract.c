/* html-extract.c
 *
 * List all of the links in the HTML file fed into stdin.
 *
 * Author:  Alastair Hughes
 * Contact: hobbitalastair at yandex dot com
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <hubbub/hubbub.h>
#include <hubbub/parser.h>

#define BUF_SIZE 4096

char* name = __FILE__;

bool string_equal(hubbub_string h, char* s) {
    /* Return true if the given two strings are equal */
    return (strlen(s) == h.len) && strncmp((char*)h.ptr, s, h.len) == 0;
}

void print_link(hubbub_string h) {
    /* Print the hubbub_string to stdout, followed by a newline, with any
     * whitespace removed.
    */
    for (size_t i = 0; i < h.len; i++) {
        char c = *(h.ptr + i);
        if (!isspace(c)) putchar(c);
    }
    putchar('\n');
}

hubbub_error process_token(const hubbub_token *token, void *data) {
    /* Process a single HTML token */
    if (token->type == HUBBUB_TOKEN_START_TAG) {
        hubbub_tag tag = token->data.tag;
        for (size_t i = 0; i < tag.n_attributes; i++) {
            if (string_equal(tag.attributes[i].name, "href")) {
                print_link(tag.attributes[i].value);
            }
        }
    }
    return HUBBUB_OK;
}

int main(int argc, char** argv) {
    if (argc > 0) name = argv[0];
    if (argc != 1) {
        printf("usage: %s\n", name);
        return EXIT_FAILURE;
    }

    hubbub_parser *parser;
    if (hubbub_parser_create("UTF-8", false, &parser) != HUBBUB_OK) {
        fprintf(stderr, "%s: failed to create parser\n", name);
        return EXIT_FAILURE;
    }
    hubbub_parser_optparams params;
    params.token_handler.handler = process_token;
    if (hubbub_parser_setopt(parser, HUBBUB_PARSER_TOKEN_HANDLER, &params)
            != HUBBUB_OK) {
        fprintf(stderr, "%s: failed to set token handler\n", name);
        return EXIT_FAILURE;
    }

    unsigned char buf[BUF_SIZE];
    size_t count = read(0, &buf, BUF_SIZE);
    while (count > 0) {
        if (hubbub_parser_parse_chunk(parser, buf, count) != HUBBUB_OK) {
            fprintf(stderr, "%s: failed to parse chunk\n", name);
            hubbub_parser_destroy(parser);
            return EXIT_FAILURE;
        }

        count = read(0, &buf, BUF_SIZE);
    }
    if (count < 0) {
        fprintf(stderr, "%s: read(): %s\n", name, strerror(errno));
        hubbub_parser_destroy(parser);
        return EXIT_FAILURE;
    }

    hubbub_parser_destroy(parser);
}

