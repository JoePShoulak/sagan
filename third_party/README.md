# Third-party software

## UniAlgo

Sagan vendors the header-only portion of
[UniAlgo](https://github.com/uni-algo/uni-algo) at commit
`6c091fa266ac03a852128429af3b7481cf50a0ab`. It provides portable Unicode NFC
normalization for identifiers.

UniAlgo is distributed under public-domain and MIT terms. See
[`uni-algo/LICENSE.md`](uni-algo/LICENSE.md).

Sagan's generated XID and emoji-property tables come from the Unicode Character
Database and are regenerated with `scripts/update_unicode_tables.sh`. The
Unicode data-file license is preserved in [`unicode/LICENSE.txt`](unicode/LICENSE.txt).
