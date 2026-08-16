CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -ffast-math
ARCH     ?= -march=native      # Apple silicon: make ARCH="-mcpu=apple-m1"
PYTHON   ?= python3
B         = build

.PHONY: all clean demo bench contact_probe pulse_probe pulse_sweep fit_pulse fit_hybrid lib check panel verify
all: $(B)/demo $(B)/analyze $(B)/bench $(B)/contact_probe $(B)/pulse_probe $(B)/pulse_sweep $(B)/stress $(B)/panel_demo $(B)/verify_stretch
$(B):            ; @mkdir -p $(B) out
$(B)/demo:    src/main.cpp     src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/analyze: tools/analyze.cpp src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/bench:   tools/bench.cpp   src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/contact_probe: tools/contact_probe.cpp src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/pulse_probe:   tools/pulse_probe.cpp   src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/pulse_sweep:   tools/pulse_sweep.cpp   src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/stress:  tools/stress.cpp  src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) -fno-fast-math $(ARCH) $< -o $@
$(B)/panel_demo: tools/panel_demo.cpp src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@
$(B)/verify_stretch: tools/verify_stretch.cpp src/cp80.hpp | $(B) ; $(CXX) $(CXXFLAGS) $(ARCH) $< -o $@

demo:  $(B)/demo    ; ./$(B)/demo out/demo.wav
bench: $(B)/bench   ; ./$(B)/bench
contact_probe: $(B)/contact_probe ; ./$(B)/contact_probe
pulse_probe: $(B)/pulse_probe ; ./$(B)/pulse_probe
pulse_sweep: $(B)/pulse_sweep ; ./$(B)/pulse_sweep
fit_pulse: $(B)/pulse_sweep ; $(PYTHON) tools/fit_pulse.py
fit_hybrid: $(B)/analyze $(B)/pulse_sweep ; $(PYTHON) tools/fit_hybrid.py
lib:   $(B)/analyze ; $(PYTHON) tools/render_lib.py out/model-lib
check: $(B)/stress  ; ./$(B)/stress          # must print 0 non-finite samples
panel: $(B)/panel_demo ; ./$(B)/panel_demo
verify: $(B)/verify_stretch ; ./$(B)/verify_stretch
clean: ; rm -rf $(B) out/*.wav out/model-lib
