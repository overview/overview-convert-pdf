#include <string>
#include <vector>
#include "json.hpp"
#include "public/fpdfview.h"

/**
 * Utility functions built for spitting MIME form-data parts that map to
 * Overview StepOutputFragment "fragments".
 *
 * Overview expects a sequence of messages, in MIME multipart format.
 * See https://tools.ietf.org/html/rfc7578#section-4 for how MIME works.
 *
 * Overview expects a sequence of fragments named:
 *
 * * 0.json (JSON representing child 0; input is supplied on cmdline)
 * * 0.txt (utf-8 text representing child 0)
 * * 0-thumbnail.png (PNG representing child 0; .jpg is an alternative)
 * * 0.blob (bytes of the page) or inherit-blob (empty)
 * * progress: {"children":{"nProcessed":1,"nTotal":4}}
 * * ... repeat if outputting multiple pages
 * * done (empty) or error (with message as text)
 *
 * ... and then the multipart close delimiter.
 */

/**
 * Calculate valid UTF-8 text representing the page's contents.
 */
std::string
getPageTextUtf8OrOutputErrorAndExit(
    FPDF_PAGE fPage,
    const std::string& mimeBoundary
);

/**
 * Outputs the page's thumbnail fragment to stdout.
 *
 * If PDF is invalid or there's no space in memory for the image buffer, outputs
 * an "error" fragment and exits.
 */
void
outputPageThumbnailFragmentOrErrorAndExit(
  FPDF_PAGE fPage,
  int pageIndex,
  const std::string& mimeBoundary
);

/**
 * Outputs the page's UTF-8 text as a text fragment to stdout.
 *
 * If PDF is invalid, outputs an "error" fragment and exits.
 */
void
outputPageTextFragmentOrErrorAndExit(
  FPDF_PAGE fPage,
  int pageIndex,
  const std::string& mimeBoundary
);

/**
 * Low-level: writes a buffer to stdout or crashes.
 */
void
outputBytes(
  const uint8_t* bytes,
  size_t len
);

/**
 * Low-level: writes a buffer to stdout or crashes.
 */
void
outputBytes(
  const std::string& bytes
);

/**
 * Low-level: writes a buffer to stdout or crashes.
 */
void
outputBytes(
  const std::vector<uint8_t>& bytes
);

/**
 * Outputs the "prefix" of a fragment: a MIME delimiter with its name.
 *
 * After calling this, outputBytes() with the fragment's contents.
 */
void
outputFragmentPrefix(
  const std::string& name,
  const std::string& mimeBoundary
);

/**
 * Convenience method to outputFragmentPrefix() and then outputBytes().
 */
void
outputFragment(
  const std::string& name,
  const std::string& bytes,
  const std::string& mimeBoundary
);

/**
 * Convenience method to outputFragmentPrefix() and then outputBytes().
 */
void
outputFragment(
  const std::string& name,
  const std::vector<uint8_t>& bytes,
  const std::string& mimeBoundary
);

/**
 * Outputs an empty "done" fragment and exits.
 *
 * Yes, that's right: exit. After Overview receives a "done" fragment, it will
 * ignore all further output. There's no point in outputting anything else, and
 * any non-zero status code will be ignored. No good can come from *not*
 * exiting after outputting "done", so we make it non-optional.
 */
void
outputDoneAndExit(
  const std::string& mimeBoundary
);

/**
 * Outputs an empty "done" fragment and exits.
 *
 * Yes, that's right: exit. After Overview receives a "done" fragment, it will
 * ignore all further output. There's no point in outputting anything else, and
 * any non-zero status code will be ignored. No good can come from *not*
 * exiting after outputting "done", so we make it non-optional.
 */
void
outputErrorAndExit(
  const std::string& message,
  const std::string& mimeBoundary
);

/**
 * Outputs a "progress" fragment.
 */
void
outputProgress(
  int nProcessed,
  int nTotal,
  const std::string& mimeBoundary
);

/**
 * Converts Pdfium's global "error" variable to a string for error reporting.
 */
std::string
formatLastPdfiumError();

/**
 * Reads metadata (if set) from document and adds to `metadata`.
 *
 * Any of the following keys may be added.
 *
 *   * Title
 *   * Author
 *   * Subject
 *   * Keywords
 *   * Creation Date
 *   * Modification Date
 *
 * All values will be truncated to 500 bytes of UTF-16.
 *
 * 0-byte metadata values will be ignored.
 *
 * If a key is already set in `metadata`, it will not be read from the PDF.
 */
void
addDocumentMetadataFromPdf(nlohmann::json& metadata, FPDF_DOCUMENT fDocument);
