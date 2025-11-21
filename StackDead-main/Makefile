CC=g++
CFLAGS=-c -Wall -DCANARY_DEBUG -DHASH_DEBUG -DLOGGER_ALL
LDFLAGS=-D_DEBUG -ggdb3 -std=c++17 -O0 -Wall -Wextra -Weffc++ \
-Waggressive-loop-optimizations -Wc++14-compat -Wmissing-declarations \
-Wcast-align -Wcast-qual -Wchar-subscripts -Wconditionally-supported \
-Wconversion -Wctor-dtor-privacy -Wempty-body -Wfloat-equal \
-Wformat-nonliteral -Wformat-security -Wformat-signedness -Wformat=2 \
-Winline -Wlogical-op -Wnon-virtual-dtor -Wopenmp-simd \
-Woverloaded-virtual -Wpacked -Wpointer-arith -Winit-self \
-Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo \
-Wstrict-null-sentinel -Wstrict-overflow=2 \
-Wsuggest-attribute=noreturn -Wsuggest-final-methods -Wsuggest-final-types \
-Wsuggest-override -Wswitch-default -Wswitch-enum -Wundef \
-Wunreachable-code -Wunused -Wuseless-cast -Wvariadic-macros \
-Wno-literal-suffix -Wno-missing-field-initializers -Wno-narrowing \
-Wno-old-style-cast -Wno-varargs -fstack-protector-strong \
-fno-omit-frame-pointer -Wlarger-than=8192 -Wstack-usage=8192 \
-pie -fPIE -Werror=vla \
-fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow \
-fsanitize=null,bounds,alignment,object-size,return,vptr,leak \
-fsanitize-address-use-after-scope -no-pie

SOURCES=logger.cpp stack.cpp main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=target

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	@$(CC) $(LDFLAGS) $(OBJECTS) -o $@	

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)
