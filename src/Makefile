pilatus-objs = PilatusCamera.o PilatusInterface.o

SRCS = $(pilatus-objs:.o=.cpp) 

CXXFLAGS += -I../include -I../../../hardware/include -I../../../common/include \
	-Wall -pthread -fPIC -g

all:	Pilatus.o

Pilatus.o:	$(pilatus-objs)
	$(LD) -o $@ -r $+

clean:
	rm -f *.o *.P

%.o : %.cpp
	$(COMPILE.cpp) -MD $(CXXFLAGS) -o $@ $<
	@cp $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	rm -f $*.d

-include $(SRCS:.cpp=.P)
