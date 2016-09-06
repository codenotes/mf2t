/* $Id: t2mf.c,v 1.5 1995/12/14 21:58:36 piet Rel piet $ */
/*
 * t2mf
 * 
 * Convert text file to a MIDI file.
 */

#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
/*#include <memory.h>*/
#include "t2mf.h"

#ifdef NO_YYLENG_VAR
#define	yyleng yylength
#endif

static jmp_buf erjump;
static int err_cont = 0;

static int TrkNr;
static int Format, Ntrks;
static int Measure, M0, Beat, Clicks;
static long T0;
static char* buffer = 0;
static int bufsiz = 0, buflen;

extern int arg_index;
extern char *arg_option;
extern long yyval;
extern int yyleng;
extern int lineno;
extern char *yytext;
extern int do_hex;
extern int eol_seen;
extern FILE  *yyin;

extern int arg_index;
extern char *arg_option;
extern int Mf_RunStat;

static int  mywritetrack();
static void checkchan();
static void checknote();
static void checkval();
static void splitval();
static void get16val();
static void checkcon();
static void checkprog();
static void checkeol();
static void gethex();

static FILE *F;

fileputc(c)
int c;
{
    return putc(c, F);
}

main(argc,argv)
int argc;
char **argv;
{
    	FILE *efopen();
	int flg;

	while (flg = crack (argc, argv, "Rr", 0)) {
		switch (flg) {
		case 'r':
		case 'R':
			Mf_RunStat = 1;
			break;
		case EOF:
			exit(1);
		}
	}

    if ( arg_index < argc )
        yyin = efopen (argv[arg_index++], "r");
    else
        yyin = stdin;
    if (arg_index < argc )
        F = efopen(argv[arg_index],"wb");
    else {
#ifdef SETMODE
        setmode (fileno(stdout), O_BINARY);
	F = stdout;
#else
        F = fdopen (fileno(stdout), "wb");
#endif
      }

    Mf_putc = fileputc;
    Mf_wtrack = mywritetrack;
    TrkNr = 0;
    Measure = 4;
    Beat = 96;
    Clicks = 96;
    M0 = 0;
    T0 = 0;
    translate();
    fclose(F);
    fclose(yyin);
    exit(0);
}

FILE *
efopen(name,mode)
char *name;
char *mode;
{
    FILE *f;
    extern int errno;
    extern char *sys_errlist[];
    extern int sys_nerr;
    char *errmess;

    if ( (f=fopen(name,mode)) == NULL ) {
        (void) fprintf(stderr,"*** ERROR *** Cannot open '%s'!\n",name);
        if ( errno <= sys_nerr )
            errmess = sys_errlist[errno];
        else
            errmess = "Unknown error!";
        (void) fprintf(stderr,"************* Reason: %s\n",errmess);
        exit(1);
    }
    return(f);
}

error(s)
char *s;
{
    fprintf(stderr,"Error: %s\n",s);
}

prs_error(s)
char *s;
{
    int c;
    int count;
    int ln = (eol_seen? lineno-1 : lineno);
    fprintf (stderr, "%d: %s\n", ln, s);
    if (yyleng > 0 && *yytext != '\n')
        fprintf (stderr, "*** %*s ***\n", yyleng, yytext);
    count = 0;
    while (count < 100 && 
	   (c=yylex()) != EOL && c != EOF) count++/* skip rest of line */;
    if (c == EOF) exit(1);
    if (err_cont)
        longjmp (erjump,1);
}

syntax()
{
    prs_error ("Syntax error");
}

translate()
{
    if (yylex()==MTHD) {
        Format = getint("MFile format");
        Ntrks = getint("MFile #tracks");
        Clicks = getint("MFile Clicks");
	if (Clicks < 0)
		Clicks = (Clicks&0xff)<<8|getint("MFile SMPTE division");
	checkeol();
        mfwrite(Format, Ntrks, Clicks, F);
    } else {
        fprintf (stderr, "Missing MFile - can't continue\n");
	exit(1);
    }
}

char data[5];
int chan;

static int mywritetrack()
{
    int opcode, c;
    long currtime = 0;
    long newtime, delta;
    int i, k;
    
    while ((opcode = yylex()) == EOL) ;
    if (opcode != MTRK)
        prs_error("Missing MTrk");
    checkeol();
    while (1) {
	err_cont = 1;
	setjmp (erjump);
        switch (yylex()) {
        case MTRK:
            prs_error("Unexpected MTrk");
	case EOF:
	    err_cont = 0;
	    error ("Unexpected EOF");
	    return -1;
        case TRKEND:
	    err_cont = 0;
	    checkeol();
            return 1;
        case INT:
            newtime = yyval;
            if ((opcode=yylex())=='/') {
                if (yylex()!=INT) prs_error("Illegal time value");
                newtime = (newtime-M0)*Measure+yyval;
                if (yylex()!='/'||yylex()!=INT) prs_error("Illegal time value");
                newtime = T0 + newtime*Beat + yyval;
		opcode = yylex();
            }
            delta = newtime - currtime;
            switch (opcode) {
            case ON:
            case OFF:
            case POPR:
		checkchan();
		checknote();
		checkval();
		mf_w_midi_event (delta, opcode, chan, data, 2L);
		break;

            case PAR:
		checkchan();
		checkcon();
		checkval();
		mf_w_midi_event (delta, opcode, chan, data, 2L);
		break;
		
            case PB:
		checkchan();
		splitval();
		mf_w_midi_event (delta, opcode, chan, data, 2L);
		break;

            case PRCH:
		checkchan();
		checkprog();
		mf_w_midi_event (delta, opcode, chan, data, 1L);
		break;
                
	    case CHPR:
		checkchan();
		checkval();
		data[0] = data[1];
		mf_w_midi_event (delta, opcode, chan, data, 1L);
		break;
	    	
            case SYSEX:
            case ARB:
	    	gethex();
		mf_w_sysex_event (delta, buffer, (long)buflen);
		break;
		
            case TEMPO:
	    	if (yylex() != INT) syntax();
		mf_w_tempo (delta, yyval);
		break;
		
            case TIMESIG: {
	        int nn, denom, cc, bb;
	        if (yylex() != INT || yylex() != '/') syntax();
		nn = yyval;
		denom = getbyte("Denom");
		cc = getbyte("clocks per click");
		bb = getbyte("32nd notes per 24 clocks");
		for (i=0, k=1 ; k<denom; i++, k<<=1);
		if (k!=denom) error ("Illegal TimeSig");
		data[0] = nn;
		data[1] = i;
		data[2] = cc;
		data[3] = bb;
		M0 += (newtime-T0)/(Beat*Measure);
		T0 = newtime;
		Measure = nn;
		Beat = 4 * Clicks / denom;
		mf_w_meta_event (delta, time_signature, data, 4L);
		}
		break;
		
            case SMPTE:
		for (i=0; i<5; i++) {
		    data[i] = getbyte("SMPTE");
		}
		mf_w_meta_event (delta, smpte_offset, data, 5L);
		break;
		
            case KEYSIG:
		data[0] = i = getint ("Keysig");
		if (i<-7 || i>7)
		    error ("Key Sig must be between -7 and 7");
		if ((c=yylex()) != MINOR && c != MAJOR)
			syntax();
		data[1] = (c == MINOR);
		mf_w_meta_event (delta, key_signature, data, 2L);
		break;
		
            case SEQNR:
		get16val ("SeqNr");
		mf_w_meta_event (delta, sequence_number, data, 2L);
		break;

	    case META: {
	        int type = yylex();
		switch (type) {
		case TRKEND:
		    type = end_of_track;
		    break;
		case TEXT:
		case COPYRIGHT:
		case SEQNAME:
		case INSTRNAME:
		case LYRIC:
		case MARKER:
		case CUE:
		    type -= (META+1);
		    break;
		case INT:
		    type = yyval;
		    break;
		default:
		    prs_error ("Illegal Meta type");
		}
		if (type == end_of_track)
		    buflen = 0;
		else
		    gethex();
		mf_w_meta_event (delta, type, buffer, (long)buflen);
		break;
	    }
            case SEQSPEC:
		gethex();
		mf_w_meta_event (delta, sequencer_specific, buffer, (long)buflen);
		break;
	    default:
	        prs_error ("Unknown input");
		break;
	    }
            currtime = newtime;
        case EOL:
	    break;
	default:
	    prs_error ("Unknown input");
	    break;
	}
	checkeol();
    }
}

getbyte(mess)
char *mess;
{
    char ermesg[100];
    getint (mess);
    if (yyval < 0 || yyval > 127) {
	sprintf (ermesg, "Wrong value (%ld) for %s", yyval, mess);
        error (ermesg);
	yyval = 0;
    }
    return yyval;
}

getint(mess)
char *mess;
{
    char ermesg[100];
    if (yylex() != INT) {
	sprintf (ermesg, "Integer expected for %s", mess);
        error (ermesg);
	yyval = 0;
    }
    return yyval;
}

static void checkchan()
{
    if (yylex() != CH || yylex() != INT) syntax();
    if (yyval < 1 || yyval > 16)
	error ("Chan must be between 1 and 16");
    chan = yyval-1;
}

static void checknote()
{
    int c;
    if (yylex() != NOTE || ((c=yylex()) != INT && c != NOTEVAL))
	syntax();
    if (c == NOTEVAL) {
        static int notes[] = {
	    9,		/* a */
	    11,		/* b */
	    0,		/* c */
	    2,		/* d */
	    4,		/* e */
	    5,		/* f */
	    7		/* g */
        };
	char *p = yytext;
        c = *p++;
        if (isupper(c)) c = tolower(c);
        yyval = notes[c-'a'];
        switch (*p) {
        case '#':
        case '+':
       	    yyval++;
	    p++;
	    break;
	case 'b':
	case 'B':
	case '-':
	    yyval--;
	    p++;
	    break;
	}	
	yyval += 12 * atoi(p);
    }
    if (yyval < 0 || yyval > 127)
	error ("Note must be between 0 and 127");
    data[0] = yyval;
}

static void checkval()
{
    if (yylex() != VAL || yylex() != INT) syntax();
    if (yyval < 0 || yyval > 127)
	error ("Value must be between 0 and 127");
    data[1] = yyval;
}

static void splitval()
{
    if (yylex() != VAL || yylex() != INT) syntax();
    if (yyval < 0 || yyval > 16383)
	error ("Value must be between 0 and 16383");
    data[0] = yyval%128;
    data[1] = yyval/128;
}

static void get16val()
{
    if (yylex() != VAL || yylex() != INT) syntax();
    if (yyval < 0 || yyval > 65535)
	error ("Value must be between 0 and 65535");
    data[0] = (yyval>>8)&0xff;
    data[1] = yyval&0xff;
}

static void checkcon()
{
    if (yylex() != CON || yylex() != INT)
	syntax();
    if (yyval < 0 || yyval > 127)
	error ("Controller must be between 0 and 127");
    data[0] = yyval;
}

static void checkprog()
{
    if (yylex() != PROG || yylex() != INT) syntax();
    if (yyval < 0 || yyval > 127)
	error ("Program number must be between 0 and 127");
    data[0] = yyval;
}

static void checkeol()
{
    if (eol_seen) return;
    if (yylex() != EOL) {
    	prs_error ("Garbage deleted");
	while (!eol_seen) yylex();	 /* skip rest of line */
    }
}

static void gethex()
{
    int c;
    buflen = 0;
    do_hex = 1;
    c = yylex();
    if (c==STRING) {
	/* Note: yytext includes the trailing, but not the starting quote */
	int i = 0;
    	if (yyleng-1 > bufsiz) {
	    bufsiz = yyleng-1;
	    if (buffer)
	    	buffer = realloc (buffer, bufsiz);
	    else
	    	buffer = malloc (bufsiz);
	    if (! buffer) error ("Out of memory");
	}
	while (i<yyleng-1) {
	    c = yytext[i++];
rescan:
	    if (c == '\\') {
	    	switch (c = yytext[i++]) {
		case '0':
		    c = '\0';
		    break;
		case 'n':
		    c = '\n';
		    break;
		case 'r':
		    c = '\r';
		    break;
		case 't':
		    c = '\t';
		    break;
		case 'x':
		    if (sscanf (yytext+i, "%2x", &c) != 1)
		    	prs_error ("Illegal \\x in string");
		    i += 2;
		    break;
		case '\r':
		case '\n':
		    while ((c=yytext[i++])==' '||c=='\t'||c=='\r'||c=='\n')
		    	/* skip whitespace */;
		    goto rescan; /* sorry EWD :=) */
		}
	    }
	    buffer[buflen++] = c;
	}	    
    }
    else if (c == INT) {
	do {
    	    if (buflen >= bufsiz) {
	        bufsiz += 128;
	        if (buffer)
	    	    buffer = realloc (buffer, bufsiz);
	        else
	    	    buffer = malloc (bufsiz);
	        if (! buffer) error ("Out of memory");
	    }
/* This test not applicable for sysex
	    if (yyval < 0 || yyval > 127)
	        error ("Illegal hex value"); */
	    buffer[buflen++] = yyval;
	    c = yylex();
	} while (c == INT);
	if (c != EOL) prs_error ("Unknown hex input");
    }
    else prs_error ("String or hex input expected");
}

long bankno (s, n)
char *s; int n;
{
    long res = 0;
    int c;
    while (n-- > 0) {
        c = (*s++);
	if (islower(c))
	    c -= 'a';
	else if (isupper(c))
	    c -= 'A';
	else
	    c -= '1';
    	res = res * 8 + c;
    }
    return res;
}
