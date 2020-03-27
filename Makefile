CXX = clang++
LD = $(CXX)
CXXFLAGS = -Wall -std=c++11 -stdlib=libc++ -I/usr/include/pdfium -O2
LDFLAGS = -Wall -std=c++11 -stdlib=libc++ -static -lm -pthread -lpdfium -O2

all: split-and-extract-pdf extract-pdf

main/%.o : main/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

main/split-and-extract-pdf.o : main/util.h

main/extract-pdf.o : main/util.h

split-and-extract-pdf: main/lodepng.o main/split-and-extract-pdf.o main/util.o
	$(LD) $^ $(LDFLAGS) -o $@

extract-pdf: main/lodepng.o main/extract-pdf.o main/util.o
	$(LD) $^ $(LDFLAGS) -o $@

clean:
	rm -f main/*.o split-and-extract-pdf extract-pdf
