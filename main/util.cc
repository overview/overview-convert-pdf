#include <cmath>
#include <codecvt>
#include <locale>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "public/cpp/fpdf_deleters.h"
#include "public/fpdf_text.h"
#include "public/fpdfview.h"
#include "lodepng.h"

#include "util.h"

static const int MaxNUtf16CharsPerPage = 100000;
static const int MaxThumbnailDimension = 700;
static const std::vector<uint8_t> EmptyPng;

// Remove "\f" characters. This helps us conform with the spec, which places
// a "\f" before every subsequent page's info.
static void
normalizeUtf16(char16_t* buf, int n) {
  for (int i = 0; i < n; i++) {
    if (buf[i] == '\f') {
      buf[i] = ' ';
    }
  }
}

/**
 * Encodes a PNG from correctly-sized buffer of pixel data.
 *
 * This overwrites data in argbBuffer.
 */
static std::vector<uint8_t>
argbToPng(uint32_t* argbBuffer, size_t width, size_t height)
{
  // Write BGR to the same buffer, destroying it. This saves memory.
  uint8_t* bgrBuffer = reinterpret_cast<uint8_t*>(argbBuffer);

  size_t n = width * height;
  for (size_t i = 0, o = 0; i < n; i++, o += 3) {
    const uint32_t argb = argbBuffer[i];
    bgrBuffer[o + 0] = (argb >>  0) & 0xff;
    bgrBuffer[o + 1] = (argb >>  8) & 0xff;
    bgrBuffer[o + 2] = (argb >> 16) & 0xff;
  }

  std::vector<uint8_t> out;
  const unsigned int err = lodepng::encode(out, &bgrBuffer[0], width, height, LCT_RGB);
  if (err) {
    return EmptyPng;
  }
  return out;
}

/**
 * Renders the given PDF page as a PNG.
 *
 * Returns "" if rendering failed. That means out-of-memory.
 */
static std::vector<uint8_t>
renderPageThumbnailPngOrOutputErrorAndExit(FPDF_PAGE page, const std::string& mimeBoundary)
{
  double pageWidth = FPDF_GetPageWidth(page);
  double pageHeight = FPDF_GetPageHeight(page);

  int width = MaxThumbnailDimension;
  int height = MaxThumbnailDimension;
  if (pageWidth > pageHeight) {
    height = static_cast<int>(std::round(1.0 * MaxThumbnailDimension * pageHeight / pageWidth));
  } else {
    width = static_cast<int>(std::round(1.0 * MaxThumbnailDimension * pageWidth / pageHeight));
  }

  std::unique_ptr<uint32_t[]> buffer(new (std::nothrow) uint32_t[width * height]);
  if (!buffer) {
    outputErrorAndExit("out of memory when creating thumbnail", mimeBoundary);
    return EmptyPng;
  }

  FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA, &buffer[0], sizeof(uint32_t) * width);
  if (!bitmap) {
    outputErrorAndExit("unknown error while creating thumbnail", mimeBoundary);
    return EmptyPng;
  }

  // TODO investigate speedup from
  // FPDF_RENDER_NO_SMOOTHTEXT, FPDF_RENDER_NO_SMOOTHIMAGE, FPDF_RENDER_NO_SMOOTHPATH
  int flags = 0;
  FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xffffffff);
  FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, flags);
  FPDFBitmap_Destroy(bitmap);

  std::vector<uint8_t> png(argbToPng(&buffer[0], width, height));
  return png;
}

std::string
getPageTextUtf8OrOutputErrorAndExit(FPDF_PAGE fPage, const std::string& mimeBoundary)
{
  std::unique_ptr<void, FPDFTextPageDeleter> textPage(FPDFText_LoadPage(fPage));
  if (!textPage) {
    outputErrorAndExit(std::string("Failed to read text from PDF page: ") + formatLastPdfiumError(), mimeBoundary);
    return std::string();
  }

  char16_t utf16Buf[MaxNUtf16CharsPerPage];
  int nChars = FPDFText_GetText(textPage.get(), 0, MaxNUtf16CharsPerPage, reinterpret_cast<unsigned short*>(&utf16Buf[0]));
  normalizeUtf16(&utf16Buf[0], nChars);
  std::u16string u16Text(&utf16Buf[0], nChars);

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
  std::string u8Text(convert.to_bytes(u16Text));
  // [adam, 2017-12-14] pdfium tends to end its string with a nullptr byte. That
  // makes tests ugly, and it gives no value. Nix the nullptr byte.
  if (u8Text.size() > 0 && u8Text[u8Text.size() - 1] == '\0') u8Text.resize(u8Text.size() - 1);

  return u8Text;
}

void
outputPageThumbnailFragmentOrErrorAndExit(FPDF_PAGE fPage, int pageIndex, const std::string& mimeBoundary)
{
  std::vector<uint8_t> pngBytes(renderPageThumbnailPngOrOutputErrorAndExit(fPage, mimeBoundary));
  outputFragment(std::to_string(pageIndex) + "-thumbnail.png", pngBytes, mimeBoundary);
}

void
outputPageTextFragmentOrErrorAndExit(FPDF_PAGE fPage, int pageIndex, const std::string& mimeBoundary)
{
  std::string utf8(getPageTextUtf8OrOutputErrorAndExit(fPage, mimeBoundary));
  outputFragment(std::to_string(pageIndex) + ".txt", utf8, mimeBoundary);
}

void
outputBytes(const uint8_t* bytes, size_t len)
{
  while (len > 0) {
    ssize_t nWritten = write(STDOUT_FILENO, bytes, len);
    if (nWritten == -1) {
      perror("Write to stdout failed");
      exit(1);
    }
    bytes += nWritten;
    len -= nWritten;
  }
}

void
outputBytes(const std::string& bytes)
{
  outputBytes(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

void
outputBytes(const std::vector<uint8_t>& bytes)
{
  outputBytes(bytes.data(), bytes.size());
}

void
outputFragmentPrefix(const std::string& name, const std::string& mimeBoundary)
{
  const std::string& prefix = std::string("\r\n--") + mimeBoundary + "\r\nContent-Disposition: form-data; name=" + name + "\r\n\r\n";
  outputBytes(prefix);
}

void
outputFragment(const std::string& name, const std::string& bytes, const std::string& mimeBoundary)
{
  outputFragmentPrefix(name, mimeBoundary);
  outputBytes(bytes);
}

void
outputFragment(const std::string& name, const std::vector<uint8_t>& bytes, const std::string& mimeBoundary)
{
  outputFragmentPrefix(name, mimeBoundary);
  outputBytes(bytes);
}

static void
outputEnd(const std::string& mimeBoundary)
{
  const std::string closeDelimiter = std::string("\r\n--") + mimeBoundary + "--";
  outputBytes(closeDelimiter);
}

void
outputDoneAndExit(const std::string& mimeBoundary)
{
  outputFragmentPrefix("done", mimeBoundary);
  outputEnd(mimeBoundary);
  exit(0);
}

void
outputErrorAndExit(const std::string& message, const std::string& mimeBoundary)
{
  outputFragment("error", message, mimeBoundary);
  outputEnd(mimeBoundary);
  exit(0);
}

void
outputProgress(int nProcessed, int nTotal, const std::string& mimeBoundary)
{
  const std::string message = std::string("{\"children\":{\"nProcessed\":") + std::to_string(nProcessed) + ",\"nTotal\":" + std::to_string(nTotal) + "\"}}";
  outputFragment("progress", message, mimeBoundary);
}

std::string
formatLastPdfiumError()
{
  unsigned long err = FPDF_GetLastError();
  switch (err) {
    case FPDF_ERR_UNKNOWN: return "unknown error";
    case FPDF_ERR_FILE: return "file not found or could not be opened";
    case FPDF_ERR_FORMAT: return "file is not a valid PDF";
    case FPDF_ERR_PASSWORD: return "file is password-protected";
    case FPDF_ERR_SECURITY: return "unsupported security scheme";
    case FPDF_ERR_PAGE: return "page not found or content error";
    default: return std::string("unknown error: ") + std::to_string(err);
  }
}
