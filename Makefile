APPS = ls stat

.PHONY: all clean $(APPS)

all: $(APPS)

$(APPS):
	$(MAKE) -C apps/$@

clean:
	for app in $(APPS); do $(MAKE) -C apps/$$app clean; done
