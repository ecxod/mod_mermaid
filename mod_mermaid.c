#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_log.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_file_info.h"
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MERMAID_CLI "/usr/local/bin/mmdc"

static int mermaid_handler(request_rec *r) {
    if (!r->handler || strcmp(r->handler, "mermaid") != 0) {
        return DECLINED;
    }

    // Prüfen, ob die Datei existiert
    apr_finfo_t finfo;
    if (apr_stat(&finfo, r->filename, APR_FINFO_SIZE, r->pool) != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Datei nicht gefunden: %s", r->filename);
        return HTTP_NOT_FOUND;
    }

    // Nur .mmd-Dateien verarbeiten
    const char *ext = strrchr(r->filename, '.');
    if (!ext || strcmp(ext, ".mmd") != 0) {
        return DECLINED;
    }

    // Eindeutige temporäre Ausgabedatei erstellen
    char *temp_output = apr_psprintf(r->pool, "/tmp/mermaid_output_%d_%ld.svg", getpid(), (long)time(NULL));

    // mmdc ausführen mit fork/exec
    pid_t pid = fork();
    if (pid == 0) {
        // Kindprozess: mmdc ausführen
        execl(MERMAID_CLI, "mmdc", "-i", r->filename, "-o", temp_output, "--quiet", NULL);
        // Wenn execl fehlschlägt
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Fehler beim Ausführen von mmdc: %s", MERMAID_CLI);
        _exit(1);
    } else if (pid > 0) {
        // Elternprozess: Warten auf Kindprozess
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "mmdc fehlgeschlagen für Datei: %s", r->filename);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
    } else {
        // Fork fehlgeschlagen
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Fork fehlgeschlagen");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // SVG-Datei öffnen
    apr_file_t *svg_file;
    apr_status_t rv = apr_file_open(&svg_file, temp_output, APR_READ | APR_BINARY, APR_OS_DEFAULT, r->pool);
    if (rv != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, "Konnte SVG-Datei nicht öffnen: %s", temp_output);
        apr_file_remove(temp_output, r->pool);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // Größe der SVG-Datei ermitteln
    apr_off_t svg_size;
    apr_file_info_get(&finfo, APR_FINFO_SIZE, svg_file);
    svg_size = finfo.size;

    // SVG-Inhalt lesen
    char *svg_content = apr_palloc(r->pool, svg_size + 1);
    apr_size_t bytes_read = svg_size;
    rv = apr_file_read(svg_file, svg_content, &bytes_read);
    apr_file_close(svg_file);

    // Temporäre Datei löschen
    apr_file_remove(temp_output, r->pool);

    if (rv != APR_SUCCESS || bytes_read != svg_size) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, "Fehler beim Lesen der SVG-Datei: %s", temp_output);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    svg_content[bytes_read] = '\0';

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