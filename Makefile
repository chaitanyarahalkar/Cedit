all: editor

editor: editor.c
	$(CC) -o cedit editor.c -Wall -W -pedantic -std=c99
clean:
	rm cedit
