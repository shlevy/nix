nix_noinst_scripts := \
  $(d)/nix-http-export.cgi \
  $(d)/nix-profile.sh \
  $(d)/nix-reduce-build

ifeq ($(OS), Darwin)
  nix_noinst_scripts += $(d)/resolve-system-dependencies.pl
endif

noinst-scripts += $(nix_noinst_scripts)

profiledir = $(sysconfdir)/profile.d

$(eval $(call install-file-as, $(d)/nix-profile.sh, $(profiledir)/nix.sh, 0644))
ifeq ($(OS), Darwin)
  $(eval $(call install-program-in, $(d)/resolve-system-dependencies.pl, $(libexecdir)/nix))
endif

clean-files += $(nix_noinst_scripts)
