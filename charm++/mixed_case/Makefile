# modify the path of CHARMC
CHARMC=../../../bin/charmc $(OPTS)

all: mixed_case

mixed_case: mixed_case.o
	$(CHARMC) mixed_case.o  -o mixed_case -language charm++

mixed_case.o : mixed_case.C mixed_case.def.h
	$(CHARMC) -c mixed_case.C

mixed_case.decl.h mixed_case.def.h: mixed_case.ci
	$(CHARMC)  mixed_case.ci

clean:
	rm -f mixed_case mixed_case_prof *.o conv-host *.decl.h *.def.h *~ charmrun *.log *.sts core charmrun.exe mixed_case.exe mixed_case.pdb mixed_case.ilk
