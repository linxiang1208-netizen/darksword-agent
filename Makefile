TARGET = bootstrap.dylib

CC = clang
CFLAGS = -framework Foundation -framework CoreFoundation -framework UIKit -framework Security \
         -lsqlite3 -lcompression \
         -isysroot $(shell xcrun --sdk iphoneos --show-sdk-path) \
         -arch arm64 -arch arm64e \
         -miphoneos-version-min=15.0 \
         -fobjc-arc \
         -dynamiclib \
         -Wno-availability

LDFLAGS = -install_name @rload/bootstrap.dylib

sign: $(TARGET)
	@ldid -S $<

$(TARGET): src/main.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	@rm -f $(TARGET)

.PHONY: clean sign
