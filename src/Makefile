pilatus-objs = PilatusCamera.o PilatusInterface.o PilatusSaving.o

SRCS = $(pilatus-objs:.o=.cpp) 

CXXFLAGS += -I../include -I../../../hardware/include -I../../../common/include \
	-I../../../third-party/CBFLib/include -Wall -pthread -fPIC -g

all:	Pilatus.o

Pilatus.o:	$(pilatus-objs)
	$(LD) -o $@ -r $+ ../../../third-party/CBFLib/lib/libcbf.a

clean:
	rm -f *.o *.P

%.o : %.cpp
	$(COMPILE.cpp) -MD $(CXXFLAGS) -o $@ $<
	@cp $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	rm -f $*.d

-include $(SRCS:.cpp=.P)
