/* $Id: mf2t.c,v 1.5 1995/12/14 20:20:10 piet Rel $ */
/*
 * mf2t
 * 
 * Convert a MIDI file to text.
 */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include "midifile.h"

static int TrkNr;
static int TrksToDo = 1;
static int Measure, M0, Beat, Clicks;
static long T0;

extern int arg_index;
extern char *arg_option;

/* options */

static int fold = 0;		/* fold long lines */
static int notes = 0;		/* print notes as a-g */
static int times = 0;		/* print times as Measure/beat/click */

static char *Onmsg  = "On ch=%d n=%s v=%d\n";
static char *Offmsg = "Off ch=%d n=%s v=%d\n";
static char *PoPrmsg = "PoPr ch=%d n=%s v=%d\n";
static char *Parmsg = "Par ch=%d c=%d v=%d\n";
static char *Pbmsg  = "Pb ch=%d v=%d\n";
static char *PrChmsg = "PrCh ch=%d p=%d\n";
static char *ChPrmsg = "ChPr ch=%d v=%d\n";

static FILE *F;

int filegetc()
{
	return(getc(F));
}

main(argc,argv)
int argc;
char **argv;
{
	FILE *efopen();
	int flg;
	
	Mf_nomerge = 1;
	while (flg = crack (argc, argv, "F|f|BbNnTtVvMm", 0)) {
		switch (flg) {
		case 'f':
		case 'F':
			if (*arg_option)
				fold = atoi(arg_option);
			else
				fold = 80;
			break;
		case 'm':
		case 'M':
			Mf_nomerge = 0;
			break;
		case 'n':
		case 'N':
			notes++;
			break;
		case 't':
		case 'T':
		case 'b':
		case 'B':
			times++;
			break;
		case 'v':
		case 'V':
			Onmsg  = "On ch=%d note=%s vol=%d\n";
			Offmsg = "Off ch=%d note=%s vol=%d\n";
			PoPrmsg = "PolyPr ch=%d note=%s val=%d\n";
			Parmsg = "Param ch=%d con=%d val=%d\n";
			Pbmsg  = "Pb ch=%d val=%d\n";
			PrChmsg = "ProgCh ch=%d prog=%d\n";
			ChPrmsg = "ChanPr ch=%d val=%d\n";
			break;
		case EOF:
			exit(1);
		}
	}
	if ( arg_index < argc )
		F = efopen(argv[arg_index++],"rb");
	else {
#ifdef SETMODE
	        setmode(fileno(stdin), O_BINARY);
		F = stdin;
#else
		F = fdopen(fileno(stdin),"rb");
#endif
	      }
	if (arg_index < argc &&
		!freopen (argv[arg_index],"w",stdout))
			error ("Can't open output file");

	initfuncs();
	Mf_getc = filegetc;
	TrkNr = 0;
	Measure = 4;
	Beat = 96;
	Clicks = 96;
	T0 = 0;
	M0 = 0;
	mfread();
	if (ferror(F)) error ("Output file error");
	fclose(F);
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
        if (TrksToDo <= 0)
	    	fprintf(stderr,"Error: Garbage at end\n",s);
	else
	    fprintf(stderr,"Error: %s\n",s);
}

	
char *
mknote(pitch)
int pitch;
{
	static char * Notes [] =
		{ "c", "c#", "d", "d#", "e", "f", "f#", "g",
		  "g#", "a", "a#", "b" };
	static char buf[5];
	if ( notes )
		sprintf (buf, "%s%d", Notes[pitch % 12], pitch/12);
	else
		sprintf (buf, "%d", pitch);
	return buf;
}

myheader(format,ntrks,division)
int format, ntrks, division;
{
	if (division & 0x8000) { /* SMPTE */
	    times = 0;		 /* Can't do beats */
	    printf("MFile %d %d %d %d\n",format,ntrks,
	    			-((-(division>>8))&0xff), division&0xff);
	} else
	    printf("MFile %d %d %d\n",format,ntrks,division);
	if (format > 2) {
		fprintf(stderr, "Can't deal with format %d files\n", format);
		exit (1);
	}
	Beat = Clicks = division;
	TrksToDo = ntrks;
}

mytrstart()
{
	printf("MTrk\n");
	TrkNr ++;
}

mytrend()
{
	printf("TrkEnd\n");
	--TrksToDo;
}

mynon(chan,pitch,vol)
int chan, pitch, vol;
{
	prtime();
	printf(Onmsg,chan+1,mknote(pitch),vol);
}

mynoff(chan,pitch,vol)
int chan, pitch, vol;
{
	prtime();
	printf(Offmsg,chan+1,mknote(pitch),vol);
}

mypressure(chan,pitch,press)
int chan, pitch, press;
{
	prtime();
	printf(PoPrmsg,chan+1,mknote(pitch),press);
}

myparameter(chan,control,value)
int chan, control, value;
{
	prtime();
	printf(Parmsg,chan+1,control,value);
}

mypitchbend(chan,lsb,msb)
int chan, lsb, msb;
{
	prtime();
	printf(Pbmsg,chan+1,128*msb+lsb);
}

myprogram(chan,program)
int chan, program;
{
	prtime();
	printf(PrChmsg,chan+1,program);
}

mychanpressure(chan,press)
int chan, press;
{
	prtime();
	printf(ChPrmsg,chan+1,press);
}

mysysex(leng,mess)
int leng;
char *mess;
{
	prtime();
	printf("SysEx");
	prhex (mess, leng);
}

mymmisc(type,leng,mess)
int type, leng;
char *mess;
{
	prtime();
	printf("Meta 0x%02x",type);
	prhex (mess, leng);
}

mymspecial(leng,mess)
int leng;
char *mess;
{
	prtime();
	printf("SeqSpec");
	prhex (mess, leng);
}

mymtext(type,leng,mess)
int type, leng;
char *mess;
{
	static char *ttype[] = {
		NULL,
		"Text",		/* type=0x01 */
		"Copyright",	/* type=0x02 */
		"TrkName",
		"InstrName",	/* ...       */
		"Lyric",
		"Marker",
		"Cue",		/* type=0x07 */
		"Unrec"
	};
	int unrecognized = (sizeof(ttype)/sizeof(char *)) - 1;

	prtime();
	if ( type < 1 || type > unrecognized )
		printf("Meta 0x%02x ",type);
	else if (type == 3 && TrkNr == 1)
		printf("Meta SeqName ");
	else
		printf("Meta %s ",ttype[type]);
	prtext (mess, leng);
}

mymseq(num)
int num;
{
	prtime();
	printf("SeqNr %d\n",num);
}

mymeot()
{
	prtime();
	printf("Meta TrkEnd\n");
}

mykeysig(sf,mi)
int sf, mi;
{
	prtime();
	printf("KeySig %d %s\n", (sf>127?sf-256:sf), (mi?"minor":"major"));
}

mytempo(tempo)
long tempo;
{
	prtime();
	printf("Tempo %ld\n",tempo);
}

mytimesig(nn,dd,cc,bb)
int nn, dd, cc, bb;
{
	int denom = 1;
	while ( dd-- > 0 )
		denom *= 2;
	prtime();
	printf("TimeSig %d/%d %d %d\n",
		nn,denom,cc,bb);
	M0 += (Mf_currtime-T0)/(Beat*Measure);
	T0 = Mf_currtime;
	Measure = nn;
	Beat = 4 * Clicks / denom;
}

mysmpte(hr,mn,se,fr,ff)
int hr, mn, se, fr, ff;
{
	prtime();
	printf("SMPTE %d %d %d %d %d\n",
		hr,mn,se,fr,ff);
}

myarbitrary(leng,mess)
int leng;
char *mess;
{
	prtime();
	printf("Arb",leng);
	prhex (mess, leng);
}

prtime()
{
	if (times) {
		long m = (Mf_currtime-T0)/Beat;
		printf ("%ld:%ld:%ld ",
			m/Measure+M0, m%Measure, (Mf_currtime-T0)%Beat);
	} else
		printf("%ld ",Mf_currtime);
}

prtext(p, leng)
unsigned char *p; int leng;
{
	int n, c;
	int pos = 25;
	
	printf("\"");
	for ( n=0; n<leng; n++ ) {
		c = *p++;
		if (fold && pos >= fold) {
			printf ("\\\n\t");
			pos = 13;	/* tab + \xab + \ */
			if (c == ' ' || c == '\t') {
				putchar ('\\');
				++pos;
			}
		}
		switch (c) {
		case '\\':
		case '"':
			printf ("\\%c", c);
			pos += 2;
			break;
		case '\r':
			printf ("\\r");
			pos += 2;
			break;
		case '\n':
			printf ("\\n");
			pos += 2;
			break;
		case '\0':
			printf ("\\0");
			pos += 2;
			break;
		default:
			if (isprint(c)) {
				putchar(c);
				++pos;
			} else {
				printf("\\x%02x" , c);
				pos += 4;
			}
		}
	}
	printf("\"\n");
}

prhex(p, leng)
unsigned char *p; int leng;
{
	int n;
	int pos = 25;

	for ( n=0; n<leng; n++,p++ ) {
		if (fold && pos >= fold) {
			printf ("\\\n\t%02x" , *p);
			pos = 14;	/* tab + ab + " ab" + \ */
		}
		else {
			printf(" %02x" , *p);
			pos += 3;
		}
	}
	printf("\n");
	
}

initfuncs()
{
	Mf_error = error;
	Mf_header =  myheader;
	Mf_starttrack =  mytrstart;
	Mf_endtrack =  mytrend;
	Mf_on =  mynon;
	Mf_off =  mynoff;
	Mf_pressure =  mypressure;
	Mf_parameter =  myparameter;
	Mf_pitchbend =  mypitchbend;
	Mf_program =  myprogram;
	Mf_chanpressure =  mychanpressure;
	Mf_sysex =  mysysex;
	Mf_metamisc =  mymmisc;
	Mf_seqnum =  mymseq;
	Mf_eot =  mymeot;
	Mf_timesig =  mytimesig;
	Mf_smpte =  mysmpte;
	Mf_tempo =  mytempo;
	Mf_keysig =  mykeysig;
	Mf_sqspecific =  mymspecial;
	Mf_text =  mymtext;
	Mf_arbitrary =  myarbitrary;
}
