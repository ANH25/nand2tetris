.SUFFIXES:
CC = cc
ifeq ($(BUILD_TYPE), optimized)
CFLAGS = -W -O
else
CFLAGS = -Wall -Wextra -g
endif
CPPFLAGS = -D__USE_MINGW_ANSI_STDIO
LIBS = -lshlwapi

COMM_LIB_DIR = ../common

all: hvm
$(COMM_LIB_DIR)/io-utils.o: $(COMM_LIB_DIR)/io-utils.h $(COMM_LIB_DIR)/io-utils.c
	$(CC) $(CFLAGS) -c $(COMM_LIB_DIR)/io-utils.c -o $(COMM_LIB_DIR)/io-utils.o
$(COMM_LIB_DIR)/n2t-common.o: $(COMM_LIB_DIR)/n2t-common.h $(COMM_LIB_DIR)/n2t-common.c
	$(CC) $(CFLAGS) -c $(COMM_LIB_DIR)/n2t-common.c -o $(COMM_LIB_DIR)/n2t-common.o
command-table.h: command-table.gperf vm-defs.h
	gperf command-table.gperf > command-table.h
segment-table.h: segment-table.gperf vm-defs.h
	gperf segment-table.gperf > segment-table.h	
hvm.o: hvm.c command-table.h segment-table.h vm-defs.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c hvm.c -I $(COMM_LIB_DIR)
hvm: $(COMM_LIB_DIR)/io-utils.o $(COMM_LIB_DIR)/n2t-common.o hvm.o
	$(CC) -o hvm $(CFLAGS) $(CPPFLAGS) $(COMM_LIB_DIR)/io-utils.o $(COMM_LIB_DIR)/n2t-common.o hvm.o $(LIBS)

clean:
	rm -f hvm hvm.o command-table.h segment-table.h
