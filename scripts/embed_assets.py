Import("env")

from pathlib import Path
import gzip


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
DATA_DIR = PROJECT_DIR / "data"
INCLUDE_DIR = PROJECT_DIR / "include" / "generated"
SRC_DIR = PROJECT_DIR / "src" / "generated"
HEADER_PATH = INCLUDE_DIR / "embedded_assets.h"
SOURCE_PATH = SRC_DIR / "embedded_assets.cpp"

TEXT_ASSET_SUFFIXES = {".html", ".css", ".js", ".svg"}
CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
    ".png": "image/png",
}
PUBLIC_ASSET_PATHS = {
    "/app.css",
    "/app.js",
    "/favicon.ico",
    "/logo.svg",
}


def to_symbol(path: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in path.strip("/")).upper()


def to_hex_lines(data: bytes, indent: str = "  ", width: int = 12) -> str:
    if not data:
        return indent + "0x00"
    parts = [f"0x{byte:02x}" for byte in data]
    lines = []
    for idx in range(0, len(parts), width):
        lines.append(indent + ", ".join(parts[idx:idx + width]))
    return ",\n".join(lines)


def build_embedded_assets(*_args, **_kwargs) -> None:
    INCLUDE_DIR.mkdir(parents=True, exist_ok=True)
    SRC_DIR.mkdir(parents=True, exist_ok=True)

    assets = []
    if DATA_DIR.exists():
        for path in sorted(DATA_DIR.rglob("*")):
            if not path.is_file() or path.suffix == ".gz":
                continue
            rel_path = "/" + path.relative_to(DATA_DIR).as_posix()
            raw_data = path.read_bytes()
            gzip_data = gzip.compress(raw_data, compresslevel=9, mtime=0) if path.suffix in TEXT_ASSET_SUFFIXES else b""
            assets.append({
                "path": rel_path,
                "symbol": to_symbol(rel_path),
                "content_type": CONTENT_TYPES.get(path.suffix, "application/octet-stream"),
                "public": rel_path in PUBLIC_ASSET_PATHS,
                "raw_data": raw_data,
                "gzip_data": gzip_data,
            })

    header = """#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct EmbeddedAsset {
  const char* path;
  const char* contentType;
  const uint8_t* rawData;
  size_t rawLength;
  const uint8_t* gzipData;
  size_t gzipLength;
  bool publicAsset;
};

extern const EmbeddedAsset kEmbeddedAssets[];
extern const size_t kEmbeddedAssetCount;
"""

    source_lines = [
        '#include "generated/embedded_assets.h"',
        "",
    ]

    for asset in assets:
        source_lines.extend([
            f'static const uint8_t ASSET_RAW_{asset["symbol"]}[] PROGMEM = {{',
            to_hex_lines(asset["raw_data"]),
            "};",
            "",
        ])
        if asset["gzip_data"]:
            source_lines.extend([
                f'static const uint8_t ASSET_GZIP_{asset["symbol"]}[] PROGMEM = {{',
                to_hex_lines(asset["gzip_data"]),
                "};",
                "",
            ])

    source_lines.append("const EmbeddedAsset kEmbeddedAssets[] = {")
    for asset in assets:
        gzip_ref = f'ASSET_GZIP_{asset["symbol"]}'
        gzip_len = str(len(asset["gzip_data"]))
        if not asset["gzip_data"]:
            gzip_ref = "nullptr"
            gzip_len = "0"
        source_lines.append(
            '  {{ "{path}", "{content_type}", ASSET_RAW_{symbol}, {raw_len}, {gzip_ref}, {gzip_len}, {public_asset} }},'.format(
                path=asset["path"],
                content_type=asset["content_type"],
                symbol=asset["symbol"],
                raw_len=len(asset["raw_data"]),
                gzip_ref=gzip_ref,
                gzip_len=gzip_len,
                public_asset="true" if asset["public"] else "false",
            )
        )
    source_lines.extend([
        "};",
        "",
        f"const size_t kEmbeddedAssetCount = {len(assets)};",
        "",
    ])

    HEADER_PATH.write_text(header, encoding="utf-8")
    SOURCE_PATH.write_text("\n".join(source_lines), encoding="utf-8")


build_embedded_assets()
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", build_embedded_assets)
