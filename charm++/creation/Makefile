# modify the path of CHARMC
CHARMC=../../../bin/charmc $(OPTS)

all: creation

creation: creation.o
	$(CHARMC) creation.o  -o creation -language charm++

creation.o : creation.C creation.def.h
	$(CHARMC) -c creation.C

creation.decl.h creation.def.h: creation.ci
	$(CHARMC)  creation.ci

clean:
	rm -f creation creation_prof *.o conv-host *.decl.h *.def.h *~ charmrun *.log *.sts core charmrun.exe creation.exe creation.pdb creation.ilk
