#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#define CTRL_KEY(k) ((k) & 0x1f)


struct termios orig_termios;

// Error handler
void die(const char *s){

	write(STDOUT_FILENO,"\x1b[2J",4); // Clear screen on exit
	write(STDOUT_FILENO, "\x1b[H",3); // Reposition on exit
	
	perror(s);
	exit(1);
}

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  die("tcsetattr");
}

void enableRawMode() {

  if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) // Termos attribute setter 
  	die("tcgetattr");

  atexit(disableRawMode); // Disable raw mode at exit 

  struct termios raw = orig_termios;
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
// Draw tildes in the buffer and not actual file 
void editorDrawRows(){

	int y;
	for(y = 0; y < 24; y++){
		write(STDOUT_FILENO, "~\r\n",3); // Vim style tilde columns 
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


	write(STDOUT_FILENO,"\x1b[2J",4); 
	write(STDOUT_FILENO,"\x1b[H",3); // Reposition cursor to top-left corner

	editorDrawRows();

	write(STDOUT_FILENO,"\x1b[H",3);
}
void editorProcessKeypress(){

	char c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'): 
			exit(0); 
			break;
	}

}


// Init code 

int main(){

	enableRawMode();

	while (1){
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}