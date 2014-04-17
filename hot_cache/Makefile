export MAKEROOT := $(shell pwd)
TARGET_DIRS := snooping_client util youku sohu
include ./env.mk

.PHONY : all clean install uninstall

all :
	@if ! [ -d $(MAKEROOT)/objs ]; then \
        echo "$(MAKEROOT)/objs is not exist, create it"; \
        mkdir $(MAKEROOT)/objs; \
    fi; \
	for entry in $(TARGET_DIRS);\
	do\
		if test -d $(MAKEROOT)/$$entry; then\
		cd $$entry;\
		make -w;\
		cd $(MAKEROOT);\
		fi;\
	done
	gcc -o SnoopingClient ./objs/*.o -pthread

clean :
	-rm $(MAKEROOT)/objs/*.o $(MAKEROOT)/SnoopingClient
	rmdir $(MAKEROOT)/objs

install:
	cp $(MAKEROOT)/SnoopingClient $(MAKEROOT)/../app/SnoopingClient

uninstall:
	-rm $(MAKEROOT)/../app/SnoopingClient
