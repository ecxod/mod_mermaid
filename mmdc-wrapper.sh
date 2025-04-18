#!/bin/bash
export PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium
export PUPPETEER_CACHE_DIR=/var/cache/puppeteer
/usr/local/bin/mmdc "$@"