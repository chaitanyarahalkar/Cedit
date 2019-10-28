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

struct editorConfig {

	struct termios orig_termios;
	int screenrows;
	int screencols;

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
char editorReadKey(){
	int nread;
	char c;

	while((nread = read(STDIN_FILENO, &c,1)) != 1){
		if(nread == -1 && errno == EAGAIN)
			die("read");
	}
	return c;
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

	abAppend(&ab,"\x1b[H",3);
	abAppend(&ab, "\x1b[?25h",6); // h -> set mode 

	// Finally write the buffer to STDOUT
	write(STDOUT_FILENO, ab.b, ab.len);

	abFree(&ab);

}
void editorProcessKeypress(){

	char c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'): 
			exit(0); 
			break;
	}

}

void initEditor(){

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