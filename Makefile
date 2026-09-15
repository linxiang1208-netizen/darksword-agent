TARGET = collector.dylib

CC = clang
CFLAGS = -framework CoreFoundation -framework CFNetwork -framework Security \
         -isysroot $(shell xcrun --sdk iphoneos --show-sdk-path) \
         -arch arm64 -arch arm64e \
         -miphoneos-version-min=15.0 \
         -dynamiclib -Oz \
         -Wno-deprecated-declarations

sign: $(TARGET)
	@ldid -S $<

$(TARGET): src/main.m
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -f $(TARGET)

.PHONY: clean sign
