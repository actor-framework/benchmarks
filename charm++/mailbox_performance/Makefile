# modify the path of CHARMC
CHARMC=../../../bin/charmc $(OPTS)

all: mailbox

mailbox: mailbox.o
	$(CHARMC) mailbox.o  -o mailbox -language charm++

mailbox.o : mailbox.C mailbox.def.h
	$(CHARMC) -c mailbox.C

mailbox.decl.h mailbox.def.h: mailbox.ci
	$(CHARMC)  mailbox.ci

clean:
	rm -f mailbox mailbox_prof *.o conv-host *.decl.h *.def.h *~ charmrun *.log *.sts core charmrun.exe mailbox.exe mailbox.pdb mailbox.ilk
