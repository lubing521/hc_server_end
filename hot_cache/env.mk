CC := gcc

#头文件目录
INCLUDE_DIR := -I$(MAKEROOT)/include

CFLAGS := $(INCLUDE_DIR) -Wall

#对所有的.o文件以.c文件创建它
%.o : %.c
	${CC} ${CFLAGS} -c $< -o $(MAKEROOT)/objs/$@
