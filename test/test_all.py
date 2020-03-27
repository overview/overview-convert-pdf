#!/usr/bin/env python3

import io
import json
import os
import os.path
import re
import shutil
import subprocess
import unittest

import multipart

TestDir = "/tmp/test-split-and-extract-pdf"


class Fragment:
    def __init__(self, name, bytes):
        self.name = name
        self.bytes = bytes


def load_expected_fragment(test_dir, name):
    return Fragment(name, read_file_bytes("/app/test/" + test_dir + "/" + name))


def read_file_bytes(path):
    with open(path, "rb") as f:
        return f.read()


# Runs do-convert-stream-to-mime-multipart on the test case in question (named
# after a directory such as 'test-xyz') and returns (retval, stdout, stderr).
def run_test_case(dirname):
    if os.path.exists(TestDir):
        shutil.rmtree(TestDir)
    os.makedirs(TestDir)
    srcdir = "/app/test/" + dirname
    with open(srcdir + "/input.blob", "rb") as input_blob:
        completed = subprocess.run(
            [
                "/app/do-convert-stream-to-mime-multipart",
                "MIME-BOUNDARY",
                read_file_bytes(srcdir + "/input.json").decode("utf-8"),
            ],
            stdin=input_blob,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=TestDir,
        )
    return (completed.returncode, completed.stdout, completed.stderr)


def bytes_to_fragments(b):
    ret = []
    bio = io.BytesIO(b)
    parser = multipart.MultipartParser(bio, b"MIME-BOUNDARY", charset=None)

    for part in parser:
        ret.append(Fragment(part.name, part.raw))

    return ret


def normalize_pdf_bytes(b):
    b = re.sub(rb"/CreationDate\(D:[0-9]{14}\)", b"/CreationDate(D:XXXXXXXXXXXXXX)", b)
    b = re.sub(
        rb"/ID\[<[A-F0-9]{32}><[A-F0-9]{32}>\]",
        b"ID[<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX><XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX>]",
        b,
    )
    return b


def normalize_file_contents(b):
    b = normalize_pdf_bytes(b)
    b = re.sub(
        rb"\n$", b"", b
    )  # remove final newline -- our test JSON examples often have one
    return b


class TestSplitPdfAndExtractText(unittest.TestCase):
    def assertFragmentBytesEqual(self, name, expect, actual):
        # In case we're testing PDFs, "normalize" to remove stuff we can't
        # set (like creation date or randomized ID)
        expect = normalize_file_contents(expect)
        actual = normalize_file_contents(actual)
        self.assertEqual(expect, actual, "Wrong fragment contents for {}".format(name))

    def assertFragmentEqualsString(self, fragment, name, string):
        self.assertFragmentEqualsBytes(
            fragment,
            name,
            string.encode("utf-8"),
            "Wrong contents in fragment {}".format(name),
        )

    def assertFragmentEqualsFile(self, fragment, testDir, name):
        self.assertFragmentEqualsBytes(
            fragment,
            name,
            read_file_bytes(testDir + "/name"),
            "Wrong contents in fragment {}".format(name),
        )

    def _runAndGatherFragments(self, testDir):
        (retval, stdout, stderr) = run_test_case(testDir)
        self.assertEqual(
            b"", stderr, "Got error on stderr: {}".format(stderr.decode("utf-8"))
        )
        self.assertEqual(
            0,
            retval,
            "Process did not exit with status code 0. stderr was: {}".format(
                stderr.decode("utf-8")
            ),
        )
        fragments = bytes_to_fragments(stdout)
        return fragments

    def _expectFragments(self, name, expect, actual):
        expect_names = list(fragment.name for fragment in expect)
        actual_names = list(fragment.name for fragment in actual)
        self.assertEqual(expect_names, actual_names, "Incorrect output sequence")

        for expect_fragment, actual_fragment in zip(expect, actual):
            if expect_fragment.name.endswith(".json"):
                self.assertEqual(
                    json.loads(expect_fragment.bytes), json.loads(actual_fragment.bytes)
                )
            else:
                self.assertFragmentBytesEqual(
                    expect_fragment.name, expect_fragment.bytes, actual_fragment.bytes
                )

    def _testFragments(self, testDir, expect):
        fragments = self._runAndGatherFragments(testDir)
        self._expectFragments(testDir, expect, fragments)

    def test_split_and_extract_2_pages(self):
        test_dir = "test-split-and-extract-2-pages"
        self._testFragments(
            test_dir,
            [
                Fragment("progress", b'{"children":{"nProcessed":0,"nTotal":2}}'),
                load_expected_fragment(test_dir, "0.json"),
                load_expected_fragment(test_dir, "0-thumbnail.png"),
                load_expected_fragment(test_dir, "0.txt"),
                load_expected_fragment(test_dir, "0.blob"),
                Fragment("progress", b'{"children":{"nProcessed":1,"nTotal":2}}'),
                load_expected_fragment(test_dir, "1.json"),
                load_expected_fragment(test_dir, "1-thumbnail.png"),
                load_expected_fragment(test_dir, "1.txt"),
                load_expected_fragment(test_dir, "1.blob"),
                Fragment("done", b""),
            ],
        )

    def test_extract_2_pages(self):
        test_dir = "test-extract-2-pages"
        self._testFragments(
            test_dir,
            [
                load_expected_fragment(test_dir, "0.json"),
                Fragment("inherit-blob", b""),
                load_expected_fragment(test_dir, "0-thumbnail.png"),
                Fragment("progress", b'{"children":{"nProcessed":1,"nTotal":2}}'),
                load_expected_fragment(test_dir, "0.txt"),
                Fragment("done", b""),
            ],
        )

    def test_error_encrypted(self):
        test_dir = "test-error-encrypted"
        self._testFragments(
            test_dir,
            [
                load_expected_fragment(test_dir, "0.json"),
                Fragment("inherit-blob", b""),
                load_expected_fragment(test_dir, "error"),
            ],
        )

    def test_error_invalid_pdf(self):
        test_dir = "test-error-invalid-pdf"
        self._testFragments(
            test_dir,
            [
                load_expected_fragment(test_dir, "0.json"),
                Fragment("inherit-blob", b""),
                load_expected_fragment(test_dir, "error"),
            ],
        )

    def test_owner_protected_pdf(self):
        test_dir = "test-owner-protected-pdf"
        # Works like any other
        self._testFragments(
            test_dir,
            [
                load_expected_fragment(test_dir, "0.json"),
                Fragment("inherit-blob", b""),
                load_expected_fragment(test_dir, "0-thumbnail.png"),
                load_expected_fragment(test_dir, "0.txt"),
                Fragment("done", b""),
            ],
        )


if __name__ == "__main__":
    unittest.main()
