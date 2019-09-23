src = ./src/mesh_viewer.c
obj = mesh-viewer
CC = cc

full :
	$(CC) -O2 $(src) -lncurses -DNCURSES -DBENCHMARK -o $(obj)
basic :
	$(CC) -O2 $(src) -o $(obj)
time :
	$(CC) -O2 $(src) -o $(obj) -DBENCHMARK
ncurses :
	$(CC) -O2 $(src) -lncurses -DNCURSES -o $(obj)

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

.PHONY : clean
clean : 
	rm $(obj)

.PHONY: install
install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(obj) $(DESTDIR)$(PREFIX)/bin/$(obj)
	chmod a+x $(DESTDIR)$(PREFIX)/bin/$(obj)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(obj)
