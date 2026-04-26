Import("env")

from pathlib import Path
import gzip
import shutil


DATA_DIR = Path(env.subst("$PROJECT_DIR")) / "data"
TEXT_ASSET_SUFFIXES = {".html", ".css", ".js", ".svg"}


def gzip_asset(src: Path) -> None:
    dst = src.with_name(src.name + ".gz")
    with src.open("rb") as source, gzip.open(dst, "wb", compresslevel=9) as zipped:
        shutil.copyfileobj(source, zipped)


def build_gzip_assets(*_args, **_kwargs) -> None:
    if not DATA_DIR.exists():
        return

    expected = set()
    for path in DATA_DIR.iterdir():
        if not path.is_file() or path.suffix not in TEXT_ASSET_SUFFIXES:
            continue
        gzip_asset(path)
        expected.add(path.with_name(path.name + ".gz"))

    for stale in DATA_DIR.glob("*.gz"):
        if stale not in expected:
            stale.unlink()


build_gzip_assets()
env.AddPreAction("$BUILD_DIR/spiffs.bin", build_gzip_assets)
