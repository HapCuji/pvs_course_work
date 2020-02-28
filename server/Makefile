CC = gcc
STD = -std=gnu99

TFLAGS = -Wall -Werror -O2 -g3
RFLAGS = -Wall -Werror -O2
DFLAGs = -Wall -Werror -O2 -g3
PF = -lrt -lpthread -lresolv
TEST_PF = -lrt -lpthread -lresolv # -lcunit
#common ??
FILES = mta_thread.c mta_process.c mta_communication.c mta_func_sock.c
EXE = mta_server
TEST_FILES = test_metod_mta.c
TEST_EXE = test_mta
RELEASE_FILE = mta_server.c 

all: clean release test_s # debug ? 

release: 
	$(CC) $(STD) $(RFLAGS) $(FILES) $(RELEASE_FILE) -o $(EXE) $(PF)

debug:
	$(CC) $(STD) $(DFLAGS) $(FILES) $(RELEASE_FILE) -o $(EXE) $(PF)

test_s:
	$(CC) $(STD) $(RFLAGS) $(FILES) $(TEST_FILES) -o $(TEST_EXE) $(TEST_PF) 

clean:
	rm -rf *.o $(EXE) $(TEST_EXE) 

## use lost mem: valgrind ./mta_server --leak-check=full
# --track-origins=yes  : for uninit
## valgrind --vgdb=yes --vgdb-error=1 ....
# and then : target remote | vgdb --pid=16153

# monitor help below:
# monitor leak_check
# monitor who_points_at (address) (len)
# monitor get_vbits (address) (len)
