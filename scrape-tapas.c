/* tapas-scrape.c
 *
 * Experimental tapas.io scraper/downloader.
 *
 * Author:  Alastair Hughes
 * Contact: hobbitalastair at yandex dot com
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <curl/curl.h>
#include <hubbub/hubbub.h>
#include <hubbub/parser.h>

char path[PATH_MAX + 1];

/* Wrapper containing page-specific information */
typedef struct {
    hubbub_parser *parser; // Parser for the main page.
    char* url; // Main page URL.
    unsigned int count; // Images downloaded so far.
    char* path; // Path to download images into.
} Page;

/* Compare a hubbub string and a C string - returns true if equal */
bool string_equal(hubbub_string h, char* c) {
    return (strlen(c) == h.len) && strncmp((char*)h.ptr, c, h.len) == 0;
}

/* Download the image at the given url */
bool download_image(Page* p, hubbub_string url) {
    CURL *curl;
    CURLcode result;
    bool ok = true;
    size_t i;
    FILE* file;
    char* url_string;

    printf("%.*s, %d\n", (int)url.len, url.ptr, p->count);

    /* Open the file to write to */
    snprintf(path, PATH_MAX + 1, "%s/%03d.jpg", p->path, p->count);
    path[PATH_MAX] = '\0';
    file = fopen(path, "w+");
    if (file == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return false;
    }

    /* Create the url buffer */
    url_string = malloc(url.len + 1);
    if (url_string == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        return false;
    }
    for (i = 0; i < url.len; i++) {
        url_string[i] = url.ptr[i];
    }
    url_string[i] = '\0'; // Null terminate the buffer!

    /* Init curl */
    curl = curl_easy_init();
    if (!curl) {
        free(url_string);
        return false;
    }

    /* Download the page */
    curl_easy_setopt(curl, CURLOPT_URL, url_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_REFERER, p->url);
    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "failed to retrieve %.*s: %s\n", (int)url.len, url.ptr,
                curl_easy_strerror(result));
        ok = false;
    }

    /* Clean up */
    curl_easy_cleanup(curl);
    free(url_string);
    p->count++;
    return ok;
}

/* Handle a token encountered when parsing the page.
 *
 * We are looking for img tags with a class of "art-image".
 * The src attribute is the URL for the image.
 */
hubbub_error token_handler(const hubbub_token *token, void *pw) {
    hubbub_tag tag;
    hubbub_string class;
    hubbub_string src;
    size_t i;
    Page* p = (Page*)pw;

    if (token->type == HUBBUB_TOKEN_START_TAG) {
        tag = token->data.tag;
        if (string_equal(tag.name, "img")) {
            class.ptr = NULL;
            src.ptr = NULL;
            for (i = 0; i < tag.n_attributes; i++) {
                if (string_equal(tag.attributes[i].name, "class")) {
                    class = tag.attributes[i].value;
                } else if (string_equal(tag.attributes[i].name, "src")) {
                    src = tag.attributes[i].value;
                }
            }
            if (class.ptr != NULL && src.ptr != NULL &&
                    string_equal(class, "art-image")) {
                download_image(p, src);
            }
        }
    }
    return HUBBUB_OK;
}

/* Read in the page, parsing it as we go.
 *
 * This will ignore failures from the hubbub parser.
 * If that turns out to be bad, return a number not equal to size * nmemb.
 */
size_t write_page(char* ptr, size_t size, size_t nmemb, void *data) {
    Page p = *((Page*)data);

    int res = hubbub_parser_parse_chunk(p.parser, (unsigned char*)ptr, size * nmemb);
    if (res != HUBBUB_OK)
    {
        fprintf(stderr, "Failed to parse page, got %d\n", res);
    }
    return size * nmemb;
}

/* Download the given URL and parse it with libhubbub.
 *
 * Returns true on success, false otherwise.
 */
bool download_page(char* url, char* path) {
    CURL *curl;
    CURLcode result;
    Page page;
    hubbub_parser_optparams params;
    bool ok = true;

    /* Populate the Page struct */
    page.url = url;
    page.path = path;
    page.count = 0;
    if (hubbub_parser_create("UTF-8", false, &(page.parser)) != HUBBUB_OK) {
        return false;
    }
    params.token_handler.handler = token_handler;
    params.token_handler.pw = &page;
    if (hubbub_parser_setopt(page.parser, HUBBUB_PARSER_TOKEN_HANDLER,
                &params) != HUBBUB_OK) {
        return false;
    }

    /* Init curl */
    curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    /* Download the page */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_page);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page);
    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "failed to retrieve %s: %s\n", url,
                curl_easy_strerror(result));
        ok = false;
    }

    /* Clean up */
    hubbub_parser_destroy(page.parser);
    curl_easy_cleanup(curl);
    return ok;
}

void usage(char* name) {
    if (name == NULL) {
        name = "tapas-scraper";
    }
    fprintf(stderr, "usage: %s <url> [<path>]\n", name);
}

int main(int argc, char** argv) {
    char* path = "./";
    char* url = "";

    if (argc == 0) {
        usage(NULL);
        return EXIT_FAILURE;
    } else if (argc == 1 || argc > 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    url = argv[1];
    if (argc == 3) {
        path = argv[2];
    }

    if (download_page(url, path)) {
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}
