#include <codecvt>
#include <locale>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <string>

#include "public/cpp/fpdf_deleters.h"
#include "public/fpdfview.h"
#include "public/fpdf_ppo.h"
#include "json.hpp"

#include "util.h"

static void
extractPdf(const char* filename, const std::string& inputJson, const std::string& mimeBoundary)
{
  FPDF_STRING fFilename(filename);
  std::unique_ptr<void, FPDFDocumentDeleter> fDocument(FPDF_LoadDocument(fFilename, nullptr));
  if (!fDocument) {
    outputErrorAndExit(std::string("Failed to open PDF: ") + formatLastPdfiumError(), mimeBoundary);
    return;
  }

  nlohmann::json jsonData = nlohmann::json::parse(inputJson);
  addDocumentMetadataFromPdf(jsonData["metadata"], fDocument.get());
  outputFragment("0.json", jsonData.dump(), mimeBoundary);
  outputFragment("inherit-blob", "", mimeBoundary);

  const int nPages = FPDF_GetPageCount(fDocument.get());
  std::vector<std::string> pageTexts;
  pageTexts.reserve(nPages);

  // Page 1: output thumbnail, collect text
  std::unique_ptr<void, FPDFPageDeleter> fPage(FPDF_LoadPage(fDocument.get(), 0));
  outputPageThumbnailFragmentOrErrorAndExit(fPage.get(), 0, mimeBoundary);
  pageTexts.push_back(getPageTextUtf8OrOutputErrorAndExit(fPage.get(), mimeBoundary));

  // Pages 2-n: collect text, reporting progress along the way
  for (int pageIndex = 1; pageIndex < nPages; pageIndex++) {
    outputProgress(pageIndex, nPages, mimeBoundary);
    fPage.reset(FPDF_LoadPage(fDocument.get(), pageIndex));
    pageTexts.push_back(getPageTextUtf8OrOutputErrorAndExit(fPage.get(), mimeBoundary));
  }

  // Output text
  outputFragmentPrefix("0.txt", mimeBoundary);
  outputBytes(pageTexts[0]);
  for (auto it = ++pageTexts.cbegin(); it != pageTexts.cend(); ++it) {
    // pages 2-n: prepend "\f"
    outputBytes("\f");
    outputBytes(*it);
  }
}

int
main(int argc, char** argv)
{
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " MIME-BOUNDARY JSON" << std::endl
              << std::endl
              << "JSON will be emitted as-is." << std::endl;

    return 1;
  }

  const std::string mimeBoundary = argv[1];
  const std::string inputJson = argv[2];

  FPDF_InitLibrary();
  extractPdf("input.blob", inputJson, mimeBoundary);

  outputDoneAndExit(mimeBoundary);

  // Never reached:
  FPDF_DestroyLibrary();
  return 0;
}
