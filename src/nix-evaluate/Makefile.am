bin_PROGRAMS = nix-evaluate

nix_evaluate_SOURCES = nix-evaluate.cc
nix_evaluate_LDADD = ../libmain/libmain.la ../libexpr/libexpr.la \
 ../libstore/libstore.la ../libutil/libutil.la \
 ../boost/format/libformat.la

AM_CXXFLAGS = \
 -I$(srcdir)/.. -I$(srcdir)/../libutil -I$(srcdir)/../libstore \
 -I$(srcdir)/../libexpr -I$(srcdir)/../libmain -I../libexpr
