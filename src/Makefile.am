AM_CXXFLAGS = $(CXXFLAGS) -I$(top_srcdir)/src -std=c++0x

bin_PROGRAMS = mi_cybathlon_controller \
	       mi_cybathlon \
	       mi_cybathlon2 \
	       mi_cybathlon3 \
	       mi_cybathlon1vs1

bin_SCRIPTS = cp_cybathlon_launcher

mi_cybathlon_controller_SOURCES = mi_cybathlon_controller.cpp mi_cybathlon_utilities.hpp
mi_cybathlon_controller_LDADD = -lcnbicore -lcnbiloop -ltobicore -ltobiid \
				-llpttrigger -lcnbiconfig


# DEPRACATED CYBATHLON PROTOCOLS. TO BE DELETED.

mi_cybathlon_SOURCES = mi_cybathlon.cpp mi_cybathlon_utilities_deprecated.hpp
mi_cybathlon_LDADD = -lcnbismr \
				  -lcnbiprotocol \
				  -lcnbicore -lcnbiloop -ltobicore -ltobiid -ltobiic \
				  -ldrawtk -llpttrigger\
				  -lSDL_mixer -lcnbiconfig

mi_cybathlon2_SOURCES = mi_cybathlon2.cpp mi_cybathlon_utilities_deprecated.hpp
mi_cybathlon2_LDADD = -lcnbismr \
				  -lcnbiprotocol \
				  -lcnbicore -lcnbiloop -ltobicore -ltobiid -ltobiic \
				  -ldrawtk -llpttrigger\
				  -lSDL_mixer -lcnbiconfig

mi_cybathlon3_SOURCES = mi_cybathlon3.cpp mi_cybathlon_utilities_deprecated.hpp
mi_cybathlon3_LDADD = -lcnbismr \
				  -lcnbiprotocol \
				  -lcnbicore -lcnbiloop -ltobicore -ltobiid -ltobiic \
				  -ldrawtk -llpttrigger\
				  -lSDL_mixer -lcnbiconfig

mi_cybathlon1vs1_SOURCES = mi_cybathlon1vs1.cpp mi_cybathlon_utilities_deprecated.hpp
mi_cybathlon1vs1_LDADD = -lcnbismr \
				  -lcnbiprotocol \
				  -lcnbicore -lcnbiloop -ltobicore -ltobiid -ltobiic \
				  -ldrawtk -llpttrigger\
				  -lSDL_mixer -lcnbiconfig

