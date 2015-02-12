# -----------------------------------------------------------------
#   Makefile for inbix - In Silico Bioinformatics
#   
#   Compilation options
#       R plugins                   WITH_R_PLUGINS
#       Web-based version check     WITH_WEBCHECK
#       Ensure 32-bit binary        FORCE_32BIT 
#       (Ignored)                   WITH_ZLIB
#       Link to LAPACK              WITH_LAPACK
#       Force dynamic linking       FORCE_DYNAMIC
#
# ---------------------------------------------------------------------

# Set this variable to either UNIX, MAC or WIN
SYS = UNIX

# Leave blank after "=" to disable; put "= 1" to enable
WITH_R_PLUGINS = 1
WITH_WEBCHECK = 1
FORCE_32BIT = 
WITH_ZLIB = 1
WITH_LAPACK = 
#FORCE_DYNAMIC = $(INBIX_FORCE_DYNAMIC)
FORCE_DYNAMIC = 1

# Put C++ compiler here; Windows has it's own specific version
CXX_UNIX = g++
CXX_WIN = g++.exe

# Any other compiler flags here ( -Wall, -g, etc)
CXXFLAGS = 
# optimized mode
CXXFLAGS += -O3 -I. -D_FILE_OFFSET_BITS=64 -Dfopen64=fopen -Wno-write-strings \
  -L/usr/local/lib
# debug mode
#CXXFLAGS += -g -I. -D_FILE_OFFSET_BITS=64 -Dfopen64=fopen -Wno-write-strings \
  -L/usr/local/lib

# Misc
LIB_LAPACK = /usr/lib/liblapack.so
LIB_BLAS = /usr/lib/libblas.so
LIB_IGRAPH = /usr/lib/libigraph.so

OUTPUT = inbix

# Some system specific flags
ifeq ($(SYS),WIN)
 CXXFLAGS += -DWIN
 CXX = $(CXX_WIN)
 ifndef FORCE_DYNAMIC
  CXXFLAGS += -static
 endif
endif

ifeq ($(SYS),UNIX)
 CXXFLAGS += -DUNIX
 CXX = $(CXX_UNIX)
 ifndef FORCE_DYNAMIC
  CXXFLAGS += -static
  LIB += /usr/local/lib/libarmadillo.a
  LIB += /usr/lib/liblapack.a
  LIB += /usr/lib/libblas/libblas.a
  LIB += /usr/lib/gcc/x86_64-linux-gnu/4.4/libgfortran.a
 else
  LIB += -larmadillo
  LIB += -llapack
  LIB += -lblas
 endif
endif

ifeq ($(SYS),MAC)
 CXXFLAGS += -DUNIX
 CXX = $(CXX_UNIX)
endif

ifeq ($(SYS),SOLARIS)
 CXXFLAGS += -fast
 CXXFLAGS += -xtarget=ultraT2  # specific Sun hardware (must be specified after the -fast option)
 CXXFLAGS += -xdepend=no       # required to fix seg fault in iropt
 LIB = -lstdc++
 LIB += -lgcc
 LIB += -lsocket         # added for socket support
 LIB += -lnsl            # added for network services support
 CXX = $(CXX_UNIX)
endif

ifdef FORCE_32BIT
 CXXFLAGS += -m32
endif

# Flags for web-based version check
ifdef WITH_WEBCHECK
ifeq ($(SYS),WIN)
 LIB += -lwsock32
endif
else
 CXXFLAGS += -DSKIP
endif

CXXFLAGS += -fopenmp

SRC = inbix.cpp plink.cpp options.cpp input.cpp binput.cpp tinput.cpp \
genome.cpp \
helper.cpp stats.cpp filters.cpp locus.cpp multi.cpp crandom.cpp	\
cluster.cpp mds.cpp output.cpp informative.cpp assoc.cpp epi.cpp	\
prephap.cpp phase.cpp trio.cpp tdt.cpp sharing.cpp genepi.cpp sets.cpp	\
perm.cpp mh.cpp genedrop.cpp gxe.cpp merge.cpp hotel.cpp multiple.cpp	\
haploCC.cpp haploTDT.cpp poo.cpp webcheck.cpp qfam.cpp linear.cpp	\
bmerge.cpp parse.cpp mishap.cpp legacy.cpp homozyg.cpp segment.cpp	\
model.cpp logistic.cpp glm.cpp dcdflib.cpp elf.cpp dfam.cpp fisher.cpp	\
linput.cpp sockets.cpp lookup.cpp proxy.cpp pdriver.cpp haploQTL.cpp	\
haplohelper.cpp haplowindow.cpp genogroup.cpp nonfounderphasing.cpp	\
clumpld.cpp genoerr.cpp em.cpp impute.cpp metaem.cpp profile.cpp	\
nlist.cpp whap.cpp simul.cpp gvar.cpp cnv.cpp step.cpp greport.cpp	\
flip.cpp qualscores.cpp cnvqt.cpp cfamily.cpp setscreen.cpp idhelp.cpp	\
tag.cpp hapglm.cpp lookup2.cpp blox.cpp zed.cpp dosage.cpp annot.cpp	\
metaanal.cpp \
ArmadilloFuncs.cpp regain.cpp InteractionNetwork.cpp CentralityRanker.cpp \
EpistasisEQtl.cpp

HDR = plink.h options.h helper.h stats.h crandom.h sets.h phase.h	\
perm.h model.h linear.h logistic.h dcdflib.h ipmpar.h cdflib.h		\
fisher.h sockets.h haplowindow.h genogroup.h clumpld.h nlist.h whap.h	\
gvar.h cnv.h cfamily.h idhelp.h zed.h StringUtils.h                     \
ArmadilloFuncs.h regain.h InteractionNetwork.h CentralityRanker.h \
EpistasisEQtl.h

ifdef WITH_R_PLUGINS
CXXFLAGS += -DWITH_R_PLUGINS
HDR += sisocks.h Rsrv.h Rconnection.h config.h
SRC += r.cpp Rconnection.cpp
ifeq ($(SYS),MAC)
LIB += -ldl
endif
ifeq ($(SYS),UNIX)
LIB += -ldl -lcrypt
endif
endif

ifdef WITH_ZLIB
CXXFLAGS += -DWITH_ZLIB
HDR += zfstream.h
SRC += zfstream.cpp
LIB += -lz
endif

ifdef WITH_LAPACK
CXXFLAGS += -DWITH_LAPACK
HDR += lapackf.h
SRC += lapackf.cpp
LIB += $(LIB_LAPACK) 
endif

OBJ = $(SRC:.cpp=.o)

all : $(OUTPUT) 

$(OUTPUT) :
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJ) $(LIB) 

$(OBJ) : $(HDR)

.cpp.o : 
	$(CXX) $(CXXFLAGS) -c $*.cpp

.SUFFIXES : .cpp .c .o $(SUFFIXES)

$(OUTPUT) : $(OBJ)

FORCE:

clean:
	rm -f *.o *~ inbix

install:
	cp inbix /usr/local/bin

dist:
	zip inbix_src.zip $(SRC) $(HDR) Makefile COPYING.txt README.*
