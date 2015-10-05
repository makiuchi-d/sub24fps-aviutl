

CC = bcc32
LN = bcc32
RC = brc32
CFLAG = -c -O1 -O2
LFLAG = -tWD

DLL = sub24fps.auf
OBJ = sub24fps.obj


all: $(DLL)

$(DLL): $(OBJ) sub24fps.res
	$(LN) -e$(DLL) $(LFLAG) $(OBJ)
	$(RC) -fe$(DLL) sub24fps.res

sub24fps.obj: sub24fps.c
	$(CC) $(CFLAG) sub24fps.c

sub24fps.res: sub24fps.rc
	$(RC) -r sub24fps.rc
