CC = gcc
CFLAGS = -Wall -Wextra `pkg-config --cflags libxml-2.0 sqlite3`
LDFLAGS = `pkg-config --libs libxml-2.0 sqlite3`
SRC = src/main.c src/xmlbuilder.c
OUT = xmlbuilder

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

test: $(OUT)
	bash tests/run_tests.sh

clean:
	rm -f $(OUT) output.sql *.db
	rm -rf tests/tmp

.PHONY: clean test
