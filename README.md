Prepares PDFs for viewing on
[Overview](https://github.com/overview/overview-server)

# Methodology

This processing step ensures a PDF has text and a thumbnail. That way,
Overview can present it.

If the input JSON's `wantSplitByPage` is `true`, this step will output one
(PDF+text+thumb) document per page and set `pageNumber`:N, where N is `1` for
the first page.

We use [PDFium](https://pdfium.googlesource.com/pdfium/) because it's fast
and it starts up quickly. (Our framework starts one process per input file,
so an out-of-memory error from one input file doesn't impact the next.)

Throughput goals: on a reasonable machine, for a small PDF...:

* Extracting text and thumbnail from a PDF should take <0.1s
* Generating a PDF per page (with text and thumbnail) should take <0.2s

# Developing

1. [Install Docker-CE](https://docs.docker.com/engine/installation/) and
   if you're on Linux, [install docker-compose](https://docs.docker.com/compose/install/#install-compose).
1. Run `docker build .`

(Useful builds: `docker build --target=test .` will compile binaries and run
unit tests. `docker build --target=production .` will produce a minimal image.)
