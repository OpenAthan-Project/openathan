import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("audio_image", ROOT / "tools/audio_image.py")
audio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audio)


class AudioImageTests(unittest.TestCase):
    def setUp(self):
        self.image, self.manifest = audio.build_image(b"ID3-normal-data", b"ID3-fajr-data")

    def validate_cpp(self, image):
        validator = os.environ.get("AUDIO_VALIDATOR", str(ROOT / "build/audio_validator"))
        self.assertTrue(Path(validator).is_file(), "Build the C++ audio_validator before running these tests")
        with tempfile.NamedTemporaryFile() as handle:
            handle.write(image)
            handle.flush()
            return subprocess.run([validator, handle.name], check=False).returncode

    @staticmethod
    def rehash_header(image):
        image[112:144] = hashlib.sha256(image[:112]).digest()
        return image

    def test_deterministic_and_unchanged_payloads(self):
        self.assertEqual(audio.build_image(b"ID3-normal-data", b"ID3-fajr-data"), (self.image, self.manifest))
        self.assertEqual(len(self.image), audio.PARTITION_SIZE)
        for track, payload in zip(self.manifest["tracks"], (b"ID3-normal-data", b"ID3-fajr-data")):
            self.assertEqual(self.image[track["offset"]:track["offset"] + track["length"]], payload)
        self.assertEqual(self.validate_cpp(self.image), 0)

    def test_missing_empty_and_overflow(self):
        with self.assertRaises(FileNotFoundError):
            audio.read_recording(Path("/nonexistent-openathan-recording.mp3"))
        for normal, fajr in ((b"", b"x"), (b"x", b""), (b"x" * audio.PARTITION_SIZE, b"y")):
            with self.assertRaises(ValueError): audio.build_image(normal, fajr)

    def test_corrupt_header_and_payload(self):
        for offset, expected in ((0, 1), (20, 2), (112, 2), (4096, 4)):
            with self.subTest(offset=offset):
                image = bytearray(self.image)
                image[offset] ^= 1
                self.assertEqual(self.validate_cpp(image), expected)

    def test_bounds_ids_codec_and_overlap(self):
        # Recompute metadata SHA so structural validation is exercised directly.
        for offset, value in ((8, 0xFFFFFFFF), (16, 2), (20, 0xFFFFFFF0), (24, 0xFFFFFFFF),
                              (24, 0), (28, 99), (64, 1), (68, 4096), (68, 4097)):
            with self.subTest(offset=offset, value=value):
                image = bytearray(self.image)
                struct.pack_into("<I", image, offset, value)
                self.assertEqual(self.validate_cpp(self.rehash_header(image)), 3)

    def test_truncated_or_erased_storage(self):
        self.assertEqual(self.validate_cpp(b""), 1)
        self.assertEqual(self.validate_cpp(b"\xff" * 4096), 1)
        self.assertEqual(self.validate_cpp(self.image[:4096]), 3)

    def test_partition_contract(self):
        audio.validate_partitions()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "partitions.csv"
            for bad in ("0x210000, 0x200000", "0x410000, 0x380000"):
                path.write_text(audio.PARTITIONS.read_text().replace(bad, "0x10000, 0x800000"))
                with self.assertRaises(ValueError): audio.validate_partitions(path)

    def test_adapter_failures_and_persistent_playback_objects(self):
        adapter = os.environ.get("AUDIO_ADAPTER_TESTS", str(ROOT / "build/audio_adapter_tests"))
        self.assertTrue(Path(adapter).is_file(), "Build audio_adapter_tests first")
        with tempfile.NamedTemporaryFile() as handle:
            handle.write(self.image)
            handle.flush()
            subprocess.run([adapter, handle.name], check=True)


if __name__ == "__main__":
    unittest.main()
