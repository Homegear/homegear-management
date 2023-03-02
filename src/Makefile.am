AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

bin_PROGRAMS = homegear-management
homegear_management_SOURCES = IpcClient.cpp main.cpp Settings.cpp GD.cpp
homegear_management_LDADD = -lpthread -lhomegear-base -lc1-net -lz -lgcrypt -lgnutls -lhomegear-ipc

if BSDSYSTEM
else
homegear_management_LDADD += -ldl
endif
