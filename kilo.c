/*** includes ***/   // to include directories

#define _DEFAULT_SOURCE   // included them above becoz the header files we are including use the macros to decide what features to dispose
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"  //  CONSTANT VAR
#define KILO_TAB_STOP 8      // tab stop a constant.
#define KILO_QUIT_TIMES 3  //we will display a warning in the status bar, and require the user to press Ctrl-Q three more times in order to quit without saving.

#define CTRL_KEY(k) ((k) & 0x1f)  // CONSTANT VAR

enum editorKey {	// constant for arrow keys
BACKSPACE = 127,  // ASCI value of backspace is 127
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

enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,// FOR MULTIPLE LINES COMMENTS
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)// for highlighting strings

 /*** data ***/

struct editorSyntax { // file type detecter
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;//flags is a bit field that will contain flags for whether to highlight numbers and whether to highlight strings for that filetype
};

typedef struct erow {       // erow stands for editors row. and stores a line of text
       int idx;
	int size;
	int rsize;  // using for tab for now
	char *chars;   // E.cx is the index of chars field
	char *render;  // using for tab for now    >>>> E.rx is the index of render field >> if no tabs in line E.rx same as E.cx >> nd if tab in line then E.rx will be greater
			// than E.cx by extra spaces taken up by tabs while rendered. 
	 unsigned char *hl;  // hl stands for highlight
	 int hl_open_comment;
}erow;

struct editorConfig{  //>>>>>>>>> global editor state<<<<<<<<<<<<<<<<<<//

int cx, cy;  // to know the position of cursor in x y plane of editor
int rx;   // to interact with tabs properly
int rowoff; //  this variable will keep track of what row of the file we currently are in. 
int coloff;
int screenrows;
int screencols;
int numrows;
erow *row;
int dirty;  // it tells us whether the file has been modified since opening or saving a file. it warns us  that we might lose the unsaved data
char *filename;   // saves a copy of the filename there when a file is opened  
char statusmsg[80];
time_t statusmsg_time;
struct editorSyntax *syntax;
struct termios orig_termios;
};

struct editorConfig E; 

/*** filetypes ***/
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
 // highlighting keywords
char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case", "printf", "scanf",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

struct editorSyntax HLDB[] = {// HLDB stands for highlight data base
  {
    "c",
    C_HL_extensions,
     C_HL_keywords,
     "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
};
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

    /*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(); 
char *editorPrompt(char *prompt, void (*callback)(char *, int) ); //  to save a file name,  // 2nd argument: prompts incremental search. 

/*** terminal ***/

void die(const char *s){
write(STDOUT_FILENO, "\x1b[2J", 4);
write(STDOUT_FILENO, "\x1b[H", 3);
perror(s);
exit(1);
}

void disableRawMode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
	 die("tcsetattr");
}
void enableRawMode()
{	
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);
	
	struct termios raw = E.orig_termios;
	
	raw.c_iflag &= ~(BRKINT | IXON | ICRNL | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag &= ~(CS8);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
	}

int editorReadKey()
{	  int nread;
	  char c;
	   while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
	if ( nread == -1 && errno != EAGAIN) die("read");
	}
if (c == '\x1b') {    // here we are using  our old function which used w a s d  to move cursor but this time to are providing an esc sequence \x1b  followed by [  before them.
		      // so the arrows can do the same if input exceeds 1 byte 
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
	    case '1' : return HOME_KEY;
	    case '2' : return DEL_KEY;
	    case '4' : return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
	    case '7' : return HOME_KEY;
	    case '8' : return END_KEY;
          }
        }
       } else {
         switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
	  case 'H' : return HOME_KEY;
	  case 'F' : return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;

	}
   }	
 return '\x1b';
} else {
	return c;
	}

}
// to get the position of cursor 
// argument 6 gives popsition of cursor

int getCursorPosition(int *rows, int *cols) {
char buf[32];
unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;// cursor position
 
  while (i < sizeof(buf) - 1) {
       if (read(STDIN_FILENO, &buf[i], 1) != 1) break; //
    	if (buf[i] == 'R') break;
	i++;}

buf[i] = '\0';  	// and as strings expect to end with null character we assign '\0'

if (buf[0] != '\x1b' || buf[1] != '[') return -1;   // these  elements gets ignored.
if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;   // passing pointer to the 3rd char of buf[] , skipping \x1b and [ chars.  it take two int values and pass them into row and col pointers.

  return 0;
}

int getWindowSize(int *rows, int *cols){
	struct winsize ws;
 if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    editorReadKey();	// it prints output before quiting program
return getCursorPosition(rows, cols) ;} // it prints escape sequence 
else{
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;}
}

/*** syntax highlighting ***/
int is_separator(int c) {  // function that takes a character and returns true if it’s considered a separator character.
  return isspace(c) || c == '\0' || strchr(",.()+-*/=~%<>[];", c) != NULL; 
}//strchr() comes from <string.h>. It looks for the first occurrence of a character in a string, and returns a pointer to the matching character in the string. 
//If the string doesn’t contain the character, strchr() returns NULL.

void editorUpdateSyntax(erow *row) {   // for getting highlighted digits
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);// unhighlighted charrater will have a value of HL_NORMAL in hl
if (E.syntax == NULL) return;
char **keywords = E.syntax->keywords;

char *scs = E.syntax->singleline_comment_start;
char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;

int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

 int prev_sep = 1;  // becox we consider the begining of the ;ine to be a separator
 int in_string = 0;
 int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
int i = 0;
  while(i < row->rsize) {
    char c = row->render[i];
unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;
    
      if (scs_len && !in_string && !in_comment) {// >> highllighting single line comment
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

  if (mcs_len && mce_len && !in_string) { // for multiline comments
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mce_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

 if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;
 if (c == '\\' && i + 1 < row->rsize) {//We should probably take escaped quotes into account when highlighting strings. If the sequence \' or \" occurs in a string,
                                       //then the escaped quote doesn’t close the string in the vast majority of languages.

//If we’re in a string and the current character is a backslash (\), and there’s at least one more character in that line that comes after the backslash,
// then we highlight the character that comes after the backslash with HL_STRING and consume it. We increment i by 2 to consume both characters at once.
//Colorful single-line comments


          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string) in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

//modified>>	if (isdigit(c)){
	if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
       if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
	(c == '.' && prev_hl == HL_NUMBER)){ //TO HIGHLIGHT DECIMAL NUMBERS
	row->hl[i] = HL_NUMBER;
    
	i++;
	prev_sep = 0;// to indicate we are in the middle of highlighting something
	continue;
  }
 }

  if (prev_sep) {  // to make sure a separator came before the keyword, before looping through each possible keyword
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2) klen--;
        if (!strncmp(&row->render[i], keywords[j], klen) &&
            is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen); //highlighting it with HL_KEYWORD1 or HL_KEYWORD2 depending on the value of kw2
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

	prev_sep = is_separator(c);
	i++;
}

 int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx + 1]);

}

int editorSyntaxToColor(int hl) {  // editorSyntaxToColor() function that maps values in hl to the actual ANSI color codes we want to draw them with.
  switch (hl) {

    case HL_COMMENT:
    case HL_MLCOMMENT: return 36; // cyan color
    case HL_KEYWORD1: return 33; // yellow 
    case HL_KEYWORD2: return 32;// green
    case HL_STRING: return 35;// magenta color
    case HL_NUMBER: return 31;
    case HL_MATCH: return 34;
    default: return 37;
  }
}
// tries to match the current filename to one of the filematch fields in the HLDB. If one matches, it’ll set E.syntax to that filetype.
void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) return;
  char *ext = strrchr(E.filename, '.');//strrchr() returns a poiinter to the last occurence of a character in a string
  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||// strcmp() returns 0 if two given strings are equal
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;

 int filerow;  // we loop through each row in the file , and call editorUpdateSyntax() on it. 
        for (filerow = 0; filerow < E.numrows; filerow++) {// now the highlightng immediately changes when the filetype changes
          editorUpdateSyntax(&E.row[filerow]);
        }


        return;
      }
      i++;
    }
  }
}


/*** row operations ***/
				//  function that converts a chars index into a render index >>> E.cx to E.rx
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {  // loop through all the characters to the left of cx, and figure out how many spaces each tab takes up.
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP); // if it’s a tab we use rx % KILO_TAB_STOP to find out how many columns we are to the right of the last tab stop,
//We add that amount to rx to get just to the left of the next tab stop	 >^// and then subtract that from KILO_TAB_STOP - 1 to find out how many columns we are to the left of the next tab stop.
    rx++;   // this will add  1 to 6 and get us exactly to the next tab stop.
  }
  return rx;
}

// to convert render index to chars before providing that to E.cx in editorFind
int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0; // current rx
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx) return cx;
  }
  return cx;
}

                   // to fill in the contents of render string
void editorUpdateRow(erow *row) { // renders tabs as multiple space characters
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)   // First, we have to loop through the chars of the row and count the tabs in order to know how much memory to allocate for render
  	if(row->chars[j] == '\t') tabs++;

  free(row->render);			//     KILO_TAB_STOP IS DEFINED 8. IN DEFINES SECTION
  row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);// The maximum number of characters needed for each tab is 8. row->size already counts 1 for each tab,
                                            // so we multiply the number of tabs by 7 and add that to row->size to get the maximum amount of memory we’ll need for the rendered row.  
  int idx = 0;

//After allocating the memory, we modify the for loop to check whether the current character is a tab. If it is, we append one space (because each tab must advance the cursor forward at least one column),
// and then append spaces until we get to a tab stop,which is a column that is divisible by 8.
  for (j = 0; j < row->size; j++) {
  if (row->chars[j] == '\t') {
    row->render[idx++] = ' ';
   while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
}else{
    row->render[idx++] = row->chars[j];   // after for loop idx contains the number of characters we copied into row>render, so we assign it to row>rsize
  }
}
  row->render[idx] = '\0';
  row->rsize = idx;
 editorUpdateSyntax(row);
}
                     // to show multiple lines of text
  // modified to add enter key,
  void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
 for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;
E.row[at].idx = at;

//modified
//void editorAppendRow(char *s, size_t len){
//E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); // we need to tell realloc how many bytes to take , so we mult. number of bytes each row take i.e sizeof(erow)* with the number of rows we want

//int at = E.numrows;  // then we take at the index of new row

E.row[at].size = len;
E.row[at].chars = malloc(len + 1);// to allocate space for new erow
memcpy(E.row[at].chars, s, len);// and then copy the given string to a new s=erow at the end of E.row array
E.row[at].chars[len] = '\0';

E.row[at].rsize = 0;
E.row[at].render = NULL;
 E.row[at].hl = NULL; // for highlighting modifying rows
E.row[at].hl_open_comment = 0;
editorUpdateRow(&E.row[at]);

E.numrows++ ;
E.dirty++; // to know  how dirty the file is
}

// we free the memory owned by the row using editorFreeRow()
void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
 free(row->hl);
}

void editorDelRow(int at) { // we are deleting a single element from an array of elements by its index.
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));  // memmove() to overwrite the deleted row struct with the rest of the rows that come after it
 for (int j = at; j < E.numrows - 1; j++) E.row[j].idx--;
  E.numrows--; //row count decreases
  E.dirty++;  // modificaton increases
}

	/* to insert a single characters  */

void editorRowInsertChar(erow *row, int at, int c) {    // at: is the index we want to insert the character into. and is inserted in the end of the string 
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2); //we add 2 because we also have to make room for the null byte
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); // and use memmove() to make room for the new character
  row->size++;  //We increment the size of the chars array, and then actually assign the character to its position in the array.
  row->chars[at] = c;
  editorUpdateRow(row);  // Finally, we call editorUpdateRow() so that the render and rsize fields get updated with the new row content
  E.dirty++;
}

//>>>>>>>>>>
                                         //important//
//Notice that editorInsertChar() doesn’t have to worry about the details of modifying an erow, and editorRowInsertChar() doesn’t have to worry about where the cursor is. 
//That is the difference between functions in the /*** editor operations ***/ section and functions in the /*** row operations ***/ section.
//>>>>>>>>>>

// appends a string to the end of a row.
void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);  //we simply memcpy() the given string to the end of the contents of row->chars
  row->size += len;  //We update row->size, call editorUpdateRow() as usual, and increment E.dirty as usual.
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

// >> for adding backspacing to the program

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);  //We just use memmove() to overwrite the deleted character with the characters that come after it
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations ***/

void editorInsertChar(int c) {  //This section will contain functions that we’ll call from editorProcessKeypress() when we’re mapping keypresses to various text editing operations
  if (E.cy == E.numrows) {  // it will happen if cursor is at tidle . 
    editorInsertRow(E.numrows, "", 0); // so we append a new row before adding a text to the file
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;  // it keeps the cursor ahead of the text entered 
}

// to handle enter key
void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++; //we increment E.cy, and set E.cx to 0 to move the cursor to the beginning of the row.
  E.cx = 0;
}

void editorDelChar() {  // >>uses editorRowDelChar() to delete the character that is to the left of the cursor.
  if (E.cy == E.numrows) return;
 if (E.cx == 0 && E.cy == 0) return;

  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;//> we delete it and move the cursor one to the left.
 } else {  // if E.cx is at the beginning of the line
    E.cx = E.row[E.cy - 1].size;// it gives the size of rows or number of
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);  // joins the row below with the row above
    editorDelRow(E.cy);
    E.cy--;
}
}

/*** file i/o ***/

// to save to disk

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;//  First we add up the lengths of each row of text, adding 1 to each one for the newline character we’ll add to the end of each line
  *buflen = totlen;  // we save the string len into buflen . to tell the calller how long the string is
  char *buf = malloc(totlen);//Then, after allocating the required memory, we loop through the rows, and memcpy() the contents of each row to the end of the buffer,.
  char *p = buf;
    for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';// , appending a newline character after each row.
    p++;
  }
  return buf; // we return buf expected the caller to free the memory
}

void editorOpen(char *filename) { 			 // will eventually be for opening and reading a file from disk.
free(E.filename);   // strdup() makes a copy of the given string, allocating the required memory and assuming you will free() that memory.
E.filename = strdup(filename);
editorSelectSyntaxHighlight();

	FILE *fp = fopen(filename, "r");   // to open an actual line     // fopen: take the file name and opens the file for reading
if(!fp) die("fopen");  // to check if inputed afilename . if yes editoropen is called. eg, kilo filename. and if only ./kilo is called it will start a blank file   

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;			//ssize comes from sys/types
 while((linelen = getline(&line, &linecap, fp)) != -1 ){  //+ The while loop works because getline() returns -1 when it gets to the end of the file and there are no more lines to read.
						/** modified **/ 
// we get the line and linelen with getline() ..  // it is also used to read lines from a file. first we passed a null line * then linecap(Line capacity) of 0
 					// line points to the line and linecap lets u know how much memory it allocated. return value is the lenght of line it read 
    while (linelen > 0 && (line[linelen - 1] == '\n' ||   //  new line  .............
							 //          		    >     to strip of new line and carriage return before returning it into our erow, as erow only stores one line of text
                           line[linelen - 1] == '\r'))  // carriage return..........
      linelen--;
	editorInsertRow(E.numrows, line, linelen);
//modified 
// editorAppendRow(line, linelen);   // so that the line contained by erow should be displayed, which are now changed in append row>> s and len arguments
 }

  free(line);
  fclose(fp);  // it returns if no file is found and terminal is exited
  E.dirty = 0;//Now you should see (modified) appear in the status bar when you first insert a character, and you should see it disappear when you save the file to disk.
}

/*will actually write the string returned by editorRowsToString() to disk.*/

void editorSave() {
 if (E.filename == NULL) {

 E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
editorSelectSyntaxHighlight();
//>>modified<<    E.filename = editorPrompt("Save as: %s");
  }

 // >>modified<< if (E.filename == NULL) return;  // if it is a new file E.filename will be null.

  int len; // to know how much size is needed for the file.

  char *buf = editorRowsToString(&len);//we call editorRowsToString(), and write() the string to the path in E.filename. 

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);  //open, o_rdwr, o_creat comes from fcntl. 

/*We tell open() we want to create a new file if it doesn’t already exist (O_CREAT), and we want to open it for reading and writing (O_RDWR). Because we used the O_CREAT flag,
 we have to pass an extra argument containing the mode (the permissions) the new file should have.0644 is the standard permissions you usually want for text files it will allow ony owner to write
and allows other to read */
									//important

/*ftruncate() sets the file’s size to the specified length. If the file is larger than that, it will cut off any data at the end of the file to make it that length.
 If the file is shorter, it will add 0 bytes at the end to make it that length.
The normal way to overwrite a file is to pass the O_TRUNC flag to open(), which truncates the file completely, making it an empty file, before writing the new data into it.
 By truncating the file ourselves to the same length as the data we are planning to write into it,
 we are making the whole overwriting operation a little bit safer in case the ftruncate() call succeeds but the write() call fails
. In that case, the file would still contain most of the data it had before. But if the file was truncated completely by the open() call and then the write() failed, you’d end up with all of your data lost.*/

	if (fd != -1) {
    if (ftruncate(fd, len) != -1) {   // whether or not error occured we ensure that the file is closed and the memory that buf points to is freed.
      if (write(fd, buf, len) == len) {

  close(fd);
  free(buf);
  E.dirty = 0;//Now you should see (modified) appear in the status bar when you first insert a character, and you should see it disappear when you save the file to disk.
  editorSetStatusMessage("%d bytes written to disk", len);
return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));//strerror() is like perror() (which we use in die()), but it takes the errno value as an argument
                                        //and returns the human-readable string for that error code, so that we can make the error a part of the status message we display to the user.
}

/*** find ***/
void editorFindCallback(char *query, int key) { // key is enter or esc
static int last_match = -1;//1 for searching forward,
  static int direction = 1;// -1 for searching backward.

static int saved_hl_line; // to save the actual color value of search result 
  static char *saved_hl = NULL;
  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
  last_match = -1;
    direction = 1;
    return;
} else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }
if (last_match == -1) direction = 1;
  int current = last_match;//current is the index of the current row we are searching
//>>modified<<< /*void editorFind() {
 // char *query = editorPrompt("Search: %s (ESC to cancel)", NULL);
//  if (query == NULL) return;*/
  int i;
  for (i = 0; i < E.numrows; i++) { // we’ll loop through all the rows of the file, and if a row contains their query string, we’ll move the cursor to the match.
     current += direction;
    if (current == -1) current = E.numrows - 1;
    else if (current == E.numrows) current = 0;
    erow *row = &E.row[current];

    char *match = strstr(row->render, query);  // 
    if (match) {
last_match = current;//When we find a match, we set last_match to current, so that if the user presses the arrow keys, we’ll start the next search from that point.
      E.cy = current;
       E.cx = editorRowRxToCx(row, match - row->render);
      E.cx = match - row->render; // this scrolls off the entire page . resulting the found being at the top line of the page
      E.rowoff = E.numrows;

	saved_hl_line = current; // if there is something to restore, we memcpy() it to the saved line’s hl and then deallocate saved_hl and set it back to NULL.
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);

	memset(&row->hl[match - row->render], HL_MATCH, strlen(query)); //match - row->render is the index into render of the match, so we use that as our index into hl.
      break;
    }
  }
}

void editorFind() {
 int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;
  char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
  if (query) {
  free(query);
 } else { // to restore back the cursor where it was before opening find prompt
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
}
}


/***  append buffer ***/    // buffer : a temporary storage area e.g : when we try to pass more than the required no. of values as input then, the remaining values will automatically hold in the input buffer

struct abuf{
char *b;		// dynamic string that will support appending    
int len;
};

#define ABUF_INIT {NULL, 0}   // we define an abuf_init constant which represents an empty buffer. 

void abAppend(struct abuf *ab, const char *s, int len){  // to append a string s to an abuf structure
  char *new = realloc(ab->b, ab->len +len);   //to give us a block of memory that is the size of the current string plus the size of the string we are appending

if (new == NULL) return;
memcpy(&new[ab->len], s, len);  // coming from string.h >> it copies the string after the end of new  string.
ab->b = new;   					// ***    APPENDS    ***//
ab->len += len;
}

void abFree(struct abuf *ab){ // freeing the current block of memory and allocating new block big enough to store the data.
 free(ab->b);					// ***  DESTRUCTOR  *** //       IT DEALLOCATES THE DYNAMIC MEMORY USED BY AN ABUF.
}

/*** output ***/

void editorScroll() {  // to keep the cursor in visible window. and call it before we refresh the screen
E.rx = 0;   // we finally set E.rx to its proper value
if (E.cy < E.numrows){
E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
}

//>>modified<< E.rx = E.cx;  // >>because scrolling should take into account the characters that are actually rendered(displayed) to the screen, and the rendered(displayed) position of the cursor.
//   we will also change every instance of E.cx with E.rx due to reason mentioned in the above line  >> >> also change it in editorRefreshScreen
                                    /* IMPORTANT */
// horizontal code is exactly parallel to vertical code. 
if (E.cy < E.rowoff){  // this if checks if the cursor is above the visible window.and if so scrolls up to where the cursor is.
E.rowoff = E.cy;
}
if (E.cy >= E.rowoff + E.screenrows){//this if statement checks if the cursor is past the bottom of the visible window, and contains slightly more complicated arithmetic
					// because E.rowoff refers to what’s at the top of the screen and we have to get E.screenrows involved to talk about whats at the bottom of the screen
E.rowoff = E.cy - E.screenrows + 1;
   }
if (E.rx < E.coloff) {    //  this statement will handle horizontal scrolling
E.coloff = E.rx;
}
if (E.rx >= E.coloff + E. screencols) {
E.coloff = E.rx - E.screencols + 1;
   }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
	int filerow = y + E.rowoff;  //  to get the row of the file that we want to display at each y position . we replaced y with E.rowoff, so we define a new varaible filerow. and use that value as index intp e.ro  
    if (filerow >= E.numrows) {		// WRAPPING OUR ROW DRAWING CODE IN IF STATEMENT
      if (E.numrows == 0 && y == E.screenrows / 3) {		// to remove welcome message if argument is passed with the txt editor.. mtlb .. ./kilo filename
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),   //snprint formats and stores a series of values and characters in the array buffer
          "this editor is created by khud bakhtiyar iqbal sofi -- version %s", KILO_VERSION);		// here we are aking int with char and formating and printing them >> giving us welcome message
        if (welcomelen > E.screencols) welcomelen = E.screencols;  
        int padding = (E.screencols - welcomelen) / 2;   //  to center the screen we div screen width by 2 and then sub half of the string length from that i.e  E.screencols/2 - welcomelen/2. 
        if (padding) {					// which tells us how to go from left of the screen to start printing
          abAppend(ab, "~", 1);			// to get the first char ~ in the start 
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);   // after tidle space is filled with spaces...
        abAppend(ab, welcome, welcomelen);   // and also adjust the sixe of the welcome message and editor if terminal size changes 
      } else {
        abAppend(ab, "~", 1);
      }
    } else {			// to draw a row thats part of text buffer, we write chars field of erows
      int len = E.row[filerow].rsize - E.coloff;
	if (len < 0) len = 0 ; // len could now also be negative meaning we would have been able to scroll past the last line. to avoid that we put len = 0
       if (len > E.screencols) len = E.screencols;   //  to cutoff the line if it goes past the end of the terminal screen
 
    char *c = &E.row[filerow].render[E.coloff]; // to get color full digits
       unsigned char *hl = &E.row[filerow].hl[E.coloff];
	 int current_color = -1;
	int j;
      for (j = 0; j < len; j++) {

 if (iscntrl(c[j])) {  // to display non printable characters
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);

 if (current_color != -1) { // to give the controll character a inverted color
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }

        } else if (hl[j] == HL_NORMAL) {

 if (current_color != -1) {
       // if (isdigit(c[j])) {
        //  abAppend(ab, "\x1b[31m", 5);  // this ecs sequence takes red color 
        //  abAppend(ab, &c[j], 1);
          abAppend(ab, "\x1b[39m", 5); // this returns it back to normal
         current_color = -1;
          }
	abAppend(ab, &c[j], 1);
	} else {
	 int color = editorSyntaxToColor(hl[j]);
         if (color != current_color) {
            current_color = color;  
	char buf[16];
          int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
          abAppend(ab, buf, clen);
		}
          abAppend(ab, &c[j], 1);
        }
      } 

    //>>modified abAppend(ab, &E.row[filerow].render[E.coloff], len);  // now the text viewer is displaying the characters in render
    abAppend(ab, "\x1b[39m", 5);
	}
    abAppend(ab, "\x1b[K", 3); // clears only a line at a time , 2 erases the whole line, 1 erases the part of the line to the left of the cursor and 0 erases part of the line to the right of the cursor.
    				// // 0 is the default argument and thats what we want so we only use k
     // if (y < E.screenrows - 1) {  // fixing the bug: it adds tidle to the last line as exception
      abAppend(ab, "\r\n", 2);
    }
  //}   /*modified*/>>> to add a status bar at the end of the screen
}

//>>>>>>>for status bar | to change color and other character modifications|<<<<<<<

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);  // invert color from backgroud i.e ; black txt on white back ground.
 char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,   //>>>> this code will give us filename and if there is no filename it will print no name
    E.dirty ? "(modified)" : "");   // it will show modified in status bar if file is edited
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
    E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);   //the current is stored in E.cy which we added 1 to becox E.cy is 0 indexed

  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);

  while (len < E.screencols) {//After printing the first status string, we want to keep printing spaces until we get to the point where if we printed the second status string,
			// it would end up against the right edge of the screen. That happens when E.screencols - len is equal to the length of the second status string. 
                       //At that point we print the status string and break out of the loop, as the entire status bar has now been printed.
   if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {

    abAppend(ab, " ", 1);
    len++;
  }
}
  abAppend(ab, "\x1b[m", 3); // switching back to normal formating.
  abAppend(ab, "\r\n", 2);   // to make room for second line beneath our status bar where we will display the message .
}

/* to draw the message bar */
void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  abAppend(ab, "\x1b[K", 3);   //First we clear the message bar with the <esc>[K escape sequence
 
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols; // Then we make sure the message will fit the width of the screen, and then display the message .
  if (msglen && time(NULL) - E.statusmsg_time < 15 ) // but only if the message is less than 5 seconds old.
    abAppend(ab, E.statusmsg, msglen);
   abAppend(ab, "\x1b[m", 3);
}

void editorRefreshScreen() {
	editorScroll(); // to call it before screen refreshes

	struct abuf ab = ABUF_INIT;
	abAppend(&ab, "\x1b[?25l", 6);  // l command turns on the various terminal features: here it hides cursor
//	abAppend(&ab, "\x1b[2J", 4);   /cleared whole screen /
	abAppend(&ab, "\x1b[H", 3);
						// changing the occerunce write()  to append   // also passing ab as argument to editordrawrows so they are also appended,
	 editorDrawRows(&ab);
 	 editorDrawStatusBar(&ab);
	 editorDrawMessageBar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);  //** modified **//  
    // to position the cursor on the screen. >> now we will be able to move cursor up as well  // modified again. we can now scroll in all directions. .  :)
  //*here we changed the H cmd with H cmd with arguments providing the value of E.cy and E.cx as they take value: col;row*//
									// we also added 1 to change their index value from 0 to 1 that the terminal uses 
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);  // h command turns off  various terminal features: here it shows cursor after refreshing the screen

	write(STDOUT_FILENO, ab.b, ab.len);   // writing buffer contents
	abFree(&ab);   // free memory used by abuf
}

		/*to print message in status bar*/
void editorSetStatusMessage(const char *fmt, ...) {  // ... makes it variadic function meaning it can take any no. of arguments. we can do that by calling va_start() and va_end on value of type va_list
        // the last argument before the ... in this case is fmt must be passed to va_start, so tht the adress of next argument is known
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);  // vsnprintf() helps us to make our own printf() style function.  ,,>> we store the resulting string in E.statusmsg 
  // we pass fmt and ap to vsnprintf() and it takes care of reading the format string and calling va_arg() to get each argument.
  va_end(ap);
  E.statusmsg_time = time(NULL);// and set statusmsg_time to current time which can be gotten by passing NULL to time()
}

/*** input ***/

// to save as
char *editorPrompt(char *prompt, void (*callback)(char *, int) ) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';  //make sure that buf ends with a \0 character, because both editorSetStatusMessage() and the caller of editorPrompt() will use it to know where the string ends.
  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {  //allow the user to press Backspace (or Ctrl-H, or Delete) in the input prompt.
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      //>>modified<< if (c == '\x1b') {
      editorSetStatusMessage("");
       if (callback) callback(buf, c);
      free(buf);
      return NULL; //When an input prompt is cancelled, we free() the buf ourselves and return NULL.
    } else if (c == '\r') {
  // >>modified<<  if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
if (callback) callback(buf, c);
  }
}

void editorMoveCursor(int key) {
erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];  // now we will be moving our cursor to the valid positions only

  switch (key) {					// to move the cursor using w a s d keys..>>>>>?? WHICH ARE NOW CHANGED TO THE ARROW KEYS
    case ARROW_LEFT:
	if(E.cx !=0){
      E.cx--;

	}else if (E.cy > 0) {    // this will let us go left at the beginning of the line and reach end of the previous line
	E.cy--;
        E.cx = E.row[E.cy].size;
	}
     break;
    case ARROW_RIGHT:
	if(row && E.cx < row->size){   //** modified ** //>> NOW WE WILL ONLY BE ABLE TO GO RIGHT WHEN THERE IS AN ACTUALL LINE AND CURSOR IS ALREADY TO THE LEFT OF THE SCREEN	**//
      E.cx++;
	}else if (row && E.cx == row->size) {
	E.cy++;
	E.cx = 0;
	}
      break;					//WONT HELP NOW>>// maths regarding this.>>>   if x = 0, y = 0;>>  y will be incremented to 1 .bcz y-1 here is = -1 which is not equal to x :)
    case ARROW_UP:
	if(E.cy !=0){
      E.cy--;
	}
      break;
    case ARROW_DOWN:
	if(E.cy < E.numrows){ /*modified*/ // > to advance past the bottom of the screen>> this will allow us to scroll through the entire files
      E.cy++;	}
      break;
  }
	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy]; // it will correct the cursor going directly down bug. and will move to the corresponding position of the cursor above or below
int rowlen = row ? row->size : 0;
if (E.cx > rowlen) {
E.cx = rowlen;
}
}

/* takes care of key combination and esc sequences  */

void editorProcessKeypress() {

 static int quit_times = KILO_QUIT_TIMES; //to keep track of how many more times the user must press Ctrl-Q to quit

 int c = editorReadKey();

   switch (c) {
	case '\r':  // gonna be our enter key
	editorInsertNewline();
	break;

     case CTRL_KEY('q'):

	if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. Forcing quit might cost you your data. "
          "Press Ctrl-Q %d more times to quit.", quit_times);
        quit_times--;  // we decrement the KILO_QUIT_TIMES each time user press ctrl-q
        return;
      }
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x12[H", 3);
	 exit(0);
	break;


	// to save a file 
       case CTRL_KEY('s'):
      editorSave();
      break;

// makes home and  end key move cursor to left and right
	  case HOME_KEY:   //The Home key already moves the cursor to the beginning of the line, since we made E.cx relative to the file instead of relative to the screen.)
      E.cx = 0;
      break;

    case END_KEY:  // end key will now move the cursor to the end of the current line.
      if( E.cy < E.numrows)
	E.cx = E.row[E.cy].size;
      break;

      case CTRL_KEY('f'):
      editorFind();
      break;

	case BACKSPACE:  // 127 is mapped for bs
	case CTRL_KEY('h'): //which is 8 in ASCI .which is was a backspace code back in the day 
	case DEL_KEY:  // delete ki is <esc>[3~ 
	 if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();  // deletes a char to the righ of the cursor  more like moving the cursor to the right then backspacing
	break;

// moves the cursor to the top or to the bottom 
   case PAGE_UP:
    case PAGE_DOWN:
      {				//^ futher more editorMoveCursor takes care of checking and fixing cursor position.
	if (c == PAGE_UP) {	// to scroll up and down an entire pages worth up and down arrow..>> the cursor will either position its self either to the top or to the bottom. 
        E.cy = 	E.rowoff;
	}else if( c == PAGE_DOWN) {
	E.cy = E.rowoff + E.screenrows -1;
	if (E.cy > E.numrows) E.cy = E.numrows;
        }

        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

//   these are now added to the editor process keypress that helps in controlling the editor
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:

      editorMoveCursor(c);
      break;

	case CTRL_KEY('l'):  // Ctrl-L is traditionally used to refresh the screen in terminal programs. In our text editor, the screen refreshes after any keypress, 
				//so we don’t have to do anything else to implement that feature
	case '\x1b': //  it is a esc key . and we already have a key for that we dont the user to enter esc char 27 unwittingly.. so we are ignoring this
	break;

	 default:   //This will allow any keypress that isn’t mapped to another editor function to be inserted directly into the text being edited
      editorInsertChar(c);
      break;
 }
quit_times = KILO_QUIT_TIMES;
}

/*** init ***/

void initEditor() {
E.cx = 0;  //  for horizontal coordinate of the cursor
E.cy = 0;  // for vertical coordinate of the cursor
E.rx = 0;   // to knowhow many  extra spaces those tabs take up when rendered. also equal to E.cx if no tabs in line.
E.rowoff = 0; // we initiallized it to 0 , which means we will be scrolled to the top of the file by default
E.coloff = 0; // same as E.rowoff , but to compare with erow displaying text vertically. it will give us exact orientation 
E.numrows = 0;
E.row = NULL;   // to get multiple lines
E.dirty = 0;
E.filename = NULL;
E.statusmsg[0] = '\0';
E.statusmsg_time = 0;   // contains timestamp when we set a status message
E.syntax = NULL;// this will provide there is no filetype for the current file . and no syntax highlighting should be done

			// for now editor will only display a single line.
if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");  // in case there
// occurs error to measure the size of terminal  . editor will exit
//E.screenrows -= 1;  // we decremented so that the editorDrawRow doesn't try to draw a line of txt at the end of the screen
//modified
E.screenrows -= 2; // We decrement E.screenrows again, and print a newline after the first status bar. We now have a blank final line once again.
}

									/***>>>>>>> INT <<<<<<<***/

int main(int argc , char *argv[]) {
	enableRawMode();
	initEditor();
	 if (argc >= 2) {
    editorOpen(argv[1]);
  }
	editorSetStatusMessage("Help: Ctrl-S =save | Ctrl-Q = quit | Crtl-F = find");

	while (1) {
	editorRefreshScreen();
	editorProcessKeypress();
	}

	return 0;
}
