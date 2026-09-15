TARGET = darksword-agent.dylib

CC = clang
CFLAGS = -framework Foundation -framework Security -framework UIKit \
         -lsqlite3 \
         -isysroot $(shell xcrun --sdk iphoneos --show-sdk-path) \
         -arch arm64 -arch arm64e \
         -miphoneos-version-min=15.0 \
         -fobjc-arc \
         -dynamiclib \
         -Wno-availability

LDFLAGS = -install_name @rload/darksword-agent.dylib

sign: $(TARGET)
	@ldid -S $<

$(TARGET): $(wildcard src/*.m)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	@rm -f $(TARGET)

.PHONY: clean sign
