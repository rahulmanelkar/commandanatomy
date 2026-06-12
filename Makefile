APPS = hello ls stat wc cat mkdir

.PHONY: all clean shell $(APPS)

all: $(APPS) shell

$(APPS):
	$(MAKE) -C apps/$@

shell: $(APPS)
	$(MAKE) -C shell

clean:
	for app in $(APPS); do $(MAKE) -C apps/$$app clean; done
	$(MAKE) -C shell clean
