# Seekline

[![CI](https://github.com/alandoure005/seekline-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/alandoure005/seekline-cpp/actions/workflows/ci.yml)

A C++17 command-line document search engine using an inverted index and BM25 ranking. It searches local text files and emits structured JSON for scripting.

## Quick start

Requires a C++17 compiler, Make, and Python 3.10+ for tests.

```sh
make test
./build/seekline examples idempotency
./build/seekline examples inventory orders
```

All distinct query terms must occur in a matching document. ASCII matching is case-insensitive. The JSON response contains the indexed document count, skipped-file count, and ranked matches with relative paths and scores. Equal scores are ordered by path for repeatability.

## Index and ranking

```text
text files -> ASCII tokens -> term/document frequency postings
query terms -> postings intersection -> BM25 scores -> ranked JSON
```

For each query term, the score uses BM25 with `k1=1.2`, `b=0.75`, and `idf=log(1+(N-df+0.5)/(df+0.5))`. Document length normalization prevents long documents from dominating solely through repetition. Duplicate query terms do not multiply scores.

The ordered posting maps keep the implementation inspectable. Index construction processes each token with ordered-map lookups; query work traverses the selected postings and sorts matching documents. The index is rebuilt on every invocation and stored in memory.

## File handling

- Recursively indexes `.txt`, `.md`, and `.log` files (lowercase extensions).
- Skips symbolic links, files larger than 1 MiB, and files containing NUL bytes.
- Reads at most 1 MiB plus one byte from each candidate file.
- Escapes control characters in JSON paths.
- Reports traversal or read errors and returns exit code `2` instead of silently reporting complete results.

Supported input is trusted, stationary local text. Concurrent file replacement and adversarial directory mutation are outside the filesystem model. ASCII tokenization is deliberate; Unicode words, stemming, stop-word removal, phrase search, and UTF-8 filename validation are not implemented. Corpus size is not globally capped, so index memory grows with the corpus. The skipped count covers symlinks and rejected candidate files, not every non-text file.

## Tests

`make test` compiles with warnings as errors and checks multi-term matching, ranking, repeated terms, oversized and binary files, symlink exclusion, JSON filename escaping, and invalid queries. GitHub Actions runs on Linux and macOS.

## Further development

Persist the index, measure memory and indexing throughput on a documented corpus, add incremental updates, and compare vector postings against ordered maps. No throughput claims are made without a published benchmark.
