"""Validate and render Sagan's per-page documentation status."""

from __future__ import annotations

from datetime import date
import re
from typing import Any


ALLOWED_STATUSES = {
    "work-in-progress",
    "review-needed",
    "publication-ready",
}


def _require(meta: dict[str, Any], key: str, page_path: str) -> Any:
    if key not in meta:
        raise ValueError(f"{page_path}: required front-matter field {key!r} is missing")
    return meta[key]


def on_page_markdown(markdown: str, page: Any, config: Any, files: Any) -> str:
    del files

    path = page.file.src_uri
    status = _require(page.meta, "status", path)
    publication_ready = _require(page.meta, "publication_ready", path)
    verified_in = _require(page.meta, "verified_in", path)
    verified_on = _require(page.meta, "verified_on", path)
    verified_by = _require(page.meta, "verified_by", path)

    if status not in ALLOWED_STATUSES:
        allowed = ", ".join(sorted(ALLOWED_STATUSES))
        raise ValueError(f"{path}: status must be one of: {allowed}")
    if not isinstance(publication_ready, bool):
        raise ValueError(f"{path}: publication_ready must be true or false")

    documentation = config.extra["documentation"]
    docs_version = documentation["version"]
    channel = documentation["channel"]
    is_ready = status == "publication-ready"
    if publication_ready != is_ready:
        raise ValueError(
            f"{path}: publication_ready must be true exactly when status is publication-ready"
        )

    if is_ready:
        version_matches = verified_in == docs_version
        targets_release = channel == "experimental" and isinstance(verified_in, str) and bool(
            re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", verified_in)
        )
        if not version_matches and not targets_release:
            raise ValueError(
                f"{path}: publication-ready pages must be verified_in {docs_version!r}"
                " or target a semantic release from the experimental channel"
            )
        if not verified_by or not isinstance(verified_by, str):
            raise ValueError(f"{path}: publication-ready pages require verified_by")
        if not verified_on:
            raise ValueError(f"{path}: publication-ready pages require verified_on")
        try:
            date.fromisoformat(str(verified_on))
        except ValueError as error:
            raise ValueError(f"{path}: verified_on must use YYYY-MM-DD") from error
        notice = (
            '!!! success "Publication-ready documentation"\n'
            f"    Confirmed by **{verified_by}** on **{verified_on}** "
            f"for documentation version **{verified_in}**.\n\n"
        )
    else:
        if any(value is not None for value in (verified_in, verified_on, verified_by)):
            raise ValueError(
                f"{path}: non-ready pages must leave verified fields null"
            )
        label = "Review needed" if status == "review-needed" else "Work in progress"
        notice = (
            f'!!! warning "Experimental documentation — {label}"\n'
            "    This page has not been confirmed as accurate or approved for publication. "
            f"Current documentation version: **{docs_version}**.\n\n"
        )

    return notice + markdown
