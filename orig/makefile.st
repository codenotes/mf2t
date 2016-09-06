# $Id: makefile.st,v 1.3 1991/11/15 19:31:00 piet Rel $
# Makefile for Atari ST with Sozobon C.
# CP = cp

CFLAGS = -O -Datarist
# If you have an old version of flex that defines yyleng
# as a macro rather than a variable, uncomment the following line:
# CFLAGS = -O -DNO_YYLENG_VAR

SRCS = crack.c mf2t.c midifile.c midifile.h t2mf.c t2mf.h \
       t2mf.fl t2mflex.c yyread.c \
       README.TXT makefile.st makefile.unx makefile.bcc makefile.msc\
       example1.mid example1.txt example2.mid example2.txt example3.mid \
       example3.txt example4.mid example4.txt example5.mid example5.txt

EXECS = mf2t.ttp t2mf.ttp
       
all:	$(EXECS)

t2mf.ttp:	midifile.o t2mf.o t2mf.h t2mflex.o crack.o
	$(CC) -f t2mf.o midifile.o crack.o t2mflex.o libfl.a -o t2mf.ttp
	
t2mflex.c: t2mf.fl
	flex -is -Ce t2mf.fl
	mv lexyy.c t2mflex.c

t2mflex.o: t2mflex.c t2mf.h
	
mf2t.ttp:	midifile.o mf2t.o crack.o
	$(CC) -f mf2t.o midifile.o crack.o -o mf2t.ttp

libfl.a:	yyread.o
	ar r libfl.a yyread.o

zoo:	$(SRCS) $(EXECS)
	zoo ah// mf2t.zoo $(SRCS) $(EXECS)

zip:	$(SRCS) $(EXECS)
	zip -9 mf2t $(SRCS) $(EXECS)
