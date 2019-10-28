#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}
#define EDITOR_VERSION "0.0.1"

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

struct editorConfig {

	struct termios orig_termios;
	int screenrows;
	int screencols;
	int cx,cy;

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
// Draw tildes in the buffer and not actual file 
void editorDrawRows(struct abuf *ab){

	int y;
	// Terminal size - Unsure temorarily set to 24 rows
	for(y = 0; y < E.screenrows; y++){

		if(y == E.screenrows / 3){

			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Editor -- version %s", EDITOR_VERSION);

			// Text out of bounds handler
			if(welcomelen > E.screencols)
				welcomelen = E.screencols;

			// Add padding to get text to center of screen
			int padding = (E.screencols - welcomelen) / 2;
			if(padding){

				abAppend(ab, "~",1);
				padding--;
			}
			while (padding--)
				abAppend(ab, " ",1);

			abAppend(ab, welcome, welcomelen);
		}
		else{

			abAppend(ab, "~",1); // Vim style tilde columns 
		}

		abAppend(ab, "\x1b[K",3); // Erases current part of current line

		// Handle last line
		if(y < E.screenrows - 1){
			abAppend(ab,"\r\n",2);
		}
	}

}
/*
	Write 4 bytes 
	\x1b = Escape character <ESC>
	[ = Begining of escape sequence
	J = Erase in display
	2 = Argument of J command (Clear entire screen)
*/
void  editorRefreshScreen(){


	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l",6); // To hide cursor while redrawing
	abAppend(&ab,"\x1b[H",3); // Reposition cursor to top-left corner

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1); // Specify exact position of cursor 
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h",6); // h -> set mode 

	// Finally write the buffer to STDOUT
	write(STDOUT_FILENO, ab.b, ab.len);

	abFree(&ab);

}

void editorMoveCursor(int key){

	switch(key){

		case ARROW_LEFT:
			// Handle cursor from going out of screen
			if(E.cx != 0)
				E.cx--;
			break;

		case ARROW_RIGHT:
			if(E.cx != E.screencols - 1)
				E.cx++;
			break;

		case ARROW_UP:
			if(E.cy != 0)
				E.cy--;
			break;

		case ARROW_DOWN:
			if(E.cy != E.screenrows - 1)
				E.cy++;
			break;

	}

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
			E.cx = E.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			/* Move cursor to top/bottom of screen 
			   Code block to create time variable in switch statement
			*/
			{
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

	if(getWindowSize(&E.screenrows, &E.screencols) == -1)
		die("getWindowSize");
}

// Init code 

int main(){

	enableRawMode();
	initEditor();

	while (1){
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}