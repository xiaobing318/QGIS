"""Regression tests for the bounded GDAL statistics input-cache exception.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from qcopilots_common import processing_input_cache as cache


def pam(values=None, *, band=1, semantic="", metadata="", band_attributes=""):
    if values is None:
        values = {
            "STATISTICS_MINIMUM": "10",
            "STATISTICS_MAXIMUM": "30",
            "STATISTICS_MEAN": "20",
            "STATISTICS_STDDEV": "5",
            "STATISTICS_VALID_PERCENT": "100",
        }
    fields = "".join(f'<MDI key="{key}">{value}</MDI>' for key, value in values.items())
    return (
        f'<PAMDataset>{semantic}<PAMRasterBand band="{band}"{band_attributes}>'
        f"<Metadata>{fields}{metadata}</Metadata></PAMRasterBand></PAMDataset>"
    )


class TestProcessingInputCache(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "terrain.tif.aux.xml"

    def write(self, text, *, band_count=1):
        self.path.write_text(text, encoding="utf-8")
        return cache.capture_input_cache(self.path, band_count=band_count)

    def test_absent_sidecar_accepts_only_statistics_and_reports_fields(self):
        before = cache.capture_input_cache(self.path, band_count=1)
        after = self.write(pam())
        changes = cache.compare_input_cache(before, after)
        self.assertIsNotNone(changes)
        self.assertEqual(changes[0]["band"], 1)
        self.assertEqual(set(changes[0]["fields"]), cache.PAM_STATISTICS_KEYS)
        self.assertEqual(
            after["file_sha256"], hashlib.sha256(self.path.read_bytes()).hexdigest()
        )
        self.assertEqual(after["file_signature"]["size"], self.path.stat().st_size)
        json.dumps(after)

    def test_statistics_values_may_change_but_semantics_must_not(self):
        before = self.write(pam(semantic="<SRS>EPSG:32650</SRS>"))
        after = self.write(
            pam(
                {
                    "STATISTICS_MINIMUM": "10",
                    "STATISTICS_MAXIMUM": "40",
                    "STATISTICS_MEAN": "21",
                    "STATISTICS_STDDEV": "6",
                    "STATISTICS_VALID_PERCENT": "98",
                },
                semantic="<SRS>EPSG:32650</SRS>",
            )
        )
        self.assertIsNotNone(cache.compare_input_cache(before, after))
        for semantic in (
            "<SRS>EPSG:4326</SRS>",
            "<SRS>EPSG:32650</SRS><GeoTransform>0,2,0,0,0,-2</GeoTransform>",
            "<SRS>EPSG:32650</SRS><Unknown>new</Unknown>",
        ):
            with self.subTest(semantic=semantic):
                current = self.write(pam(semantic=semantic))
                self.assertIsNone(cache.compare_input_cache(before, current))

    def test_absent_sidecar_cannot_add_semantic_content(self):
        before = cache.capture_input_cache(self.path, band_count=1)
        for extra in (
            "<SRS>EPSG:4326</SRS>",
            "<GeoTransform>0,1,0,0,0,-1</GeoTransform>",
            "<Metadata><MDI key=\"other\">x</MDI></Metadata>",
        ):
            with self.subTest(extra=extra):
                self.assertIsNone(
                    cache.compare_input_cache(before, self.write(pam(semantic=extra)))
                )
        for text in (
            pam(metadata='<MDI key="STATISTICS_APPROXIMATE">YES</MDI>'),
            pam(band_attributes=' colorInterp="Gray"'),
            pam().replace(
                "</PAMRasterBand>", "<NoDataValue>-9999</NoDataValue></PAMRasterBand>"
            ),
        ):
            self.assertIsNone(cache.compare_input_cache(before, self.write(text)))

    def test_statistics_deletion_is_not_permitted(self):
        before = self.write(pam())
        after = self.write(pam({"STATISTICS_MINIMUM": "10"}))
        self.assertIsNone(cache.compare_input_cache(before, after))
        self.path.unlink()
        self.assertIsNone(
            cache.compare_input_cache(before, cache.capture_input_cache(self.path, 1))
        )

    def test_existing_empty_pam_wrappers_can_receive_statistics(self):
        for empty in (
            "<PAMDataset />",
            '<PAMDataset><PAMRasterBand band="1" /></PAMDataset>',
            '<PAMDataset><PAMRasterBand band="1">'
            "<Metadata /></PAMRasterBand></PAMDataset>",
        ):
            with self.subTest(empty=empty):
                before = self.write(empty)
                self.assertIsNotNone(
                    cache.compare_input_cache(before, self.write(pam()))
                )

    def test_non_statistics_whitespace_remains_semantic(self):
        old = {"STATISTICS_MINIMUM": "10"}
        new = {"STATISTICS_MINIMUM": "11"}
        before = self.write(pam(old, metadata='<MDI key="OTHER"></MDI>'))
        after = self.write(pam(new, metadata='<MDI key="OTHER"> </MDI>'))
        self.assertIsNone(cache.compare_input_cache(before, after))
        before = self.write(pam(old, semantic="<Unknown><Child /></Unknown>"))
        after = self.write(pam(new, semantic="<Unknown><Child /> </Unknown>"))
        self.assertIsNone(cache.compare_input_cache(before, after))

    def test_unchanged_snapshot_and_non_statistics_touch(self):
        snapshot = self.write(pam())
        self.assertEqual(cache.compare_input_cache(snapshot, snapshot), [])
        with mock.patch.object(cache, "_stat_signature", wraps=cache._stat_signature):
            self.assertEqual(
                cache.compare_input_cache(
                    snapshot, cache.capture_input_cache(self.path, 1)
                ),
                [],
            )
        os.utime(
            self.path,
            ns=(self.path.stat().st_atime_ns, self.path.stat().st_mtime_ns + 1000000000),
        )
        self.assertIsNone(
            cache.compare_input_cache(snapshot, cache.capture_input_cache(self.path, 1))
        )

    def test_real_band_count_is_required(self):
        before = cache.capture_input_cache(self.path)
        after = self.write(pam(), band_count=None)
        self.assertIsNone(cache.compare_input_cache(before, after))
        self.assertIsNone(self.write(pam(band=2), band_count=1))
        for count in (True, 0, -1, "1"):
            with self.subTest(count=count):
                self.assertIsNone(cache.capture_input_cache(self.path, count))
        for band in (0, -1, "01", "1.0", "one"):
            with self.subTest(band=band):
                self.assertIsNone(self.write(pam(band=band), band_count=2))

    def test_multiple_bands_are_tracked_independently(self):
        before = cache.capture_input_cache(self.path, band_count=2)
        contents = pam().replace(
            "</PAMDataset>", pam(band=2).removeprefix("<PAMDataset>")
        )
        current = self.write(contents, band_count=2)
        accepted = cache.compare_input_cache(before, current)
        self.assertEqual([item["band"] for item in accepted], [1, 2])
        self.assertIsNone(
            cache.compare_input_cache(current, self.write(pam(), band_count=2))
        )

    def test_non_default_metadata_domains_are_not_statistics_exceptions(self):
        before = cache.capture_input_cache(self.path, band_count=1)
        current = self.write(pam().replace("<Metadata>", '<Metadata domain="other">'))
        self.assertIsNone(cache.compare_input_cache(before, current))
        current = self.write(
            "<PAMDataset><Metadata>"
            '<MDI key="STATISTICS_MINIMUM">1</MDI>'
            "</Metadata></PAMDataset>"
        )
        self.assertIsNone(cache.compare_input_cache(before, current))

    def test_invalid_statistics_are_exact_only(self):
        invalid = (
            {"STATISTICS_MINIMUM": "NaN"},
            {"STATISTICS_MAXIMUM": "inf"},
            {"STATISTICS_MEAN": "1e999"},
            {"STATISTICS_STDDEV": "-1"},
            {"STATISTICS_VALID_PERCENT": "101"},
            {"STATISTICS_VALID_PERCENT": "-0.1"},
            {"STATISTICS_MINIMUM": "20", "STATISTICS_MAXIMUM": "10"},
            {"STATISTICS_MINIMUM": "10", "STATISTICS_MEAN": "9"},
            {"STATISTICS_MAXIMUM": "10", "STATISTICS_MEAN": "11"},
            {"STATISTICS_MEAN": ""},
            {"STATISTICS_MEAN": "1_000"},
            {"STATISTICS_MEAN": "١٠"},
            {"STATISTICS_MAXIMUM": "1", "STATISTICS_MEAN": "1.00000000000000001"},
        )
        for values in invalid:
            with self.subTest(values=values):
                self.assertIsNone(self.write(pam(values)))

    def test_duplicate_or_ambiguous_metadata_is_exact_only(self):
        valid = pam()
        invalid = (
            valid.replace(
                "</Metadata>", '<MDI key="STATISTICS_MINIMUM">9</MDI></Metadata>'
            ),
            valid.replace("</PAMRasterBand>", "<Metadata /></PAMRasterBand>"),
            valid.replace("</PAMDataset>", '<PAMRasterBand band="1" /></PAMDataset>'),
            valid.replace('key="STATISTICS_MEAN"', 'key="STATISTICS_MEAN" extra="yes"'),
            valid.replace('>20</MDI>', '>20<Extra /></MDI>'),
            valid.replace('</MDI>', '</MDI>mixed', 1),
        )
        for text in invalid:
            with self.subTest(text=text):
                self.assertIsNone(self.write(text))

    def test_dtd_entities_comments_pi_and_encoding_are_exact_only(self):
        invalid = (
            '<!DOCTYPE PAMDataset [<!ENTITY x "10">]>'
            + pam().replace(">10</MDI>", ">&x;</MDI>"),
            pam().replace("<PAMDataset>", "<PAMDataset><!-- comment -->"),
            pam().replace("<PAMDataset>", "<PAMDataset><?instruction value?>"),
            "<!-- comment -->" + pam(),
            pam() + "<?instruction value?>",
            '<?xml version="1.0" encoding="ISO-8859-1"?>' + pam(),
            pam().replace("<PAMDataset>", '<PAMDataset xmlns="urn:unknown">'),
            "",
            "<PAMDataset>",
        )
        for text in invalid:
            with self.subTest(text=text):
                self.assertIsNone(self.write(text))
        self.path.write_bytes(pam().encode("utf-16"))
        self.assertIsNone(cache.capture_input_cache(self.path, 1))

    def test_xml_budgets_are_enforced(self):
        with mock.patch.object(cache, "MAX_PAM_BYTES", 64):
            self.assertIsNone(self.write(pam()))
        with mock.patch.object(cache, "MAX_PAM_NODES", 3):
            self.assertIsNone(self.write(pam()))
        with mock.patch.object(cache, "MAX_PAM_DEPTH", 2):
            self.assertIsNone(self.write(pam()))

    def test_stable_read_rejects_a_different_open_file_identity(self):
        self.write(pam())
        actual = os.fstat

        def changed(fd):
            signature = actual(fd)
            values = {
                name: getattr(signature, name)
                for name in (
                    "st_size", "st_mtime_ns", "st_ctime_ns",
                    "st_dev", "st_ino", "st_mode",
                )
            }
            values["st_ino"] += 1
            return SimpleNamespace(**values)

        with mock.patch.object(cache.os, "fstat", side_effect=changed):
            self.assertIsNone(cache.capture_input_cache(self.path, 1))

    def test_stable_read_rejects_changes_during_the_read(self):
        self.write(pam())
        actual = os.fstat
        calls = 0

        def changed_after_read(fd):
            nonlocal calls
            calls += 1
            if calls == 2:
                metadata = self.path.stat()
                os.utime(
                    self.path,
                    ns=(metadata.st_atime_ns, metadata.st_mtime_ns + 1000000000),
                )
            return actual(fd)

        with mock.patch.object(cache.os, "fstat", side_effect=changed_after_read):
            self.assertIsNone(cache.capture_input_cache(self.path, 1))

    def test_only_aux_xml_regular_files_are_candidates(self):
        other = self.path.with_name("terrain.tif")
        other.write_text(pam(), encoding="utf-8")
        self.assertIsNone(cache.capture_input_cache(other, 1))
        self.path.mkdir()
        self.assertIsNone(cache.capture_input_cache(self.path, 1))


if __name__ == "__main__":
    unittest.main()
