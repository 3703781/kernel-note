
define =
lib =
lib_dir =
c_flag = -Og -std=gnu11 -Wall -g -pthread
header = # autoconf.h  # $(abspath $(wildcard ./*.h))
header_dir = ./
target = fifo_test

defines = $(addprefix -D, $(define))
libs = $(addprefix -l, $(lib))
lib_dirs = $(addprefix -L, $(lib_dir))
c_sources = $(wildcard ./*.c)
incluces = $(addprefix -I, $(header_dir))
headers = $(addprefix -include , $(header))

c_flags += $(c_flag) $(defines) $(incluces) $(headers) $(lib_dirs) $(libs)

all: $(target)

$(target): $(c_sources) Makefile
	gcc $(c_flags) $(c_sources) -o $@

.phony: clean

clean:
	rm -rf $(target)

