#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_log.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_file_info.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MERMAID_CLI "/usr/local/bin/mmdc"
#define TEMP_OUTPUT "/tmp/mermaid_output.svg"

static int mermaid_handler(request_rec *r) {
    if (!r->handler || strcmp(r->handler, "mermaid") != 0) {
        return DECLINED;
    }

    apr_finfo_t finfo;
    if (apr_stat(&finfo, r->filename, APR_FINFO_SIZE, r->pool) != APR_SUCCESS) {
        return HTTP_NOT_FOUND;
    }

    const char *ext = strrchr(r->filename, '.');
    if (!ext || strcmp(ext, ".mmd") != 0) {
        return DECLINED;
    }

    char *cmd = apr_pstrcat(r->pool, MERMAID_CLI, " -i ", r->filename, " -o ", TEMP_OUTPUT, " --quiet", NULL);
    int ret = system(cmd);
    if (ret != 0) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Fehler beim Ausführen von mmdc: %s", cmd);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    apr_file_t *svg_file;
    apr_status_t rv = apr_file_open(&svg_file, TEMP_OUTPUT, APR_READ | APR_BINARY, APR_OS_DEFAULT, r->pool);
    if (rv != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, "Konnte temporäre SVG-Datei nicht öffnen: %s", TEMP_OUTPUT);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    apr_off_t svg_size;
    apr_file_info_get(&finfo, APR_FINFO_SIZE, svg_file);
    svg_size = finfo.size;

    char *svg_content = apr_palloc(r->pool, svg_size + 1);
    apr_size_t bytes_read = svg_size;
    rv = apr_file_read(svg_file, svg_content, &bytes_read);
    apr_file_close(svg_file);

    if (rv != APR_SUCCESS || bytes_read != svg_size) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, "Fehler beim Lesen der SVG-Datei: %s", TEMP_OUTPUT);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    svg_content[bytes_read] = '\0';

    unlink(TEMP_OUTPUT);

    // HTML-Vorlage mit eingebettetem SVG
    const char *html_template = "<!DOCTYPE html>\n"
                                "<html lang=\"en\">\n"
                                "<head>\n"
                                "<meta charset=\"UTF-8\">\n"
                                "<title>Mermaid Diagramm</title>\n"
                                "</head>\n"
                                "<body>\n"
                                "<h1>Diagramm</h1>\n"
                                "%s\n"
                                "</body>\n"
                                "</html>\n";
    char *html_content = apr_psprintf(r->pool, html_template, svg_content);

    // HTTP-Header für HTML
    ap_set_content_type(r, "text/html");

    // HTML-Inhalt senden
    ap_rwrite(html_content, strlen(html_content), r);

    return OK;
}

static void register_hooks(apr_pool_t *pool) {
    ap_hook_handler(mermaid_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA mermaid_module = {
    STANDARD20_MODULE_STUFF,
    NULL, // Per-directory config
    NULL, // Per-server config
    NULL, // Merge dir config
    NULL, // Merge server config
    NULL, // Command table (keine Direktiven)
    register_hooks // Hook registration
};
