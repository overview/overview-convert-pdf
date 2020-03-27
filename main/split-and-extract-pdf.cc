#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>

#include "public/cpp/fpdf_deleters.h"
#include "public/fpdfview.h"
#include "public/fpdf_annot.h"
#include "public/fpdf_ppo.h"
#include "public/fpdf_save.h"
#include "public/fpdf_text.h"
#include "lodepng.h"

#include "util.h"
#include "json.hpp"

using json = nlohmann::json;

class StdoutWrite : public FPDF_FILEWRITE {
public:
  StdoutWrite() {
    FPDF_FILEWRITE::version = 1;
    FPDF_FILEWRITE::WriteBlock = WriteBlockCallback;
  }

  static int WriteBlockCallback(FPDF_FILEWRITE* pFileWrite, const void* data, unsigned long size) {
    outputBytes(reinterpret_cast<const uint8_t*>(data), size); // or crash!
    return size; // non-zero
  }
};

/**
 * Outputs the page as an entire PDF.
 */
static void
outputPageBlobFragment(
    FPDF_DOCUMENT fDocument,
    FPDF_PAGE fPage,
    int pageIndex,
    const std::string& mimeBoundary
)
{
  outputFragmentPrefix(std::to_string(pageIndex) + ".blob", mimeBoundary);

  std::unique_ptr<void, FPDFDocumentDeleter> outDocument(FPDF_CreateNewDocument());
  std::string pageIndexString = std::to_string(pageIndex + 1);
  if (!FPDF_ImportPages(outDocument.get(), fDocument, pageIndexString.c_str(), 0)) {
    outputErrorAndExit(std::string("Error outputting page with index ") + std::to_string(pageIndex) + ": " + formatLastPdfiumError(), mimeBoundary);
    return;
  }

  StdoutWrite write;
  FPDF_SaveAsCopy(outDocument.get(), &write, FPDF_REMOVE_SECURITY);
}

static void
splitAndExtractPdf(
    const char* filename,
    const std::string& mimeBoundary,
    const std::string& jsonTemplate
)
{
  FPDF_STRING fFilename(filename);
  std::unique_ptr<void, FPDFDocumentDeleter> fDocument(FPDF_LoadDocument(fFilename, nullptr));
  if (!fDocument) {
    outputErrorAndExit(std::string("Failed to open PDF: ") + formatLastPdfiumError(), mimeBoundary);
    return;
  }

  json pageJson = json::parse(jsonTemplate);
  addDocumentMetadataFromPdf(pageJson["metadata"], fDocument.get());

  const int nPages = FPDF_GetPageCount(fDocument.get());

  for (int pageIndex = 0; pageIndex < nPages; ++pageIndex) {
    // in-between: progress (should come immediately before JSON)
    outputProgress(pageIndex, nPages, mimeBoundary);

    // Load page
    std::unique_ptr<void, FPDFPageDeleter> fPage(FPDF_LoadPage(fDocument.get(), pageIndex));
    if (!fPage) {
      outputErrorAndExit(std::string("Failed to read PDF page: ") + formatLastPdfiumError(), mimeBoundary);
      return;
    }

    // 1. JSON (must come first)
    pageJson["metadata"]["pageNumber"] = pageIndex + 1;
    const std::string jsonName(std::to_string(pageIndex) + ".json");
    outputFragment(jsonName, pageJson.dump(), mimeBoundary);

    // 2. Thumbnail
    outputPageThumbnailFragmentOrErrorAndExit(fPage.get(), pageIndex, mimeBoundary);

    // 3. Text
    outputPageTextFragmentOrErrorAndExit(fPage.get(), pageIndex, mimeBoundary);

    // 4. Blob
    outputPageBlobFragment(fDocument.get(), fPage.get(), pageIndex, mimeBoundary);
  }
}

int
main(int argc, char** argv)
{
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " MIME-BOUNDARY JSON-TEMPLATE" << std::endl
              << std::endl
              << "JSON-TEMPLATE will be emitted for each page; its metadata.pageNumber will "
              << "be a page number starting with 1." << std::endl;

    return 1;
  }

  const std::string mimeBoundary(argv[1]);
  const std::string jsonTemplate(argv[2]);

  FPDF_InitLibrary();

  splitAndExtractPdf("input.blob", mimeBoundary, jsonTemplate);

  outputDoneAndExit(mimeBoundary);

  // Never reached:
  FPDF_DestroyLibrary();
  return 0;
}
