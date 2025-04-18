all:
    apxs -c mod_mermaid.c

install:
    apxs -i mod_mermaid.la

clean:
    rm -rf *.o *.lo *.la *.so *.slo .libs/
