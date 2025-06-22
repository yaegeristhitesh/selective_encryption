# Detect platform
UNAME_S := $(shell uname -s)

# Compiler
CXX = g++

# Common flags
CXXFLAGS = -std=c++17 -c -fPIC -Icodec

# Platform-specific settings
ifeq ($(UNAME_S), Darwin)  # macOS
    OPENSSL_CFLAGS = -I/opt/homebrew/opt/openssl/include
    OPENSSL_LIBS   = -L/opt/homebrew/opt/openssl/lib -lssl -lcrypto
    EIGEN_CFLAGS   = -I/opt/homebrew/include
    OPENCV_CFLAGS  = -I/opt/homebrew/include/opencv4
    FFMPEG_CFLAGS  = -I/opt/homebrew/Cellar/ffmpeg/7.1.1/include
    FFMPEG_LDFLAGS = -L/opt/homebrew/Cellar/ffmpeg/7.1.1/lib
else  # Linux (e.g., inside Docker)
    OPENSSL_CFLAGS = -I/usr/include/openssl
    OPENSSL_LIBS   = -lssl -lcrypto
    EIGEN_CFLAGS   = -I/usr/include/eigen3
    OPENCV_CFLAGS  = -I/usr/include/opencv4
    FFMPEG_CFLAGS  =
    FFMPEG_LDFLAGS =
endif

PKG_CFLAGS = $(shell pkg-config --cflags libavformat libavcodec libswscale libavutil opencv4)
PKG_LIBS   = $(shell pkg-config --libs libavformat libavcodec libswscale libavutil opencv4)

CXXFLAGS += $(OPENSSL_CFLAGS) $(EIGEN_CFLAGS) $(OPENCV_CFLAGS) $(FFMPEG_CFLAGS) $(PKG_CFLAGS)
LDFLAGS = $(OPENSSL_LIBS) $(PKG_LIBS) $(FFMPEG_LDFLAGS)

# Target executable
TARGET = run

# Build dirs
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib


# Default rule
all: $(TARGET)

# Compile object files for codec modules
$(OBJ_DIR)/util.o: codec/util.cpp codec/util.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) codec/util.cpp -o $(OBJ_DIR)/util.o

$(OBJ_DIR)/compress.o: codec/compress.cpp codec/compress.h codec/util.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) codec/compress.cpp -o $(OBJ_DIR)/compress.o

$(OBJ_DIR)/decompress.o: codec/decompress.cpp codec/decompress.h codec/util.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) codec/decompress.cpp -o $(OBJ_DIR)/decompress.o

$(OBJ_DIR)/main.o: main.cpp codec/compress.h codec/decompress.h codec/util.h encryption_schemes/common.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) main.cpp -o $(OBJ_DIR)/main.o

# Compile object files for encryption schemes
$(OBJ_DIR)/common.o: encryption_schemes/common.cpp encryption_schemes/common.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) encryption_schemes/common.cpp -o $(OBJ_DIR)/common.o

$(OBJ_DIR)/scheme_1.o: encryption_schemes/scheme1/scheme_1.cpp encryption_schemes/scheme1/scheme_1.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) encryption_schemes/scheme1/scheme_1.cpp -o $(OBJ_DIR)/scheme_1.o

# Build shared libraries for codec modules
$(LIB_DIR)/libutil.so: $(OBJ_DIR)/util.o
	@mkdir -p $(LIB_DIR)
	$(CXX) -shared -o $(LIB_DIR)/libutil.so $(OBJ_DIR)/util.o -L/opt/homebrew/Cellar/ffmpeg/7.1.1/lib -lavformat -lavcodec -lswscale -lavutil

$(LIB_DIR)/libcompress.so: $(OBJ_DIR)/compress.o
	@mkdir -p $(LIB_DIR)
	$(CXX) -shared -o $(LIB_DIR)/libcompress.so $(OBJ_DIR)/compress.o -L$(LIB_DIR) -lutil -L/opt/homebrew/Cellar/ffmpeg/7.1.1/lib -lavformat -lavcodec -lswscale -lavutil

$(LIB_DIR)/libdecompress.so: $(OBJ_DIR)/decompress.o
	@mkdir -p $(LIB_DIR)
	$(CXX) -shared -o $(LIB_DIR)/libdecompress.so $(OBJ_DIR)/decompress.o -L$(LIB_DIR) -lutil -L/opt/homebrew/Cellar/ffmpeg/7.1.1/lib -lavformat -lavcodec -lswscale -lavutil

# Updated shared library for encryption schemes (link with OpenCV, OpenSSL, and FFmpeg)
$(LIB_DIR)/libencryption.so: $(OBJ_DIR)/common.o $(OBJ_DIR)/scheme_1.o 
	@mkdir -p $(LIB_DIR)
	$(CXX) -shared -o $(LIB_DIR)/libencryption.so $(OBJ_DIR)/common.o $(OBJ_DIR)/scheme_1.o $(OPENCV_LIBS) $(OPENSSL_LIBS) $(LDFLAGS)

# Link the main executable with the shared libraries (and OpenCV)
$(TARGET): $(OBJ_DIR)/main.o $(LIB_DIR)/libutil.so $(LIB_DIR)/libcompress.so $(LIB_DIR)/libdecompress.so $(LIB_DIR)/libencryption.so
	$(CXX) $(OBJ_DIR)/main.o -L$(LIB_DIR) -lutil -lcompress -ldecompress -lencryption $(OPENSSL_LIBS) $(LDFLAGS) $(OPENCV_LIBS) -o $(TARGET)


# Clean up build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
