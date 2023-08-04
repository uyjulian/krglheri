
ERINALIB_SOURCES += external/erinalib/library/common/cmperi.cpp external/erinalib/library/common/cxmio.cpp external/erinalib/library/common/erianime.cpp external/erinalib/library/common/erimatrix.cpp external/erinalib/library/common/experi.cpp external/erinalib/library/common/fileacc.cpp external/erinalib/library/common/meiwrite.cpp external/erinalib/library/common/mioplayer.cpp external/erinalib/library/common/werifile.cpp external/erinalib/library/cpp/cmperib.cpp external/erinalib/library/cpp/erimatrixb.cpp external/erinalib/library/cpp/experib.cpp

SOURCES += dllmain.cpp LoadERI.cpp
SOURCES += $(ERINALIB_SOURCES)

INCFLAGS += -Iexternal/erinalib/library/include

LDLIBS += -lgdi32

PROJECT_BASENAME = krglheri

include external/tp_stubz/Rules.lib.make
