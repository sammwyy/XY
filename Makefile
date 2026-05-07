EXAMPLES := render_images audio input_status debug_text custom_font

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

$(EXAMPLES):
	$(MAKE) -C examples/$@ all

clean:
	for example in $(EXAMPLES); do $(MAKE) -C examples/$$example clean; done

