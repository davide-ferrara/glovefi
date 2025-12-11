CC = gcc
TARGET = glovefi
SRCS = main.c
CFLAGS = -W -Wall -O2
LDLIBS = $(shell pkg-config --libs libsystemd)
INSTALL_PATH = /usr/bin

$(TARGET): $(SRCS)
	@echo "Building $(TARGET)..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	@echo "Removing $(TARGET)..."
	rm $(TARGET)

install: $(TARGET)
	@echo "Installing $(TARGET) in $(INSTALL_PATH)!"
	install -m 755 $(TARGET) $(INSTALL_PATH) 

uninstall:
	@echo "Uninstalling $(TARGET)!"
	sudo rm -f $(INSTALL_PATH)/$(TARGET)

.PHONY: all clean install uninstall
