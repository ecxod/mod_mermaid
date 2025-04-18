# mod_mermaid

Apache-Modul zur Verarbeitung von Mermaid-Diagrammen (.mmd-Dateien).

## Voraussetzungen
- Apache2
- Mermaid-CLI (`mmdc`)
- apxs (Apache Extension Tool)



### sudo nano /etc/apache2/envvars
```sh
export PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium
export PUPPETEER_CACHE_DIR=/var/cache/puppeteer
```

### wrapper script 
```
cp mmdc-wrapper.sh /usr/local/bin/mmdc-wrapper.sh
chmod +x /usr/local/bin/mmdc-wrapper.sh
```

## Kompilieren und Installieren
```bash
apxs -c -i mod_mermaid.c
a2enmod mermaid
service apache2 restart
```