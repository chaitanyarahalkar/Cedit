// Compiler issue fixed 
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>


#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}
#define EDITOR_VERSION "0.0.1"
#define EDITOR_TAB_STOP 8

enum editorKey {

  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY, 
  PAGE_UP,
  PAGE_DOWN

};

// To store text from editor
typedef struct erow {

	int size;
	int rsize;
	char *chars;
	char *render;

}erow;


struct editorConfig {

	struct termios orig_termios;
	int screenrows;
	int screencols;
	int numrows;
	int cx,cy;
	int rx; // Horizontal coordinate 
	int rowoff; // For vertical screen scrolling 
	int coloff; // For horizontal scrolling
	erow *row;
	char *filename;
	char statusmessage[80];
	time_t statusmsg_time;
};

struct editorConfig E;
// Error handler
void die(const char *s){

	write(STDOUT_FILENO,"\x1b[2J",4); // Clear screen on exit
	write(STDOUT_FILENO, "\x1b[H",3); // Reposition on exit
	
	perror(s);
	exit(1);
}

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
  die("tcsetattr");
}

void enableRawMode() {

  if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) // Termos attribute setter 
  	die("tcgetattr");

  atexit(disableRawMode); // Disable raw mode at exit 

  struct termios raw = E.orig_termios;
  /*
  	 - Disable echo and canonical mode
  	 - Disable SIGINT SIGTSTP signals
  	 - Disable Ctrl-V
  */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |ISIG); 
  raw.c_lflag &= ~(ICRNL | IXON); // Disable Ctrl-S, Ctrl-Q & Ctrl-M
  raw.c_lflag &= ~(OPOST); // Turn off output processing
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Miscellaneous flags
  raw.c_cflag |= (CS8);
  raw.c_cc[VMIN] = 0; // Minimum bytes of input needed before read() can return 
  raw.c_cc[VTIME] = 1; // Maximum amount of time to wait before read() returns  
 


  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
  	die("tcsetattr");
}

// Manage low level terminal input 
int editorReadKey(){
	int nread;
	char c;

	while((nread = read(STDIN_FILENO, &c,1)) != 1){
		if(nread == -1 && errno == EAGAIN)
			die("read");
	}

	if(c == '\x1b'){

		char seq[3];

		if(read(STDIN_FILENO, &seq[0],1) != 1)
			return '\x1b';

		if(read(STDIN_FILENO, &seq[1],1) != 1)
			return '\x1b';

		// To move cursor with arrow keys
		if(seq[0] == '['){

			if(seq[1] >= '0' && seq[1] <= '9'){
				if(read(STDIN_FILENO,&seq[2],1) != 1)
					return '\x1b';

				if(seq[2] == '~'){

					switch(seq[1]){
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			else{

				switch(seq[1]){

					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;

				}
			
			}

		}
		else if(seq[0] == 'O'){

			switch(seq[1]){

				case 'H': return HOME_KEY;
				case 'F': return END_KEY; 
			}
		}
		return '\x1b';
	}
	else{

		return c;
	}
}

int getCursorPosition(int *rows,int *cols){

	char buf[32];
	unsigned int i = 0; 



	if(write(STDOUT_FILENO, "\x1b[6n",4) != 4)
		return -1;

	while(i < sizeof(buf) - 1){
		if(read(STDIN_FILENO, &buf[i],1) != 1)
			break;

		if(buf[i] == 'R')
			break;

		i++;	

	}
	buf[i] = '\0';
	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  	return 0;
}

// Utility function to get size of window
int getWindowSize(int *rows, int *cols){
	struct winsize ws;

	// ioctl syscall 
	if(ioctl(STDOUT_FILENO,TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){

		// Escape sequence to send cursor move to right and then down (999 px)
		if(write(STDOUT_FILENO, "\x1b[999C\xb[999B",12) != 12)
			return -1;

		return getCursorPosition(rows,cols);
	}
	else{

		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

void editorUpdateRow(erow *row){

	int tabs = 0;
	int j;

	for(j = 0; j < row->size; j++)
		if(row->chars[j] == '\t')
			tabs++;

	free(row->render);
	row->render = malloc(row->size + tabs * (EDITOR_TAB_STOP - 1) + 1);

	int idx = 0;

	for(j = 0; j < row->size; j++){

		if(row->chars[j] == '\t'){

			row->render[idx++] = ' ';
			while(idx % EDITOR_TAB_STOP != 0)
				row->render[idx++] = ' ';
		}
		else
			row->render[idx++] = row->chars[j];
	}

	row->render[idx] = '\0';
	row->rsize = idx;

}



void editorAppendRow(char *s, size_t len){


	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	
	E.row[at].chars[len] = '\0';
	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	editorUpdateRow(&E.row[at]);

	E.numrows++;
}

void editorOpen(char *filename){

	free(E.filename);
	E.filename = strdup(filename);

	FILE *fp = fopen(filename, "r");
	if(!fp) 
		die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	// File reader read multiple lines

	while((linelen = getline(&line, &linecap, fp)) != -1){

			// Strip carriage return and newline characters
			while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
				linelen--;

			editorAppendRow(line, linelen);
	}

	free(line);
	fclose(fp);

}
// Append buffer to limit write() syscalls
struct abuf{

	char *b;
	int len;

};

void abAppend(struct abuf *ab, const char *s,int len){

	char *new = realloc(ab->b, ab->len + len);

	if(new == NULL)
		return;

	memcpy(&new[ab->len],s,len);

	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab){

	// Free allocated buffer
	free(ab->b);

}

void editorScroll(){

	E.rx = 0;
	if(E.cy < E.numrows)
		E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

	if(E.cy < E.rowoff)
		E.rowoff = E.cy;

	if(E.cy >= E.rowoff + E.screenrows){
		E.rowoff = E.cy - E.screenrows + 1;
	}

	if(E.rx < E.coloff){

		E.coloff = E.rx;
	}

	if(E.rx >= E.coloff + E.screencols){

		E.coloff = E.rx - E.screencols + 1;
	}
}
// Status bar drawing utility -> Inverted colors 
void editorDrawStatusBar(struct abuf *ab){

	abAppend(ab, "\x1b[7m",4);

	char status[80],rstatus[80];
	int len = sprintf(status, sizeof(status), "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.numrows);

	int rlen = sprintf(rstatus, sizeof(rstatus), "%d%d",E.cy + 1, E.numrows);

	if(len > E.screencols)
		len = E.screencols;

	abAppend(ab, status, len);

	while(len < E.screencols){

		if(E.screencols - len == rlen){

			abAppend(ab,rstatus, rlen);
			break;
		}
		else{
			abAppend(ab," ",1);
			len++;
		}
	
	}

	abAppend(ab, "\x1b[m",3);
	abAppend(ab, "\r\n",2);
}

void editorDrawMessageBar(struct abuf *ab){

	abAppend(ab,"\x1b[K",3);

	int msglen = strlen(E.statusmsg);

	if(msglen > E.screencols)
		msglen = E.screencols;

	// 5 second message flash
	if(msglen && time(NULL) - E.statusmsg_time < 5)
		abAppend(ab, E.statusmsg, msglen);

}
// Draw tildes in the buffer and not actual file 
void editorDrawRows(struct abuf *ab) {
  int y;

  for (y = 0; y < E.screenrows; y++) {

  	int filerow = y + E.rowoff;

    if (filerow >= E.numrows) {

      // Print welcome message to center of terminal 
      if (E.numrows == 0 && y == E.screenrows / 3) {

        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Editor -- version %s", EDITOR_VERSION);

        if (welcomelen > E.screencols) welcomelen = E.screencols;

        int padding = (E.screencols - welcomelen) / 2;

        if (padding) {

          abAppend(ab, "~", 1);
          padding--;

        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } 
      else {

        abAppend(ab, "~", 1);

      }

    } 
    else {

      // Wrapping lines
      int len = E.row[filerow].rsize - E.coloff;
      if(len < 0)
      	len = 0;

      if (len > E.screencols) 
      	len = E.screencols;

      abAppend(ab, &E.row[filerow].render[E.coloff], len);

    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);

  }

}

int editorRowCxToRx(erow *row, int cx){

	int rx = 0;
	int j;

	for(j = 0; j < cx; j++){

		if(row->chars[j] == '\t')
			rx += (EDITOR_TAB_STOP - 1) - (rx % EDITOR_TAB_STOP);

		rx++;
	}

	return rx;
}

/*
	Write 4 bytes 
	\x1b = Escape character <ESC>
	[ = Begining of escape sequence
	J = Erase in display
	2 = Argument of J command (Clear entire screen)
*/
void  editorRefreshScreen(){

	editorScroll();

	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l",6); // To hide cursor while redrawing
	abAppend(&ab,"\x1b[H",3); // Reposition cursor to top-left corner

	editorDrawRows(&ab);
	editorDrawStatusBar(&ab);
	editorDrawMessageBar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1); // Specify exact position of cursor 
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h",6); // h -> set mode 

	// Finally write the buffer to STDOUT
	write(STDOUT_FILENO, ab.b, ab.len);

	abFree(&ab);

}

void editorSetStatusMessage(const char *fmt, ...){

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
	va_end(ap);

	E.statusmsg_time = time(NULL);

}

void editorMoveCursor(int key){

	// Scroll to right
	erow * row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch(key){

		case ARROW_LEFT:
			// Handle cursor from going out of screen
			if(E.cx != 0)
				E.cx--;
			else if(E.cy > 0){
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;

		case ARROW_RIGHT:
			if(row && E.cx < row->size)
				E.cx++;

			else if(row && E.cx == row->size){
				E.cy++;
				E.cx = 0;
			}
			break;

		case ARROW_UP:
			if(E.cy != 0)
				E.cy--;
			break;

		case ARROW_DOWN:
			if(E.cy  < E.numrows)
				E.cy++;
			break;

	}

	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? row->size : 0;

	if(E.cx > rowlen)
		E.cx = rowlen;

}
void editorProcessKeypress(){

	int c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO,"\x1b[2J",4);
			write(STDOUT_FILENO,"\x1b[H",3); 
			exit(0); 
			break;

		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
			if(E.cy < E.numrows)
				E.cx = E.row[E.cy].size;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			/* Move cursor to top/bottom of screen 
			   Code block to create time variable in switch statement
			*/
			{
				if(c == PAGE_UP)
					E.cy = E.rowoff;

				else if(c == PAGE_DOWN){

					E.cy = E.rowoff + E.screenrows - 1;
					if(E.cy > E.numrows)
						E.cy = E.numrows;

				}

				int times = E.screenrows;
				while(times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}

}

void initEditor(){

	// Base position of cursor
	E.cx = 0;
	E.cy = 0;
	E.rx = 0;
	E.numrows = 0;
	E.row = NULL;
	E.rowoff = 0;
	E.coloff = 0;
	E.filename = NULL;
	E.statusmsg_time = 0;
	E.statusmsg[0] = '\0';

	if(getWindowSize(&E.screenrows, &E.screencols) == -1)
		die("getWindowSize");

	E.screenrows -= 2;
}

// Init code 

int main(int argc, char *argv[]){

	enableRawMode();
	initEditor();

	// Open file if provided
	if(argc >= 2){

		editorOpen(argv[1]);
	}

	editorSetStatusMessage("HELP: Ctrl-Q = Quit");

	while (1){
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}